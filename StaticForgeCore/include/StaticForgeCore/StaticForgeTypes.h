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
		constexpr uint64_t ALIGNMENT_FILE = 4096;
	
		struct StaticForgeFileEntry {
			StaticForgePath filepath;
			StaticForgePath relativeFilepath; /*< normalized relative file name to the src dir */

			uint64_t fileSize;
			uint64_t blockOffset;
			uint64_t hashName;
			uint64_t indexOffset;
			uint32_t filePadding;
		};

		// Order of the elements is important for the reading
		#pragma pack(push, 1)
		struct StaticForgeHeader {
			char magic[4] = { 'S', 'F', 'P', 'K' };

			uint64_t version = 0;				/*< version of the archive */
			uint64_t fileCount = 0;				/*< number of files in this archive */
			uint64_t indexOffset = 0;			/*< location of the start of the index table */
			uint64_t indexSize = 0;				/*< size of the index table */
			uint64_t dataOffset = 0;			/*< location of the start of the data block */
			uint64_t nameTableHeaderOffset = 0;	/*< location of the file name table. 0 if  not active */
		};

		struct StaticForgeIndexEntry {
			uint64_t hashName;		/*< relative path gehashed e.g. "textures/background.png" -> hash */
			uint64_t fileOffset;	/*< location of the start of the file */
			uint64_t fileSize;		/*< original size of the file */
			uint32_t filePadding;	/*< padding needed to align to ALIGNMENT_FILE */
			uint32_t checksum;		/*< hash created with the file data to validate its content on load */
		};

		struct StaticForgeNameTableHeader {
			uint64_t entryOffset = 0;        /*< offset to name table entries */
			uint64_t stringDataOffset = 0;   /*< offset to raw string data block */
			uint64_t stringDataSize = 0;     /*< size of string data block */
		};

		struct StaticForgeNameTableEntry {
			uint64_t hash = 0;              /*< hash of the filename */
			uint32_t nameLength = 0;        /*< length in bytes */
			uint32_t nameOffset = 0;        /*< relative to stringDataOffset */
		};
	#pragma pack(pop)

	}

}