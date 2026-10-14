#pragma once
#include <string>

class Config;

namespace Actions {

	// prints "[Error] context: message", several messages (one per line) as a list below the context
	void PrintError(const std::string& context, const std::string& error);

	bool Help(const Config& config);

	bool Info(const Config& config);

	bool Pack(const Config& config);

	bool Extract(const Config& config);

	bool Verify(const Config& config);

}