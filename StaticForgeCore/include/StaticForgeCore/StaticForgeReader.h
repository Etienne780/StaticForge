#pragma once
#include "StaticForgeTypes.h"

namespace StaticForge {

	class StaticForgeArchive;

	class StaticForgeReader {
	public:
		StaticForgeReader() = default;
		~StaticForgeReader() = default;

		StaticForgeArchive Load(const StaticForgePath& path);

	private:

	};

}