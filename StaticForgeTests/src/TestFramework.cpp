#include <iostream>
#include <mutex>
#include <atomic>

#include "TestFramework.h"
#include "TestRunner.h"

namespace fs = std::filesystem;

namespace SFTest {

	static std::mutex s_outputMutex;
	static std::atomic<size_t> s_failures{ 0 };
	static std::atomic<size_t> s_tempCounter{ 0 };
	static fs::path s_runDirectory;
	static bool s_keepFiles = false;

	std::vector<TestCase>& Registry() {
		static std::vector<TestCase> registry;
		return registry;
	}

	Registrar::Registrar(const char* name, TestFunction function, bool stress) {
		Registry().push_back({ name, function, stress });
	}

	void ReportFailure(const char* file, int line, const std::string& expression, const std::string& message) {
		s_failures++;

		// only the file name, the full path is long and not needed
		const std::string fileName = fs::path(file).filename().string();

		std::lock_guard<std::mutex> lock(s_outputMutex);
		std::cout << "    FAILED " << fileName << ":" << line << ": " << expression;
		if (!message.empty())
			std::cout << "\n      -> " << message;
		std::cout << std::endl;
	}

	const fs::path& GetRunDirectory() {
		return s_runDirectory;
	}

	TempDir::TempDir() {
		m_path = s_runDirectory / ("t" + std::to_string(s_tempCounter++));
		fs::create_directories(m_path);
	}

	TempDir::~TempDir() {
		if (s_keepFiles)
			return;

		// only removes the folder this object created inside of the run folder
		std::error_code ec;
		if (!m_path.empty() && m_path.parent_path() == s_runDirectory)
			fs::remove_all(m_path, ec);
	}

	fs::path TempDir::operator/(const std::string& utf8) const {
		return m_path / fs::u8path(utf8);
	}

	size_t GetFailureCount() {
		return s_failures;
	}

	void SetRunDirectory(const fs::path& path, bool keepFiles) {
		s_runDirectory = path;
		s_keepFiles = keepFiles;
	}

}
