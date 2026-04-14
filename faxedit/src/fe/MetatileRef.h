#ifndef FE_METATILE_REF_H
#define FE_METATILE_REF_H

#include <cstddef>
#include <string>
#include <optional>

namespace fe {

	struct MetatileRef {
		std::optional<std::size_t> src_screen; // nullopt = metadata
		std::size_t metatile;

		enum class Type : unsigned char {
			ScreenTilemap,
			MattockAnimation,
			PushBlockDrawBlock,
			PushBlockSource,
			PushBlockTarget
		} type;

		std::string to_string(bool p_incl_mt_no = false) const;
	};

}

#endif
