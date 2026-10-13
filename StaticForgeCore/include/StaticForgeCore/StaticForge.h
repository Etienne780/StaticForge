#pragma once
#include "StaticForgeArchive.h"
#include "StaticForgeBuilder.h"
#include "StaticForgeReader.h"
#include "StaticForgeTypes.h"

namespace StaticForge {

	/*
		File Layout (all values little endian):

		HEADER (52 bytes)
		- magic number				char[4]
		- version					uint64_t
		- file count				uint64_t
		- index offset				uint64_t -> INDEX
		- index size				uint64_t
		- data offset				uint64_t -> DATA BLOCKS
		- name table header Offset	uint64_t -> NAME TABLE HEADER (0 if not stored)

		------------ 32 byte alignment (header takes 64 bytes) ------------

		INDEX (40 bytes per file)
		- hash/name			uint64_t (64 bit FNV-1a of the relative path)
		- fileOffset		uint64_t -> HEADER.data offset + DATA BLOCKS
		- fileSize			uint64_t
		- compressedSize	uint64_t (including framing, 0 if not compressed)
		- filePadding		uint32_t
		- checksum			uint32_t (32 bit FNV-1a of the stored data)

		------------ 4096 byte alignment ------------

		DATA BLOCKS (each padded to 4096 bytes)
		- raw file data (texture.dds, model.mesh, ...)
		- compressed: [uint32_t compressedSize][uint32_t originalSize][data] per block

		[optional]
		NAME TABLE HEADER
		- entry offset			uint64_t -> NAME TABLE ENTRIES
		- string data offset	uint64_t -> NAME TABLE STRING DATA

		[optional]
		NAME TABLE ENTRIES (16 bytes per file)
		- hash			uint64_t
		- name length	uint32_t
		- name offset	uint32_t -> NAME TABLE HEADER.string data offset + NAME TABLE STRING DATA

		[optional]
		NAME TABLE STRING DATA
		- raw string data ('textures/level1/background.png', ...)
	*/

}