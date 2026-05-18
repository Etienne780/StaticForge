#include "StaticForgeReader.h"
#include "StaticForgeArchive.h"

namespace StaticForge {

	StaticForgeArchive StaticForgeReader::Load(const StaticForgePath& path) {
		StaticForgeArchive archive{ path };
		

		return archive;
	}

}