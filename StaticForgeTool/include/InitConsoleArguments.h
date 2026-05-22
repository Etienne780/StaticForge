#pragma once
#include "ConsoleArgument.h"
#include "ArgumentNames.h"

inline void InitConsoleArguments() {
	using CAM = ConsoleArgumentManager;

	auto filepathParser = [](const std::string& value, void* output) {
		std::filesystem::path path{ value };
		*static_cast<std::filesystem::path*>(output) = std::move(path);
	};

	auto stringParser = [](const std::string& value, void* output) {
		*static_cast<std::string*>(output) = value;
	};

	auto boolParser = [](const std::string& value, void* output) {
		bool* b = static_cast<bool*>(output);
		*b = (value == "1" || value == "true" || value == "yes" || value == "y");
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

	auto& extractArg = CAM::RegisterConsoleArgument(ArgName::EXTRACT, ArgName::EXTRACT_SHORTNAME, ConsoleArgType::Filepath);
	extractArg.SetEvaluationFunction(filepathParser);

	auto& outdirArg = CAM::RegisterConsoleArgument(ArgName::OUTDIR, ArgName::OUTDIR_SHORTNAME, ConsoleArgType::Filepath);
	outdirArg.SetEvaluationFunction(filepathParser);

	auto& verboseArg = CAM::RegisterConsoleArgument(ArgName::VERBOSE, ArgName::VERBOSE_SHORTNAME, ConsoleArgType::Bool);
	verboseArg.SetEvaluationFunction(boolParser);

	auto& helpArg = CAM::RegisterConsoleArgument(ArgName::HELP, ArgName::HELP_SHORTNAME, ConsoleArgType::None);

	auto& storeNameArg = CAM::RegisterConsoleArgument(ArgName::STORE_NAMES, ArgName::STORE_NAMES_SHORTNAME, ConsoleArgType::Bool);
	storeNameArg.SetEvaluationFunction(boolParser);
	
}