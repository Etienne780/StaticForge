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
		- offset		uint64_t
		- size			uint64_t
		- checksum		uint32_t

		DATA BLOCKS
		- texture.dds
		- model.mesh
		- sound.ogg
		- script.lua

		Für Performance nehmen viele Engines:
		
		festen Header
		64-bit Offsets
		aligned blocks (4096 bytes)
		Hashes statt Strings
		ZSTD/LZ4 Compression
	*/

}