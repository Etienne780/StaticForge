#include <iostream>
#include <algorithm> 
#include "Config.h"
#include "ConsoleArgument.h"
#include "ArgumentNames.h"

Config::Config(int count, char* argv[]) {
	if (count == 0) {
		AddError("Invalid number of arguments write --help for more information: number of arguments '" + std::to_string(count) + "'!");
		return;
	}

	for (int i = 0; i < count; i++) {
		ProcessArgument(&i, count, argv);
	}

	CalculateCurrentMode();
	ValidateConfig();
}

bool Config::IsValid() const {
	return m_error.size() == 0;
}

ConfigMode Config::GetMode() const {
	return m_mode;
}

const std::vector<std::filesystem::path>& Config::GetSourcePaths() const {
	return m_srcAbsolPaths;
}

std::filesystem::path Config::GetOutputPath() const {
	return m_outputAbsolPath;
}

const std::vector<std::filesystem::path>& Config::GetInfoPaths() const {
	return m_infoPaths;
}

const std::string Config::GetArchiveName() const {
	return m_archiveName;
}

bool Config::GetCreateOutputDir() const {
	return m_createOutputDir;
}

bool Config::GetPrintHelp() const {
	return m_printHelp;
}

bool Config::GetVerbosePrint() const {
	return m_verbosePrint;
}

bool Config::GetStoreNames() const {
	return m_storeNames;
}

const std::string& Config::GetError() const {
	return m_error;
}

void Config::ProcessArgument(int* index, int count, char* argv[]) {
	char* argName = argv[*index];

	if (!IsArgumentFlag(argName)) {
		AddError("Invalid argument at index " + std::to_string(*index) + ": '" + std::string(argName) + "'. Expected format '--name'!");
		return;
	}

	std::string name = std::string(argName + 2);
	std::string value;

	if (*index + 1 < count && !IsArgumentFlag(argv[*index + 1])) {
		value = std::string(argv[*index + 1]);
		// increment index to skip value argument in outer for
		*index += 1;
	}

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

	if (arg->CanEvaluateValue()) {
		if (value.empty()) {
			AddError("Argument '" + name + "' requires a value");
			return;
		}

		std::vector<std::string> values;
		if (!TryGetValueList(value, &values)) {
			AddError("Failed to parse value list for argument '" + name + "'!");
			return;
		}

		for (auto& v : values) {
			void* output = GetOutput(arg);
			if (!output) {
				AddError("No fitting output found for argument '" + name + "'!");
				return;
			}

			arg->EvaluateValue(v, output);
		}
	}
	else {
		EvaluateFlagArgument(arg);
	}
}

void Config::EvaluateFlagArgument(const ConsoleArgument* arg) {
	if (arg->GetName() == ArgName::HELP) {
		m_printHelp = true;
		return;
	}
}

void* Config::GetOutput(const ConsoleArgument* arg) {
	if (!arg)
		return nullptr;

	if (arg->GetName() == ArgName::SOURCE) {
		m_srcAbsolPaths.emplace_back("");
		return static_cast<void*>(&m_srcAbsolPaths.back());
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

	if (arg->GetName() == ArgName::INFO) {
		m_infoPaths.emplace_back("");
		return static_cast<void*>(&m_infoPaths.back());
	}

	if (arg->GetName() == ArgName::VERBOSE) {
		return static_cast<void*>(&m_verbosePrint);
	}

	if (arg->GetName() == ArgName::STORE_NAMES) {
		return static_cast<void*>(&m_storeNames);
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

void Config::CalculateCurrentMode() {
	bool hasHelp = m_printHelp;
	bool hasInfo = !m_infoPaths.empty();
	bool hasPack = (!m_srcAbsolPaths.empty() && !m_outputAbsolPath.empty());

	m_activeModeCount = (hasHelp ? 1 : 0) + (hasInfo ? 1 : 0) + (hasPack ? 1 : 0);

	if (m_activeModeCount > 1) {
		m_mode = ConfigMode::UNKNOWN;
		return;
	}
	
	if (hasHelp) {
		m_mode = ConfigMode::HELP;
	} else if (hasInfo) {
		m_mode = ConfigMode::INFO;
	}
	else if (hasPack) {
		m_mode = ConfigMode::PACK;
	}
}
void Config::ValidateConfig() {
	bool hasPartialPack = (!m_srcAbsolPaths.empty() || !m_outputAbsolPath.empty());

	if (m_activeModeCount > 1) {
		AddError("Multiple modes selected. Choose exactly one: pack (--source + --output), --info, or --help");
		return;
	}
	if (m_activeModeCount == 0 && !hasPartialPack) {
		AddError("No mode selected. Use --help for usage information.");
		return;
	}

	switch (m_mode) {
	case ConfigMode::HELP:
		if (!m_srcAbsolPaths.empty())
			AddError("Cannot set source paths while printing help");
		if (!m_outputAbsolPath.empty())
			AddError("Cannot set output path while printing help");
		if (!m_infoPaths.empty())
			AddError("Cannot set info path while printing help");
		if (m_createOutputDir)
			AddError("Cannot set mkdir while printing help");
		if (m_archiveName != "main")
			AddError("Cannot set archive name while printing help");
		if (m_verbosePrint)
			AddError("Cannot set verbose while printing help");
		if (m_storeNames)
			AddError("Cannot set store name while printing help");
		break;

	case ConfigMode::INFO:
		if (!m_srcAbsolPaths.empty())
			AddError("--source cannot be used with --info");
		if (!m_outputAbsolPath.empty())
			AddError("--output cannot be used with --info");
		if (m_createOutputDir)
			AddError("--mkdir cannot be used with --info");
		if (m_archiveName != "main")
			AddError("--name cannot be used with --info");
		if (m_storeNames)
			AddError("--storename cannot be used with --info");
		// --verbose
		break;

	case ConfigMode::PACK:
		if (m_srcAbsolPaths.empty())
			AddError("--source is required for packing");
		if (m_outputAbsolPath.empty())
			AddError("--output is required for packing");
		if (!m_infoPaths.empty())
			AddError("--info cannot be used with packing");
		// --name, --mkdir, --verbose
		break;

	case ConfigMode::UNKNOWN:
	default:
		AddError("Unknown config");
		break;
	}

	if (hasPartialPack && m_mode != ConfigMode::PACK) {
		if (m_srcAbsolPaths.empty())
			AddError("--source is missing for packing (--output is set)");
		if (m_outputAbsolPath.empty())
			AddError("--output is missing for packing (--source is set)");
	}
}

bool Config::IsArgumentFlag(const char* arg) {
	return arg && arg[0] == '-' && (arg[1] == '-' || arg[1] != '\0');
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