#pragma once
#include <string_view>

/*
	archive: sets archive name, type=string, musst be set
	exclude: sets file extensions that should be ignored, type=string[], can be set
	store-names: sets if the names of this archive should be stored, type=bool, can be set
	compressL: sets that the archive should be compressed if large enough (4096)
*/

namespace StaticForge::Internal::MetaParams {
	
	const std::string_view ARCHIVE = "archive"; 
	const std::string_view EXCLUDE = "exclude";
	const std::string_view STORE_NAMES = "store-names";
	const std::string_view COMPRESS = "compress";

}