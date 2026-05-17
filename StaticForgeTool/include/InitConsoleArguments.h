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

}