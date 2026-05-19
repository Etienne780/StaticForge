#include "Internal/InternalHelpers.h"
#include "StaticForgeReader.h"
#include "StaticForgeArchive.h"

namespace StaticForge {

	StaticForgeReader::StaticForgeReader() 
		: ErrorSupport(Internal::HAS_HEADER) {
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
			*errorOut = "Stop hallucinating paths";
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

		auto readLE64 = [&](uint64_t& out) {
			uint64_t tmp = 0;
			stream.read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
			if (stream.gcount() != sizeof(tmp)) 
				return false;

			out = Internal::IsLittleEndian() ? tmp : Internal::SwapEndian(tmp);
			return true;
		};

		if (!readLE64(header.version)) {
			*errorOut = "Failed to read header field 'version'";
			return false;
		}

		if (header.version != m_supportedVersion) {
			*errorOut = "Invalid file version '" + std::to_string(header.version) +
				"', reader only supports version '" + std::to_string(m_supportedVersion) + "'";
			return false;
		}

		if (!readLE64(header.fileCount) ||
			!readLE64(header.indexOffset) ||
			!readLE64(header.indexSize) ||
			!readLE64(header.dataOffset)) {
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

			auto readLE64 = [&](uint64_t& out) -> bool {
				uint64_t tmp = 0;
				stream.read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
				if (stream.gcount() != sizeof(tmp))
					return false;

				out = Internal::IsLittleEndian() ? tmp : Internal::SwapEndian(tmp);
				return true;
			};

			auto readLE32 = [&](uint32_t& out) -> bool {
				uint32_t tmp = 0;
				stream.read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
				if (stream.gcount() != sizeof(tmp))
					return false;

				out = Internal::IsLittleEndian() ? tmp : Internal::SwapEndian(tmp);
				return true;
			};

			if (!readLE64(entry.hashName) ||
				!readLE64(entry.fileOffset) ||
				!readLE64(entry.fileSize) ||
				!readLE32(entry.filePadding) ||
				!readLE32(entry.checksum)) {
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

}