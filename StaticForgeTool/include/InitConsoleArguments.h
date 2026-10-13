#pragma once
#include <charconv>
#include <optional>

#include "ConsoleArgument.h"
#include "ArgumentNames.h"

inline void InitConsoleArguments() {
	using CAM = ConsoleArgumentManager;

	auto filepathParser = [](const std::string& value, void* output) {
		std::filesystem::path path{ value };
		*static_cast<std::filesystem::path*>(output) = std::move(path);
		return true;
	};

	auto stringParser = [](const std::string& value, void* output) {
		*static_cast<std::string*>(output) = value;
		return true;
	};

	auto boolParser = [](const std::string& value, void* output) {
		bool* b = static_cast<bool*>(output);
		*b = (value == "1" || value == "true" || value == "yes" || value == "y");
		return true;
	};

	auto uintParser = [](const std::string& value, void* output) {
		uint64_t result = 0;
		auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
		// whole value has to be a number
		if (ec != std::errc{} || end != value.data() + value.size())
			return false;

		*static_cast<std::optional<uint64_t>*>(output) = result;
		return true;
	};

	auto& sourceArg = CAM::RegisterConsoleArgument(ArgName::SOURCE, ArgName::SOURCE_SHORTNAME, ConsoleArgType::Filepath);
	sourceArg.SetAllowMultiple(true);
	sourceArg.SetEvaluationFunction(filepathParser);

	auto& outputArg = CAM::RegisterConsoleArgument(ArgName::OUTPUT, ArgName::OUTPUT_SHORTNAME, ConsoleArgType::Filepath);
	outputArg.SetEvaluationFunction(filepathParser);

	auto& makeDirArg = CAM::RegisterConsoleArgument(ArgName::MAKE_DIR, ArgName::MAKE_DIR_SHORTNAME, ConsoleArgType::Bool);
	makeDirArg.SetEvaluationFunction(boolParser);

	auto& archiveNameArg = CAM::RegisterConsoleArgument(ArgName::ARCHIVE_NAME, ArgName::ARCHIVE_NAME_SHORTNAME, ConsoleArgType::String);
	archiveNameArg.SetEvaluationFunction(stringParser);

	auto& infoArg = CAM::RegisterConsoleArgument(ArgName::INFO, ArgName::INFO_SHORTNAME, ConsoleArgType::Filepath);
	infoArg.SetEvaluationFunction(filepathParser);
	infoArg.SetAllowMultiple(true);

	auto& outdirArg = CAM::RegisterConsoleArgument(ArgName::OUTDIR, ArgName::OUTDIR_SHORTNAME, ConsoleArgType::Filepath);
	outdirArg.SetEvaluationFunction(filepathParser);

	auto& verboseArg = CAM::RegisterConsoleArgument(ArgName::VERBOSE, ArgName::VERBOSE_SHORTNAME, ConsoleArgType::Bool);
	verboseArg.SetEvaluationFunction(boolParser);

	auto& helpArg = CAM::RegisterConsoleArgument(ArgName::HELP, ArgName::HELP_SHORTNAME, ConsoleArgType::None);

	auto& storeNameArg = CAM::RegisterConsoleArgument(ArgName::STORE_NAMES, ArgName::STORE_NAMES_SHORTNAME, ConsoleArgType::Bool);
	storeNameArg.SetEvaluationFunction(boolParser);

	auto& compressArg = CAM::RegisterConsoleArgument(ArgName::COMPRESS_NAMES, ArgName::COMPRESS_NAMES_SHORTNAME, ConsoleArgType::Bool);
	compressArg.SetEvaluationFunction(boolParser);

	auto& compressBlockSizeArg = CAM::RegisterConsoleArgument(ArgName::COMPRESS_BLOCK_SIZE, ArgName::COMPRESS_BLOCK_SIZE_SHORTNAME, ConsoleArgType::Int);
	compressBlockSizeArg.SetEvaluationFunction(uintParser);

	auto& compressTableSizeArg = CAM::RegisterConsoleArgument(ArgName::COMPRESS_TABLE_SIZE, ArgName::COMPRESS_TABLE_SIZE_SHORTNAME, ConsoleArgType::Int);
	compressTableSizeArg.SetEvaluationFunction(uintParser);

	auto& compressMinMatchArg = CAM::RegisterConsoleArgument(ArgName::COMPRESS_MIN_MATCH, ArgName::COMPRESS_MIN_MATCH_SHORTNAME, ConsoleArgType::Int);
	compressMinMatchArg.SetEvaluationFunction(uintParser);

}
