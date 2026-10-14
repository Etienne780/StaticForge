#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace StaticForge::Internal {

	/**
	 * @brief Error list per thread, all functions are thread-safe.
	 *
	 * The functions without "All" only see the errors of the calling thread.
	 * Threads are numbered in the order of their first error, starting with 1.
	 */
	class ThreadErrors {
	public:
		/**
		 * @brief Adds an error for the calling thread (same format as ErrorSupport).
		 */
		void Add(const std::string& error);

		bool IsValid() const;
		std::string Get() const;
		std::string Take();
		void Clear();

		/**
		 * @brief Returns the errors of all threads, each line starts with "[Thread <n>] ".
		 */
		std::string GetAll() const;
		std::string TakeAll();
		void ClearAll();

	private:
		struct Entry {
			size_t number = 0;
			std::vector<std::string> errors;
		};

		mutable std::mutex m_mutex;
		std::unordered_map<std::thread::id, Entry> m_errors;
		size_t m_nextNumber = 1;

		std::string GetAllLocked() const;
	};

}
