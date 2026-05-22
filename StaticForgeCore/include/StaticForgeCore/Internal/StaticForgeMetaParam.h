#pragma once
#include <string>
#include <unordered_map>
#include "StaticForgeMetaParamNames.h"

namespace StaticForge::Internal {

	class StaticForgeMetaParam;

	void InitMetaParams();

	const std::unordered_map<std::string, StaticForgeMetaParam>& GetAllMetaParams();

	enum class MetaParamType {
		None = 0,
		String,
		Bool,
		List,
	};

	class StaticForgeMetaParam {
	public:
		StaticForgeMetaParam() = default;
		StaticForgeMetaParam(const std::string& name, MetaParamType type, MetaParamType subT = MetaParamType::None);
		~StaticForgeMetaParam() = default;

		std::string name;
		MetaParamType type = MetaParamType::None;
		MetaParamType subType = MetaParamType::None;
	};

}