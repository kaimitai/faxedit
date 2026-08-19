#ifndef FE_SCRIPT_TYPES_H
#define FE_SCRIPT_TYPES_H

#include <unordered_set>
#include <vector>

using byte = unsigned char;

namespace fe::script {

	struct ScriptSemanticInfo {
		std::vector<byte> gifts;
		std::unordered_set<byte> shop_items;
	};

}

#endif
