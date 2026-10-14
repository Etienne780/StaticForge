#include "ErrorSupport.h"

namespace StaticForge {

	bool ErrorSupport::IsValid() const {
		return m_errors.empty();
	}

	void ErrorSupport::AddError(const std::string& error) {
		m_errors.emplace_back(error);
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

	std::string ErrorSupport::TakeError() {
		std::string result = GetError();
		ClearErrors();
		return result;
	}

	void ErrorSupport::ClearErrors() {
		m_errors.clear();
	}

}