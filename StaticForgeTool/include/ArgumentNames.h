#pragma once
#include <string_view>

const std::string_view HELP_MESSAGE = R"""(
| name    | short | description                                                                | type     | default | allow multi |
|---------|-------|----------------------------------------------------------------------------|----------|---------|-------------|
| source  | src	  | Source path(s) of the files to be packed (can be specified multiple times) | filepath | -       | true        |
| output  | o	  | Output path for the packed archive file                                    | filepath | -       | false       |
| mkdir   | p	  | Create the output directory if it does not exist                           | bool     | false   | false       |
| name    | n	  | Default archive name if no `.sfpak.meta` file is found                     | string   | "main"  | false       |
| info    | i	  | Display header and index information of a `.sfpak` file (read-only mode)   | filepath | -       | false       |
| verbose | v	  | Enable debug mode (verbose console output)                                 | bool     | false   | false       |
| help    | h	  | Show this help message                                                     | -        | -       | false       |
| store-  | sn	  | Stores the file names for better debuging                                  | bool     | false   | false       |
| Names   | 	  |                                                                            |          |         |             | 
)""";
// --store-names

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
	const std::string_view EXTRACT_SHORTNAME = "e";

	const std::string_view OUTDIR = "outdir";
	const std::string_view OUTDIR_SHORTNAME = "od";

	const std::string_view VERBOSE = "verbose";
	const std::string_view VERBOSE_SHORTNAME = "v";

	const std::string_view HELP = "help";
	const std::string_view HELP_SHORTNAME = "h";

	const std::string_view STORE_NAME = "storename";
	const std::string_view STORE_NAME_SHORTNAME = "sn";
	
}