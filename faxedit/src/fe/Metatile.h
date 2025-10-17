#ifndef FE_METATILE_H
#define FE_METATILE_H

#include <vector>

using byte = unsigned char;
using Tilemap = std::vector<std::vector<byte>>;

namespace fe {

	struct Metatile {

		byte m_attr_tl, m_attr_tr, m_attr_bl, m_attr_br,
			m_block_property;
		Tilemap m_tilemap;

		Metatile(byte l_tl, byte l_tr, byte l_bl, byte l_br, byte p_attributes,
			byte p_block_property);

		byte get_palette_attribute(std::size_t p_x, std::size_t p_y) const;
	};

}

#endif
