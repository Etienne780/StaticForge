#include "Internal/StaticForgeMetaParam.h"

namespace StaticForge::Internal {

	static bool s_initMetaParams = false;
	static std::unordered_map<std::string, StaticForgeMetaParam> s_metaParams;

	static void AddParam(const std::string_view& name, MetaParamType type, MetaParamType subType = MetaParamType::None) {
		std::string strName{ name };
		s_metaParams[strName] = StaticForgeMetaParam{ strName, type, subType };
	}

	void InitMetaParams() {
		if (s_initMetaParams)
			return;
		s_initMetaParams = true;

		using MType = MetaParamType;

		AddParam(MetaParams::ARCHIVE, MType::String);
		AddParam(MetaParams::EXCLUDE, MType::List, MType::String);
		AddParam(MetaParams::STORE_NAME, MType::Bool);
	}

	const std::unordered_map<std::string, StaticForgeMetaParam>& GetAllMetaParams() {
		if(!s_initMetaParams)
			InitMetaParams();

		return s_metaParams;
	}

	StaticForgeMetaParam::StaticForgeMetaParam(const std::string& n, MetaParamType t, MetaParamType subT)
		: name(n), type(t), subType(subT) {
	}

}