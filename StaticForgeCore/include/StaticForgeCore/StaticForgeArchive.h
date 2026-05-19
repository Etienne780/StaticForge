#pragma once
#include <vector>
#include <unordered_map>
#include <fstream>
#include "StaticForgeTypes.h"
#include "StaticForgeReader.h"
#include "Internal/ErrorSupport.h"

namespace StaticForge {

	class StaticForgeReader;

	class StaticForgeArchive : public Internal::ErrorSupport {
	friend class StaticForgeReader;
	public:
		StaticForgeArchive();
		~StaticForgeArchive();

		StaticForgeArchive(StaticForgeArchive&&) noexcept = default;
		StaticForgeArchive& operator=(StaticForgeArchive&&) noexcept = default;
		StaticForgeArchive(const StaticForgeArchive&) = delete;
		StaticForgeArchive& operator=(const StaticForgeArchive&) = delete;

		bool LoadAsset(const std::string& key, std::vector<std::byte>& outData);

		bool OpenFileStream();
		void CloseFileStream();
		bool IsFileStreamOpen();

		const StaticForgePath& GetPath() const;
		uint64_t GetVersion() const;
		size_t GetFileCount() const;
		uint64_t GetIndexOffset() const;
		uint64_t GetIndexSize() const;
		uint64_t GetDataOffset() const;

		uint64_t GetHashName(size_t fileIndex) const;
		uint64_t GetFileOffset(size_t fileIndex) const;
		uint64_t GetFileSize(size_t fileIndex) const;
		uint64_t GetFilePadding(size_t fileIndex) const;

	private:
		StaticForgePath m_path;
		std::ifstream m_stream;

		Internal::StaticForgeHeader m_header;
		std::vector<Internal::StaticForgeIndexEntry> m_indexEntries;
		std::unordered_map<uint64_t, size_t> m_hashNameToEntry;
		
		bool LoadEntry(Internal::StaticForgeIndexEntry* entry, std::vector<std::byte>& outData, std::string* errorOut);
		
		Internal::StaticForgeIndexEntry* GetIndexEntry(uint64_t hash);
	};

}