#pragma once
#include <string>
#include "StaticForgeTypes.h"
#include "ErrorSupport.h"

#include "Internal/StaticForgeMetaParam.h"

namespace StaticForge::Internal {

	struct MetaToken;
	enum class MetaTokenType;

	class StaticForgeMeta : public ErrorSupport {
	public:
		StaticForgeMeta();
		~StaticForgeMeta() = default;

		bool Load(const StaticForgePath& path);

		const StaticForgePath& GetPath() const;
		const StaticForgeMetaData& GetLoadedMetaData();

	private:
		struct ParserContext {
			const std::vector<Internal::MetaToken>* tokens = nullptr;
			size_t currentPos = 0;

			ParserContext(const std::vector<Internal::MetaToken>* t)
				: tokens(t) {
			}
		};

		std::string m_error;

		StaticForgePath m_path;
		StaticForgeMetaData m_metaData;

		bool CheckFilepath(std::string* errorOut) const;
		bool ParseTokens(const std::vector<Internal::MetaToken>& tokens, std::string* errorOut);
		bool ParseStatment(
			const Internal::MetaToken* first,
			const std::vector<const Internal::MetaToken*>& values,
			std::string* errorOut
		);

		bool ParseValue(MetaParamType type, MetaParamType subType, const std::vector<const Internal::MetaToken*>& values, void* output);
		bool ParseList(MetaParamType subType, const std::vector<const Internal::MetaToken*>& values, void* output);
		
		bool Get(const Internal::MetaToken** outToken, ParserContext& ctx);
		void UnGet(ParserContext& ctx);
		bool GetList(std::vector<const Internal::MetaToken*>* outTokenList, ParserContext& ctx);
		bool Expect(Internal::MetaTokenType type, const Internal::MetaToken* token);

		void* GetOutput(const std::string& name);
		std::string BuildString(const std::vector<const Internal::MetaToken*>& outTokenList);

		void FormatData(StaticForgeMetaData* data);
		bool TryGetIdentifierSuggestions(const std::string& name, std::string* outSuggestion);
		void Reset();
	};

}