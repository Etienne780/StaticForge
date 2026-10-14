#include <algorithm>

#include "Internal/ThreadErrors.h"

namespace StaticForge::Internal {

	static std::string JoinLines(const std::vector<std::string>& lines, const std::string& prefix) {
		std::string result;
		for (size_t i = 0; i < lines.size(); i++) {
			result += prefix + lines[i];
			if (i + 1 < lines.size())
				result += '\n';
		}
		return result;
	}

	void ThreadErrors::Add(const std::string& error) {
		std::lock_guard<std::mutex> lock(m_mutex);
		auto& entry = m_errors[std::this_thread::get_id()];
		if (entry.number == 0)
			entry.number = m_nextNumber++;

		// same format as ErrorSupport
		entry.errors.push_back((entry.errors.empty() ? "[Error]: " : ">        ") + error);
	}

	bool ThreadErrors::IsValid() const {
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_errors.find(std::this_thread::get_id());
		return it == m_errors.end() || it->second.errors.empty();
	}

	std::string ThreadErrors::Get() const {
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_errors.find(std::this_thread::get_id());
		return it == m_errors.end() ? std::string() : JoinLines(it->second.errors, "");
	}

	std::string ThreadErrors::Take() {
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_errors.find(std::this_thread::get_id());
		if (it == m_errors.end())
			return {};

		std::string result = JoinLines(it->second.errors, "");
		it->second.errors.clear();
		return result;
	}

	void ThreadErrors::Clear() {
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_errors.find(std::this_thread::get_id());
		if (it != m_errors.end())
			it->second.errors.clear();
	}

	std::string ThreadErrors::GetAll() const {
		std::lock_guard<std::mutex> lock(m_mutex);
		return GetAllLocked();
	}

	std::string ThreadErrors::TakeAll() {
		std::lock_guard<std::mutex> lock(m_mutex);
		std::string result = GetAllLocked();
		for (auto& [id, entry] : m_errors)
			entry.errors.clear();
		return result;
	}

	void ThreadErrors::ClearAll() {
		std::lock_guard<std::mutex> lock(m_mutex);
		// the numbers are kept, so a thread keeps its number
		for (auto& [id, entry] : m_errors)
			entry.errors.clear();
	}

	std::string ThreadErrors::GetAllLocked() const {
		// sorted by thread number
		std::vector<const Entry*> entries;
		for (const auto& [id, entry] : m_errors) {
			if (!entry.errors.empty())
				entries.push_back(&entry);
		}
		std::sort(entries.begin(), entries.end(), [](const Entry* a, const Entry* b) { return a->number < b->number; });

		std::string result;
		for (const Entry* entry : entries) {
			if (!result.empty())
				result += '\n';
			result += JoinLines(entry->errors, "[Thread " + std::to_string(entry->number) + "] ");
		}
		return result;
	}

}
