#include <iostream>
#include "StaticForgeArchive.h"
#include "Internal/InternalHelpers.h"

namespace StaticForge {

	StaticForgeArchive::StaticForgeArchive()
		: ErrorSupport(Internal::HAS_HEADER) {
	}

	StaticForgeArchive::~StaticForgeArchive() {
		CloseFileStream();
	}

	bool StaticForgeArchive::LoadAsset(const std::string& key, std::vector<std::byte>& outData) {
		uint64_t h = Internal::HashFilename(Internal::NormalizeFilepathSlashes(key));
		auto* entry = GetIndexEntry(h);
		if (!entry) {
			AddError("Failed to load asset, entry with key '" + key + "' not found");
			return false;
		}

		std::string error;
		if (!LoadEntry(entry, outData, &error)) {
			AddError("Failed to load asset '" + key + "': " + error);
			return false;
		}

		return true;
	}

	bool StaticForgeArchive::OpenFileStream() {
		m_stream.open(m_path, std::ios::binary | std::ios::in);
		return m_stream.is_open();
	}

	void StaticForgeArchive::CloseFileStream() {
		m_stream.close();
	}

	bool StaticForgeArchive::IsFileStreamOpen() {
		return m_stream.is_open();
	}

	const StaticForgePath& StaticForgeArchive::GetPath() const {
		return m_path;
	}

	uint64_t StaticForgeArchive::GetVersion() const {
		return m_header.version;
	}

	size_t StaticForgeArchive::GetFileCount() const {
		return m_indexEntries.size();
	}

	uint64_t StaticForgeArchive::GetIndexOffset() const {
		return m_header.indexOffset;
	}

	uint64_t StaticForgeArchive::GetIndexSize() const {
		return m_header.indexSize;
	}

	uint64_t StaticForgeArchive::GetDataOffset() const {
		return m_header.dataOffset;
	}

	uint64_t StaticForgeArchive::GetHashName(size_t index) const {
		return m_indexEntries[index].hashName;
	}

	uint64_t StaticForgeArchive::GetFileOffset(size_t index) const {
		return m_indexEntries[index].fileOffset;
	}

	uint64_t StaticForgeArchive::GetFileSize(size_t index) const {
		return m_indexEntries[index].fileSize;
	}

	uint64_t StaticForgeArchive::GetFilePadding(size_t index) const {
		return m_indexEntries[index].filePadding;
	}

	bool StaticForgeArchive::LoadEntry(
		Internal::StaticForgeIndexEntry* entry, 
		std::vector<std::byte>& outData,
		std::string* errorOut
	) {
		if (!IsFileStreamOpen()) {
			if (!OpenFileStream()) {
				*errorOut = "Failed to open archive file at '" + m_path.u8string() + "'";
				return false;
			}
		}

		uint64_t start = entry->fileOffset;
		uint64_t size = entry->fileSize;

		outData.resize(size);

		m_stream.seekg(
			static_cast<std::streamoff>(start), 
			std::ios::beg
		);

		if (!m_stream) {
			*errorOut = "Failed to seek at offset " + std::to_string(start);
			return false;
		}

		m_stream.read(
			reinterpret_cast<char*>(outData.data()),
			size
		);

		if (!m_stream) {
			*errorOut = "Failed to read " + std::to_string(size) + " bytes at offset " + std::to_string(start);
			return false;
		}

		uint32_t computedChecksum = Internal::FNV1a(outData.data(), outData.size(), 2166136261u);
		if (computedChecksum != entry->checksum) {
			*errorOut = "Checksum mismatch for entry (expected " + std::to_string(entry->checksum) + ", got " + std::to_string(computedChecksum) + ")";
			return false;
		}

		return true;
	}

	Internal::StaticForgeIndexEntry* StaticForgeArchive::GetIndexEntry(uint64_t hash) {
		auto it = m_hashNameToEntry.find(hash);
		if (it == m_hashNameToEntry.end())
			return nullptr;
#ifdef DEBUG
		if (it->second >= m_indexEntries.size()) {
			std::cout << "StaticForgeArchive::GetIndexEntry: Index entry out of bounds for hash '" << hash << "'" << std::endl;
			return nullptr;
		}
#endif 

		return &m_indexEntries[it->second];
	}

}