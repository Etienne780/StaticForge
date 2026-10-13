#include <cstring>

#include "Internal/InternalHelpers.h"
#include "Internal/LZ4Compression.h"
#include "StaticForgeReader.h"
#include "StaticForgeArchive.h"

namespace StaticForge {

	// checks that [offset, offset + size) is inside the archive file
	static bool InFile(uint64_t offset, uint64_t size, uint64_t fileSize) {
		uint64_t end = 0;
		return Internal::SafeAddU64(offset, size, &end) && end <= fileSize;
	}

	StaticForgeReader::StaticForgeReader()
		: ErrorSupport(HAS_HEADER) {
	}

	bool StaticForgeReader::Load(
		const StaticForgePath& path,
		StaticForgeArchive* archiveOut
	) {
		ClearErrors();

		if (!archiveOut) {
			AddError("Load needs a valid 'StaticForgeArchive' object");
			return false;
		}

		std::string error;
		if (!ValidatePackPath(path, &error)) {
			AddError("ValidatePackPath: " + error);
			return false;
		}

		std::error_code ec;
		const uint64_t fileSize = std::filesystem::file_size(path, ec);
		if (ec) {
			AddError("Failed to get file size of '" + path.u8string() + "': " + ec.message());
			return false;
		}

		// resets the archive so it is allways fresh
		archiveOut->Reset();

		// open file
		archiveOut->m_path = path;
		archiveOut->OpenFileStream();
		if (!archiveOut->m_stream.is_open()) {
			AddError("Failed to open file");
			return false;
		}

		if (!ReadHeader(archiveOut, fileSize, &error)) {
			AddError("ReadHeader: " + error);
			return false;
		}

		if (!ReadIndex(archiveOut, fileSize, &error)) {
			AddError("ReadIndex: " + error);
			return false;
		}

		if (archiveOut->StoresNames()) {
			if (!ReadNameTable(archiveOut, fileSize, &error)) {
				AddError("ReadNameTable: " + error);
				return false;
			}
		}
		
		return true;
	}

	bool StaticForgeReader::ValidatePackPath(const StaticForgePath& path, std::string* errorOut) {
		if (!path.has_extension()) {
			*errorOut = "path musst be a path to a '" + std::string(PACK_FILE_EXTENSION) + "' file";
			return false;
		}

		StaticForgePath extension = path.extension();
		if (extension != PACK_FILE_EXTENSION) {
			*errorOut = "Invalid extension '" + extension.u8string() + "' musst be '" + std::string(PACK_FILE_EXTENSION) + "'";
			return false;
		}

		std::error_code ec;
		if (!std::filesystem::is_regular_file(path, ec)) {
			*errorOut = "Archive '" + path.u8string() + "' does not exist or is not a file!";
			return false;
		}

		return true;
	}

	bool StaticForgeReader::ReadHeader(StaticForgeArchive* archive, uint64_t fileSize, std::string* errorOut) const {
		auto& stream = archive->m_stream;
		auto& header = archive->m_header;

		constexpr char expectedMagic[4] = {
			'S', 'F', 'P', 'K'
		};

		stream.read(header.magic, sizeof(header.magic));
		if (stream.gcount() != sizeof(header.magic)) {
			*errorOut = "Failed to read magic";
			return false;
		}

		if (std::memcmp(header.magic, expectedMagic, sizeof(expectedMagic)) != 0) {
			*errorOut = "Invalid archive magic";
			return false;
		}

		if (!ReadLE64(stream, header.version)) {
			*errorOut = "Failed to read header field 'version'";
			return false;
		}

		if (header.version != VERSION) {
			*errorOut = "Invalid file version '" + std::to_string(header.version) +
				"', reader only supports version '" + std::to_string(VERSION) + "'";
			return false;
		}

		if (!ReadLE64(stream, header.fileCount) ||
			!ReadLE64(stream, header.indexOffset) ||
			!ReadLE64(stream, header.indexSize) ||
			!ReadLE64(stream, header.dataOffset) ||
			!ReadLE64(stream, header.nameTableHeaderOffset)) {
			*errorOut = "Failed to read header fields";
			return false;
		}

		// a corrupted header must not lead to huge allocations or reads outside of the file
		uint64_t expectedIndexSize = 0;
		if (!Internal::SafeMulU64(header.fileCount, sizeof(Internal::StaticForgeIndexEntry), &expectedIndexSize) ||
			header.indexSize != expectedIndexSize) {
			*errorOut = "Index size '" + std::to_string(header.indexSize) + "' does not match file count '"
				+ std::to_string(header.fileCount) + "' (archive is corrupted)";
			return false;
		}

		if (!InFile(header.indexOffset, header.indexSize, fileSize)) {
			*errorOut = "Index table is outside of the file (archive is corrupted)";
			return false;
		}

		if (header.dataOffset > fileSize) {
			*errorOut = "Data offset is outside of the file (archive is corrupted)";
			return false;
		}

		if (header.nameTableHeaderOffset != 0 &&
			!InFile(header.nameTableHeaderOffset, sizeof(Internal::StaticForgeNameTableHeader), fileSize)) {
			*errorOut = "Name table is outside of the file (archive is corrupted)";
			return false;
		}

		return true;
	}

	bool StaticForgeReader::ReadIndex(StaticForgeArchive* archive, uint64_t fileSize, std::string* errorOut) const {
		auto& stream = archive->m_stream;
		auto& header = archive->m_header;

		archive->m_indexEntries.clear();
		archive->m_indexEntries.reserve(header.fileCount);
		archive->m_hashNameToEntry.clear();
		archive->m_hashNameToEntry.reserve(header.fileCount);

		const uint64_t start = header.indexOffset;
		stream.seekg(start, std::ios::beg);
		if (stream.fail()) {
			*errorOut = "Failed to seek to index offset " + std::to_string(start);
			return false;
		}

		for (uint64_t i = 0; i < header.fileCount; i++) {
			Internal::StaticForgeIndexEntry entry{};

			if (!ReadLE64(stream, entry.hashName) ||
				!ReadLE64(stream, entry.fileOffset) ||
				!ReadLE64(stream, entry.fileSize) ||
				!ReadLE64(stream, entry.compressedFileSize) ||
				!ReadLE32(stream, entry.filePadding) ||
				!ReadLE32(stream, entry.checksum)) {
				*errorOut = "Failed to read index entry " + std::to_string(i) + " (unexpected EOF)";
				return false;
			}

			// the stored data of every entry has to be inside of the file
			uint64_t dataStart = 0;
			const uint64_t storedSize = entry.compressedFileSize != 0 ? entry.compressedFileSize : entry.fileSize;
			if (!Internal::SafeAddU64(header.dataOffset, entry.fileOffset, &dataStart) ||
				!InFile(dataStart, storedSize, fileSize)) {
				*errorOut = "Data of index entry " + std::to_string(i) + " is outside of the file (archive is corrupted)";
				return false;
			}

			if (archive->m_hashNameToEntry.count(entry.hashName)) {
				*errorOut = "Index entry " + std::to_string(i) + " has a duplicate hash (archive is corrupted)";
				return false;
			}

			archive->m_indexEntries.push_back(entry);
			archive->m_hashNameToEntry[entry.hashName] = i;
		}

		if (stream.fail()) {
			*errorOut = "Stream error while reading index";
			return false;
		}

		return true;
	}

	bool StaticForgeReader::ReadNameTable(StaticForgeArchive* archive, uint64_t fileSize, std::string* errorOut) const {
		auto& stream = archive->m_stream;
		auto& header = archive->m_header;

		archive->m_indexToName.clear();
		archive->m_indexToName.reserve(header.fileCount);

		stream.seekg(header.nameTableHeaderOffset);
		if (!stream) {
			*errorOut = "Failed to seek to name table header offset";
			return false;
		}

		Internal::StaticForgeNameTableHeader h{};

		if (!ReadLE64(stream, h.entryOffset) ||
			!ReadLE64(stream, h.stringDataOffset) ||
			!ReadLE64(stream, h.stringDataSize) ||
			!ReadLE64(stream, h.stringDataCompressedSize)) {
			*errorOut = "Failed to read name table header (unexpected EOF)";
			return false;
		}

		uint64_t entriesSize = 0;
		if (!Internal::SafeMulU64(header.fileCount, sizeof(Internal::StaticForgeNameTableEntry), &entriesSize) ||
			!InFile(h.entryOffset, entriesSize, fileSize)) {
			*errorOut = "Name table entries are outside of the file (archive is corrupted)";
			return false;
		}

		if (!InFile(h.stringDataOffset, h.stringDataCompressedSize, fileSize)) {
			*errorOut = "Name table string data is outside of the file (archive is corrupted)";
			return false;
		}

		// a 5 byte match block expands to at most 65535 bytes, a larger size is not possible
		constexpr uint64_t maxExpansion = 65535 / 5 + 1;
		uint64_t maxStringDataSize = 0;
		if (!Internal::SafeMulU64(h.stringDataCompressedSize, maxExpansion, &maxStringDataSize) ||
			h.stringDataSize > maxStringDataSize) {
			*errorOut = "Name table string data size is invalid (archive is corrupted)";
			return false;
		}

		// read and decompress all names at once
		std::vector<std::byte> compressed(static_cast<size_t>(h.stringDataCompressedSize));
		stream.seekg(static_cast<std::streamoff>(h.stringDataOffset));
		stream.read(reinterpret_cast<char*>(compressed.data()), static_cast<std::streamsize>(compressed.size()));
		if (!stream) {
			*errorOut = "Failed to read name table string data";
			return false;
		}

		auto& compressor = *archive->m_compressor;
		std::vector<std::byte> stringData = compressor.Decompress(compressed.data(), compressed.size(), static_cast<size_t>(h.stringDataSize));
		if (!compressor.IsValid() || stringData.size() != h.stringDataSize) {
			*errorOut = "Failed to decompress name table string data: " + compressor.GetError();
			return false;
		}

		stream.seekg(static_cast<std::streamoff>(h.entryOffset));
		if (!stream) {
			*errorOut = "Failed to seek to name table entries";
			return false;
		}

		for (uint64_t i = 0; i < header.fileCount; i++) {
			Internal::StaticForgeNameTableEntry entry{};

			if (!ReadLE64(stream, entry.hash) ||
				!ReadLE32(stream, entry.nameLength) ||
				!ReadLE32(stream, entry.nameOffset)) {
				*errorOut = "Failed to read name table entry " + std::to_string(i) + " (unexpected EOF)";
				return false;
			}

			if (!InFile(entry.nameOffset, entry.nameLength, h.stringDataSize)) {
				*errorOut = "Name of name table entry " + std::to_string(i) + " is outside of the string data (archive is corrupted)";
				return false;
			}

			const char* nameStart = reinterpret_cast<const char*>(stringData.data()) + entry.nameOffset;
			archive->m_indexToName[i] = std::string(nameStart, entry.nameLength);
		}

		return true;
	}

	bool StaticForgeReader::ReadLE64(std::ifstream& stream, uint64_t& out) const {
		uint64_t tmp = 0;
		stream.read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
		if (stream.gcount() != sizeof(tmp))
			return false;

		out = Internal::IsLittleEndian() ? tmp : Internal::SwapEndian(tmp);
		return true;
	}

	bool StaticForgeReader::ReadLE32(std::ifstream& stream, uint32_t& out) const {
		uint32_t tmp = 0;
		stream.read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
		if (stream.gcount() != sizeof(tmp))
			return false;

		out = Internal::IsLittleEndian() ? tmp : Internal::SwapEndian(tmp);
		return true;
	}

}