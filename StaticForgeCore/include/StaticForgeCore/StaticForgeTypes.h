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

		constexpr std::string_view CONSOLE_SEPERATOR = "===================================";

		// Alignment has to be a power of two (2^n) 
		constexpr uint64_t ALIGNMENT_HEADER = 32;
		constexpr uint64_t ALIGNMENT_INDEX_ENTRY = 32;
		constexpr uint64_t ALIGNMENT_FILE = 4096;
	
		struct StaticForgeFileEntry {
			StaticForgePath filepath;

			uint64_t fileSize;
			uint64_t blockOffset;
			uint64_t alignedFileSize;
			uint64_t hashName;
			uint64_t indexOffset;
		};

		// Order of the elements is important for the reading
		#pragma pack(push, 1)
		struct StaticForgeHeader {
			char magic[4] = { 'S', 'F', 'P', 'K' };

			uint64_t version = 0;		/*< version of the archive */
			uint64_t fileCount = 0;		/*< number of files in this archive */
			uint64_t indexOffset = 0;	/*< location of the start of the index tabel */
			uint64_t indexSize = 0;		/*< size of the index tabel */
			uint64_t dataOffset = 0;	/*< location of the start of the data block */
		};

		struct StaticForgeIndexEntry {
			uint64_t hashName;		/*< relative path gehashed e.g. "textures/background.png" -> hash */
			uint64_t fileOffset;	/*< location of the start of the file */
			uint64_t fileSize;		/*< size of the file aligend to ALIGNMENT_FILE */
			uint32_t checksum;		/*< hash created with the file data to validate its content on load */
		};
	#pragma pack(pop)

	}

}