#include <iostream>
#include <functional>
#include <StaticForgeCore/StaticForge.h>
#include "InitConsoleArguments.h"
#include "ConsoleActions.h"
#include "Config.h"

int main(int argc, char** argv) {
	std::cout << "======== StaticForgeTool v" << StaticForge::VERSION << " ========" << std::endl;

	InitConsoleArguments();

	// skip first argument
	char** newArgv = argv + 1;
	Config config{ argc - 1, newArgv };

	if (!config.IsValid()) {
		std::cout << config.GetError() << std::endl;
		return -1;
	}

	ConfigMode mode = config.GetMode();

	// select action
	std::function<bool(const Config&)> action = nullptr;
	switch (mode) {
	case ConfigMode::HELP:
		action = Actions::Help;
		break;
	case ConfigMode::INFO:
		action = Actions::Info;
		break;
	case ConfigMode::PACK:
		action = Actions::Pack;
		break;	
	case ConfigMode::UNKNOWN:
	default:
		std::cout << "Invalid config" << std::endl;
		return -1;
	}

	// execute action
	if (!action(config)) {
		return -1;
	}

	return 0;
}