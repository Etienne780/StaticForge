#pragma once
#include <filesystem>

// only used by the runner in main.cpp
namespace SFTest {

	/**
	 * @brief Number of failed checks since the start of the run.
	 */
	size_t GetFailureCount();

	/**
	 * @brief Sets the folder for the temporary files of the run.
	 */
	void SetRunDirectory(const std::filesystem::path& path, bool keepFiles);

}
