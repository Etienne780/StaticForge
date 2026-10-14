#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <filesystem>

namespace SFTest {

	using TestFunction = void (*)();

	struct TestCase {
		std::string name;
		TestFunction function = nullptr;
		bool stress = false;
	};

	/**
	 * @brief All registered tests, in the order of registration.
	 */
	std::vector<TestCase>& Registry();

	/**
	 * @brief Registers a test, used by TEST_CASE and STRESS_TEST.
	 */
	struct Registrar {
		Registrar(const char* name, TestFunction function, bool stress);
	};

	/**
	 * @brief Marks the current test as failed, can be called from several threads.
	 */
	void ReportFailure(const char* file, int line, const std::string& expression, const std::string& message);

	/**
	 * @brief Thrown by REQUIRE to end the current test.
	 */
	struct RequireFailed {};

	/**
	 * @brief Folder for the files of the run, removed at the end unless --keep is used.
	 */
	const std::filesystem::path& GetRunDirectory();

	/**
	 * @brief Temporary folder inside of the run folder, removed when destroyed unless --keep is used.
	 */
	class TempDir {
	public:
		TempDir();
		~TempDir();

		TempDir(const TempDir&) = delete;
		TempDir& operator=(const TempDir&) = delete;

		const std::filesystem::path& Path() const { return m_path; }
		std::filesystem::path operator/(const std::string& utf8) const;

	private:
		std::filesystem::path m_path;
	};

	template<typename T>
	std::string ToString(const T& value) {
		std::ostringstream stream;
		stream << value;
		return stream.str();
	}

}

#define SF_TEST_CONCAT2(a, b) a##b
#define SF_TEST_CONCAT(a, b) SF_TEST_CONCAT2(a, b)

#define SF_TEST_REGISTER(name, stress, id) \
	static void SF_TEST_CONCAT(sfTest_, id)(); \
	static SFTest::Registrar SF_TEST_CONCAT(sfTestRegistrar_, id)(name, &SF_TEST_CONCAT(sfTest_, id), stress); \
	static void SF_TEST_CONCAT(sfTest_, id)()

// normal test, runs every time
#define TEST_CASE(name) SF_TEST_REGISTER(name, false, __LINE__)
// slow test, only runs with --stress
#define STRESS_TEST(name) SF_TEST_REGISTER(name, true, __LINE__)

// the test continues after a failed CHECK
#define CHECK(expr) \
	do { if (!(expr)) SFTest::ReportFailure(__FILE__, __LINE__, #expr, ""); } while (false)

#define CHECK_MSG(expr, msg) \
	do { if (!(expr)) SFTest::ReportFailure(__FILE__, __LINE__, #expr, SFTest::ToString(msg)); } while (false)

#define CHECK_EQ(a, b) \
	do { \
		const auto& sfA = (a); const auto& sfB = (b); \
		if (!(sfA == sfB)) \
			SFTest::ReportFailure(__FILE__, __LINE__, #a " == " #b, SFTest::ToString(sfA) + " != " + SFTest::ToString(sfB)); \
	} while (false)

// the test ends after a failed REQUIRE
#define REQUIRE(expr) \
	do { if (!(expr)) { SFTest::ReportFailure(__FILE__, __LINE__, #expr, ""); throw SFTest::RequireFailed{}; } } while (false)

#define REQUIRE_MSG(expr, msg) \
	do { if (!(expr)) { SFTest::ReportFailure(__FILE__, __LINE__, #expr, SFTest::ToString(msg)); throw SFTest::RequireFailed{}; } } while (false)
