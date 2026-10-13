#include "Utf8Console.h"

#if defined(_WIN32)
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif
#	ifndef NOMINMAX
#		define NOMINMAX
#	endif
#	include <windows.h>
#	include <shellapi.h>
#	pragma comment(lib, "shell32.lib")

Utf8Console::Utf8Console()
	: m_oldCodePage(GetConsoleOutputCP()) {
	SetConsoleOutputCP(CP_UTF8);
}

Utf8Console::~Utf8Console() {
	SetConsoleOutputCP(m_oldCodePage);
}

std::vector<std::string> Utf8Console::GetArguments(int argc, char** argv) const {
	// argv on windows uses the ANSI code page, so e.g. chinese names would become '??'
	int count = 0;
	LPWSTR* wideArgs = CommandLineToArgvW(GetCommandLineW(), &count);
	if (!wideArgs)
		return std::vector<std::string>(argv + (argc > 0 ? 1 : 0), argv + argc);

	std::vector<std::string> result;
	for (int i = 1; i < count; i++) {
		int size = WideCharToMultiByte(CP_UTF8, 0, wideArgs[i], -1, nullptr, 0, nullptr, nullptr);
		std::string arg(size > 0 ? size - 1 : 0, '\0');
		if (size > 1)
			WideCharToMultiByte(CP_UTF8, 0, wideArgs[i], -1, arg.data(), size, nullptr, nullptr);
		result.push_back(std::move(arg));
	}

	LocalFree(wideArgs);
	return result;
}

#else

Utf8Console::Utf8Console() = default;
Utf8Console::~Utf8Console() = default;

std::vector<std::string> Utf8Console::GetArguments(int argc, char** argv) const {
	// arguments are already UTF-8
	return std::vector<std::string>(argv + (argc > 0 ? 1 : 0), argv + argc);
}

#endif
