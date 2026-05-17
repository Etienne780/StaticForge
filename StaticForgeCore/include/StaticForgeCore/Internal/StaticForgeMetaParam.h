#pragma once
#include <string>
#include <unordered_map>
#include "StaticForgeMetaParamNames.h"

namespace StaticForge::Internal {

	class StaticForgeMetaParam;

	void InitMetaParams();

	const std::unordered_map<std::string, StaticForgeMetaParam>& GetAllMetaParams();

	enum class MetaParamType {
		String = 0,
		Bool
	};

	class StaticForgeMetaParam {
	public:
		StaticForgeMetaParam() = default;
		StaticForgeMetaParam(const std::string& name, MetaParamType type);
		~StaticForgeMetaParam() = default;

		std::string name;
		MetaParamType type = MetaParamType::String;
	};

}