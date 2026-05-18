#include <iostream>
#include <algorithm> 
#include <functional>
#include "Config.h"
#include "ConsoleArgument.h"
#include "ArgumentNames.h"

Config::Config(int count, char* argv[]) {
	if (count % 2 != 0 || count == 0) {
		AddError("Invalid number of arguments: number of arguments '" + std::to_string(count) + "'!");
		return;
	}

	for (int i = 0; i < count; i += 2) {
		ProcessArgument(i, count, argv);
	}

	ValidateConfig();
}

bool Config::IsValid() {
	return m_error.size() == 0;
}

const std::vector<std::filesystem::path>& Config::GetSourcePaths() const {
	return m_srcAbsolPath;
}

std::filesystem::path Config::GetOutputPath() const {
	return m_outputAbsolPath;
}

const std::string Config::GetArchiveName() const {
	return m_archiveName;
}

bool Config::GetCreateOutputDir() const {
	return m_createOutputDir;
}

bool Config::GetDebug() const {
	return m_debug;
}

const std::string& Config::GetError() const {
	return m_error;
}

void Config::ProcessArgument(int index, int count, char* argv[]) {
	char* argName = argv[index];

	if (!(argName[0] == '-' && argName[1] == '-')) {
		AddError("Invalid argument at index " + std::to_string(index) + ": '" + std::string(argName) + "'. Expected format '--name'!");
		return;
	}

	std::string name = std::string(argName + 2);
	std::string value = std::string(argv[index + 1]);

	EvaluateArgument(name, value);
}

void Config::EvaluateArgument(const std::string& name, const std::string& value) {
	using CAM = ConsoleArgumentManager;
	
	const ConsoleArgument* arg = CAM::FindArgument(name);
	if (!arg) {
		const std::string errorMsg = "Unkown argument '" + name + "'";

		std::string suggestion;
		if (CAM::TryGetSuggestionMessage(name, suggestion)) {
			AddError(errorMsg + ". " + suggestion);
		}
		else {
			AddError(errorMsg);
		}

		return;
	}

	if (!arg->GetAllowMultiple()) {
		auto [it, inserted] = m_usedArguments.insert(arg->GetName());

		if (!inserted) {
			AddError("Multiple definitions of '" + arg->GetName() + "'");
			return;
		}
	}

	std::vector<std::string> values;
	if (!TryGetValueList(value, &values)) {
		AddError("Failed to parse value list for argument '" + name + "'!");
		return;
	}

	for (auto& v : values) {
		void* output = GetOuput(arg);
		if (!output) {
			AddError("No fitting output found for argument '" + name + "'!");
			return;
		}

		arg->EvaluateValue(v, output);
	}
}

void* Config::GetOuput(const ConsoleArgument* arg) {
	if (!arg) 
		return nullptr;

	if (arg->GetName() == ArgName::SOURCE) {
		m_srcAbsolPath.emplace_back("");
		return static_cast<void*>(&m_srcAbsolPath.back());
	}

	if (arg->GetName() == ArgName::OUTPUT) {
		return static_cast<void*>(&m_outputAbsolPath);
	}

	if (arg->GetName() == ArgName::MAKE_DIR) {
		return static_cast<void*>(&m_createOutputDir);
	}

	if (arg->GetName() == ArgName::ARCHIVE_NAME) {
		return static_cast<void*>(&m_archiveName);
	}

	if (arg->GetName() == ArgName::DEBUG_NAME) {
		return static_cast<void*>(&m_debug);
	}

	return nullptr;
}

bool Config::TryGetValueList(const std::string& value, std::vector<std::string>* outList) {
	size_t pos = value.find_first_of(',');
	if (pos == std::string::npos) {
		(*outList).emplace_back(value);
		return true;
	}

	(*outList).clear();

	size_t start = 0;
	while (start <= value.size()) {
		size_t end = value.find(',', start);

		if (end == std::string::npos)
			end = value.size();

		std::string token = value.substr(start, end - start);
		token = TrimStr(token);

		if (token.empty())
			return false;

		(*outList).emplace_back(std::move(token));

		start = end + 1;
	}

	return !(*outList).empty();
}

void Config::ValidateConfig() {
	if (m_srcAbsolPath.empty()) {
		AddError("No source paths defined!");
		return;
	}

	if (m_outputAbsolPath.empty()) {
		AddError("No output path defined!");
		return;
	}
}

std::string Config::TrimStr(const std::string& str) {
	size_t start = 0;
	size_t end = str.size();

	// left trim
	while (start < end && std::isspace(static_cast<unsigned char>(str[start]))) {
		++start;
	}

	// right trim
	while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
		--end;
	}

	return str.substr(start, end - start);
}

void Config::AddError(const std::string& error) {
	m_error.append("[Error]: ");
	m_error.append(error);
	m_error.push_back('\n');
}