#pragma once
#include <string>
#include <unordered_set>
#include <filesystem>

class ConsoleArgument;

enum class ConfigMode {
	UNKNOWN = 0,
	HELP,
	INFO,
	PACK,
};

class Config  {
public:
	Config(int count, char* argv[]);

	bool IsValid() const;
	ConfigMode GetMode() const;

	const std::vector<std::filesystem::path>& GetSourcePaths() const;
	std::filesystem::path GetOutputPath() const;
	std::filesystem::path GetInfoPath() const;

	const std::string GetArchiveName() const;
	bool GetCreateOutputDir() const;
	bool GetPrintHelp() const;
	bool GetVerbosePrint() const;

	const std::string& GetError() const;

private:
	std::string m_error;
	std::unordered_set<std::string> m_usedArguments;
	ConfigMode m_mode = ConfigMode::UNKNOWN;
	int m_activeModeCount = 0;

	std::vector<std::filesystem::path> m_srcAbsolPath;
	std::filesystem::path m_outputAbsolPath;
	std::filesystem::path m_infoPath;

	std::string m_archiveName = "main";
	bool m_createOutputDir = false;
	bool m_printHelp = false;
	bool m_verbosePrint = false;

	void ProcessArgument(int* index, int count, char* argv[]);
	void EvaluateArgument(const std::string& name, const std::string& value);

	void EvaluateFlagArgument(const ConsoleArgument* argument);
	void* GetOuput(const ConsoleArgument* argument);
	bool TryGetValueList(const std::string& value, std::vector<std::string>* outList);
	void CalculateCurrentMode();

	void ValidateConfig();

	static bool IsArgumentFlag(const char* arg);
	static std::string TrimStr(const std::string& str);
	void AddError(const std::string& error);
};