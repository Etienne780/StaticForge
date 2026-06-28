#pragma once
#include <string_view>
#include <fstream>

#include "StaticForgeTypes.h"
#include "ErrorSupport.h"

namespace StaticForge::Internal {

	namespace Symbols {
		
		inline constexpr char ASSIGN = '=';
		inline constexpr char SEPERATOR = ',';
		inline constexpr char DELIMITER = ';';
		inline constexpr char COMMENT = '#';

	}

	enum class MetaTokenType {
		Identifier = 0,
		Assign,		// =
		Separator,	// ,
		Delimiter,	// ;
		EndOfFile,
		UNKOWN,
	};

	struct MetaToken {
		std::string_view content;
		MetaTokenType type = MetaTokenType::UNKOWN;
		uint32_t line = 1, column = 1;

		MetaToken() = default;
		MetaToken(MetaTokenType t, uint32_t l, uint32_t c)
			: type(t), line(l), column(c) {
		}
		MetaToken(std::string_view cont, MetaTokenType t, uint32_t l, uint32_t c)
			: content(cont), type(t), line(l), column(c) {
		}
	};

	class MetaTokenizer : public ErrorSupport {
	public:
		MetaTokenizer(const StaticForgePath& path);

		const std::vector<MetaToken>& GetTokens() const;

	private:
		std::string m_fileContent;
		const char* m_cur = nullptr;
		const char* m_end = nullptr;
		uint32_t m_line = 1, m_column = 1;

		std::vector<MetaToken> m_tokens;

		void ProcessContent();
		void ProcessChar(char c);

		void SkipComment();
		void ReadIdentifier();

		bool Get(char& c);
		void Unget();
		bool EndOfFile();
		void Advance(char c);

		void AddToken(MetaTokenType type);
		void AddToken(MetaTokenType type, const char* contentStart, const char* contentEnd, uint32_t line, uint32_t column);
	};

}