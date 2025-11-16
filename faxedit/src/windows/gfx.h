#ifndef FE_GFX_H
#define FE_GFX_H

#include <./SDL3/SDL.h>
#include <map>
#include <vector>
#include "./../common/klib/NES_tile.h"
#include "./../fe/Sprite_definitions.h"

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

		// cache all metatile definitions
		std::vector<SDL_Texture*> m_metatile_gfx;
		std::map<std::size_t, std::vector<SDL_Texture*>> m_sprite_gfx;

		SDL_Palette* m_nes_palette;

		Uint32 HOT_PINK_TRANSPARENT;

		// surface operations
		SDL_Surface* create_sdl_surface(int p_w, int p_h,
			bool p_transparent = false) const;
		void put_nes_pixel(SDL_Surface* srf, int x, int y, byte p_palette_index,
			bool p_transparent = false) const;
		SDL_Texture* surface_to_texture(SDL_Renderer* p_rnd, SDL_Surface* p_srf,
			bool p_destroy_surface = true);

		static const std::vector<std::vector<byte>> NES_PALETTE;

		void delete_texture(SDL_Texture* p_txt);

	public:
		gfx(SDL_Renderer* p_rnd);
		~gfx(void);

		// image caching operations
		void generate_atlas(SDL_Renderer* p_rnd, const std::vector<klib::NES_tile>& p_tiles, const std::vector<byte>& p_palette);
		void generate_mt_texture(SDL_Renderer* p_rnd, const std::vector<std::vector<byte>>& p_mt_def, std::size_t p_idx, std::size_t p_sub_palette_no);

		// blitting operations
		void blit(SDL_Renderer* p_rnd, SDL_Texture* p_texture, int p_x, int p_y) const;
		void blit_to_screen(SDL_Renderer* p_rnd, int tile_no, int sub_palette_no, int x, int y) const;
		void draw_pixel_rect_on_screen(SDL_Renderer* p_rnd, SDL_Color p_color, int pixel_x, int pixel_y, int pixel_w, int pixel_h) const;
		void draw_rect_on_screen(SDL_Renderer* p_rnd, SDL_Color p_color, int x, int y, int w, int h) const;
		void draw_sprite_on_screen(SDL_Renderer* p_rnd, std::size_t p_sprite_no, std::size_t p_frame_no, int x, int y) const;

		SDL_Texture* get_atlas(void) const;
		SDL_Texture* get_screen_texture(void) const;
		SDL_Texture* get_metatile_texture(std::size_t p_mt_no) const;
		std::size_t get_anim_frame_count(std::size_t p_sprite_no) const;

		void draw_nes_tile_on_surface(SDL_Surface* p_srf, int dst_x, int dst_y,
			const klib::NES_tile& tile, const std::vector<byte>& p_palette,
			bool p_transparent = false, bool h_flip = false, bool v_flip = false) const;

		static void set_app_icon(SDL_Window* p_window, const unsigned char* p_pixels);

		void gen_sprites(SDL_Renderer* p_rnd,
			const std::map<std::size_t, fe::Sprite_gfx_definiton>& p_defs);
	};

}

#endif
