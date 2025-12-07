#ifndef FE_SCENE_H
#define FE_SCENE_H

#include <cstddef>

// scenes are per world, or per buildings screen

namespace fe {

	struct Scene {
		std::size_t m_palette,
			m_tileset,
			m_music;

		Scene(std::size_t p_palette, std::size_t p_tileset, std::size_t p_music) :
			m_palette{ p_palette },
			m_tileset{ p_tileset },
			m_music{ p_music }
		{
		}

		Scene(void) : Scene(256, 256, 256)
		{
		}

	};

}

#endif
