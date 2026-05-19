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
		archiveOut->m_stream.open(path, std::ios::binary);
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
			*errorOut = "path musst be a path to a file oder so";
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

	bool StaticForgeReader::ReadHeader(StaticForgeArchive* archive, std::string* errorOut) {
		auto& stream = archive->m_stream;
		auto& header = archive->m_header;
		
		constexpr char expectedMagic[4] = {
			'S', 'F', 'P', 'K'
		};

		stream.read(
			reinterpret_cast<char*>(&header),
			sizeof(header)
		);

		if (stream.fail()) {
			*errorOut = "Failed to read";
			return false;
		}

		if (std::memcmp(header.magic, expectedMagic, sizeof(expectedMagic)) != 0) {
			*errorOut = "Invalid archive magic";
			return false;
		}

		if (header.version != m_supportedVersion) {
			*errorOut = "Invalid file version '" + std::to_string(header.version) + "', reader only supports version '" + std::to_string(m_supportedVersion) + "'";
			return false;
		}

		return true;
	}

	bool StaticForgeReader::ReadIndex(StaticForgeArchive* archive, std::string* errorOut) {
		auto& stream = archive->m_stream;
		auto& header = archive->m_header;

		archive->m_indexEntries.reserve(header.fileCount);
		archive->m_hashNameToEntry.reserve(header.fileCount);

		const uint64_t indexEntrySize = Internal::GetIndexEntrySize();
		const uint64_t indexEntryPadding = indexEntrySize - sizeof(Internal::StaticForgeIndexEntry);

		const uint64_t start = header.indexOffset;
		const uint64_t end = header.indexOffset + header.indexSize;

		stream.seekg(start, std::ios::beg);

		uint64_t fileCount = 0;

		for (uint64_t i = 0; i < header.fileCount; i++) {
			Internal::StaticForgeIndexEntry entry;

			stream.read(reinterpret_cast<char*>(&entry), sizeof(entry));

			if (!stream) {
				*errorOut = "Failed to read index entry " + std::to_string(i);
				return false;
			}

			if (indexEntryPadding > 0) {
				stream.seekg(indexEntryPadding, std::ios::cur);

				if (!stream) {
					*errorOut = "Failed to skip index padding at entry " + std::to_string(i);
					return false;
				}
			}

			archive->m_indexEntries.push_back(entry);
			archive->m_hashNameToEntry[entry.hashName] = i;
		}

		return true;
	}

}