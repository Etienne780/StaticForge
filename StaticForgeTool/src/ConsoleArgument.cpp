#include <cmath>
#include <iostream>
#include "ConsoleArgument.h"

ConsoleArgument::ConsoleArgument(const std::string& name, const std::string& shortName, ConsoleArgType type)
	: m_name(name), m_shortName(shortName), m_type(type) {
}

void ConsoleArgument::EvaluateValue(const std::string& value, void* output) const {
	m_evaluationFunc(value, output);
}

bool ConsoleArgument::MatchesName(const std::string& other) const {
	return other == m_name || other == m_shortName;
}

bool ConsoleArgument::TryGetSuggestionMessage(const std::string& other, std::string& outSuggestion) const {
	constexpr size_t maxOffChars = 3;
	const std::string suggestionPrefix = "Did you mean ";

	auto checkName = [&](const std::string& name, const std::string& other) -> bool {
		size_t nameSize = name.size();
		size_t otherSize = other.size();
		
		size_t maxSize = std::max(nameSize, otherSize);
		size_t minSize = std::min(nameSize, otherSize);
		
		size_t lengthDiff = maxSize - minSize;
		
		if (lengthDiff > maxOffChars)
			return false;

		size_t offChars = 0;
		for (size_t i = 0; i < minSize; i++) {
			if (name[i] != other[i])
				offChars++;

			if(offChars > maxOffChars)
				return false;
		}

		if (minSize == 0 || offChars >= minSize - 1)
			return false;

		return true;
	};

	if (checkName(m_name, other)) {
		outSuggestion = suggestionPrefix + "'" + m_name + "'?";
		return true;
	}

	if (checkName(m_shortName, other)) {
		outSuggestion = suggestionPrefix + "'" + m_shortName + "'?";
		return true;
	}

	return false;
}

ConsoleArgType ConsoleArgument::GetType() const {
	return m_type;
}

const std::string& ConsoleArgument::GetName() const {
	return m_name;
}

const std::string& ConsoleArgument::GetShortForm() const {
	return m_shortName;
}

bool ConsoleArgument::GetAllowMultiple() const {
	return m_allowMultiple;
}

ConsoleArgument& ConsoleArgument::SetEvaluationFunction(EvalFunc&& func) {
	m_evaluationFunc = std::move(func);
	return *this;
}

ConsoleArgument& ConsoleArgument::SetName(const std::string& name) {
	m_name = name;
	return *this;
}

ConsoleArgument& ConsoleArgument::SetShortForm(const std::string& shortName) {
	m_shortName = shortName;
	return *this;
}

ConsoleArgument& ConsoleArgument::SetAllowMultiple(bool value) {
	m_allowMultiple = value;
	return *this;
}



ConsoleArgument& ConsoleArgumentManager::RegisterConsoleArgument(const  std::string_view& name, const std::string_view& shortName, ConsoleArgType type) {
#ifdef DEBUG
	for (auto& arg : s_arguments) {
		// Case 1: same full name
		if (arg.GetName() == name) {
			std::cout
				<< "[Warning]: Argument '" << name << "(" << shortName << ")'"
				<< " conflicts with existing argument name '"
				<< arg.GetName() << "(" << arg.GetShortForm() << ")'."
				<< std::endl;
		}

		// Case 2: same short name matches existing full name
		if (arg.GetName() == shortName) {
			std::cout
				<< "[Warning]: Argument short name '" << shortName << "' of '"
				<< name << "(" << shortName << ")'"
				<< " conflicts with existing argument name '"
				<< arg.GetName() << "(" << arg.GetShortForm() << ")'."
				<< std::endl;
		}

		// Case 3: same short name collision
		if (arg.GetShortForm() == shortName) {
			std::cout
				<< "[Warning]: Argument short name '"
				<< shortName << "' of '"
				<< name << "(" << shortName << ")'"
				<< " conflicts with existing argument short name of '"
				<< arg.GetName() << "(" << arg.GetShortForm() << ")'."
				<< std::endl;
		}

		// Case 4: existing short name equals new full name
		if (arg.GetShortForm() == name) {
			std::cout
				<< "[Warning]: Argument name '"
				<< name << "(" << shortName << ")'"
				<< " conflicts with existing short name '"
				<< arg.GetShortForm() << "' of '"
				<< arg.GetName() << "(" << arg.GetShortForm() << ")'."
				<< std::endl;
		}
	}
#endif

	std::string strName = std::string(name);
	std::string strShortName = std::string(shortName);
	s_arguments.emplace_back(strName, strShortName, type);
	
	size_t index = s_arguments.size() - 1;
	s_nameToArgIndex[strName] = index;
	s_nameToArgIndex[strShortName] = index;

	return s_arguments.back();
}

const ConsoleArgument* ConsoleArgumentManager::FindArgument(const std::string& name) {
	auto it = s_nameToArgIndex.find(name);
	if (it == s_nameToArgIndex.end())
		return nullptr;
	return &s_arguments[it->second];
}

bool ConsoleArgumentManager::TryGetSuggestionMessage(const std::string& other, std::string& outSuggestion) {
	for (auto& arg : s_arguments) {
		if (arg.TryGetSuggestionMessage(other, outSuggestion))
			return true;
	}
	
	return false;
}

const std::vector<ConsoleArgument> ConsoleArgumentManager::GetAllArguments() {
	return s_arguments;
}
