#include "Internal/InternalHelpers.h"
#include "StaticForgeReader.h"
#include "StaticForgeArchive.h"

namespace StaticForge {

	StaticForgeReader::StaticForgeReader() 
		: ErrorSupport(HAS_HEADER) {
	}

	bool StaticForgeReader::Load(
		const StaticForgePath& path,
		StaticForgeArchive* archiveOut
	) {
		if (!archiveOut) {
			AddError("Load needs a valid 'StaticForgeArchive' object");
			return false;
		}

		std::string error;
		if (!ValidatePackPath(path, &error)) {
			AddError("ValidatePackPath: " + error);
			return false;
		}

		// open file
		archiveOut->m_path = path;
		archiveOut->OpenFileStream();
		if (!archiveOut->m_stream.is_open()) {
			AddError("Failed to open file");
			return false;
		}

		if (!ReadHeader(archiveOut, &error)) {
			AddError("ReadHeader: " + error);
			return false;
		}

		if (!ReadIndex(archiveOut, &error)) {
			AddError("ReadIndex: " + error);
			return false;
		}

		if (archiveOut->StoresNames()) {
			if (!ReadNameTable(archiveOut, &error)) {
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

		if (!std::filesystem::exists(path)) {
			*errorOut = "Output path '" + path.u8string() + "' does not exist!";
			return false;
		}

		return true;
	}

	bool StaticForgeReader::ReadHeader(StaticForgeArchive* archive, std::string* errorOut) const {
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

		return true;
	}

	bool StaticForgeReader::ReadIndex(StaticForgeArchive* archive, std::string* errorOut) const {
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

			archive->m_indexEntries.push_back(entry);
			archive->m_hashNameToEntry[entry.hashName] = i;
		}

		if (stream.fail()) {
			*errorOut = "Stream error while reading index";
			return false;
		}

		return true;
	}

	bool StaticForgeReader::ReadNameTable(StaticForgeArchive* archive, std::string* errorOut) const {
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
			!ReadLE64(stream, h.stringDataOffset)) {
			*errorOut = "Failed to read name table header (unexpected EOF)";
			return false;
		}

		stream.seekg(h.entryOffset);
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

			auto returnPos = stream.tellg();
			if (returnPos < 0) {
				*errorOut = "Failed to get current stream position while reading name table";
				return false;
			}

			stream.seekg(
				static_cast<std::streamoff>(h.stringDataOffset + entry.nameOffset)
			);

			if (!stream) {
				*errorOut = "Failed to seek to string data for name table entry " + std::to_string(i);
				return false;
			}

			std::string name(entry.nameLength, '\0');

			stream.read(name.data(), static_cast<std::streamsize>(entry.nameLength));

			if (!stream) {
				*errorOut = "Failed to read string data for name table entry " + std::to_string(i);
				return false;
			}

			stream.seekg(returnPos);

			if (!stream) {
				*errorOut = "Failed to restore stream position after reading name table entry " + std::to_string(i);
				return false;
			}

			archive->m_indexToName[i] = std::move(name);
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