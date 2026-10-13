#pragma once
#include <string_view>

const std::string_view HELP_MESSAGE = R"""(
Pattern:
	--CommandName value ... (value can be optional);
	bool arguments can be used without a value: '--verbose' is the same as '--verbose true'

Commands:
|---------|-------|----------------------------------------------------------------------------|----------|---------|-------------|
| name    | short | description                                                                | type     | default | allow multi |
|---------|-------|----------------------------------------------------------------------------|----------|---------|-------------|
| source  | src	  | Source path(s) of the files to be packed (can be specified multiple times) | filepath | -       | true        |
| output  | o	  | Output path for the packed archive file                                    | filepath | -       | false       |
| mkdir   | p	  | Create the output directory if it does not exist                           | bool     | false   | false       |
| name    | n	  | Default archive name if no `.sfpak.meta` file is found                     | string   | "main"  | false       |
| info    | i	  | Display header and index information of a `.sfpak` file (read-only mode)   | filepath | -       | true        |
| extract | x     | Extracts `.sfpak` file(s) into --output/<archive name>/ (needs --output)   | filepath | -       | true        |
| verbose | v	  | Enable debug mode (verbose console output)                                 | bool     | false   | false       |
| help    | h	  | Show this help message                                                     | -        | -       | false       |
| store-  | sn    | Stores the file names for better debuging                                  | bool     | false   | false       |
| names   | 	  |                                                                            |          |         |             |
| compress| c     | Compresses files if they are larger than 4096 bytes                        | bool     | false   | false       |
|---------|-------|----------------------------------------------------------------------------|----------|---------|-------------|

Compression settings (only used with --compress, override the values of .sfpak.meta files):
|---------------------|-------|------------------------------------------------------------------|------|---------|-------------|
| name                | short | description                                                      | type | default | range       |
|---------------------|-------|------------------------------------------------------------------|------|---------|-------------|
| compress-block-size | cbs   | Size in bytes of the blocks a file is split into for compression | int  | 65536   | 1024 - 64MB |
| compress-table-size | cts   | Match table size as power of two (entries = 2^value)             | int  | 16      | 8 - 24      |
| compress-min-match  | cmm   | Shortest repeated sequence that is stored as a reference         | int  | 8       | 4 - 65535   |
|---------------------|-------|------------------------------------------------------------------|------|---------|-------------|

)""";

namespace ArgName {
	
	const std::string_view SOURCE = "source";
	const std::string_view SOURCE_SHORTNAME = "src";

	const std::string_view OUTPUT = "output";
	const std::string_view OUTPUT_SHORTNAME = "o";

	const std::string_view MAKE_DIR = "mkdir";
	const std::string_view MAKE_DIR_SHORTNAME = "p";

	const std::string_view ARCHIVE_NAME = "name";
	const std::string_view ARCHIVE_NAME_SHORTNAME = "n";

	const std::string_view INFO = "info";
	const std::string_view INFO_SHORTNAME = "i";

	const std::string_view EXTRACT = "extract";
	const std::string_view EXTRACT_SHORTNAME = "x";

	const std::string_view VERBOSE = "verbose";
	const std::string_view VERBOSE_SHORTNAME = "v";

	const std::string_view HELP = "help";
	const std::string_view HELP_SHORTNAME = "h";

	const std::string_view STORE_NAMES = "store-names";
	const std::string_view STORE_NAMES_SHORTNAME = "sn";

	const std::string_view COMPRESS_NAMES = "compress";
	const std::string_view COMPRESS_NAMES_SHORTNAME = "c";

	const std::string_view COMPRESS_BLOCK_SIZE = "compress-block-size";
	const std::string_view COMPRESS_BLOCK_SIZE_SHORTNAME = "cbs";

	const std::string_view COMPRESS_TABLE_SIZE = "compress-table-size";
	const std::string_view COMPRESS_TABLE_SIZE_SHORTNAME = "cts";

	const std::string_view COMPRESS_MIN_MATCH = "compress-min-match";
	const std::string_view COMPRESS_MIN_MATCH_SHORTNAME = "cmm";
	
}