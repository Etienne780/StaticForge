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
		bool ValidatePackPath(const StaticForgePath& path, std::string* errorOut);
		bool ReadHeader(StaticForgeArchive* archive, std::string* errorOut) const;
		bool ReadIndex(StaticForgeArchive* archive, std::string* errorOut) const;
		bool ReadNameTable(StaticForgeArchive* archive, std::string* errorOut) const;

		bool ReadLE64(std::ifstream& stream, uint64_t& out) const;
		bool ReadLE32(std::ifstream& stream, uint32_t& out) const;
	};

}