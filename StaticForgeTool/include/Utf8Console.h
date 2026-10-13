#pragma once
#include <string>
#include <vector>

/**
 * @brief Makes the console work with UTF-8 (e.g. chinese file names).
 *
 * On windows the console output is switched to UTF-8 while this object exists,
 * on other platforms nothing has to be done.
 */
class Utf8Console {
public:
	Utf8Console();
	~Utf8Console();

	Utf8Console(const Utf8Console&) = delete;
	Utf8Console& operator=(const Utf8Console&) = delete;

	/**
	 * @brief Returns the command line arguments as UTF-8 strings (without the program name).
	 */
	std::vector<std::string> GetArguments(int argc, char** argv) const;

private:
	unsigned int m_oldCodePage = 0;
};
