#pragma once
#include <string>
#include <vector>

namespace StaticForge {

    constexpr bool HAS_HEADER = true;

    /**
     * @brief Base class that provides simple error collection and validation support.
     *
     * Stores error messages and allows derived classes to report validation state.
     */
    class ErrorSupport {
    public:
        /**
         * @brief Creates a new ErrorSupport instance.
         *
         * @param hasHeader Indicates whether the associated data contains a valid header.
         */
        ErrorSupport(bool hasHeader);

        /**
         * @brief Virtual destructor.
         */
        virtual ~ErrorSupport() = default;

        /**
         * @brief Checks whether the object is in a valid state.
         *
         * @return true if no errors are stored; otherwise false.
         */
        bool IsValid() const;

        /**
         * @brief Returns all collected error messages as a single string.
         *
         * @return Combined error message string.
         */
        std::string GetError() const;

    protected:
        /**
         * @brief Adds an error message to the internal error list.
         *
         * @param error Error message to store.
         */
        void AddError(const std::string& error);

    private:
        bool m_hasHeader = false;
        std::vector<std::string> m_errors;
    };

}