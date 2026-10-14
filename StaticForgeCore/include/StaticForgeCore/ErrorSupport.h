#pragma once
#include <string>
#include <vector>

namespace StaticForge {

    /**
     * @brief Base class that provides simple error collection and validation support.
     *
     * Stores error messages and allows derived classes to report validation state.
     */
    class ErrorSupport {
    public:
        ErrorSupport() = default;

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
         * @return Error messages, one per line.
         */
        std::string GetError() const;

        /**
         * @brief Returns all collected error messages and clears them.
         *
         * @return Combined error message string.
         */
        std::string TakeError();

        /**
         * @brief Removes all collected error messages.
         */
        void ClearErrors();

    protected:
        /**
         * @brief Adds an error message to the internal error list.
         *
         * @param error Error message to store.
         */
        void AddError(const std::string& error);

    private:
        std::vector<std::string> m_errors;
    };

}