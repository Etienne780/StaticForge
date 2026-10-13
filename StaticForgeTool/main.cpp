#include <iostream>
#include <functional>
#include <StaticForgeCore/StaticForge.h>
#include "InitConsoleArguments.h"
#include "ConsoleActions.h"
#include "Config.h"
#include "Utf8Console.h"

int main(int argc, char** argv) {
	Utf8Console console;

	std::cout << "======== StaticForgeTool v" << StaticForge::VERSION << " ========" << std::endl;

	InitConsoleArguments();

	Config config{ console.GetArguments(argc, argv) };

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
	case ConfigMode::EXTRACT:
		action = Actions::Extract;
		break;
	case ConfigMode::UNKNOWN:
	default:
		std::cout << "Invalid config" << std::endl;
		return -1;
	}

	// execute action, catches unexpected errors so the tool never ends without a message
	try {
		if (!action(config)) {
			return -1;
		}
	}
	catch (const std::exception& e) {
		std::cout << "[Error]: Unexpected error: " << e.what() << std::endl;
		return -1;
	}

	return 0;
}
