#pragma once
#include <filesystem>
#include <string_view>
#include "Internal/StaticForgeMetaParam.h"

namespace StaticForge {

	using StaticForgePath = std::filesystem::path;

	constexpr uint64_t VERSION = 1;

	constexpr std::string_view PACK_FILE_EXTENSION = ".sfpak";
	constexpr std::string_view META_FILE_EXTENSION = ".sfpak.meta";

	namespace Internal {

		// Alignment has to be a power of two (2^n) 
		constexpr uint64_t ALIGNMENT_HEADER = 32;
		constexpr uint64_t ALIGNMENT_INDEX_ENTRY = 32;
		constexpr uint64_t ALIGNMENT_FILE = 4096;
	
		struct StaticForgeFileEntry {
			StaticForgePath filepath;

			uint64_t size;
			uint64_t blockOffset;
			uint64_t alignedSize;
			uint64_t hashName;
			uint64_t indexOffset;
		};

		#pragma pack(push, 1)
		struct StaticForgeHeader {
			char magic[4] = { 'S', 'F', 'P', 'K' };

			uint64_t version = 0;
			uint64_t fileCount = 0;
			uint64_t indexOffset = 0;
			uint64_t indexSize = 0;
			uint64_t dataOffset = 0;
		};

		struct StaticForgeIndexEntry {
			uint64_t hashName;
			uint64_t offset;
			uint64_t size;
			uint32_t checksum;
		};
	#pragma pack(pop)

	}

}