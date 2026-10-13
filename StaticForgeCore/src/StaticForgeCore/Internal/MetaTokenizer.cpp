#include "Internal/MetaTokenizer.h"

namespace StaticForge::Internal {

	MetaTokenizer::MetaTokenizer(const StaticForgePath& path) 
		: ErrorSupport(!HAS_HEADER){
		std::ifstream fileStream(path, std::ios::binary | std::ios::ate);
		if (!fileStream.is_open()) {
			AddError("Failed to open file: " + path.u8string());
			return;
		}

		m_fileContent.assign(static_cast<size_t>(fileStream.tellg()), '\0');
		fileStream.seekg(0);
		fileStream.read(m_fileContent.data(), static_cast<std::streamsize>(m_fileContent.size()));
		fileStream.close();

		m_cur = m_fileContent.data();
		m_end = m_fileContent.data() + m_fileContent.size();

		// skips the UTF-8 BOM some editors write at the start of the file
		if (m_fileContent.rfind("\xEF\xBB\xBF", 0) == 0)
			m_cur += 3;

		ProcessContent();
	}

	bool MetaTokenizer::IsIdentifierChar(char c) {
		const unsigned char uc = static_cast<unsigned char>(c);
		// bytes >= 0x80 are part of UTF-8 characters (e.g. archive = chinese chars;)
		return std::isalnum(uc) || c == '_' || c == '.' || c == '-' || uc >= 0x80;
	}

	const std::vector<MetaToken>& MetaTokenizer::GetTokens() const {
		return m_tokens;
	}

	void MetaTokenizer::ProcessContent() {
		char c;
		while (Get(c)) {
			ProcessChar(c);
		}

		AddToken(MetaTokenType::EndOfFile);
	}

	void MetaTokenizer::ProcessChar(char c) {
		Advance(c);

		if (std::isspace(static_cast<unsigned char>(c)))
			return;

		switch (c) {
		case Symbols::ASSIGN:
			AddToken(MetaTokenType::Assign);
			break;
		case Symbols::SEPERATOR:
			AddToken(MetaTokenType::Separator);
			break;
		case Symbols::DELIMITER:
			AddToken(MetaTokenType::Delimiter);
			break;
		case Symbols::COMMENT:
			SkipComment();
			break;
		default:
			if (IsIdentifierChar(c)) {
				Unget();
				--m_column;
				ReadIdentifier();
				break;
			}
			AddError(std::string("Unexpected character '") + c + "'");
			AddToken(MetaTokenType::UNKNOWN);
			break;
		}
	}

	void MetaTokenizer::SkipComment() {
		char c;
		while (Get(c)) {
			Advance(c);
			if (c == '\n') break;
		}
	}

	void MetaTokenizer::ReadIdentifier() {
		uint32_t startLine = m_line;
		uint32_t startColumn = m_column;

		char c;
		const char* textStart = m_cur;
		while (!EndOfFile()) {
			c = *m_cur;
			if (!IsIdentifierChar(c))
				break;
			m_cur++;
			Advance(c); 
		}

		AddToken(MetaTokenType::Identifier, textStart, m_cur, startLine, startColumn);
	}

	bool MetaTokenizer::Get(char& c) {
		if (m_cur >= m_end)
			return false;
		c = *m_cur++;
		return true;
	}

	void MetaTokenizer::Unget() {
		m_cur--;
	}

	bool MetaTokenizer::EndOfFile() {
		return m_cur >= m_end;
	}

	void MetaTokenizer::Advance(char c) {
		if (c == '\n') {
			m_line++;
			m_column = 1;
		}
		// UTF-8 continuation bytes belong to the previous character
		else if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) {
			m_column++;
		}
	}

	void MetaTokenizer::AddToken(MetaTokenType type) {
		m_tokens.emplace_back(type, m_line, m_column);
	}

	void MetaTokenizer::AddToken(
		MetaTokenType type, 
		const char* cStart, 
		const char* cEnd, 
		uint32_t line, 
		uint32_t column
	) {
		m_tokens.emplace_back(
			std::string_view{ cStart, static_cast<size_t>(cEnd - cStart) },
			type, 
			line,
			column
		);
	}

}