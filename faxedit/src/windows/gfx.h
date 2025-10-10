#ifndef FE_GFX_H
#define FE_GFX_H

#include <./SDL3/SDL.h>
#include <vector>
#include "./../common/klib/NES_tile.h"

using byte = unsigned char;

namespace fe {

	class gfx {

		std::vector<std::vector<SDL_Texture*>> m_textures;
		SDL_Palette* m_nes_palette;

		// surface operations
		SDL_Surface* create_sdl_surface(int p_w, int p_h) const;
		void put_nes_pixel(SDL_Surface* srf, int x, int y, byte p_palette_index);
		SDL_Texture* surface_to_texture(SDL_Renderer* p_rnd, SDL_Surface* p_srf, bool p_destroy_surface = true);

		static const std::vector<std::vector<byte>> NES_PALETTE;

	public:
		gfx(void);
		~gfx(void);

		// blitting operations
		void blit(SDL_Renderer* p_rnd, SDL_Texture* p_texture, int p_x, int p_y) const;

		void generate_textures(SDL_Renderer* p_rnd, const std::vector<klib::NES_tile>& p_tiles);

		SDL_Texture* get_texture(std::size_t p_chunk_no, std::size_t p_txt_no) const;
	};

}

#endif
