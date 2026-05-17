#include "Internal/InternalHelpers.h"

namespace StaticForge::Internal {

	uint64_t AlignSize(uint64_t fileSize, uint64_t targetAligment) {
		return (fileSize + targetAligment - 1) & ~(targetAligment - 1);
	}

	uint64_t GetHeaderSize() {
		return AlignSize(static_cast<uint64_t>(sizeof(Internal::StaticForgeHeader)), ALIGNMENT_HEADER);
	}

	uint64_t GetIndexEntrySize() {
		return AlignSize(static_cast<uint64_t>(sizeof(Internal::StaticForgeIndexEntry)), ALIGNMENT_INDEX_ENTRY);
	}

	std::string GetFullExtension(const StaticForgePath& path) {
		std::string filename = path.filename().u8string();
		auto pos = filename.find('.');
		if (pos == std::string::npos)
			return "";
		return filename.substr(pos);
	}

}