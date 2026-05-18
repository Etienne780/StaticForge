#pragma once
#include <string>
#include <vector>

namespace StaticForge::Internal {

    constexpr bool HAS_HEADER = true;

    class ErrorSupport {
    public:
        ErrorSupport(bool hasHeader);
        ~ErrorSupport() = default;

        bool IsValid() const;
        std::string GetError() const;

    protected:
        void AddError(const std::string& error);

    private:
        bool m_hasHeader = false;
        std::vector<std::string> m_errors;
    };
    
}