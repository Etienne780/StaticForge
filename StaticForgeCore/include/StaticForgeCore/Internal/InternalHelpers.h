#pragma once
#include <stdint.h>
#include "StaticForgeTypes.h"

namespace StaticForge::Internal {

    uint32_t FNV1a(const void* data, size_t size, uint32_t hash = 2166136261u);

	std::string NormalizeFilepathSlashes(const std::string path);

	uint64_t AlignSize(uint64_t fileSize, uint64_t targetAligment);

	uint64_t GetHeaderSize();
	uint64_t GetIndexEntrySize();

	uint64_t HashFilename(const std::string& filename);

	std::string GetFullExtension(const StaticForgePath& path);
	std::string GetFormatedSizeStr(uint64_t bytes);

    uint16_t SwapEndian(uint16_t v);
    uint32_t SwapEndian(uint32_t v);
    uint64_t SwapEndian(uint64_t v);

    bool IsLittleEndian() noexcept;
}