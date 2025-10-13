#ifndef FE_GFX_H
#define FE_GFX_H

#include <./SDL3/SDL.h>
#include <vector>
#include "./../common/klib/NES_tile.h"

using byte = unsigned char;
using NES_Palette = std::vector<byte>;

namespace fe {

	class gfx {

		// background tile atlas, tiles will be pulled from here when rendering the background gfx of a screen
		// based on a vector<NES tile> and a NES palette with 4 sub-palettes
		// 256 x 4 tiles, one row of tiles per sub-palette. Total dimensions (256*8 x 4x8) = 1024x32 pixels
		SDL_Texture* m_atlas;
		// texture for holding the screen data
		SDL_Texture* m_screen_texture;

		SDL_Palette* m_nes_palette;

		// surface operations
		SDL_Surface* create_sdl_surface(int p_w, int p_h) const;
		void put_nes_pixel(SDL_Surface* srf, int x, int y, byte p_palette_index);
		SDL_Texture* surface_to_texture(SDL_Renderer* p_rnd, SDL_Surface* p_srf, bool p_destroy_surface = true);

		static const std::vector<std::vector<byte>> NES_PALETTE;

	public:
		gfx(SDL_Renderer* p_rnd);
		~gfx(void);

		// atlast operations
		void generate_atlas(SDL_Renderer* p_rnd, const std::vector<klib::NES_tile>& p_tiles, const std::vector<byte>& p_palette);

		// blitting operations
		void blit(SDL_Renderer* p_rnd, SDL_Texture* p_texture, int p_x, int p_y) const;
		void blit_to_screen(SDL_Renderer* p_rnd, int tile_no, int sub_palette_no, int x, int y) const;
		void draw_rect_on_screen(SDL_Renderer* p_rnd, SDL_Color p_color, int x, int y, int w, int h) const;

		SDL_Texture* get_screen_texture(void) const;
	};

}

#endif
