#pragma once
#include <string>
#include "StaticForgeTypes.h"
#include "Internal/ErrorSupport.h"

namespace StaticForge::Internal {

	struct MetaToken;
	enum class MetaTokenType;

	class StaticForgeMeta : public ErrorSupport {
	public:
		StaticForgeMeta();
		~StaticForgeMeta() = default;

		bool Load(const StaticForgePath& path);

		const StaticForgePath& GetPath() const;
		const std::string& GetArchiveName() const;


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
		std::string m_archiveName;

		bool CheckFilepath(std::string* errorOut) const;
		bool ParseTokens(const std::vector<Internal::MetaToken>& tokens, std::string* errorOut);
		bool ParseStatment(
			const Internal::MetaToken* first,
			const Internal::MetaToken* second,
			std::string* errorOut
		);

		bool ParseValue(MetaParamType type, const Internal::MetaToken* value, void* output);
		
		bool Get(const Internal::MetaToken** outToken, ParserContext& ctx);
		bool Expect(Internal::MetaTokenType type, const Internal::MetaToken* token);

		void* GetOuput(const std::string& name);
		bool TryGetIdentifierSuggestions(const std::string& name, std::string* outSuggestion);
		void Reset();
	};

}