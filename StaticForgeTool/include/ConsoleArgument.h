#pragma once
#include <string>
#include <vector>
#include <functional>

enum class ConsoleArgType {
	Filepath,
	String,
	Bool,
	Int,
	Float
};

class ConsoleArgument {
public:
	using EvalFunc = std::function<void(const std::string& value, void* output)>;

	ConsoleArgument() = default;
	ConsoleArgument(const std::string& name, const std::string& shortName, ConsoleArgType type);
	~ConsoleArgument() = default;

	void EvaluateValue(const std::string& value, void* output) const;

	bool MatchesName(const std::string& other) const;

	bool TryGetSuggestionMessage(const std::string& other, std::string& outSuggestion) const;

	ConsoleArgType GetType() const;

	const std::string& GetName() const;
	const std::string& GetShortForm() const;

	bool GetAllowMultiple() const;

	ConsoleArgument& SetEvaluationFunction(EvalFunc&& func);

	ConsoleArgument& SetName(const std::string& name);
	ConsoleArgument& SetShortForm(const std::string& shortName);

	ConsoleArgument& SetAllowMultiple(bool value);

private:
	EvalFunc m_evaluationFunc;
	ConsoleArgType m_type;

	std::string m_name;
	std::string m_shortName;

	bool m_allowMultiple = false;
};



class ConsoleArgumentManager {
public:
	ConsoleArgumentManager() = delete;
	ConsoleArgumentManager(const ConsoleArgumentManager&) = delete;
	ConsoleArgumentManager(ConsoleArgumentManager&&) = delete;

	ConsoleArgumentManager& operator=(const ConsoleArgumentManager&) = delete;
	ConsoleArgumentManager& operator=(ConsoleArgumentManager&&) = delete;

	static ConsoleArgument& RegisterConsoleArgument(const std::string_view& name, const std::string_view& shortName, ConsoleArgType type);

	static const ConsoleArgument* FindArgument(const std::string& name);
	static bool TryGetSuggestionMessage(const std::string& other, std::string& outSuggestion);

	static const std::vector<ConsoleArgument> GetAllArguments();

private:
	static inline std::vector<ConsoleArgument> s_arguments;
	static inline std::unordered_map<std::string, size_t> s_nameToArgIndex;
};