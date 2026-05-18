#include "StaticForgeArchive.h"

namespace StaticForge {

	StaticForgeArchive::StaticForgeArchive() 
		: ErrorSupport(Internal::HAS_HEADER) {
	}

	bool StaticForgeArchive::LoadAsset(uint64_t hash, std::vector<std::byte>& outData) {
		return false;
	}

	void StaticForgeArchive::SetStaticForgeHeader(Internal::StaticForgeHeader&& header) {

	}

	void StaticForgeArchive::PushStaticForgeEntry(Internal::StaticForgeIndexEntry&& entry) {

	}

}