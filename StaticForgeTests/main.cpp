#include <iostream>
#include <chrono>
#include <random>
#include <exception>

#include "TestFramework.h"
#include "TestRunner.h"

namespace fs = std::filesystem;

static void PrintHelp() {
	std::cout <<
		"Usage: StaticForgeTests [options] [filter...]\n"
		"\n"
		"  filter     only runs tests whose name contains one of the filters\n"
		"  --stress   also runs the stress tests (large files, many files, fuzzing)\n"
		"  --list     lists the tests without running them\n"
		"  --keep     keeps the temporary files of the run\n"
		"  --help     shows this help\n";
}

// a new folder for the files of this run, it must not exist yet
static bool CreateRunDirectory(fs::path* out) {
	std::error_code ec;
	const fs::path root = fs::temp_directory_path(ec) / "StaticForgeTests";
	if (ec)
		return false;

	fs::create_directories(root, ec);

	std::random_device device;
	for (int attempt = 0; attempt < 10; attempt++) {
		const fs::path path = root / ("run-" + std::to_string(device()));
		if (fs::create_directory(path, ec)) {
			*out = path;
			return true;
		}
	}
	return false;
}

int main(int argc, char** argv) {
	bool stress = false;
	bool list = false;
	bool keep = false;
	std::vector<std::string> filters;

	for (int i = 1; i < argc; i++) {
		const std::string arg = argv[i];
		if (arg == "--stress") stress = true;
		else if (arg == "--list") list = true;
		else if (arg == "--keep") keep = true;
		else if (arg == "--help") { PrintHelp(); return 0; }
		else if (arg.rfind("--", 0) == 0) { std::cout << "Unknown option '" << arg << "'\n"; PrintHelp(); return 1; }
		else filters.push_back(arg);
	}

	// selected tests
	std::vector<const SFTest::TestCase*> tests;
	for (const auto& test : SFTest::Registry()) {
		if (test.stress && !stress)
			continue;

		bool matches = filters.empty();
		for (const auto& filter : filters)
			matches = matches || test.name.find(filter) != std::string::npos;

		if (matches)
			tests.push_back(&test);
	}

	if (list) {
		for (const auto* test : tests)
			std::cout << test->name << (test->stress ? "  [stress]" : "") << "\n";
		return 0;
	}

	fs::path runDirectory;
	if (!CreateRunDirectory(&runDirectory)) {
		std::cout << "Failed to create a temporary folder for the test files\n";
		return 1;
	}
	SFTest::SetRunDirectory(runDirectory, keep);

	std::cout << "Running " << tests.size() << " tests" << (stress ? " (with stress tests)" : "") << "\n";
	std::cout << "Files: " << runDirectory.u8string() << "\n\n";

	size_t failedTests = 0;
	const auto runStart = std::chrono::steady_clock::now();

	for (const auto* test : tests) {
		std::cout << "[ RUN  ] " << test->name << std::endl;

		const size_t failuresBefore = SFTest::GetFailureCount();
		const auto start = std::chrono::steady_clock::now();

		try {
			test->function();
		}
		catch (const SFTest::RequireFailed&) {
			// already reported
		}
		catch (const std::exception& e) {
			SFTest::ReportFailure("exception", 0, "unexpected exception", e.what());
		}
		catch (...) {
			SFTest::ReportFailure("exception", 0, "unexpected exception", "unknown type");
		}

		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
		const bool failed = SFTest::GetFailureCount() != failuresBefore;
		if (failed)
			failedTests++;

		std::cout << (failed ? "[FAILED] " : "[  OK  ] ") << test->name << " (" << ms << " ms)" << std::endl;
	}

	const auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - runStart).count();
	std::cout << "\n" << (tests.size() - failedTests) << " of " << tests.size() << " tests passed (" << totalMs << " ms)\n";

	// only the folder of this run is removed, the parent only if it is empty
	if (!keep) {
		std::error_code ec;
		fs::remove_all(runDirectory, ec);
		fs::remove(runDirectory.parent_path(), ec);
	}

	return failedTests == 0 ? 0 : 1;
}
