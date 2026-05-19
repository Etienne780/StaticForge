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
		uint64_t h = Internal::HashFilename(key);
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

		m_stream.read(
			reinterpret_cast<char*>(outData.data()),
			size
		);

		if (!m_stream) {
			*errorOut = "Failed to read " + std::to_string(size) + " bytes at offset " + std::to_string(start);
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