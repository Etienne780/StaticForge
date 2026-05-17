#include "Internal/StaticForgeMetaParam.h"

namespace StaticForge::Internal {

	static bool s_initMetaParams = false;
	static std::unordered_map<std::string, StaticForgeMetaParam> s_metaParams;

	static void AddParam(const std::string_view& name, MetaParamType type) {
		std::string strName{ name };
		s_metaParams[strName] = StaticForgeMetaParam{ strName, type };
	}

	void InitMetaParams() {
		if (s_initMetaParams)
			return;
		s_initMetaParams = true;

		AddParam(MetaParams::ARCHIVE, MetaParamType::String);
	}

	const std::unordered_map<std::string, StaticForgeMetaParam>& GetAllMetaParams() {
		if(!s_initMetaParams)
			InitMetaParams();

		return s_metaParams;
	}

	StaticForgeMetaParam::StaticForgeMetaParam(const std::string& n, MetaParamType t) 
		: name(n), type(t) {
	}

}