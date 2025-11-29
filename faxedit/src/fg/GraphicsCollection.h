#ifndef FG_GRAPHICS_COLLECTION_H
#define FG_GRAPHICS_COLLECTION_H

#include <vector>
#include "./../common/klib/NES_tile.h"
#include "ObjectAnimation.h"

using byte = unsigned char;

namespace fg {

	struct GraphicsCollection {
		std::vector<std::vector<byte>> m_palette;
		std::vector<klib::NES_tile> m_nes_tiles;
		std::vector<ObjectAnimation> m_obj_anims;
	};

}

#endif
