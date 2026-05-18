#pragma once
#include "StaticForgeTypes.h"

namespace StaticForge {

	class StaticForgeArchive;

	class StaticForgeReader {
	public:
		StaticForgeReader() = default;
		~StaticForgeReader() = default;

		bool Load(
			const StaticForgePath& path,
			StaticForgeArchive* archiveOut,
			std::string* errorOut
		);

	private:

	};

}