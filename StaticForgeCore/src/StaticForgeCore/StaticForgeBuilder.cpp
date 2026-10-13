#include <algorithm>
#include <variant>
#include <iostream>

#include "StaticForgeBuilder.h"
#include "Internal/InternalHelpers.h"
#include "Internal/StaticForgeMeta.h"
#include "Internal/LZ4Compression.h"

namespace StaticForge {

	StaticForgeBuilder::StaticForgeBuilder()
		: ErrorSupport(HAS_HEADER)
		, m_compressor(std::make_unique<Internal::LZ4Compression>(Internal::IsLittleEndian(), !Internal::LZ4_STORE_HEADER)) {
	}

	StaticForgeBuilder::StaticForgeBuilder(const std::string& archiveName, const std::vector<StaticForgePath>& sourcePaths, const StaticForgePath& outputPath)
		: ErrorSupport(HAS_HEADER)
		, m_archiveName(archiveName)
		, m_srcPaths(sourcePaths)
		, m_outputPath(outputPath)
		, m_compressor(std::make_unique<Internal::LZ4Compression>(Internal::IsLittleEndian(), !Internal::LZ4_STORE_HEADER)) {
	}

	StaticForgeBuilder::~StaticForgeBuilder() {
	}

	bool StaticForgeBuilder::Build() {
		Reset();

		std::string error;
		if (!CheckCompressionSettings(m_compressionBlockSize, m_compressionTableSizeLog2, m_compressionMinMatchLength, &error)) {
			AddError("CheckCompressionSettings: " + error);
			return false;
		}

		if (!CheckFilepaths(&error)) {
			AddError("CheckFilepaths: " + error);
			return false;
		}

		if (!ScanFiles(&error)) {
			AddError("ScanFiles: " + error);
			return false;
		}

		if (!CreateGroups(&error)) {
			AddError("CreateGroups: " + error);
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

	void StaticForgeBuilder::Reset() {
		ClearErrors();
		m_archiveGroups.clear();
	}

	bool StaticForgeBuilder::CheckCompressionSettings(
		const std::optional<uint64_t>& blockSize,
		const std::optional<uint64_t>& tableSizeLog2,
		const std::optional<uint64_t>& minMatchLength,
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
			&& inRange("Compression min match length", minMatchLength, MIN_COMPRESSION_MIN_MATCH_LENGTH, MAX_COMPRESSION_MIN_MATCH_LENGTH);
	}

	void StaticForgeBuilder::ApplyCompressionSettings(ArchiveGroup& archive, const Internal::StaticForgeMetaData& meta) const {
		archive.compressionBlockSize = m_compressionBlockSize.value_or(
			meta.compressionBlockSize.value_or(DEFAULT_COMPRESSION_BLOCK_SIZE));

		archive.compressionTableSizeLog2 = static_cast<uint32_t>(m_compressionTableSizeLog2.value_or(
			meta.compressionTableSizeLog2.value_or(DEFAULT_COMPRESSION_TABLE_SIZE_LOG2)));

		archive.compressionMinMatchLength = static_cast<uint32_t>(m_compressionMinMatchLength.value_or(
			meta.compressionMinMatchLength.value_or(DEFAULT_COMPRESSION_MIN_MATCH_LENGTH)));
	}

	// the archive name becomes a file name, so it must not point to another directory
	static bool IsValidArchiveName(const std::string& name) {
		return !name.empty()
			&& name != "."
			&& name != ".."
			&& name.find('/') == std::string::npos
			&& name.find('\\') == std::string::npos;
	}

	bool StaticForgeBuilder::CheckFilepaths(std::string* errorOut) {
		if (m_isDebugActive) {
			std::cout << Internal::CONSOLE_SEPERATOR << std::endl;
			std::cout << "Validating file paths" << std::endl;
			std::cout << Internal::CONSOLE_SEPERATOR << std::endl;
			std::cout << "output path: " << m_outputPath.u8string() << std::endl;
			
			if (m_srcPaths.size() <= 1) {
				std::cout << "source path: " << (m_srcPaths.empty() ? std::string() : m_srcPaths[0].u8string()) << std::endl;
			}
			else {
				std::cout << "source paths: " << std::endl;
				for (const auto& p : m_srcPaths) {
					std::cout << "- " << p.u8string() << std::endl;
				}
			}

			std::cout << std::endl;
		}

		std::error_code ec;

		if (!IsValidArchiveName(m_archiveName)) {
			*errorOut = "Invalid archive name '" + m_archiveName + "' (must not be empty, contain '/' or '\\' or be '.' or '..')";
			return false;
		}

		// check src paths
		for (const auto& p : m_srcPaths) {
			if (!std::filesystem::exists(p, ec)) {
				*errorOut = "Source path '" + p.u8string() + "' does not exist!";
				return false;
			}

			if (!std::filesystem::is_directory(p, ec)) {
				*errorOut = "Source path '" + p.u8string() + "' must be a directory!";
				return false;
			}
		}

		if (Internal::HasExtension(m_outputPath, PACK_FILE_EXTENSION, true)) {
			*errorOut = "Output path must be a directory path, not an archive file: '" + m_outputPath.u8string()
				+ "' (the archive file name comes from the archive name (--name) or a meta file)";
			return false;
		}

		// checks/creates output path
		if (!std::filesystem::exists(m_outputPath, ec)) {
			if (!m_createOutputDir) {
				*errorOut = "Output path '" + m_outputPath.u8string() + "' does not exist!";
				return false;
			}
			else {
				std::filesystem::create_directories((m_outputPath.has_filename() ? m_outputPath : m_outputPath.parent_path()), ec);
				if (ec) {
					*errorOut = "Failed to create output directory '" + m_outputPath.u8string() + "': " + ec.message();
					return false;
				}
			}
		}
		else if (!std::filesystem::is_directory(m_outputPath, ec)) {
			*errorOut = "Output path '" + m_outputPath.u8string() + "' must be a directory!";
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
			*errorOut = "Failed to read directory '" + directory.u8string() + "': " + ec.message();
			return false;
		}

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

		if (m_isDebugActive) {
			std::cout << Internal::CONSOLE_SEPERATOR << "\n";
			std::cout << "Scanning  files" << "\n";
			std::cout << Internal::CONSOLE_SEPERATOR << "\n";

			std::cout << "found meta files:" << std::endl;
		}
		
		// search for meta data files
		for (const auto& files : srcFiles) {
			for (const auto& fullPath : files) {
				if (!Internal::HasExtension(fullPath, META_FILE_EXTENSION))
					continue;

				metaBuilder.Load(fullPath);
				if (!metaBuilder.IsValid()) {
					*errorOut = "Failed to read meta file '" + fullPath.u8string() + "': " + metaBuilder.GetError();
					return false;
				}

				const auto& metaData = metaBuilder.GetLoadedMetaData();
				if (metaData.archiveName.empty()) {
					*errorOut = "Meta file '" + fullPath.u8string() + "' has archive param not set";
					return false;
				}

				if (!IsValidArchiveName(metaData.archiveName)) {
					*errorOut = "Meta file '" + fullPath.u8string() + "' has an invalid archive name '" + metaData.archiveName
						+ "' (must not contain '/' or '\\' and must not be '.' or '..')";
					return false;
				}

				std::string compressionError;
				if (!CheckCompressionSettings(metaData.compressionBlockSize, metaData.compressionTableSizeLog2, metaData.compressionMinMatchLength, &compressionError)) {
					*errorOut = "Meta file '" + fullPath.u8string() + "': " + compressionError;
					return false;
				}

				std::string dir = fullPath.parent_path().u8string();

				auto it = dirToArchiveMeta.find(dir);
				if (it != dirToArchiveMeta.end()) {
					*errorOut = "Conflicting archive definitions in directory '" + dir + "'";
					return false;
				}

				dirToArchiveMeta[dir] = metaData;
				
				if (m_isDebugActive) {
					auto booltoStr = [](bool v) -> const char*{
						return v ? "true" : "false";
					};
					
					std::cout << "  meta file:" << "\n";
					std::cout << "  - path: " << fullPath.u8string() << "\n";
					std::cout << "  - archiveName: " << metaData.archiveName << "\n";
					std::cout << "  - exclude: [";
					
					const size_t excludeSize = metaData.excludedExtensions.size();
					if (excludeSize > 0) {
						for (size_t i = 0; i < excludeSize - 1; i++) {
							std::cout << metaData.excludedExtensions[i] << ", ";
						}

						std::cout << metaData.excludedExtensions[excludeSize - 1];
					}
					std::cout << "]\n";

					std::cout << "  - store-names: " << booltoStr(m_storeNames ? true : metaData.storeNames) << "\n";
					std::cout << "  - compress: " << booltoStr(m_compress ? true : metaData.compress) << "\n";

					auto optToStr = [](const std::optional<uint64_t>& v) -> std::string {
						return v ? std::to_string(*v) : "not set";
					};
					std::cout << "  - compress-block-size: " << optToStr(metaData.compressionBlockSize) << "\n";
					std::cout << "  - compress-table-size: " << optToStr(metaData.compressionTableSizeLog2) << "\n";
					std::cout << "  - compress-min-match: " << optToStr(metaData.compressionMinMatchLength) << "\n";
					std::cout << std::endl;
				}
			}
		}

		// if no meta files were found
		if (m_isDebugActive && dirToArchiveMeta.empty())
			std::cout << "- none" << std::endl << std::endl;

		m_archiveGroups.reserve(dirToArchiveMeta.size());

		// adds files to archive groups
		for (size_t srcIndex = 0; srcIndex < m_srcPaths.size(); srcIndex++) {
			const StaticForgePath& src = m_srcPaths[srcIndex];

			for (const auto& fullPath : srcFiles[srcIndex]) {
				std::error_code ec;

				if (Internal::HasExtension(fullPath, META_FILE_EXTENSION))
					continue;

				StaticForgePath relativePath = fs::relative(fullPath, src, ec);
				if (ec) {
					*errorOut = "Failed to compute relative path for: " + fullPath.u8string();
					return false;
				}

				uint64_t fileSize = fs::file_size(fullPath, ec);
				if (ec) {
					*errorOut = "Failed to read file size: " + fullPath.u8string();
					return false;
				}

				auto archiveMeta = ResolveArchive(fullPath, dirToArchiveMeta);
				if (archiveMeta.archiveName.empty()) {
					*errorOut = "Archive name was not set. The name must be provided either via the meta file or manually.";
					return false;
				}

				if (ExcludedFileExtension(fullPath, archiveMeta.excludedExtensions))
					continue;

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
				if (archive.seenHashes.count(h)) {
					*errorOut = "Hash collision: '" + relpathStr +
						"' collides with '" + archive.seenHashes[h] + "'";
					return false;
				}
				archive.seenHashes[h] = relpathStr;

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

		for (auto& [archiveName, archive] : m_archiveGroups) {
			// the name is UTF-8 and may contain dots, so the extension is appended and not replaced
			StaticForgePath outputPath = m_outputPath / std::filesystem::u8path(archive.name + std::string(PACK_FILE_EXTENSION));

			stream.open(outputPath, std::ios::out | std::ios::binary | std::ios::trunc);
			if (!stream.is_open()) {
				*errorOut = "Failed to create file with path '" + outputPath.u8string() + "'";
				return false;
			}

			if (!CalculateDataStart(archive, &error)) {
				*errorOut = "CalculateDataStart of group '" + archiveName + "': " + error;
				stream.close();
				return false;
			}

			if (!WriteData(archive, stream, &error)) {
				*errorOut = "WriteData: " + error;
				stream.close();
				return false;
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

			if (archive.storeNames) {
				stream.seekp(static_cast<std::streamoff>(archive.nameTableStart));
				if (stream.fail()) {
					*errorOut = "seekp to name table start failed";
					return false;
				}

				if (!BuildNameTable(archive, &error)) {
					*errorOut = "BuildNameTable of group '" + archiveName + "': " + error;
					stream.close();
					return false;
				}

				if (!WriteNameTable(archive, stream, &error)) {
					*errorOut = "WriteNameTable: " + error;
					stream.close();
					return false;
				}
			}

			stream.close();
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
		
		if (m_isDebugActive) {
			std::cout << Internal::CONSOLE_SEPERATOR << std::endl;
			std::cout << "Building name table offsets for archive '" << archive.name << "(" << archive.files.size() << ")'" << std::endl;
			std::cout << Internal::CONSOLE_SEPERATOR << std::endl;
		}

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

		if (m_isDebugActive) {
			std::cout << "total size: " << archive.totalArchiveSize << " bytes" << std::endl;
			std::cout << "name table start: " << archive.nameTableStart << std::endl;
			std::cout << "name data start: " << archive.nameStringDataStart << std::endl;
			std::cout << "name data size: " << archive.nameStringDataSize << " bytes, compressed: "
				<< archive.nameStringDataCompressed.size() << " bytes" << std::endl;
			std::cout << std::endl;
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
			*errorOut = "seekp failed";
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
			*errorOut = "Failed to write header!";
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
				*errorOut = "Failed to write index at index'" 
					+ std::to_string(i) + "' name='" 
					+ fe.filepath.u8string() + "'";
				return false;
			}

			if (m_isDebugActive) {
				std::cout << "  entry " << i << ":" << std::endl;
				std::cout << "  - size: " << indexEntrySize << std::endl;
			}
		}

		if (m_isDebugActive) {
			std::cout << std::endl;
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

		if (m_isDebugActive && archive.compress) {
			std::cout << "compression: block size " << archive.compressionBlockSize
				<< ", table size 2^" << archive.compressionTableSizeLog2
				<< ", min match length " << archive.compressionMinMatchLength << std::endl;
		}

		// write padding
		// header + index table = archive.dataStart

		std::vector<char> pad(archive.dataStart, 0);
		stream.write(pad.data(), archive.dataStart);

		if (stream.fail()) {
			*errorOut = "Failed to write data0 padding";
			return false;
		}

		if (m_isDebugActive) {
			std::cout << "data (" + std::to_string(archive.files.size()) + "):" << std::endl;
		}

		uint64_t totalOffset = 0;
		for (size_t i = 0; i < archive.files.size(); i++) {
			auto& f = archive.files[i];

			// load file
			std::ifstream fileStream(f.filepath, std::ios::binary);
			if (!fileStream.is_open()) {
				*errorOut = "Failed to open file: " + f.filepath.u8string();
				return false;
			}

			const bool useCompression = archive.compress 
				&& f.fileSize > blockSize;

			uint32_t checksum = 2166136261u;
			uint64_t written = 0;

			while (fileStream) {
				fileStream.read(buffer.data(), static_cast<std::streamsize>(readSize));
				std::streamsize bytesRead = fileStream.gcount();

				if (bytesRead <= 0)
					break;

				if (useCompression) {
					auto compressData = m_compressor->Compress(buffer.data(), bytesRead);
					uint64_t dataSize = static_cast<uint64_t>(compressData.size());

					if (!m_compressor->IsValid()) {
						*errorOut = "Failed to write compressed data block '" + f.filepath.u8string() + "':\n   " + m_compressor->GetError();
						return false;
					}

					// Framing: [uint32_t compressedBlockSize][uint32_t originalBlockSize][data], little endian like the rest of the file
					uint32_t compSize32 = static_cast<uint32_t>(dataSize);
					uint32_t origSize32 = static_cast<uint32_t>(bytesRead);
					if (!Internal::IsLittleEndian()) {
						compSize32 = Internal::SwapEndian(compSize32);
						origSize32 = Internal::SwapEndian(origSize32);
					}

					checksum = Internal::FNV1a(reinterpret_cast<const char*>(&compSize32), sizeof(uint32_t), checksum);
					checksum = Internal::FNV1a(reinterpret_cast<const char*>(&origSize32), sizeof(uint32_t), checksum);
					checksum = Internal::FNV1a(compressData.data(), dataSize, checksum);

					stream.write(reinterpret_cast<const char*>(&compSize32), sizeof(uint32_t));
					stream.write(reinterpret_cast<const char*>(&origSize32), sizeof(uint32_t));
					stream.write(reinterpret_cast<const char*>(compressData.data()), dataSize);

					written += sizeof(uint32_t) * 2 + dataSize;
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
					*errorOut = "Failed to write data block '" + f.filepath.u8string() + "'";
					return false;
				}				
			}

			if (written != f.fileSize && !useCompression) {
				*errorOut = "Failed to write data block '" + f.filepath.u8string() + "'";
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
				*errorOut = "Failed to write data block, file padding to large '" + f.filepath.u8string() + "'";
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
				std::cout << "  data " << i << ":\n";
				std::cout << "  - offset: " << f.blockOffset << "\n";
				std::cout << "  - file size: " << f.fileSize << "\n";
				if (useCompression)
					std::cout << "  - compressed file size: " << written << "\n";
				std::cout << "  - file padding: " << f.filePadding << "\n";
				std::cout << "  - file aligned size: " << alignedFileSize << "\n";
				std::cout << "  - checksum: " << checksum << "\n";
			}
		}

		if (!Internal::SafeAddU64(archive.dataStart, totalOffset, &archive.totalArchiveSize)) {
			*errorOut = "Total archive size overflow";
			return false;
		}

		archive.nameTableStart = archive.totalArchiveSize;

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
			*errorOut = "Table Data: " + error;
			return false;
		}

		if (!WriteNameTableStringData(archive, stream, &error)) {
			*errorOut = "String Data: " + error;
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
				Internal::WriteLE(stream, e.hash);
				Internal::WriteLE(stream, e.nameLength);
				Internal::WriteLE(stream, e.nameOffset);
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
		const auto& data = archive.nameStringDataCompressed;
		stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));

		if (stream.fail()) {
			*errorOut = "Failed to write compressed string data";
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