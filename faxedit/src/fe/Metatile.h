#ifndef FE_METATILE_H
#define FE_METATILE_H

#include <vector>

using byte = unsigned char;
using Tilemap = std::vector<std::vector<byte>>;

namespace fe {

	class Metatile {

		byte m_attr_tl, m_attr_tr, m_attr_bl, m_attr_br;
		Tilemap m_tilemap;

	public:

		Metatile(byte l_tl, byte l_tr, byte l_bl, byte l_br, byte p_attributes);

		const Tilemap& get_tilemap(void) const;
		byte get_attr_tl(void) const;
		byte get_attr_tr(void) const;
		byte get_attr_br(void) const;
		byte get_attr_bl(void) const;

		byte get_palette_attribute(std::size_t p_x, std::size_t p_y) const;
	};

}

#endif

