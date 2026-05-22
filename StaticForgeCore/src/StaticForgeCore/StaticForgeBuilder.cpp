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

	StaticForgeBuilder& StaticForgeBuilder::SetStoreNames(bool active) {
		m_storeNames = active;
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
		std::unordered_map<std::string, Internal::StaticForgeMetaData> dirToArchiveMeta;

		if (m_isDebugActive) {
			std::cout << Internal::CONSOLE_SEPERATOR << std::endl;
			std::cout << "Scanning  files" << std::endl;
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

				std::string dir = entry.path().parent_path().u8string();

				auto it = dirToArchiveMeta.find(dir);
				if (it != dirToArchiveMeta.end()) {
					*errorOut = "Conflicting archive definitions in directory '" + dir + "'";
					return false;
				}

				Internal::StaticForgeMetaData metaData{};
				metaData.archiveName = archiveName;
				metaData.excludedExtensions = metaBuilder.GetExcludedExtensions();
				metaData.storeNames = metaBuilder.GetStoreNames();

				dirToArchiveMeta[dir] = metaData;

				if (m_isDebugActive) {
					std::cout << "  meta file:" << std::endl;
					std::cout << "  - path:" << fullPath << std::endl;
					std::cout << "  - archiveName: " << archiveName << std::endl;
					std::cout << std::endl;
				}
			}
		}

		// if no meta files were found
		if (m_isDebugActive && dirToArchiveMeta.empty())
			std::cout << "- none" << std::endl << std::endl;

		m_archiveGroups.reserve(dirToArchiveMeta.size());

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

				auto archiveMeta = ResolveArchive(entry.path(), dirToArchiveMeta);
				if (archiveMeta.archiveName.empty()) {
					*errorOut = "Archive name was not set. The name must be provided either via the meta file or manually.";
					return false;
				}

				if (ExcludedFileExtension(relativePath.extension(), archiveMeta.excludedExtensions)) {
					continue;
				}

				auto& archive = m_archiveGroups[archiveMeta.archiveName];

				if (archive.name.empty()) {
					archive.name = archiveMeta.archiveName;
					archive.storeNames = m_storeNames ? true : archiveMeta.storeNames;
					archive.files.reserve(50);
				}

				std::string relpathStr = Internal::NormalizeFilepathSlashes(relativePath.u8string());
				uint64_t h = Internal::HashFilename(relpathStr);
				if (archive.seenHashes.count(h)) {
					*errorOut = "Hash collision: '" + relpathStr +
						"' collides with '" + archive.seenHashes[h] + "'";
					return false;
				}
				archive.seenHashes[h] = relpathStr;

				uint64_t fileAligned64;
				if (!Internal::SafeAlignSize(fileSize, Internal::ALIGNMENT_FILE, &fileAligned64)) {
					*errorOut = "File alignment overflow file size: '" + std::to_string(fileSize) + 
						"' bytes; aligment size: '" + std::to_string(Internal::ALIGNMENT_FILE) + " bytes'";
					return false;
				}

				Internal::StaticForgeFileEntry f{};
				f.filepath = fullPath;
				f.relativeUtf8 = relpathStr;
				f.fileSize = fileSize;
				f.filePadding = static_cast<uint32_t>(fileAligned64 - fileSize);
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

		if (m_archiveGroups.empty()) {
			m_archiveGroups[m_archiveName] = {};
			m_archiveGroups[m_archiveName].name = m_archiveName;
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

			if (archive.storeNames) {
				if (!BuildNameTable(archive, &error)) {
					*errorOut = "BuildNameTable of group '" + archiveName + "': " + error;
					return false;
				}
			}

			if (!WriteFile(archive, &error)) {
				*errorOut = "WriteFile of group '" + archiveName + "': " + error;
				return false;
			}
		}

		return true;
	}

	bool StaticForgeBuilder::BuildIndex(ArchiveGroup& archive, std::string* errorOut) const {
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

		if (m_isDebugActive) {
			std::cout << Inter::CONSOLE_SEPERATOR << std::endl;
			std::cout << "Building index table for archive '" << archive.name << "(" << archive.files.size() << ")'" << std::endl;
			std::cout << Inter::CONSOLE_SEPERATOR << std::endl;
			std::cout << "entries:" << std::endl;
		}

		uint64_t totalOffset = 0;
		for (size_t i = 0; i < fileEntryCount; i++) {
			auto& f = archive.files[i];

			f.blockOffset = totalOffset;

			if (f.filePadding > Internal::ALIGNMENT_FILE) {
				*errorOut = "Invalid padding computed";
				return false;
			}

			uint64_t alignedFileSize;
			if (!Inter::SafeAddU64(f.fileSize, static_cast<uint64_t>(f.filePadding), &alignedFileSize)) {
				*errorOut =
					"Aligned file size overflow for file '" +
					f.relativeUtf8 + "'";
				return false;
			}

			if (!Inter::SafeAddU64(totalOffset, alignedFileSize, &totalOffset)) {
				*errorOut =
					"Archive data offset overflow while processing file '" +
					f.relativeUtf8 + "'";
				return false;
			}

			if (m_isDebugActive) {
				std::cout << "  path: " << f.filepath << std::endl;
				std::cout << "  blockOffset: " << f.blockOffset << std::endl;
			}
		}

		if (!Inter::SafeAddU64(archive.dataStart, totalOffset, &archive.totalArchiveSize)) {
			*errorOut = "Total archive size overflow";
			return false;
		}

		if (archive.totalArchiveSize > MAX_ARCHIVE_SIZE) {
			*errorOut = "Archive too large";
			return false;
		}

		if (m_isDebugActive) {
			std::cout << "total size: " << archive.totalArchiveSize << " bytes" << std::endl;
			std::cout << std::endl;
		}

		return true;
	}

	bool StaticForgeBuilder::BuildNameTable(ArchiveGroup& archive, std::string* errorOut) const {
		namespace Inter = Internal;

		const uint64_t sizeNameTableHeader = sizeof(Internal::StaticForgeNameTableHeader);
		const uint64_t sizeNameTableEntry = sizeof(Internal::StaticForgeNameTableEntry);
		
		if (m_isDebugActive) {
			std::cout << Internal::CONSOLE_SEPERATOR << std::endl;
			std::cout << "Building name table offsets for archive '" << archive.name << "(" << archive.files.size() << ")'" << std::endl;
			std::cout << Internal::CONSOLE_SEPERATOR << std::endl;
		}

		archive.nameTableStart = archive.totalArchiveSize;

		if (archive.files.size() > UINT64_MAX) {
			*errorOut = "Too many files";
			return false;
		}

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
		
		uint32_t totalStrOffset = 0;
		for (auto& f : archive.files) {
			size_t strSize = f.relativeUtf8.size();

			if (strSize > UINT32_MAX) {
				*errorOut = "Filename too large for uint32_t storage";
				return false;
			}

			uint32_t strSize32 = static_cast<uint32_t>(strSize);

			if (totalStrOffset > UINT32_MAX - strSize32) {
				*errorOut = "Name table string offset overflow";
				return false;
			}

			f.nameStrDataOffset = totalStrOffset;
			f.nameStrDataLength = strSize32;

			totalStrOffset += strSize32;
		}

		archive.nameStringDataSize = archive.totalArchiveSize - archive.nameStringDataStart;

		if (m_isDebugActive) {
			std::cout << "total size: " << archive.totalArchiveSize << " bytes" << std::endl;
			std::cout << "name table start: " << archive.nameTableStart << std::endl;
			std::cout << "name data start: " << archive.nameStringDataStart << std::endl;
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
			std::cout << "Writer for archive '" << archive.name << "(" << archive.files.size() << ")'" << std::endl;
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

		if (archive.storeNames) {
			if (!WriteNameTable(archive, stream, &error)) {
				*errorOut = "WriteNameTable: " + error;
				stream.close();
				return false;
			}
		}

		stream.close();
		return true;
	}

	bool StaticForgeBuilder::WriteHeader(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const {
		const uint64_t entrySize = static_cast<uint64_t>(archive.files.size());
		
		uint64_t headerSize;
		if (!Internal::GetHeaderSize(&headerSize)) {
			*errorOut = "mach besser error auf egnlisch";
			return false;
		}
		
		uint64_t indexSize;
		if (!Internal::SafeMulU64(sizeof(Internal::StaticForgeIndexEntry), entrySize, &indexSize)) {
			*errorOut = "mach besser error auf egnlisch";
			return false;
		}

		Internal::StaticForgeHeader h{};
		h.version = VERSION;
		h.fileCount = entrySize;
		h.indexOffset = headerSize;
		h.indexSize = indexSize;
		h.dataOffset = archive.dataStart;
		h.nameTableHeaderOffset = archive.storeNames ? archive.nameTableStart : 0;


		if (Internal::IsLittleEndian()) {
			stream.write(
				reinterpret_cast<const char*>(&h),
				sizeof(Internal::StaticForgeHeader)
			);
		}
		else {
			stream.write(h.magic, sizeof(h.magic));
			WriteLE(stream, h.version);
			WriteLE(stream, h.fileCount);
			WriteLE(stream, h.indexOffset);
			WriteLE(stream, h.indexSize);
			WriteLE(stream, h.dataOffset);
			WriteLE(stream, h.nameTableHeaderOffset);
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
		const uint64_t indexEntrySize = sizeof(Internal::StaticForgeIndexEntry);

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

			auto pos = stream.tellp();
			if (pos < 0) {
				*errorOut = "tellp failed";
				return false;
			}
			fe.indexOffset = static_cast<uint64_t>(pos);

			if (Internal::IsLittleEndian()) {
				stream.write(
					reinterpret_cast<const char*>(&e),
					indexEntrySize
				);
			}
			else {
				WriteLE(stream, e.hashName);
				WriteLE(stream, e.fileOffset);
				WriteLE(stream, e.fileSize);
				WriteLE(stream, e.filePadding);
				WriteLE(stream, e.checksum);
			}

			if (stream.fail()) {
				*errorOut = "Failed to write index at index'" 
					+ std::to_string(i) + "' name='" 
					+ fe.filepath.u8string() + "'";
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
		std::vector<char> buffer(blockSize);

		if (m_isDebugActive) {
			std::cout << "data (" + std::to_string(archive.files.size()) + "):" << std::endl;
		}

		std::streampos pos = stream.tellp();
		if (pos < 0) {
			*errorOut = "tellp failed";
			return false;
		}

		uint64_t current = static_cast<uint64_t>(static_cast<std::streamoff>(pos));

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
			auto indexPos = f.indexOffset;
			uint64_t checksumPos;
			if (!Internal::SafeAddU64(
				static_cast<uint64_t>(indexPos),
				offsetof(Internal::StaticForgeIndexEntry, checksum),
				&checksumPos))
			{
				*errorOut = "Checksum position overflow";
				return false;
			}

			stream.flush();
			stream.seekp(checksumPos);

			if (stream.fail()) {
				*errorOut = "seekp failed";
				return false;
			}

			if (Internal::IsLittleEndian()) {
				stream.write(
					reinterpret_cast<const char*>(&checksum),
					sizeof(checksum)
				);
			}
			else {
				WriteLE(stream, checksum);
			}

			stream.seekp(currentPos);

			if (stream.fail()) {
				*errorOut = "seekp failed";
				return false;
			}

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

	bool StaticForgeBuilder::WriteNameTable(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const {
		std::string error;
		if (!WriteNameTableHeader(archive, stream, &error)) {
			*errorOut = "Header: " + error;
			return false;
		}

		if (!WriteNameTableData(archive, stream, &error)) {
			*errorOut = "Data: " + error;
			return false;
		}

		if (!WriteNameTableStringData(archive, stream, &error)) {
			*errorOut = "StringData: " + error;
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

		if (Internal::IsLittleEndian()) {
			stream.write(
				reinterpret_cast<const char*>(&h),
				headerSize
			);
		}
		else {
			WriteLE(stream, h.entryOffset);
			WriteLE(stream, h.stringDataOffset);
			WriteLE(stream, h.stringDataSize);
		}

		if (stream.fail()) {
			*errorOut = "Failed to write header!";
			return false;
		}

		if (m_isDebugActive) {
			std::cout << "Name table header:" << std::endl;
			std::cout << "  offset: " << archive.nameTableStart << std::endl;
			std::cout << "  size: " << headerSize << std::endl;
			std::cout << std::endl;
		}

		return true;
	}

	bool StaticForgeBuilder::WriteNameTableData(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const {
		if (m_isDebugActive) {
			std::cout << "name table (" + std::to_string(archive.files.size()) + "):" << std::endl;
		}
		
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
				auto writeLE = [&stream](auto value) {
					auto le = Internal::SwapEndian(value);
					stream.write(reinterpret_cast<const char*>(&le), sizeof(le));
				};

				writeLE(e.hash);
				writeLE(e.nameLength);
				writeLE(e.nameOffset);
			}

			if (stream.fail()) {
				*errorOut = "Failed to write index at index'"
					+ std::to_string(i) + "' name='"
					+ f.filepath.u8string() + "'";
				return false;
			}
		}
		
		return true;
	}

	bool StaticForgeBuilder::WriteNameTableStringData(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const {
		for (size_t i = 0; i < archive.files.size(); i++) {
			auto& f = archive.files[i];
			
			std::string name = f.relativeUtf8;

			stream.write(
				name.data(),
				name.size()
			);

			if (stream.fail()) {
				*errorOut = "Failed to write string data at index'"
					+ std::to_string(i) + "' name='"
					+ f.filepath.u8string() + "'";
				return false;
			}
		}
		return true;
	}

	Internal::StaticForgeMetaData StaticForgeBuilder::ResolveArchive(
		const StaticForgePath& filePath,
		const std::unordered_map<std::string, Internal::StaticForgeMetaData>& dirToArchiveMeta
	) {
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

	bool StaticForgeBuilder::ExcludedFileExtension(const StaticForgePath& extension, const std::vector<std::string>& extensions) {
		if (extensions.empty())
			return false;

		std::string ext = extension.u8string();
		std::transform(ext.begin(), ext.end(), ext.begin(),
			[](unsigned char c) { return std::tolower(c); });
		
		return std::find(extensions.begin(), extensions.end(), ext) != extensions.end();
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