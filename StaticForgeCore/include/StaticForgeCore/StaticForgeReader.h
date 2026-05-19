#pragma once
#include "StaticForgeTypes.h"
#include "Internal/ErrorSupport.h"

namespace StaticForge {

	class StaticForgeArchive;

	class StaticForgeReader : public Internal::ErrorSupport {
	public:
		StaticForgeReader();
		~StaticForgeReader() = default;

		bool Load(
			const StaticForgePath& path,
			StaticForgeArchive* archiveOut
		);

	private:
		const uint64_t m_supportedVersion = 1;

		bool ValidatePackPath(const StaticForgePath& path, std::string* errorOut);
		bool ReadHeader(StaticForgeArchive* archive, std::string* errorOut) const;
		bool ReadIndex(StaticForgeArchive* archive, std::string* errorOut) const;
	};

}