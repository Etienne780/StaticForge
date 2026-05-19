#include <iostream>
#include <variant>

#include "StaticForgeBuilder.h"
#include "Internal/InternalHelpers.h"
#include "Internal/StaticForgeMeta.h"

namespace StaticForge {

	StaticForgeBuilder::StaticForgeBuilder()
		: ErrorSupport(Internal::HAS_HEADER) {
	}

	StaticForgeBuilder::StaticForgeBuilder(const std::string& archiveName, const std::vector<StaticForgePath>& sourcePaths, const StaticForgePath& outputPath)
		: ErrorSupport(Internal::HAS_HEADER), m_archiveName(archiveName), m_srcPaths(sourcePaths), m_outputPath(outputPath) {
	}

	bool StaticForgeBuilder::Build() {
		std::string error;
		if (!CheckFilepaths(&error)) {
			AddError("CheckFilepaths: " + error);
			return false;
		}

		if (!ScanFiles(&error)) {
			AddError("ScanFiles: " + error);
			return false;
		}

		if (!BuildGroups(&error)) {
			AddError("BuildGroups: " + error);
			return false;
		}

		return true;
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

	bool StaticForgeBuilder::CheckFilepaths(std::string* errorOut) {
		if (m_isDebugActive) {
			std::cout << Internal::CONSOLE_SEPERATOR << std::endl;
			std::cout << "Validating file paths" << std::endl;
			std::cout << Internal::CONSOLE_SEPERATOR << std::endl;
			std::cout << "output path: " << m_outputPath << std::endl;
			
			if (m_srcPaths.size() <= 1) {
				std::cout << "source path: " << (m_srcPaths.empty() ? "" : m_srcPaths[0]) << std::endl;
			}
			else {
				std::cout << "source paths: " << std::endl;
				for (const auto& p : m_srcPaths) {
					std::cout << "- " << p << std::endl;
				}
			}

			std::cout << std::endl;
		}

		// check src paths
		for (const auto& p : m_srcPaths) {
			if (!std::filesystem::exists(p)) {
				*errorOut = "Source path '" + p.u8string() + "' does not exist!";
				return false;
			}
		}

		if (m_outputPath.has_extension()) {
			*errorOut = "Output path must be a directory path: '" + m_outputPath.u8string() + "'";
			return false;
		}

		// checks/creates output path
		if (!std::filesystem::exists(m_outputPath)) {
			if (!m_createOutputDir) {
				*errorOut = "Output path '" + m_outputPath.u8string() + "' does not exist!";
				return false;
			}
			else {
				if (!std::filesystem::create_directories((m_outputPath.has_filename() ? m_outputPath : m_outputPath.parent_path()))) {
					*errorOut = "Failed to create output directory '" + m_outputPath.u8string() + "'!";
					return false;
				}
			}
		}

		return true;
	}

	bool StaticForgeBuilder::ScanFiles(std::string* errorOut) {
		namespace fs = std::filesystem;
		Internal::StaticForgeMeta metaBuilder;
		std::unordered_map<std::string, std::string> dirToArchive;

		if (m_isDebugActive) {
			std::cout << Internal::CONSOLE_SEPERATOR << std::endl;
			std::cout << "Scaning files" << std::endl;
			std::cout << Internal::CONSOLE_SEPERATOR << std::endl;

			std::cout << "found meta files:" << std::endl;
		}

		for (const auto& src : m_srcPaths) {
			for (const auto& entry : fs::recursive_directory_iterator(src, fs::directory_options::skip_permission_denied)) {
				if (!entry.is_regular_file())
					continue;

				StaticForgePath fullPath = entry.path();
				std::string ext = Internal::GetFullExtension(fullPath);
				if (ext != META_FILE_EXTENSION)
					continue;

				metaBuilder.Load(fullPath);
				if (!metaBuilder.IsValid()) {
					*errorOut = "Failed to read meta file '" + fullPath.u8string() + "': " + metaBuilder.GetError();
					return false;
				}

				std::string archiveName = metaBuilder.GetArchiveName();
				if (archiveName.empty()) {
					*errorOut = "Meta file '" + fullPath.u8string() + "' has archive param not set";
					return false;
				}

				if (m_isDebugActive) {
					std::cout << "  meta file:" << std::endl;
					std::cout << "  - path:" << fullPath << std::endl;
					std::cout << "  - archiveName: " << archiveName << std::endl;
					std::cout << std::endl;
				}

				std::string dir = entry.path().parent_path().u8string();
				dirToArchive[dir] = metaBuilder.GetArchiveName();
			}
		}

		// if no meta files where  found
		if (m_isDebugActive && dirToArchive.empty())
			std::cout << "- none" << std::endl << std::endl;

		m_archiveGroups.reserve(dirToArchive.size());

		for (const auto& src : m_srcPaths) {
			for (const auto& entry : fs::recursive_directory_iterator(src, fs::directory_options::skip_permission_denied)) {
				std::error_code ec;
				if (!entry.is_regular_file(ec)) {
					ec.clear();
					continue;
				}

				StaticForgePath fullPath = entry.path();

				if (Internal::GetFullExtension(fullPath) == META_FILE_EXTENSION) {
					ec.clear();
					continue;
				}

				StaticForgePath relativePath = fs::relative(fullPath, src, ec);
				if (ec) {
					*errorOut = "Failed to compute relative path for: " + fullPath.u8string();
					return false;
				}

				uint64_t fileSize = entry.file_size(ec);
				if (ec) {
					*errorOut = "Failed to read file size: " + fullPath.u8string();
					return false;
				}

				std::string archiveName = ResolveArchive(entry.path(), dirToArchive);
				if (archiveName.empty()) {
					*errorOut = "Archive name was not set. The name must be provided either via the meta file or manually.";
					return false;
				}

				auto& archive = m_archiveGroups[archiveName];

				if (archive.name.empty())
					archive.name = archiveName;

				std::string relpathStr = relativePath.u8string();
				uint64_t h = Internal::HashFilename(relpathStr);
				if (archive.seenHashes.count(h)) {
					*errorOut = "Hash collision: '" + relpathStr +
						"' collides with '" + archive.seenHashes[h] + "'";
					return false;
				}
				archive.seenHashes[h] = relpathStr;

				Internal::StaticForgeFileEntry f{};
				f.filepath = fullPath;
				f.fileSize = fileSize;
				f.filePadding = static_cast<uint32_t>(Internal::AlignSize(fileSize, Internal::ALIGNMENT_FILE) - fileSize);
				f.hashName = h;
				f.blockOffset = 0;

				archive.files.push_back(std::move(f));
			}
		}

		if (m_isDebugActive) {
			std::cout << "found files:" << std::endl;

			std::vector<std::string> archiveNames;
			archiveNames.reserve(m_archiveGroups.size());

			for (const auto& [name, _] : m_archiveGroups) {
				archiveNames.push_back(name);
			}

			std::sort(archiveNames.begin(), archiveNames.end());

			for (const auto& archiveName : archiveNames) {
				const auto& archive = m_archiveGroups.at(archiveName);

				std::cout << "archive: " << archiveName << std::endl;
				std::cout << "  file count: " << archive.files.size() << std::endl;

				for (const auto& file : archive.files) {
					std::cout
						<< "  - file: " << file.filepath.u8string() << std::endl
						<< "      hash: " << file.hashName << std::endl
						<< "      file size: " << file.fileSize << " bytes" << std::endl
						<< "      aligned file size: " << file.fileSize + file.filePadding<< " bytes" << std::endl;
				}

				std::cout << std::endl;
			}

			std::cout << std::endl;
		}

		return true;
	}


	bool StaticForgeBuilder::BuildGroups(std::string* errorOut) {
		std::string error;

		for (auto& [archiveName, archive] : m_archiveGroups) {
			if (!BuildIndex(archive, &error)) {
				*errorOut = "BuildIndex of group '" + archiveName + "': " + error;
				return false;
			}

			if (!WriteFile(archive, &error)) {
				*errorOut = "WriteFile of group '" + archiveName + "': " + error;
				return false;
			}
		}

		return true;
	}

	bool StaticForgeBuilder::BuildIndex(ArchiveGroup& archive, std::string* errorOut) const {
		uint64_t sizeHeader = Internal::GetHeaderSize();
		uint64_t sizeIndexEntry = Internal::GetIndexEntrySize();

		size_t fileEntryCount = archive.files.size();
		uint64_t totalOffset =
			Internal::AlignSize(
				sizeHeader + (static_cast<uint64_t>(fileEntryCount) * sizeIndexEntry),
				Internal::ALIGNMENT_FILE
			);

		archive.dataStart = totalOffset;

		if (m_isDebugActive) {
			std::cout << Internal::CONSOLE_SEPERATOR << std::endl;
			std::cout << "Building index tabel for archive '" << archive.name << "(" << archive.files.size() << ")'" << std::endl;
			std::cout << Internal::CONSOLE_SEPERATOR << std::endl;
			std::cout << "entrys:" << std::endl;
		}

		for (size_t i = 0; i < fileEntryCount; i++) {
			auto& f = archive.files[i];

			f.blockOffset = totalOffset;
			totalOffset += f.fileSize + f.filePadding;

			if (m_isDebugActive) {
				std::cout << "  path: " << f.filepath << std::endl;
				std::cout << "  blockOffset: " << f.blockOffset << std::endl;
			}
		}

		archive.totalArchiveSize = totalOffset;

		if (m_isDebugActive) {
			std::cout << "total size: " << totalOffset << " bytes" << std::endl;
			std::cout << std::endl;
		}

		return true;
	}

	bool StaticForgeBuilder::WriteFile(ArchiveGroup& archive, std::string* errorOut) const {
		StaticForgePath outputPath = m_outputPath / archive.name;
		outputPath.replace_extension(PACK_FILE_EXTENSION);

		if (!IsEnoughSpaceAvailable(outputPath.parent_path(), archive.totalArchiveSize)) {
			*errorOut = "Not enough space available size needed '" + 
				Internal::GetFormatedSizeStr(archive.totalArchiveSize) + 
				"'";
			return false;
		}

		std::string error;

		std::ofstream stream;
		stream.open(outputPath, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!stream.is_open()) {
			*errorOut = "Failed to create file with path '" + outputPath.u8string() + "'";
			return false;
		}

		if (m_isDebugActive) {
			std::cout << Internal::CONSOLE_SEPERATOR << std::endl;
			std::cout << "Writer for archive'" << archive.name << "(" << archive.files.size() << ")'" << std::endl;
			std::cout << Internal::CONSOLE_SEPERATOR << std::endl;
		}

		if (!WriteHeader(archive, stream, &error)) {
			*errorOut = "WriteHeader: " + error;
			stream.close();
			return false;
		}

		if (!WriteIndex(archive, stream, &error)) {
			*errorOut = "WriteIndex: " + error;
			stream.close();
			return false;
		}

		if (!WriteData(archive, stream, &error)) {
			*errorOut = "WriteData: " + error;
			stream.close();
			return false;
		}

		stream.close();
		return true;
	}

	bool StaticForgeBuilder::WriteHeader(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const {
		const uint64_t entrySize = static_cast<uint64_t>(archive.files.size());
		const uint64_t headerSize = Internal::GetHeaderSize();
		const uint64_t indexSize = Internal::GetIndexEntrySize() * entrySize;

		Internal::StaticForgeHeader h{};
		h.version = VERSION;
		h.fileCount = entrySize;
		h.indexOffset = headerSize;
		h.indexSize = indexSize;
		h.dataOffset = archive.dataStart;


		if (Internal::IsLittleEndian()) {
			stream.write(
				reinterpret_cast<const char*>(&h),
				sizeof(Internal::StaticForgeHeader)
			);
		}
		else {
			auto writeLE = [&stream](auto value) {
				auto le = Internal::SwapEndian(value);
				stream.write(reinterpret_cast<const char*>(&le), sizeof(le));
			};

			stream.write(h.magic, sizeof(h.magic));
			writeLE(h.version);
			writeLE(h.fileCount);
			writeLE(h.indexOffset);
			writeLE(h.indexSize);
			writeLE(h.dataOffset);
		}

		if (stream.fail()) {
			*errorOut = "Failed to write header!";
			return false;
		}

		// add padding
		uint64_t padding = headerSize - sizeof(Internal::StaticForgeHeader);
		if (padding > 0) {
			std::vector<char> pad(padding, 0);
			stream.write(pad.data(), padding);

			if (stream.fail()) {
				*errorOut = "Failed to write data padding";
				return false;
			}
		}

		if (m_isDebugActive) {
			std::cout << "header:" << std::endl;
			std::cout << "  offset: " << 0 << std::endl;
			std::cout << "  size: " << headerSize << std::endl;
			std::cout << std::endl;
		}

		return true;
	}

	bool StaticForgeBuilder::WriteIndex(ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const {
		const uint64_t indexEntrySize = Internal::GetIndexEntrySize();

		if (m_isDebugActive) {
			std::cout << "index (" + std::to_string(archive.files.size()) + "):" << std::endl;
		}

		for (size_t i = 0; i < archive.files.size(); i++) {
			auto& fe = archive.files[i];

			Internal::StaticForgeIndexEntry e{};
			e.hashName = fe.hashName;
			e.fileOffset = fe.blockOffset;
			e.fileSize = fe.fileSize;
			e.filePadding = fe.filePadding;
			e.checksum = 0;

			fe.indexOffset = stream.tellp();


			if (Internal::IsLittleEndian()) {
				stream.write(
					reinterpret_cast<const char*>(&e),
					sizeof(Internal::StaticForgeIndexEntry)
				);
			}
			else {
				auto writeLE = [&stream](auto value) {
					auto le = Internal::SwapEndian(value);
					stream.write(reinterpret_cast<const char*>(&le), sizeof(le));
				};

				writeLE(e.hashName);
				writeLE(e.fileOffset);
				writeLE(e.fileSize);
				writeLE(e.filePadding);
				writeLE(e.checksum);
			}

			if (stream.fail()) {
				*errorOut = "Failed to write index!";
				return false;
			}

			if (m_isDebugActive) {
				std::cout << "  entry " << i << ":" << std::endl;
				std::cout << "  - offset: " << fe.indexOffset << std::endl;
				std::cout << "  - size: " << indexEntrySize << std::endl;
			}
		}

		if (m_isDebugActive) {
			std::cout << std::endl;
		}

		return true;
	}

	bool StaticForgeBuilder::WriteData(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const {
		constexpr uint64_t blockSize = Internal::ALIGNMENT_FILE;// 4096

		if (m_isDebugActive) {
			std::cout << "data (" + std::to_string(archive.files.size()) + "):" << std::endl;
		}

		uint64_t current = static_cast<uint64_t>(stream.tellp());

		if (archive.dataStart > current) {
			uint64_t padding = archive.dataStart - current;

			std::vector<char> pad(padding, 0);
			stream.write(pad.data(), padding);

			if (stream.fail()) {
				*errorOut = "Failed to write data0 padding";
				return false;
			}
		}

		for (size_t i = 0; i < archive.files.size(); i++) {
			auto& f = archive.files[i];

			// load file
			std::ifstream fileStream(f.filepath, std::ios::binary);
			if (!fileStream.is_open()) {
				*errorOut = "Failed to open file: " + f.filepath.u8string();
				return false;
			}

			std::vector<char> buffer(blockSize);

			uint32_t checksum = 2166136261u;
			uint64_t written = 0;

			while (fileStream) {
				fileStream.read(buffer.data(), blockSize);
				std::streamsize bytesRead = fileStream.gcount();

				if (bytesRead <= 0)
					break;

				checksum = Internal::FNV1a(buffer.data(), bytesRead, checksum);

				stream.write(buffer.data(), bytesRead);

				if (stream.fail()) {
					*errorOut = "Failed to write data block '" + f.filepath.u8string() + "'";
					return false;
				}

				written += bytesRead;
			}

			if (written != f.fileSize) {
				*errorOut = "Failed to write data block '" + f.filepath.u8string() + "'";
				return false;
			}

			// add padding
			uint64_t padding = f.filePadding;
			if (padding > 0) {
				std::vector<char> pad(padding, 0);
				stream.write(pad.data(), padding);

				if (stream.fail()) {
					*errorOut = "Failed to write data padding";
					return false;
				}
			}

			auto currentPos = stream.tellp();

			// update checksum value
			auto pos = f.indexOffset;
			auto checksumPos =
				pos + offsetof(Internal::StaticForgeIndexEntry, checksum);

			stream.flush();
			stream.seekp(checksumPos);

			if (Internal::IsLittleEndian()) {
				stream.write(
					reinterpret_cast<const char*>(&checksum),
					sizeof(checksum)
				);
			}
			else {
				auto writeLE = [&stream](auto value) {
					auto le = Internal::SwapEndian(value);
					stream.write(reinterpret_cast<const char*>(&le), sizeof(le));
				};

				writeLE(checksum);
			}

			stream.seekp(currentPos);

			if (m_isDebugActive) {
				std::cout << "  data " << i << ":" << std::endl;
				std::cout << "  - offset: " << f.blockOffset<< std::endl;
				std::cout << "  - file size: " << f.fileSize << std::endl;
				std::cout << "  - file padding: " << f.filePadding << std::endl;
				std::cout << "  - file aligned size: " << f.fileSize + f.filePadding << std::endl;
				std::cout << "  - checksum: " << checksum << std::endl;
			}
		}

		if (m_isDebugActive) {
			std::cout << std::endl;
		}

		return true;
	}

	std::string StaticForgeBuilder::ResolveArchive(
		const StaticForgePath& filePath,
		const std::unordered_map<std::string, std::string>& dirToArchive
	) {
		StaticForgePath dir = filePath.parent_path();

		while (!dir.empty()) {
			auto it = dirToArchive.find(dir.u8string());
			if (it != dirToArchive.end())
				return it->second;

			StaticForgePath parent = dir.parent_path();
			if (parent == dir)
				break;
			dir = parent;
		}

		return m_archiveName;
	}

	bool StaticForgeBuilder::IsEnoughSpaceAvailable(const StaticForgePath& path, uint64_t fileSize) {
		namespace fs = std::filesystem;

		std::error_code ec;
		fs::space_info space = fs::space(path, ec);

		if (ec)
			return false;

		return space.available >= static_cast<std::uintmax_t>(fileSize);
	}

}