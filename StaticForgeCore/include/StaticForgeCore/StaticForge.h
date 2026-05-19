#pragma once
#include "StaticForgeArchive.h"
#include "StaticForgeBuilder.h"
#include "StaticForgeReader.h"
#include "StaticForgeTypes.h"

namespace StaticForge {

	/*
		HEADER
		- magic number	char[]
		- version		uint64_t
		- file count	uint64_t
		- index offset	uint64_t
		- index size	uint64_t
		- data offset	uint64_t

		INDEX
		- hash/name		uint64_t
		- fileOffset	uint64_t
		- fileSize		uint64_t
		- filePadding	uint32_t
		- checksum		uint32_t

		DATA BLOCKS
		- texture.dds
		- model.mesh
		- sound.ogg
		- script.lua
	*/

}