#pragma once
#include <string_view>

/*
	archive: sets archive name, type=string, musst be set
	exclude: sets file extensions that should be ignored, type=string[], can be set
	store-names: sets if the names of this archive should be stored, type=bool, can be set
	compress: sets that the archive should be compressed if large enough (4096)
	compress-block-size: block size in bytes used for compression, type=int, can be set
	compress-table-size: compressor match table size as power of two, type=int, can be set
	compress-min-match: shortest match the compressor stores as reference, type=int, can be set
*/

namespace StaticForge::Internal::MetaParams {

	const std::string_view ARCHIVE = "archive";
	const std::string_view EXCLUDE = "exclude";
	const std::string_view STORE_NAMES = "store-names";
	const std::string_view COMPRESS = "compress";
	const std::string_view COMPRESS_BLOCK_SIZE = "compress-block-size";
	const std::string_view COMPRESS_TABLE_SIZE = "compress-table-size";
	const std::string_view COMPRESS_MIN_MATCH = "compress-min-match";

}
