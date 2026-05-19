#include <iostream>
#include <StaticForgeCore/StaticForge.h>
#include "Config.h"
#include "InitConsoleArguments.h"

int main(int argc, char** argv) {
	std::cout << "======== StaticForgeTool v" << StaticForge::VERSION << " ========" << std::endl << std::endl;

	std::cout << "arg count: " << argc << std::endl;
	std::cout << "args: " << argc << std::endl;
	for (int i = 0; i < argc; i++)
		std::cout << "- " << argv[i] << std::endl;
	std::cout << std::endl;

	InitConsoleArguments();

	// skip first argument
	char** newArgv = argv + 1;
	Config config{ argc - 1, newArgv };

	if (!config.IsValid()) {
		std::cout << config.GetError() << std::endl;
		std::cin;
		return -1;
	}

	auto name = config.GetArchiveName();
	StaticForge::StaticForgeBuilder builder{ (name.empty() ? "main" : name) };
	builder
		.SetSourcePath(config.GetSourcePaths())
		.SetOutputPath(config.GetOutputPath())
		.SetCreateOutputDir(config.GetCreateOutputDir())
		.SetDebugMode(config.GetDebug());

	if (!builder.Build()) {
		std::cout << builder.GetError() << std::endl;
		std::cin;
		return -1;
	}

	StaticForge::StaticForgeArchive archive;
	StaticForge::StaticForgeReader reader;
	if (!reader.Load(config.GetOutputPath() / "main.sfpak", &archive)) {
		std::cout << reader.GetError() << std::endl;
		std::cin;
		return -1;
	}

	std::vector<std::byte> data;
	if (!archive.LoadAsset("Player_Skibidi_Bibub.png", data)) {
		std::cout << archive.GetError() << std::endl;
		std::cin;
		return -1;
	}

	std::cout << "Build was successful" << std::endl;

	return 0;
}