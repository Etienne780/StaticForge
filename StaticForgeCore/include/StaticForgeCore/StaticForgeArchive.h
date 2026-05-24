#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>
#include "StaticForgeTypes.h"
#include "StaticForgeReader.h"
#include "Internal/ErrorSupport.h"
#include "Internal/MmapFile.h"

namespace StaticForge {

	class StaticForgeReader;

	struct AssetView {
		const std::byte* data = nullptr;
		size_t size = 0;	
	};

	class StaticForgeArchive : public Internal::ErrorSupport {
	friend class StaticForgeReader;
	public:
		StaticForgeArchive();
		~StaticForgeArchive();

		StaticForgeArchive(const StaticForgeArchive&) = delete;
		StaticForgeArchive& operator=(const StaticForgeArchive&) = delete;

		StaticForgeArchive(StaticForgeArchive&&) noexcept = default;
		StaticForgeArchive& operator=(StaticForgeArchive&&) noexcept = default;


		bool OpenFileStream();
		void CloseFileStream();
		bool IsFileStreamOpen() const;

		bool LoadAsset(const std::string& key, std::vector<std::byte>& outData);
		
		bool OpenMapped();
		void CloseMapped();
		bool IsMapped() const;
		
		AssetView GetAssetMapped(const std::string& key);
		bool LoadAssetMapped(const std::string& key, std::vector<std::byte>& outData);

		bool StoresNames() const;

		const StaticForgePath& GetPath() const;
		uint64_t GetVersion() const;
		size_t GetFileCount() const;
		uint64_t GetIndexOffset() const;
		uint64_t GetIndexSize() const;
		uint64_t GetDataOffset() const;

		std::string GetName(size_t fileIndex) const;
		uint64_t GetHashName(size_t fileIndex) const;
		uint64_t GetFileOffset(size_t fileIndex) const;
		uint64_t GetFileSize(size_t fileIndex) const;
		uint64_t GetFilePadding(size_t fileIndex) const;

	private:
		StaticForgePath m_path;
		std::ifstream m_stream;
		Internal::MmapFile m_mmap;

		Internal::StaticForgeHeader m_header;
		std::vector<Internal::StaticForgeIndexEntry> m_indexEntries;
		std::unordered_map<uint64_t, size_t> m_hashNameToEntry;
		std::unordered_map<size_t, std::string> m_indexToName;
		
		bool LoadEntry(Internal::StaticForgeIndexEntry* entry, std::vector<std::byte>& outData, std::string* errorOut);
		bool LoadEntryMapped(Internal::StaticForgeIndexEntry* entry, std::vector<std::byte>& outData, std::string* errorOut);
		
		Internal::StaticForgeIndexEntry* GetIndexEntry(uint64_t hash);

		bool VerifyChecksum(const std::byte* data, uint64_t size,
			uint32_t expected, std::string* errorOut) const;
	};

}