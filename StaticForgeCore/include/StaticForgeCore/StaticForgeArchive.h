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
		~StaticForgeArchive() = default;

		StaticForgeArchive(StaticForgeArchive&&) noexcept = default;
		StaticForgeArchive& operator=(StaticForgeArchive&&) noexcept = default;
		StaticForgeArchive(const StaticForgeArchive&) = delete;
		StaticForgeArchive& operator=(const StaticForgeArchive&) = delete;

		bool LoadAsset(uint64_t hash, std::vector<std::byte>& outData);

	private:
		StaticForgePath m_path;
		std::ifstream m_stream;

		Internal::StaticForgeHeader m_header;
		std::vector<Internal::StaticForgeIndexEntry> m_indexEntries;
		std::unordered_map<uint64_t, size_t> m_hashNameToEntry;
		
	};

}