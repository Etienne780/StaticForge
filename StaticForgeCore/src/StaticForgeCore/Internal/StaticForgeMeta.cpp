#include "Internal/StaticForgeMeta.h"
#include "Internal/InternalHelpers.h"
#include "Internal/MetaTokenizer.h"

namespace StaticForge::Internal {

	StaticForgeMeta::StaticForgeMeta() 
		: ErrorSupport(!HAS_HEADER) {
	}

	bool StaticForgeMeta::Load(const StaticForgePath& path) {
		Reset();

		m_path = path;
		std::string error;
		if (!CheckFilepath(&error)) {
			AddError(error);
			return false;
		}

		// Tokenizer has to outlife the tokens
		MetaTokenizer tokenizer{ path };
		if (!tokenizer.IsValid()) {
			AddError("Tokenizer Failed: " + tokenizer.GetError());
			return false;
		}

		const auto& tokens = tokenizer.GetTokens();
		if (!ParseTokens(tokens, &error)) {
			AddError(error);
			return false;
		}

		return true;
	}

	const StaticForgePath& StaticForgeMeta::GetPath() const {
		return m_path;
	}

	const std::string& StaticForgeMeta::GetArchiveName() const {
		return m_archiveName;
	}

	bool StaticForgeMeta::CheckFilepath(std::string* errorOut) const {
		if (!std::filesystem::exists(m_path)) {
			*errorOut =
				"Meta file does not exist: '" +
				m_path.u8string() + "'";
			return false;
		}

		std::string fullExtension = Internal::GetFullExtension(m_path);
		if (fullExtension != META_FILE_EXTENSION) {
			*errorOut =
				"Invalid meta file extension for '" +
				m_path.u8string() +
				"' (expected '" +
				std::string(META_FILE_EXTENSION) +
				"')";
			return false;
		}

		return true;
	}

	bool StaticForgeMeta::ParseTokens(const std::vector<Internal::MetaToken>& tokens, std::string* errorOut) {
		ParserContext ctx{ &tokens };
		std::string error;

		using TType = Internal::MetaTokenType;

		auto addErrTokeAt = [errorOut](const std::string& error, const Internal::MetaToken* token) {
			if (!token) {
				*errorOut = error;
				return;
			}

			std::string e = error;
			e.append(" at line: ");
			e.append(std::to_string(token->line));
			e.append(", column: ");
			e.append(std::to_string(token->column));

			*errorOut = e;
		};

		const Internal::MetaToken* assignToken = nullptr;
		const Internal::MetaToken* delimiterToken = nullptr;
		const Internal::MetaToken* firstIdentifierToken = nullptr;
		const Internal::MetaToken* secondIdentifierToken = nullptr;
		while (Get(&firstIdentifierToken, ctx)) {
			bool gotAssign = Get(&assignToken, ctx);
			bool gotSecond = Get(&secondIdentifierToken, ctx);
			bool gotDelim = Get(&delimiterToken, ctx);

			if (!gotAssign || !gotSecond || !gotDelim) {
				*errorOut = "Unexpected end of file";
				return false;
			}

			if (!Expect(TType::Identifier, firstIdentifierToken)) {
				addErrTokeAt("Expected identifer", firstIdentifierToken);
				return false;
			}

			if (!Expect(TType::Assign, assignToken)) {
				addErrTokeAt("Expected assign operator", assignToken);
				return false;
			}

			if (!Expect(TType::Identifier, secondIdentifierToken)) {
				addErrTokeAt("Expected identifer", secondIdentifierToken);
				return false;
			}

			if (!Expect(TType::Delimiter, delimiterToken)) {
				addErrTokeAt("Expected delimiter", delimiterToken);
				return false;
			}

			if (!ParseStatment(
				firstIdentifierToken,
				secondIdentifierToken,
				&error
			)) {
				*errorOut = error;
				return false;
			}
		}

		return true;
	}

	bool StaticForgeMeta::ParseStatment(
		const Internal::MetaToken* first,
		const Internal::MetaToken* second,
		std::string* errorOut
	) {
		if (!first || !second || first->content.empty() || second->content.empty())
			return false;

		const auto& metaParams = GetAllMetaParams();
		
		std::string n = std::string(first->content);
		auto it = metaParams.find(n);
		if (it == metaParams.end()) {
			std::string suggestion = "";
			bool found = TryGetIdentifierSuggestions(n, &suggestion);

			*errorOut = "Unkown identifier '" + n + (found ? "'. " + suggestion : "'");
			return false;
		}

		void* output = GetOuput(n);
		if (!output) {
			*errorOut = "Output for identifier '" + n + "' not defined";
			return false;
		}

		if (!ParseValue(it->second.type, second, output)) {
			*errorOut = "Failed to parse value '" + std::string(second->content) + "'";
			return false;
		}

		return true;
	}

	bool StaticForgeMeta::ParseValue(MetaParamType type, const Internal::MetaToken* value, void* output) {
		switch (type) {
		case MetaParamType::String: {
			std::string* str = static_cast<std::string*>(output);
			*str = std::string(value->content);
			break;
		}
		case MetaParamType::Bool: {
			std::string v{ value->content };
			std::transform(v.begin(), v.end(), v.begin(),
				[](unsigned char c) { return std::tolower(c); });

			bool* b = static_cast<bool*>(output);
			*b = (v == "true");
			break;
		}
		default: {
			return false;
		}
		}
		
		return true;
	}

	bool StaticForgeMeta::Get(const Internal::MetaToken** outToken, ParserContext& ctx) {
		if (ctx.currentPos >= ctx.tokens->size()) {
			*outToken = nullptr;
			return false;
		}

		*outToken = &(*ctx.tokens)[ctx.currentPos];
		ctx.currentPos++;

		if ((*outToken)->type == Internal::MetaTokenType::EndOfFile)
			return false;

		return true;
	}

	bool StaticForgeMeta::Expect(Internal::MetaTokenType type, const Internal::MetaToken* token) {
		return type == (token ? token->type : Internal::MetaTokenType::UNKOWN);
	}

	void* StaticForgeMeta::GetOuput(const std::string& name) {
		if (name == MetaParams::ARCHIVE) {
			return static_cast<void*>(&m_archiveName);
		}

		return nullptr;
	}

	bool StaticForgeMeta::TryGetIdentifierSuggestions(const std::string& name, std::string* outSuggestion) {
		constexpr size_t maxOffChars = 3;
		const std::string suggestionPrefix = "Did you mean ";
		
		const auto& metaParams = GetAllMetaParams();

		for (const auto& [other, _] : metaParams) {
			size_t nameSize = name.size();
			size_t otherSize = other.size();

			size_t maxSize = std::max(nameSize, otherSize);
			size_t minSize = std::min(nameSize, otherSize);

			size_t lengthDiff = maxSize - minSize;

			if (lengthDiff > maxOffChars)
				continue;

			size_t offChars = 0;
			for (size_t i = 0; i < minSize; i++) {
				if (name[i] != other[i])
					offChars++;

				if (offChars > maxOffChars)
					break;
			}

			if (minSize == 0 || offChars >= minSize - 1)
				continue;

			*outSuggestion = suggestionPrefix + "'" + other + "'?";
			return true;
		}

		return false;
	}

	void StaticForgeMeta::Reset() {
		m_error.clear();

		m_path.clear();
		m_archiveName.clear();
	}

}