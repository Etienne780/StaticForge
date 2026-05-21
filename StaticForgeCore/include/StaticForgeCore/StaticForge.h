#pragma once
#include "StaticForgeArchive.h"
#include "StaticForgeBuilder.h"
#include "StaticForgeReader.h"
#include "StaticForgeTypes.h"

namespace StaticForge {

	/*
		HEADER
		- magic number				char[4]
		- version					uint64_t
		- file count				uint64_t
		- index offset				uint64_t -> INDEX
		- index size				uint64_t
		- data offset				uint64_t -> DATA BLOCKS
		- name table header Offset	uint64_t -> NAME TABLE HEADER

		------------ 64 byte aligment ------------

		INDEX
		- hash/name		uint64_t
		- fileOffset	uint64_t
		- fileSize		uint64_t
		- filePadding	uint32_t
		- checksum		uint32_t

		------------ 4096 byte aligment ------------

		DATA BLOCKS
		- raw file data (texture.dds, model.mesh, ...)

		[optional]
		NAME TABLE HEADER
		- nameTableOffset	uint64_t -> NAME TABLE DATA		

		[optional]
		NAME TABLE DATA
		- hash			uint64_t
		- nameLength	uint32_t
		- nameOffset	uint64_t -> NAME TABLE STRING DATA

		[optional]
		NAME TABLE STRING DATA
		- raw string data ('textures/level1/background.png', ...)
	*/

}