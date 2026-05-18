#pragma once
#include <string_view>

/*

| name   | short form | desc                                                | type		| optional	| default | allow multiple |
|--------|------------|-----------------------------------------------------|-----------|-----------|---------|----------------|
| source | src        | source path of the files that should be packed      | filepath	| false		| -		  | true           |
| output | o          | output path for the packed file                     | filepath	| false		| -		  |	false          |
| mkdir  | p          | create the output directory if it does not exist    | bool		| true		| false	  | false          |
| name	 | n          | default archive name if no .sfpak.meta found		| string	| true		| "main"  | false          |
| debug	 | d          | says if debug mode should be active					| bool		| true		| false	  | false          |
																																
*/


namespace ArgName {
	
	const std::string_view SOURCE = "source";
	const std::string_view SOURCE_SHORTNAME = "src";

	const std::string_view OUTPUT = "output";
	const std::string_view OUTPUT_SHORTNAME = "o";

	const std::string_view MAKE_DIR = "mkdir";
	const std::string_view MAKE_DIR_SHORTNAME = "p";

	const std::string_view ARCHIVE_NAME = "name";
	const std::string_view ARCHIVE_NAME_SHORTNAME = "n";

	const std::string_view DEBUG_NAME = "debug";
	const std::string_view DEBUG_NAME_SHORTNAME = "d";
	
}