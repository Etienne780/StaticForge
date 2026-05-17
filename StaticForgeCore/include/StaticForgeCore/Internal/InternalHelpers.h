#pragma once
#include <stdint.h>
#include "StaticForgeTypes.h"

namespace StaticForge::Internal {

	uint64_t AlignSize(uint64_t fileSize, uint64_t targetAligment);

	uint64_t GetHeaderSize();
	uint64_t GetIndexEntrySize();

	std::string GetFullExtension(const StaticForgePath& path);

}