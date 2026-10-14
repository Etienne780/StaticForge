#include <fstream>
#include <sstream>
#include <charconv>

#include "Internal/BuildCache.h"
#include "Internal/InternalHelpers.h"

namespace StaticForge::Internal {

	static bool ReadLine(std::istream& in, std::string* lineOut) {
		if (!std::getline(in, *lineOut))
			return false;
		if (!lineOut->empty() && lineOut->back() == '\r')
			lineOut->pop_back();
		return true;
	}

	// reads a line "<key> <value>"
	static bool ReadValue(std::istream& in, std::string_view key, std::string* valueOut) {
		std::string line;
		if (!ReadLine(in, &line))
			return false;
		if (line.size() <= key.size() || line.compare(0, key.size(), key) != 0 || line[key.size()] != ' ')
			return false;

		*valueOut = line.substr(key.size() + 1);
		return true;
	}

	// splits the first space separated token off the text
	static bool NextToken(std::string_view* text, std::string_view* tokenOut) {
		const size_t end = text->find(' ');
		if (end == 0 || text->empty())
			return false;

		*tokenOut = text->substr(0, end);
		text->remove_prefix(end == std::string_view::npos ? text->size() : end + 1);
		return true;
	}

	template<typename T>
	static bool ParseNumber(std::string_view text, T* valueOut, int base = 10) {
		auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), *valueOut, base);
		return ec == std::errc{} && end == text.data() + text.size();
	}

	bool ReadBuildCache(const StaticForgePath& path, BuildCache* cacheOut) {
		std::ifstream in(path, std::ios::binary);
		if (!in.is_open())
			return false;

		std::string value;
		uint64_t cacheVersion = 0;
		if (!ReadValue(in, "StaticForgeCache", &value) || !ParseNumber(value, &cacheVersion) || cacheVersion != BUILD_CACHE_VERSION)
			return false;

		if (!ReadValue(in, "format", &value) || !ParseNumber(value, &cacheOut->formatVersion))
			return false;

		if (!ReadValue(in, "output", &cacheOut->outputPath) || !ReadValue(in, "settings", &cacheOut->settings))
			return false;

		std::string_view rest;
		std::string_view token;
		if (!ReadValue(in, "archive", &value))
			return false;
		rest = value;
		if (!NextToken(&rest, &token) || !ParseNumber(token, &cacheOut->archiveSize)
			|| !NextToken(&rest, &token) || !ParseNumber(token, &cacheOut->archiveModified) || !rest.empty())
			return false;

		uint64_t fileCount = 0;
		if (!ReadValue(in, "files", &value) || !ParseNumber(value, &fileCount))
			return false;

		cacheOut->files.clear();
		std::string line;
		for (uint64_t i = 0; i < fileCount; i++) {
			if (!ReadLine(in, &line))
				return false;

			// "<size> <modified> <content hash> <path>", the path is the rest of the line and may contain spaces
			BuildCacheFile file;
			rest = line;
			if (!NextToken(&rest, &token) || !ParseNumber(token, &file.size)
				|| !NextToken(&rest, &token) || !ParseNumber(token, &file.modified)
				|| !NextToken(&rest, &token) || !ParseNumber(token, &file.contentHash, 16)
				|| rest.empty())
				return false;

			file.path = std::string(rest);
			cacheOut->files.push_back(std::move(file));
		}

		return true;
	}

	bool WriteBuildCache(const StaticForgePath& path, const BuildCache& cache, std::string* errorOut) {
		namespace fs = std::filesystem;

		std::ostringstream out;
		out << "StaticForgeCache " << BUILD_CACHE_VERSION << "\n";
		out << "format " << cache.formatVersion << "\n";
		out << "output " << cache.outputPath << "\n";
		out << "settings " << cache.settings << "\n";
		out << "archive " << cache.archiveSize << " " << cache.archiveModified << "\n";
		out << "files " << cache.files.size() << "\n";
		for (const auto& file : cache.files)
			out << file.size << " " << file.modified << " " << std::hex << file.contentHash << std::dec << " " << file.path << "\n";

		std::error_code ec;
		fs::create_directories(path.parent_path(), ec);
		if (ec) {
			*errorOut = "Failed to create cache directory '" + path.parent_path().generic_u8string() + "': " + ec.message();
			return false;
		}

		// written to a temporary file first and renamed when it is complete
		StaticForgePath tempPath = path;
		tempPath += ".tmp";
		{
			std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
			const std::string text = out.str();
			file.write(text.data(), static_cast<std::streamsize>(text.size()));
			if (!file) {
				*errorOut = "Failed to write cache file '" + tempPath.generic_u8string() + "'";
				fs::remove(tempPath, ec);
				return false;
			}
		}

		fs::rename(tempPath, path, ec);
		if (ec) {
			*errorOut = "Failed to rename cache file '" + tempPath.generic_u8string() + "': " + ec.message();
			fs::remove(tempPath, ec);
			return false;
		}

		return true;
	}

	bool GetModifiedTime(const StaticForgePath& path, int64_t* timeOut) {
		std::error_code ec;
		const auto time = std::filesystem::last_write_time(path, ec);
		if (ec)
			return false;

		*timeOut = static_cast<int64_t>(time.time_since_epoch().count());
		return true;
	}

	bool HashFileContent(const StaticForgePath& path, uint64_t* hashOut) {
		std::ifstream in(path, std::ios::binary);
		if (!in.is_open())
			return false;

		std::vector<char> buffer(64 * 1024);
		uint64_t hash = FNV1a64(nullptr, 0);
		while (in) {
			in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
			const std::streamsize read = in.gcount();
			if (read <= 0)
				break;
			hash = FNV1a64(buffer.data(), static_cast<size_t>(read), hash);
		}

		if (in.bad())
			return false;

		*hashOut = hash;
		return true;
	}

}
