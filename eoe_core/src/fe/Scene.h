#ifndef FE_SCENE_H
#define FE_SCENE_H

#include <cstddef>

// scenes are per world, or per buildings screen

using byte = unsigned char;

namespace fe {

	struct Scene {
		std::size_t m_palette,
			m_tileset,
			m_music;

		byte m_x, m_y;

		Scene(std::size_t p_palette, std::size_t p_tileset, std::size_t p_music,
			byte p_x, byte p_y) :
			m_palette{ p_palette },
			m_tileset{ p_tileset },
			m_music{ p_music },
			m_x{ p_x },
			m_y{ p_y }
		{
		}

		Scene(void) : Scene(256, 256, 256, 16, 16)
		{
		}

		byte get_pos_as_byte(void) const {
			return (m_y << 4) + m_x;
		}
	};

}

#endif
