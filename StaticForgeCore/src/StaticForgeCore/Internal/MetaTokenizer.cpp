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

		ProcessContent();
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
		case Symbols::DELIMITER:
			AddToken(MetaTokenType::Delimiter);
			break;
		case Symbols::COMMENT:
			SkipComment();
			break;
		default:
			if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
				Unget(); 
				--m_column;
				ReadIdentifier();
				break;
			}
			AddError(std::string("Unexpected character '") + c + "'");
			AddToken(MetaTokenType::UNKOWN);
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
			if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') 
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
		else {
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