#include <algorithm>
#include <variant>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <map>

#include "StaticForgeBuilder.h"
#include "Internal/InternalHelpers.h"
#include "Internal/StaticForgeMeta.h"
#include "Internal/LZ4Compression.h"

namespace StaticForge {

	StaticForgeBuilder::StaticForgeBuilder()
		: m_compressor(std::make_unique<Internal::LZ4Compression>(Internal::IsLittleEndian(), !Internal::LZ4_STORE_HEADER)) {
	}

	StaticForgeBuilder::StaticForgeBuilder(const std::string& archiveName, const std::vector<StaticForgePath>& sourcePaths, const StaticForgePath& outputPath)
		: m_archiveName(archiveName)
		, m_srcPaths(sourcePaths)
		, m_outputPath(outputPath)
		, m_compressor(std::make_unique<Internal::LZ4Compression>(Internal::IsLittleEndian(), !Internal::LZ4_STORE_HEADER)) {
	}

	StaticForgeBuilder::~StaticForgeBuilder() {
	}

	bool StaticForgeBuilder::Build() {
		Reset();

		std::string error;
		if (!CheckCompressionSettings(m_compressionBlockSize, m_compressionTableSizeLog2, m_compressionMinMatchLength, m_compressionMinSaving, &error)) {
			AddError(error);
			return false;
		}

		if (!CheckFilepaths(&error)) {
			AddError(error);
			return false;
		}

		if (!ScanFiles(&error)) {
			AddError(error);
			return false;
		}

		// archives are written to temporary files first, so a failed build keeps the old archives
		if (!CreateOutputDir(&error) || !CreateGroups(&error) || !CommitArchives(&error)) {
			AddError(error);
			RemoveTempFiles();
			return false;
		}

		return true;
	}

	bool StaticForgeBuilder::CreateOutputDir(std::string* errorOut) {
		namespace fs = std::filesystem;

		const StaticForgePath dir = m_outputPath.has_filename() ? m_outputPath : m_outputPath.parent_path();
		std::error_code ec;
		if (fs::exists(dir, ec))
			return true;

		// the highest directory that gets created, removed again if the build fails
		StaticForgePath root = dir;
		for (StaticForgePath parent = dir.parent_path(); !parent.empty() && parent != root && !fs::exists(parent, ec); parent = parent.parent_path())
			root = parent;

		fs::create_directories(dir, ec);
		if (ec) {
			*errorOut = "Failed to create output directory '" + m_outputPath.generic_u8string() + "': " + ec.message();
			return false;
		}

		m_createdOutputDir = dir;
		m_createdOutputRoot = root;
		return true;
	}

	bool StaticForgeBuilder::CommitArchives(std::string* errorOut) {
		for (const auto& [tempPath, path] : m_tempFiles) {
			std::error_code ec;
			std::filesystem::rename(tempPath, path, ec);
			if (ec) {
				*errorOut = "Failed to rename '" + tempPath.generic_u8string() + "' to '" + path.generic_u8string() + "': " + ec.message();
				return false;
			}
		}

		m_tempFiles.clear();
		return true;
	}

	void StaticForgeBuilder::RemoveTempFiles() {
		namespace fs = std::filesystem;
		std::error_code ec;

		for (const auto& [tempPath, path] : m_tempFiles)
			fs::remove(tempPath, ec);
		m_tempFiles.clear();

		// only empty directories are removed
		if (!m_createdOutputDir.empty()) {
			for (StaticForgePath dir = m_createdOutputDir; ; dir = dir.parent_path()) {
				if (!fs::remove(dir, ec) || dir == m_createdOutputRoot)
					break;
			}
		}
	}

	StaticForgeBuilder& StaticForgeBuilder::SetArchiveName(const std::string& archiveName) {
		m_archiveName = archiveName;
		return *this;
	}

	StaticForgeBuilder& StaticForgeBuilder::SetSourcePath(const std::vector<StaticForgePath>& paths) {
		m_srcPaths = paths;
		return *this;
	}

	StaticForgeBuilder& StaticForgeBuilder::AddSourcePath(const StaticForgePath& path) {
		m_srcPaths.push_back(path);
		return *this;
	}

	StaticForgeBuilder& StaticForgeBuilder::SetOutputPath(const StaticForgePath& path) {
		m_outputPath = path;
		return *this;
	}

	StaticForgeBuilder& StaticForgeBuilder::SetCreateOutputDir(bool value) {
		m_createOutputDir = value;
		return *this;
	}
	
	StaticForgeBuilder& StaticForgeBuilder::SetDebugMode(bool active) {
		m_isDebugActive = active;
		return *this;
	}

	StaticForgeBuilder& StaticForgeBuilder::SetStoreNames(bool active) {
		m_storeNames = active;
		return *this;
	}

	StaticForgeBuilder& StaticForgeBuilder::SetCompress(bool active) {
		m_compress = active;
		return *this;
	}

	StaticForgeBuilder& StaticForgeBuilder::SetCompressionBlockSize(uint64_t size) {
		m_compressionBlockSize = size;
		return *this;
	}

	StaticForgeBuilder& StaticForgeBuilder::SetCompressionTableSizeLog2(uint64_t sizeLog2) {
		m_compressionTableSizeLog2 = sizeLog2;
		return *this;
	}

	StaticForgeBuilder& StaticForgeBuilder::SetCompressionMinMatchLength(uint64_t length) {
		m_compressionMinMatchLength = length;
		return *this;
	}

	StaticForgeBuilder& StaticForgeBuilder::SetCompressionMinSaving(uint64_t percent) {
		m_compressionMinSaving = percent;
		return *this;
	}

	void StaticForgeBuilder::Reset() {
		ClearErrors();
		m_archiveGroups.clear();
		m_tempFiles.clear();
		m_createdOutputDir.clear();
		m_createdOutputRoot.clear();
	}

	bool StaticForgeBuilder::CheckCompressionSettings(
		const std::optional<uint64_t>& blockSize,
		const std::optional<uint64_t>& tableSizeLog2,
		const std::optional<uint64_t>& minMatchLength,
		const std::optional<uint64_t>& minSaving,
		std::string* errorOut
	) const {
		auto inRange = [errorOut](const std::string& name, const std::optional<uint64_t>& value, uint64_t min, uint64_t max) {
			if (!value || (*value >= min && *value <= max))
				return true;

			*errorOut = name + " '" + std::to_string(*value) + "' is out of range, must be between '"
				+ std::to_string(min) + "' and '" + std::to_string(max) + "'";
			return false;
		};

		return inRange("Compression block size", blockSize, MIN_COMPRESSION_BLOCK_SIZE, MAX_COMPRESSION_BLOCK_SIZE)
			&& inRange("Compression table size", tableSizeLog2, MIN_COMPRESSION_TABLE_SIZE_LOG2, MAX_COMPRESSION_TABLE_SIZE_LOG2)
			&& inRange("Compression min match length", minMatchLength, MIN_COMPRESSION_MIN_MATCH_LENGTH, MAX_COMPRESSION_MIN_MATCH_LENGTH)
			&& inRange("Compression min saving", minSaving, MIN_COMPRESSION_MIN_SAVING, MAX_COMPRESSION_MIN_SAVING);
	}

	void StaticForgeBuilder::ApplyCompressionSettings(ArchiveGroup& archive, const Internal::StaticForgeMetaData& meta) const {
		archive.compressionBlockSize = m_compressionBlockSize.value_or(
			meta.compressionBlockSize.value_or(DEFAULT_COMPRESSION_BLOCK_SIZE));

		archive.compressionTableSizeLog2 = static_cast<uint32_t>(m_compressionTableSizeLog2.value_or(
			meta.compressionTableSizeLog2.value_or(DEFAULT_COMPRESSION_TABLE_SIZE_LOG2)));

		archive.compressionMinMatchLength = static_cast<uint32_t>(m_compressionMinMatchLength.value_or(
			meta.compressionMinMatchLength.value_or(DEFAULT_COMPRESSION_MIN_MATCH_LENGTH)));

		archive.compressionMinSaving = static_cast<uint32_t>(m_compressionMinSaving.value_or(
			meta.compressionMinSaving.value_or(DEFAULT_COMPRESSION_MIN_SAVING)));
	}

	// a block or file is only stored compressed if it gets at least minSaving percent smaller
	static bool IsWorthCompressing(uint64_t originalSize, uint64_t compressedSize, uint32_t minSaving) {
		return compressedSize < originalSize
			&& (originalSize - compressedSize) * 100 >= originalSize * minSaving;
	}

	// the archive name becomes a file name, so it must not point to another directory
	static bool IsValidArchiveName(const std::string& name) {
		return !name.empty()
			&& name != "."
			&& name != ".."
			&& name.find('/') == std::string::npos
			&& name.find('\\') == std::string::npos;
	}

	// the path as given, with the absolute path in brackets if it differs
	static std::string PathWithAbsolute(const StaticForgePath& path) {
		std::error_code ec;
		const StaticForgePath absolute = std::filesystem::absolute(path, ec).lexically_normal();
		if (ec || absolute == path)
			return path.u8string();
		return path.u8string() + " (" + absolute.u8string() + ")";
	}

	static std::string ToHex(uint64_t value, int width) {
		std::ostringstream stream;
		stream << std::hex << std::setw(width) << std::setfill('0') << value;
		return stream.str();
	}

	static void PrintSection(const std::string& title) {
		std::cout << Internal::CONSOLE_SEPERATOR << "\n" << title << "\n" << Internal::CONSOLE_SEPERATOR << "\n";
	}

	bool StaticForgeBuilder::CheckFilepaths(std::string* errorOut) {
		if (m_isDebugActive) {
			PrintSection("Paths");
			std::cout << "output path: " << PathWithAbsolute(m_outputPath) << std::endl;

			if (m_srcPaths.size() <= 1) {
				std::cout << "source path: " << (m_srcPaths.empty() ? std::string() : PathWithAbsolute(m_srcPaths[0])) << std::endl;
			}
			else {
				std::cout << "source paths: " << std::endl;
				for (const auto& p : m_srcPaths) {
					std::cout << "- " << PathWithAbsolute(p) << std::endl;
				}
			}

			std::cout << std::endl;
		}

		std::error_code ec;

		if (!IsValidArchiveName(m_archiveName)) {
			*errorOut = "Invalid archive name '" + m_archiveName + "' (must not be empty, contain '/' or '\\' or be '.' or '..')";
			return false;
		}

		if (m_srcPaths.empty()) {
			*errorOut = "No source path set";
			return false;
		}

		if (m_outputPath.empty()) {
			*errorOut = "No output path set";
			return false;
		}

		// check src paths
		for (const auto& p : m_srcPaths) {
			if (!std::filesystem::exists(p, ec)) {
				*errorOut = "Source path '" + p.generic_u8string() + "' does not exist!";
				return false;
			}

			if (!std::filesystem::is_directory(p, ec)) {
				*errorOut = "Source path '" + p.generic_u8string() + "' must be a directory!";
				return false;
			}
		}

		if (Internal::HasExtension(m_outputPath, PACK_FILE_EXTENSION, true)) {
			*errorOut = "Output path must be a directory path, not an archive file: '" + m_outputPath.generic_u8string()
				+ "' (the archive file name comes from the archive name (--name) or a meta file)";
			return false;
		}

		// a missing output directory is created when the archives are written
		if (!std::filesystem::exists(m_outputPath, ec)) {
			if (!m_createOutputDir) {
				*errorOut = "Output path '" + m_outputPath.generic_u8string() + "' does not exist!";
				return false;
			}
		}
		else if (!std::filesystem::is_directory(m_outputPath, ec)) {
			*errorOut = "Output path '" + m_outputPath.generic_u8string() + "' must be a directory!";
			return false;
		}

		return true;
	}

	bool StaticForgeBuilder::ListFiles(const StaticForgePath& directory, std::vector<StaticForgePath>* outFiles, std::string* errorOut) const {
		namespace fs = std::filesystem;

		std::error_code ec;
		fs::recursive_directory_iterator it(directory, fs::directory_options::skip_permission_denied, ec);
		for (; !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
			std::error_code typeEc;
			if (it->is_regular_file(typeEc))
				outFiles->push_back(it->path());
		}

		if (ec) {
			*errorOut = "Failed to read directory '" + directory.generic_u8string() + "': " + ec.message();
			return false;
		}

		// the order of the file system differs between systems, sorted by UTF-8 the build is the same everywhere
		std::sort(outFiles->begin(), outFiles->end(), [](const StaticForgePath& a, const StaticForgePath& b) {
			return a.u8string() < b.u8string();
		});

		return true;
	}

	bool StaticForgeBuilder::ScanFiles(std::string* errorOut) {
		namespace fs = std::filesystem;
		Internal::StaticForgeMeta metaBuilder;
		std::unordered_map<std::string, Internal::StaticForgeMetaData> dirToArchiveMeta;

		// files of every source path, the directories are only read once
		std::vector<std::vector<StaticForgePath>> srcFiles(m_srcPaths.size());
		for (size_t i = 0; i < m_srcPaths.size(); i++) {
			if (!ListFiles(m_srcPaths[i], &srcFiles[i], errorOut))
				return false;
		}

		if (m_isDebugActive)
			PrintSection("Meta files");
		
		// search for meta data files
		for (const auto& files : srcFiles) {
			for (const auto& fullPath : files) {
				if (!Internal::HasExtension(fullPath, META_FILE_EXTENSION))
					continue;

				metaBuilder.Load(fullPath);
				if (!metaBuilder.IsValid()) {
					*errorOut = "Failed to read meta file '" + fullPath.generic_u8string() + "': " + metaBuilder.GetError();
					return false;
				}

				const auto& metaData = metaBuilder.GetLoadedMetaData();
				if (metaData.archiveName.empty()) {
					*errorOut = "Meta file '" + fullPath.generic_u8string() + "' has archive param not set";
					return false;
				}

				if (!IsValidArchiveName(metaData.archiveName)) {
					*errorOut = "Meta file '" + fullPath.generic_u8string() + "' has an invalid archive name '" + metaData.archiveName
						+ "' (must not contain '/' or '\\' and must not be '.' or '..')";
					return false;
				}

				std::string compressionError;
				if (!CheckCompressionSettings(metaData.compressionBlockSize, metaData.compressionTableSizeLog2, metaData.compressionMinMatchLength, metaData.compressionMinSaving, &compressionError)) {
					*errorOut = "Meta file '" + fullPath.generic_u8string() + "': " + compressionError;
					return false;
				}

				std::string dir = fullPath.parent_path().u8string();

				auto it = dirToArchiveMeta.find(dir);
				if (it != dirToArchiveMeta.end()) {
					*errorOut = "Conflicting archive definitions in directory '" + dir + "'";
					return false;
				}

				dirToArchiveMeta[dir] = metaData;
				
				// the values as they are in the meta file, the used values are printed when the archive is written
				if (m_isDebugActive) {
					std::cout << fullPath.generic_u8string() << "\n";
					std::cout << "  - archive: " << metaData.archiveName << "\n";
					std::cout << "  - exclude: [";
					for (size_t i = 0; i < metaData.excludedExtensions.size(); i++)
						std::cout << (i > 0 ? ", " : "") << metaData.excludedExtensions[i];
					std::cout << "]\n";
					std::cout << "  - store-names: " << (metaData.storeNames ? "true" : "false") << "\n";
					std::cout << "  - compress: " << (metaData.compress ? "true" : "false") << "\n";

					auto printOpt = [](const char* name, const std::optional<uint64_t>& v) {
						if (v)
							std::cout << "  - " << name << ": " << *v << "\n";
					};
					printOpt("compress-block-size", metaData.compressionBlockSize);
					printOpt("compress-table-size", metaData.compressionTableSizeLog2);
					printOpt("compress-min-match", metaData.compressionMinMatchLength);
					printOpt("compress-min-saving", metaData.compressionMinSaving);
				}
			}
		}

		if (m_isDebugActive) {
			if (dirToArchiveMeta.empty())
				std::cout << "none, all files go into archive '" << m_archiveName << "'\n";
			std::cout << std::endl;
		}

		// excluded files per archive, only for the debug output
		std::map<std::string, std::vector<std::string>> excludedFiles;

		m_archiveGroups.reserve(dirToArchiveMeta.size());

		// a path may only exist once across all archives of a build
		struct SeenFile {
			std::string relativePath;
			std::string archiveName;
			StaticForgePath fullPath;
		};
		std::unordered_map<uint64_t, SeenFile> seenHashes;

		// adds files to archive groups
		for (size_t srcIndex = 0; srcIndex < m_srcPaths.size(); srcIndex++) {
			const StaticForgePath& src = m_srcPaths[srcIndex];

			for (const auto& fullPath : srcFiles[srcIndex]) {
				std::error_code ec;

				if (Internal::HasExtension(fullPath, META_FILE_EXTENSION))
					continue;

				// computed from the path itself, a symbolic link keeps its own path and the content of its target is stored
				StaticForgePath relativePath = fullPath.lexically_normal().lexically_relative(src.lexically_normal());
				if (relativePath.empty() || *relativePath.begin() == "..") {
					*errorOut = "Failed to compute relative path for: " + fullPath.generic_u8string();
					return false;
				}

				uint64_t fileSize = fs::file_size(fullPath, ec);
				if (ec) {
					*errorOut = "Failed to read file size: " + fullPath.generic_u8string();
					return false;
				}

				auto archiveMeta = ResolveArchive(fullPath, dirToArchiveMeta);
				if (archiveMeta.archiveName.empty()) {
					*errorOut = "Archive name was not set. The name must be provided either via the meta file or manually.";
					return false;
				}

				if (ExcludedFileExtension(fullPath, archiveMeta.excludedExtensions)) {
					if (m_isDebugActive)
						excludedFiles[archiveMeta.archiveName].push_back(fullPath.generic_u8string());
					continue;
				}

				auto& archive = m_archiveGroups[archiveMeta.archiveName];

				// sets the settings of the archive group if it is new
				if (archive.name.empty()) {
					archive.name = archiveMeta.archiveName;
					archive.storeNames = m_storeNames ? true : archiveMeta.storeNames;
					archive.compress = m_compress ? true : archiveMeta.compress;
					ApplyCompressionSettings(archive, archiveMeta);
					archive.files.reserve(50);
				}

				std::string relpathStr = Internal::NormalizeFilepathSlashes(relativePath.u8string());
				uint64_t h = Internal::HashFilename(relpathStr);
				auto seen = seenHashes.find(h);
				if (seen != seenHashes.end()) {
					const SeenFile& other = seen->second;
					if (other.relativePath != relpathStr) {
						*errorOut = "Hash collision: '" + relpathStr +
							"' collides with '" + other.relativePath + "'";
					}
					else {
						*errorOut = "Duplicate path '" + relpathStr + "': '" +
							other.fullPath.generic_u8string() + "' (archive '" + other.archiveName + "') and '" +
							fullPath.generic_u8string() + "' (archive '" + archive.name + "')";
					}
					return false;
				}
				seenHashes[h] = { relpathStr, archive.name, fullPath };

				Internal::StaticForgeFileEntry f{};
				f.filepath = fullPath;
				f.relativeUtf8 = relpathStr;
				f.fileSize = fileSize;
				f.filePadding = 0; // filled during writing
				f.hashName = h;
				f.blockOffset = 0;// filled during writing

				archive.files.push_back(std::move(f));
			}
		}

		// the files of an archive are stored sorted by their path, also when they come from several source paths
		for (auto& [name, archive] : m_archiveGroups) {
			std::sort(archive.files.begin(), archive.files.end(), [](const Internal::StaticForgeFileEntry& a, const Internal::StaticForgeFileEntry& b) {
				return a.relativeUtf8 < b.relativeUtf8;
			});
		}

		// without files an empty archive is written
		if (m_archiveGroups.empty()) {
			m_archiveGroups[m_archiveName] = {};
			m_archiveGroups[m_archiveName].name = m_archiveName;
		}

		if (m_isDebugActive) {
			PrintSection("Files");

			// archives with files and archives where all files were excluded, sorted by name
			std::map<std::string, const ArchiveGroup*> archives;
			for (const auto& [name, archive] : m_archiveGroups)
				archives[name] = &archive;
			for (const auto& [name, _] : excludedFiles)
				archives.emplace(name, nullptr);

			for (const auto& [name, archive] : archives) {
				const size_t fileCount = archive ? archive->files.size() : 0;
				auto excluded = excludedFiles.find(name);

				std::cout << "archive '" << name << "' (" << fileCount << (fileCount == 1 ? " file" : " files");
				if (excluded != excludedFiles.end())
					std::cout << ", " << excluded->second.size() << " excluded";
				if (!archive)
					std::cout << ", not written";
				std::cout << ")\n";

				if (archive) {
					for (const auto& file : archive->files)
						std::cout << "  - " << file.relativeUtf8 << " (" << file.fileSize << " bytes) <- " << file.filepath.generic_u8string() << "\n";
				}

				if (excluded != excludedFiles.end()) {
					for (const auto& path : excluded->second)
						std::cout << "  - excluded: " << path << "\n";
				}
			}

			std::cout << std::endl;
		}

		return true;
	}


	bool StaticForgeBuilder::CreateGroups(std::string* errorOut) {
		/*
		* Order:
		* - calculate data start
		* - insert padding until data start
		* - wirte file data and compress if used
		*   - during writing save block offsets
		* - write header and index table
		* - build name table
		* - write name table
		*/

		std::string error;
		std::ofstream stream;

		// written in order of the name, so the output is always the same
		std::map<std::string, ArchiveGroup*> sortedGroups;
		for (auto& [name, group] : m_archiveGroups)
			sortedGroups[name] = &group;

		for (auto& [archiveName, archivePtr] : sortedGroups) {
			ArchiveGroup& archive = *archivePtr;

			// the name is UTF-8 and may contain dots, so the extension is appended and not replaced
			StaticForgePath outputPath = m_outputPath / std::filesystem::u8path(archive.name + std::string(PACK_FILE_EXTENSION));
			StaticForgePath tempPath = outputPath;
			tempPath += ".tmp";
			m_tempFiles.emplace_back(tempPath, outputPath);

			stream.open(tempPath, std::ios::out | std::ios::binary | std::ios::trunc);
			if (!stream.is_open()) {
				*errorOut = "Failed to create file '" + tempPath.generic_u8string() + "'";
				return false;
			}

			if (!CalculateDataStart(archive, &error)) {
				*errorOut = "Archive '" + archiveName + "': " + error;
				stream.close();
				return false;
			}

			if (m_isDebugActive) {
				PrintSection("Writing archive '" + archive.name + "' -> " + outputPath.generic_u8string());
				std::cout << "store-names: " << (archive.storeNames ? "true" : "false") << "\n";
				std::cout << "compress: " << (archive.compress ? "true" : "false") << "\n";
				if (archive.compress) {
					std::cout << "compression: block size " << archive.compressionBlockSize
						<< ", table size 2^" << archive.compressionTableSizeLog2
						<< ", min match " << archive.compressionMinMatchLength
						<< ", min saving " << archive.compressionMinSaving << "%\n";
				}

				uint64_t headerSize = 0;
				Internal::GetHeaderSize(&headerSize);
				const uint64_t entrySize = sizeof(Internal::StaticForgeIndexEntry);
				std::cout << "header: offset 0, " << headerSize << " bytes (" << sizeof(Internal::StaticForgeHeader) << " + padding)\n";
				std::cout << "index: offset " << headerSize << ", " << archive.files.size() << " x " << entrySize
					<< " = " << archive.files.size() * entrySize << " bytes\n";
				std::cout << "data: offset " << archive.dataStart << "\n";
				std::cout << "files (" << archive.files.size() << "):\n";
			}

			if (!WriteData(archive, stream, &error)) {
				*errorOut = "Archive '" + archiveName + "': " + error;
				stream.close();
				return false;
			}

			if (!WriteHeader(archive, stream, &error)) {
				*errorOut = "Archive '" + archiveName + "': " + error;
				stream.close();
				return false;
			}

			if (!WriteIndex(archive, stream, &error)) {
				*errorOut = "Archive '" + archiveName + "': " + error;
				stream.close();
				return false;
			}

			if (archive.storeNames) {
				stream.seekp(static_cast<std::streamoff>(archive.nameTableStart));
				if (stream.fail()) {
					*errorOut = "Archive '" + archiveName + "': Failed to seek to the name table";
					return false;
				}

				if (!BuildNameTable(archive, &error)) {
					*errorOut = "Archive '" + archiveName + "': " + error;
					stream.close();
					return false;
				}

				if (!WriteNameTable(archive, stream, &error)) {
					*errorOut = "Archive '" + archiveName + "': " + error;
					stream.close();
					return false;
				}
			}

			const uint64_t archiveEnd = archive.storeNames
				? static_cast<uint64_t>(stream.tellp())
				: archive.nameTableStart; // end of the data
			stream.close();

			// a file written again uncompressed can leave old bytes behind the end of the archive
			std::error_code ec;
			const uint64_t fileSize = std::filesystem::file_size(tempPath, ec);
			if (!ec && fileSize > archiveEnd)
				std::filesystem::resize_file(tempPath, archiveEnd, ec);
			if (ec) {
				*errorOut = "Failed to set the size of archive '" + outputPath.generic_u8string() + "': " + ec.message();
				return false;
			}

			if (m_isDebugActive) {
				if (archive.storeNames) {
					const uint64_t headerSize = sizeof(Internal::StaticForgeNameTableHeader);
					const uint64_t entrySize = sizeof(Internal::StaticForgeNameTableEntry);
					const uint64_t stored = archive.nameStringDataCompressed.size();
					std::cout << "name table:\n";
					std::cout << "  - header: offset " << archive.nameTableStart << ", " << headerSize << " bytes\n";
					std::cout << "  - entries: offset " << archive.nameTableStart + headerSize << ", "
						<< archive.files.size() << " x " << entrySize << " = " << archive.files.size() * entrySize << " bytes\n";
					std::cout << "  - names: offset " << archive.nameStringDataStart << ", " << archive.nameStringDataSize << " bytes";
					if (stored == archive.nameStringDataSize)
						std::cout << ", raw\n";
					else
						std::cout << ", compressed " << stored << " bytes\n";
				}

				std::cout << "archive size: " << archiveEnd << " bytes\n" << std::endl;
			}
		}

		return true;
	}

	bool StaticForgeBuilder::CalculateDataStart(ArchiveGroup& archive, std::string* errorOut) const {
		namespace Inter = Internal;

		const uint64_t sizeIndexEntry = sizeof(Inter::StaticForgeIndexEntry);
		uint64_t sizeHeader;
		if (!Inter::GetHeaderSize(&sizeHeader)) {
			*errorOut = "File header alignment overflow file";
			return false;
		}

		size_t fileEntryCount = archive.files.size();

		uint64_t indexTableSize;
		if (!Inter::SafeMulU64(static_cast<uint64_t>(fileEntryCount), sizeIndexEntry, &indexTableSize)) {
			*errorOut = "Index table size overflow";
			return false;
		}

		uint64_t totalHeaderSize;
		if (!Inter::SafeAddU64(sizeHeader, indexTableSize, &totalHeaderSize)) {
			*errorOut = "Archive header + index table size overflow";
			return false;
		}

		if (!Inter::SafeAlignSize(totalHeaderSize, Internal::ALIGNMENT_FILE, &archive.dataStart)) {
			*errorOut = "Archive data alignment overflow";
			return false;
		}

		return true;
	}

	bool StaticForgeBuilder::BuildNameTable(ArchiveGroup& archive, std::string* errorOut) const {
		namespace Inter = Internal;

		const uint64_t sizeNameTableHeader = sizeof(Internal::StaticForgeNameTableHeader);
		const uint64_t sizeNameTableEntry = sizeof(Internal::StaticForgeNameTableEntry);

		// gets set during write data at the end
		// archive.nameTableStart = archive.totalArchiveSize;

		uint64_t nameTableSize;
		if (!Inter::SafeMulU64(static_cast<uint64_t>(archive.files.size()), sizeNameTableEntry, &nameTableSize)) {
			*errorOut = "Name table size overflow";
			return false;
		}

		uint64_t totalNameHeaderSize;
		if (!Inter::SafeAddU64(sizeNameTableHeader, nameTableSize, &totalNameHeaderSize)) {
			*errorOut = "Archive name header + name data table size overflow";
			return false;
		}

		if (!Inter::SafeAddU64(archive.totalArchiveSize, totalNameHeaderSize, &archive.totalArchiveSize)) {
			*errorOut = "Total archive size overflow";
			return false;
		}
		
		archive.nameStringDataStart = archive.totalArchiveSize;
		
		constexpr uint32_t uint32_max = std::numeric_limits<uint32_t>::max();

		// all names packed end to end, compressed as one block below
		std::string stringData;

		uint32_t totalStrOffset = 0;
		for (auto& f : archive.files) {
			size_t strSize = f.relativeUtf8.size();

			if (strSize > uint32_max) {
				*errorOut = "Filename too large to be stored in a uint32_t";
				return false;
			}

			uint32_t strSize32 = static_cast<uint32_t>(strSize);

			if (totalStrOffset > uint32_max - strSize32) {
				*errorOut = "Name table string offset overflow";
				return false;
			}

			f.nameStrDataOffset = totalStrOffset;
			f.nameStrDataLength = strSize32;

			totalStrOffset += strSize32;
			stringData += f.relativeUtf8;
		}

		archive.nameStringDataSize = stringData.size();
		archive.nameStringDataCompressed = m_compressor->Compress(stringData.data(), stringData.size());
		if (!m_compressor->IsValid()) {
			*errorOut = "Failed to compress name table string data: " + m_compressor->GetError();
			return false;
		}

		// stored raw if it doesn't get small enough, marked by compressed size == size
		if (!IsWorthCompressing(stringData.size(), archive.nameStringDataCompressed.size(), archive.compressionMinSaving)) {
			const auto* raw = reinterpret_cast<const std::byte*>(stringData.data());
			archive.nameStringDataCompressed.assign(raw, raw + stringData.size());
		}

		return true;
	}

	bool StaticForgeBuilder::WriteHeader(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const {
		const uint64_t entrySize = static_cast<uint64_t>(archive.files.size());
		
		uint64_t headerSize;
		if (!Internal::GetHeaderSize(&headerSize)) {
			*errorOut = "Failed to calculate aligned header size";
			return false;
		}

		uint64_t indexSize;
		if (!Internal::SafeMulU64(sizeof(Internal::StaticForgeIndexEntry), entrySize, &indexSize)) {
			*errorOut = "Index table size overflow";
			return false;
		}

		Internal::StaticForgeHeader h{};
		h.version = VERSION;
		h.fileCount = entrySize;
		h.indexOffset = headerSize;
		h.indexSize = indexSize;
		h.dataOffset = archive.dataStart;
		h.nameTableHeaderOffset = archive.storeNames ? archive.nameTableStart : 0;

		stream.flush();
		stream.seekp(0);

		if (stream.fail()) {
			*errorOut = "Failed to seek to the header";
			return false;
		}

		if (Internal::IsLittleEndian()) {
			stream.write(
				reinterpret_cast<const char*>(&h),
				sizeof(Internal::StaticForgeHeader)
			);
		}
		else {
			stream.write(h.magic, sizeof(h.magic));
			Internal::WriteLE(stream, h.version);
			Internal::WriteLE(stream, h.fileCount);
			Internal::WriteLE(stream, h.indexOffset);
			Internal::WriteLE(stream, h.indexSize);
			Internal::WriteLE(stream, h.dataOffset);
			Internal::WriteLE(stream, h.nameTableHeaderOffset);
		}

		if (stream.fail()) {
			*errorOut = "Failed to write the header";
			return false;
		}

		// add header padding
		uint64_t padding = headerSize - sizeof(Internal::StaticForgeHeader);
		if (padding > 0) {
			std::vector<char> pad(padding, 0);
			stream.write(pad.data(), padding);

			if (stream.fail()) {
				*errorOut = "Failed to write header padding";
				return false;
			}
		}

		return true;
	}

	bool StaticForgeBuilder::WriteIndex(ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const {
		const uint64_t indexEntrySize = sizeof(Internal::StaticForgeIndexEntry);

		for (size_t i = 0; i < archive.files.size(); i++) {
			auto& fe = archive.files[i];

			Internal::StaticForgeIndexEntry e{};
			e.hashName = fe.hashName;
			e.fileOffset = fe.blockOffset;
			e.fileSize = fe.fileSize;
			e.compressedFileSize = fe.compressedFileSize;
			e.filePadding = fe.filePadding;
			e.checksum = fe.checksum;

			if (Internal::IsLittleEndian()) {
				stream.write(
					reinterpret_cast<const char*>(&e),
					indexEntrySize
				);
			}
			else {
				Internal::WriteLE(stream, e.hashName);
				Internal::WriteLE(stream, e.fileOffset);
				Internal::WriteLE(stream, e.fileSize);
				Internal::WriteLE(stream, e.compressedFileSize);
				Internal::WriteLE(stream, e.filePadding);
				Internal::WriteLE(stream, e.checksum);
			}

			if (stream.fail()) {
				*errorOut = "Failed to write index entry " + std::to_string(i)
					+ " ('" + fe.filepath.generic_u8string() + "')";
				return false;
			}
		}

		return true;
	}

	bool StaticForgeBuilder::WriteData(ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const {
		constexpr uint64_t blockSize = Internal::ALIGNMENT_FILE;// 4096

		// files are read (and compressed) in blocks of this size
		const uint64_t readSize = archive.compressionBlockSize;
		std::vector<char> buffer(readSize);

		m_compressor->SetTableSizeLog2(archive.compressionTableSizeLog2);
		m_compressor->SetMinMatchLength(archive.compressionMinMatchLength);

		// write padding
		// header + index table = archive.dataStart

		std::vector<char> pad(archive.dataStart, 0);
		stream.write(pad.data(), archive.dataStart);

		if (stream.fail()) {
			*errorOut = "Failed to write the space for header and index";
			return false;
		}

		uint64_t totalOffset = 0;
		for (size_t i = 0; i < archive.files.size(); i++) {
			auto& f = archive.files[i];

			// load file
			std::ifstream fileStream(f.filepath, std::ios::binary);
			if (!fileStream.is_open()) {
				*errorOut = "Failed to open file: " + f.filepath.generic_u8string();
				return false;
			}

			bool useCompression = archive.compress
				&& f.fileSize > blockSize;

			const uint64_t fileStart = archive.dataStart + totalOffset;
			uint32_t checksum = 2166136261u;
			uint64_t written = 0;
			// only for the debug output
			uint64_t blockCount = 0;
			uint64_t rawBlockCount = 0;
			uint64_t compressedAttempt = 0;

			while (fileStream) {
				fileStream.read(buffer.data(), static_cast<std::streamsize>(readSize));
				std::streamsize bytesRead = fileStream.gcount();

				if (bytesRead <= 0)
					break;

				if (useCompression) {
					auto compressData = m_compressor->Compress(buffer.data(), bytesRead);

					if (!m_compressor->IsValid()) {
						*errorOut = "Failed to write compressed data block '" + f.filepath.generic_u8string() + "': " + m_compressor->GetError();
						return false;
					}

					// a block that doesn't get small enough is stored raw, marked by compressed size == original size
					const bool storeRaw = !IsWorthCompressing(static_cast<uint64_t>(bytesRead), compressData.size(), archive.compressionMinSaving);
					const char* blockData = storeRaw ? buffer.data() : reinterpret_cast<const char*>(compressData.data());
					const uint64_t dataSize = storeRaw ? static_cast<uint64_t>(bytesRead) : static_cast<uint64_t>(compressData.size());

					// Framing: [uint32_t compressedBlockSize][uint32_t originalBlockSize][data], little endian like the rest of the file
					uint32_t compSize32 = static_cast<uint32_t>(dataSize);
					uint32_t origSize32 = static_cast<uint32_t>(bytesRead);
					if (!Internal::IsLittleEndian()) {
						compSize32 = Internal::SwapEndian(compSize32);
						origSize32 = Internal::SwapEndian(origSize32);
					}

					checksum = Internal::FNV1a(reinterpret_cast<const char*>(&compSize32), sizeof(uint32_t), checksum);
					checksum = Internal::FNV1a(reinterpret_cast<const char*>(&origSize32), sizeof(uint32_t), checksum);
					checksum = Internal::FNV1a(blockData, dataSize, checksum);

					stream.write(reinterpret_cast<const char*>(&compSize32), sizeof(uint32_t));
					stream.write(reinterpret_cast<const char*>(&origSize32), sizeof(uint32_t));
					stream.write(blockData, dataSize);

					written += sizeof(uint32_t) * 2 + dataSize;
					blockCount++;
					if (storeRaw)
						rawBlockCount++;
				}
				else {
					checksum = Internal::FNV1a(buffer.data(), bytesRead, checksum);

					stream.write(
						reinterpret_cast<const char*>(buffer.data()),
						bytesRead
					);

					written += bytesRead;
				}

				if (stream.fail()) {
					*errorOut = "Failed to write data block '" + f.filepath.generic_u8string() + "'";
					return false;
				}
			}

			// if the whole file doesn't get small enough, it is written again uncompressed at the same position
			if (useCompression && !IsWorthCompressing(f.fileSize, written, archive.compressionMinSaving)) {
				compressedAttempt = written;
				useCompression = false;
				checksum = 2166136261u;
				written = 0;

				stream.seekp(static_cast<std::streamoff>(fileStart));
				fileStream.clear();
				fileStream.seekg(0);
				if (stream.fail() || fileStream.fail()) {
					*errorOut = "Failed to seek for writing '" + f.filepath.generic_u8string() + "' uncompressed";
					return false;
				}

				while (fileStream) {
					fileStream.read(buffer.data(), static_cast<std::streamsize>(readSize));
					std::streamsize bytesRead = fileStream.gcount();
					if (bytesRead <= 0)
						break;

					checksum = Internal::FNV1a(buffer.data(), bytesRead, checksum);
					stream.write(buffer.data(), bytesRead);
					written += bytesRead;

					if (stream.fail()) {
						*errorOut = "Failed to write data block '" + f.filepath.generic_u8string() + "'";
						return false;
					}
				}
			}

			if (written != f.fileSize && !useCompression) {
				*errorOut = "Failed to write data block '" + f.filepath.generic_u8string() + "'";
				return false;
			}

			// add padding
			uint64_t fileAligned64 = 0;
			if (!Internal::SafeAlignSize(written, blockSize, &fileAligned64)) {
				*errorOut = "File alignment overflow file size: '" + std::to_string(written) +
					"' bytes; aligment size: '" + std::to_string(blockSize) + " bytes'";
				return false;
			}

			// file padding cant be larger than uint32_t
			if ((fileAligned64 - written) > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max() - 1)) {
				*errorOut = "Failed to write data block, file padding too large '" + f.filepath.generic_u8string() + "'";
				return false;
			}
			uint32_t filePadding = static_cast<uint32_t>(fileAligned64 - written);

			if (filePadding > blockSize) {
				*errorOut = "Failed to write data block, invalid file padding computed";
				return false;
			}

			if (filePadding > 0) {
				std::vector<char> filePad(filePadding, 0);
				stream.write(filePad.data(), filePadding);

				if (stream.fail()) {
					*errorOut = "Failed to write data padding";
					return false;
				}
			}

			f.filePadding = filePadding;
			f.compressedFileSize = useCompression ? written : 0;
			f.blockOffset = totalOffset;
			f.checksum = checksum;

			uint64_t alignedFileSize;
			if (!Internal::SafeAddU64(written, static_cast<uint64_t>(filePadding), &alignedFileSize)) {
				*errorOut =
					"Aligned file size overflow for file '" +
					f.relativeUtf8 + "'";
				return false;
			}

			if (!Internal::SafeAddU64(totalOffset, alignedFileSize, &totalOffset)) {
				*errorOut =
					"Archive data offset overflow while processing file '" +
					f.relativeUtf8 + "'";
				return false;
			}

			if (m_isDebugActive) {
				std::cout << "  [" << i << "] " << f.relativeUtf8 << "\n";
				std::cout << "      - hash: " << ToHex(f.hashName, 16) << "\n";
				std::cout << "      - offset: " << f.blockOffset << " (in file " << fileStart << ")\n";
				std::cout << "      - size: " << f.fileSize << " bytes\n";

				std::cout << "      - stored: ";
				if (useCompression) {
					std::cout << written << " bytes compressed (-" << (f.fileSize - written) * 100 / f.fileSize << "%), "
						<< blockCount << (blockCount == 1 ? " block" : " blocks");
					if (rawBlockCount > 0)
						std::cout << ", " << rawBlockCount << " raw";
					std::cout << "\n";
				}
				else if (!archive.compress)
					std::cout << "raw\n";
				else if (f.fileSize <= blockSize)
					std::cout << "raw, too small to compress (<= " << blockSize << " bytes)\n";
				else
					std::cout << "raw, compressed " << compressedAttempt << " bytes, saving below " << archive.compressionMinSaving << "%\n";

				std::cout << "      - padding: " << f.filePadding << " bytes (aligned " << alignedFileSize << " bytes)\n";
				std::cout << "      - checksum: " << ToHex(checksum, 8) << "\n";
			}
		}

		if (!Internal::SafeAddU64(archive.dataStart, totalOffset, &archive.totalArchiveSize)) {
			*errorOut = "Total archive size overflow";
			return false;
		}

		archive.nameTableStart = archive.totalArchiveSize;

		return true;
	}

	bool StaticForgeBuilder::WriteNameTable(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const {
		std::string error;
		if (!WriteNameTableHeader(archive, stream, &error)) {
			*errorOut = error;
			return false;
		}

		if (!WriteNameTableData(archive, stream, &error)) {
			*errorOut = error;
			return false;
		}

		if (!WriteNameTableStringData(archive, stream, &error)) {
			*errorOut = error;
			return false;
		}

		return true;
	}

	bool StaticForgeBuilder::WriteNameTableHeader(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const {
		uint64_t headerSize = sizeof(Internal::StaticForgeNameTableHeader);
		
		Internal::StaticForgeNameTableHeader h{};
		h.entryOffset = archive.nameTableStart + headerSize;
		h.stringDataOffset = archive.nameStringDataStart;
		h.stringDataSize = archive.nameStringDataSize;
		h.stringDataCompressedSize = static_cast<uint64_t>(archive.nameStringDataCompressed.size());

		if (Internal::IsLittleEndian()) {
			stream.write(
				reinterpret_cast<const char*>(&h),
				headerSize
			);
		}
		else {
			Internal::WriteLE(stream, h.entryOffset);
			Internal::WriteLE(stream, h.stringDataOffset);
			Internal::WriteLE(stream, h.stringDataSize);
			Internal::WriteLE(stream, h.stringDataCompressedSize);
		}

		if (stream.fail()) {
			*errorOut = "Failed to write the name table header";
			return false;
		}

		return true;
	}

	bool StaticForgeBuilder::WriteNameTableData(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const {
		for (size_t i = 0; i < archive.files.size(); i++) {
			using Entry = Internal::StaticForgeNameTableEntry;
			auto& f = archive.files[i];

			Entry e{};
			e.hash = f.hashName;
			e.nameLength = f.nameStrDataLength;
			e.nameOffset = f.nameStrDataOffset;

			if (Internal::IsLittleEndian()) {
				stream.write(
					reinterpret_cast<const char*>(&e),
					sizeof(Entry)
				);
			}
			else {
				Internal::WriteLE(stream, e.hash);
				Internal::WriteLE(stream, e.nameLength);
				Internal::WriteLE(stream, e.nameOffset);
			}

			if (stream.fail()) {
				*errorOut = "Failed to write name table entry " + std::to_string(i)
					+ " ('" + f.filepath.generic_u8string() + "')";
				return false;
			}
		}
		
		return true;
	}

	bool StaticForgeBuilder::WriteNameTableStringData(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const {
		const auto& data = archive.nameStringDataCompressed;
		stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));

		if (stream.fail()) {
			*errorOut = "Failed to write the name table string data";
			return false;
		}
		return true;
	}

	Internal::StaticForgeMetaData StaticForgeBuilder::ResolveArchive(
		const StaticForgePath& filePath,
		const std::unordered_map<std::string, Internal::StaticForgeMetaData>& dirToArchiveMeta
	) const {
		StaticForgePath dir = filePath.parent_path();

		while (!dir.empty()) {
			auto it = dirToArchiveMeta.find(dir.u8string());
			if (it != dirToArchiveMeta.end())
				return it->second;

			StaticForgePath parent = dir.parent_path();
			if (parent == dir)
				break;
			dir = parent;
		}

		Internal::StaticForgeMetaData dummy{};
		dummy.archiveName = m_archiveName;

		return dummy;
	}

	bool StaticForgeBuilder::ExcludedFileExtension(const StaticForgePath& filePath, const std::vector<std::string>& extensions) const {
		for (const auto& ext : extensions) {
			if (Internal::HasExtension(filePath, ext, true))
				return true;
		}

		return false;
	}

}