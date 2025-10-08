#ifndef FE_GFX_H
#define FE_GFX_H

#include <./SDL3/SDL.h>
#include <vector>
#include "./../common/klib/NES_tile.h"

using byte = unsigned char;

namespace fe {

	class gfx {

		std::vector<SDL_Texture*> m_textures;

		static const std::vector<std::vector<byte>> NES_PALETTE;

	public:

		gfx(void) = default;
		void generate_textures(SDL_Renderer* p_rnd, const std::vector<klib::NES_tile>& p_tiles);

		SDL_Texture* get_texture(std::size_t p_txt_no) const;
	};

}

#endif
