#pragma once
#include <vector>
#include <unordered_map>
#include "StaticForgeTypes.h"
#include "StaticForgeReader.h"

namespace StaticForge {

	class StaticForgeReader;

	class StaticForgeArchive {
	friend class StaticForgeReader;
	public:
		~StaticForgeArchive() = default;

	private:
		StaticForgeArchive(const StaticForgePath& path);

		StaticForgePath m_path;

		Internal::StaticForgeHeader m_header;
		std::vector<Internal::StaticForgeIndexEntry> m_indexEntries;
		std::unordered_map<uint64_t, size_t> m_hashNameToEntry;

		void SetStaticForgeHeader(Internal::StaticForgeHeader&& header);
		void PushStaticForgeHeader(Internal::StaticForgeIndexEntry&& entry);
		
	};

}