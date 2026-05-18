#pragma once
#include <string>
#include <unordered_set>
#include <filesystem>

class ConsoleArgument;

class Config  {
public:
	Config(int count, char* argv[]);

	bool IsValid();

	const std::vector<std::filesystem::path>& GetSourcePaths() const;
	std::filesystem::path GetOutputPath() const;
	const std::string GetArchiveName() const;
	bool GetCreateOutputDir() const;
	bool GetDebug() const;

	const std::string& GetError() const;

private:
	std::string m_error;
	std::unordered_set<std::string> m_usedArguments;

	std::vector<std::filesystem::path> m_srcAbsolPath;
	std::filesystem::path m_outputAbsolPath;
	
	std::string m_archiveName = "main";
	bool m_createOutputDir = false;
	bool m_debug = false;

	void ProcessArgument(int index, int count, char* argv[]);
	void EvaluateArgument(const std::string& name, const std::string& value);

	void* GetOuput(const ConsoleArgument* argument);
	bool TryGetValueList(const std::string& value, std::vector<std::string>* outList);

	void ValidateConfig();

	static std::string TrimStr(const std::string& str);
	void AddError(const std::string& error);
};