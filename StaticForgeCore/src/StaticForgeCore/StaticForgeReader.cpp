#include "StaticForgeReader.h"
#include "StaticForgeArchive.h"

namespace StaticForge {

	bool StaticForgeReader::Load(
		const StaticForgePath& path,
		StaticForgeArchive* archiveOut,
		std::string* errorOut
	) {
		if (!archiveOut) {
			return false;
		}

		return true;
	}

}