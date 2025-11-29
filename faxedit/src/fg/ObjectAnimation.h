#ifndef FG_PORTRAIT_H
#define FG_PORTRAIT_H

#include <vector>
#include <cstddef>
#include "AnimationFrame.h"
#include "./../common/klib/NES_tile.h"

using byte = unsigned char;

namespace fg {

	struct ObjectAnimation {
		std::vector<std::size_t> m_tile_lookup;
		std::vector<fg::AnimationFrame> m_frames;
	};

}

#endif
