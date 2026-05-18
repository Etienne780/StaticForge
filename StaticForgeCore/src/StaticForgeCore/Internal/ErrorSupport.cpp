#include "Internal/ErrorSupport.h"

namespace StaticForge::Internal {

	ErrorSupport::ErrorSupport(bool hasHeader)
		: m_hasHeader(hasHeader) {
	}

	bool ErrorSupport::IsValid() const {
		return m_errors.empty();
	}

	void ErrorSupport::AddError(const std::string& error) {
		constexpr const char* header = "[Error]: ";
		constexpr const char* indent = ">        ";

		if (m_hasHeader) {
			const char* prefix = m_errors.empty() ? header : indent;
			m_errors.emplace_back(std::string(prefix) + error);
		}
		else {
			m_errors.emplace_back(error);
		}
	}

	std::string ErrorSupport::GetError() const {
		std::string result;

		for (size_t i = 0; i < m_errors.size(); ++i) {
			result += m_errors[i];

			if (i + 1 < m_errors.size()) {
				result += '\n';
			}
		}

		return result;
	}

}