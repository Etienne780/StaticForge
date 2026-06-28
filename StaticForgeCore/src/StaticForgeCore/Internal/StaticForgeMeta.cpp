#include <variant>

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

		FormatData(&m_metaData);

		return true;
	}

	const StaticForgePath& StaticForgeMeta::GetPath() const {
		return m_path;
	}

	const StaticForgeMetaData& StaticForgeMeta::GetLoadedMetaData() {
		return 	m_metaData;
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
		std::vector<const Internal::MetaToken*> values;
		while (Get(&firstIdentifierToken, ctx)) {
			bool gotAssign = Get(&assignToken, ctx);
			bool gotValues = GetList(&values, ctx);
			bool gotDelim = Get(&delimiterToken, ctx);

			if (!gotAssign || !gotValues || !gotDelim) {
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

			if (!Expect(TType::Delimiter, delimiterToken)) {
				addErrTokeAt("Expected delimiter", delimiterToken);
				return false;
			}

			if (!ParseStatment(
				firstIdentifierToken,
				values,
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
		const std::vector<const Internal::MetaToken*>& values,
		std::string* errorOut
	) {
		if (!first || first->content.empty() || values.empty())
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

		// validate values
		for (const auto& v : values) {
			if (v->content.empty())
				return false;
		}

		void* output = GetOutput(n);
		if (!output) {
			*errorOut = "Output for identifier '" + n + "' not defined";
			return false;
		}

		auto& param = it->second;
		if (!ParseValue(param.type, param.subType, values, output)) {
			*errorOut = "Failed to parse value '" + BuildString(values) + "'";
			return false;
		}

		return true;
	}

	bool StaticForgeMeta::ParseValue(MetaParamType type, MetaParamType subType, const std::vector<const Internal::MetaToken*>& values, void* output) {		
		using MTType = MetaParamType;
		
		switch (type) {
		case MTType::String: {
			std::string* str = static_cast<std::string*>(output);
			*str = std::string{ values[0]->content };
			break;
		}
		case MTType::Bool: {
			std::string v{ values[0]->content };

			std::transform(v.begin(), v.end(), v.begin(),
				[](unsigned char c) { return std::tolower(c); });

			bool* b = static_cast<bool*>(output);
			if (v == "true") {
				*b = true;
			}
			else if (v == "false") {
				*b = false;
			}
			else {
				return false;
			}
			break;
		}
		case MTType::List: {
			if (!ParseList(subType, values, output))
				return false;
			break;
		}
		case MTType::None:
		default: {
			return false;
		}
		}
		
		return true;
	}

	template<typename T>
	std::vector<T>* GetVectorType(const std::vector<const Internal::MetaToken*>& values, void* output) {
		auto* vec = static_cast<std::vector<T>*>(output);

		vec->clear();
		vec->reserve(values.size());
		return vec;
	};

	bool StaticForgeMeta::ParseList(MetaParamType subType, const std::vector<const Internal::MetaToken*>& values, void* output) {
		using MTType = MetaParamType;

		switch (subType) {
		case MTType::String: {
			auto* vec = GetVectorType<std::string>(values, output);

			for (const auto* v : values) {
				if (!v || v->content.empty())
					continue;

				vec->emplace_back(v->content);
			}

			break;
		}
		case MTType::Bool: { 
			auto* vec = GetVectorType<uint8_t>(values, output);

			for (const auto* v : values) {
				if (!v || v->content.empty())
					continue;

				bool out = false;
				if (!ParseValue(MTType::Bool, MTType::None, { v }, static_cast<void*>(&out)))
					return false;

				vec->emplace_back(static_cast<uint8_t>(out));
			}

			break;
		}
		case MTType::List:
		case MTType::None:
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

	void StaticForgeMeta::UnGet(ParserContext& ctx) {
		if (ctx.currentPos <= 0)

			return;
		ctx.currentPos--;
	}

	bool StaticForgeMeta::GetList(std::vector<const Internal::MetaToken*>* out, ParserContext & ctx) {
		using MTType = Internal::MetaTokenType;

		out->clear();

		const Internal::MetaToken* token = nullptr;

		while (Get(&token, ctx)) {
			if (!token)
				return false;

			// end of statement
			if (Expect(MTType::Delimiter, token)) {
				UnGet(ctx);
				break;
			}

			// separators are ignored
			if (Expect(MTType::Separator, token))
				continue;

			// only identifiers allowed as values
			if (!Expect(MTType::Identifier, token))
				return false;

			out->push_back(token);
		}

		return true;
	}

	bool StaticForgeMeta::Expect(Internal::MetaTokenType type, const Internal::MetaToken* token) {
		return type == (token ? token->type : Internal::MetaTokenType::UNKOWN);
	}

	void* StaticForgeMeta::GetOutput(const std::string& name) {
		if (name == MetaParams::ARCHIVE) {
			return static_cast<void*>(&m_metaData.archiveName);
		}

		if (name == MetaParams::EXCLUDE) {
			return static_cast<void*>(&m_metaData.excludedExtensions);
		}

		if (name == MetaParams::STORE_NAMES) {
			return static_cast<void*>(&m_metaData.storeNames);
		}

		return nullptr;
	}

	std::string StaticForgeMeta::BuildString(const std::vector<const Internal::MetaToken*>& outTokenList) {
		if (outTokenList.empty())
			return {};

		std::string result;
		result.reserve(outTokenList.size() * 4);

		for (size_t i = 0; i < outTokenList.size() - 1; i++) {
			result.append(outTokenList[i]->content);
			result.append(", ");
		}

		result.append(outTokenList[outTokenList.size() - 1]->content);

		return result;
	}


	void StaticForgeMeta::FormatData(StaticForgeMetaData* data) {
		{
			// normalizes exclude extensions to lower case
			auto& extensions = data->excludedExtensions;

			std::vector<std::string> result;
			result.reserve(extensions.size());

			for (const auto& ext : extensions) {
				std::string lower = ext;
				std::transform(lower.begin(), lower.end(), lower.begin(),
					[](unsigned char c) { return std::tolower(c); });

				result.push_back(lower);
			}

			extensions = result;
		}
	}

	bool StaticForgeMeta::TryGetIdentifierSuggestions(const std::string& name, std::string* outSuggestion) {
		const std::string suggestionPrefix = "Did you mean ";
		
		const auto& metaParams = GetAllMetaParams();

		for (const auto& [other, _] : metaParams) {
			if (!MatchesSuggestion(name, other))
				continue;

			*outSuggestion = suggestionPrefix + "'" + other + "'?";
			return true;
		}

		return false;
	}

	void StaticForgeMeta::Reset() {
		m_error.clear();

		m_path.clear();
		m_metaData = {};
	}

}