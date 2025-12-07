#ifndef FE_GFX_H
#define FE_GFX_H

#include <./SDL3/SDL.h>
#include <map>
#include <utility>
#include <set>
#include <vector>
#include "./../common/klib/NES_tile.h"
#include "./../fe/Sprite_definitions.h"
#include "./../fe/Metatile.h"

using byte = unsigned char;
using NES_Palette = std::vector<byte>;

namespace fe {

	struct GfxResult {
		// Final patched CHR set (256 entries, with reserved slots respected)
		std::vector<klib::NES_tile> m_tileset;

		// Mapping from tile -> CHR index (HUD or custom - 0-255)
		std::map<klib::NES_tile, std::size_t> m_index_map;

		// Usage tracking: tile -> list of (metatile, quadrant)
		std::map<klib::NES_tile, std::vector<std::pair<std::size_t, std::size_t>>> m_usage;

		// Sub-palette assignment: metatileNo -> palette number (0..3)
		std::vector<std::size_t> m_metatile_sub_palette;
	};

	class gfx {

		// background tile atlas, tiles will be pulled from here when rendering the background gfx of a screen
		// based on a vector<NES tile> and a NES palette with 4 sub-palettes
		// 256 x 4 tiles, one row of tiles per sub-palette. Total dimensions (256*8 x 4x8) = 1024x32 pixels
		SDL_Texture* m_atlas;
		// texture for holding the screen data
		SDL_Texture* m_screen_texture;

		// cache all metatile definitions
		std::vector<SDL_Texture*> m_metatile_gfx, m_door_req_gfx;
		std::map<std::size_t, std::vector<SDL_Texture*>> m_sprite_gfx;
		// map from (world, screen) to SDL_Texture of all its world metatiles as texture
		std::map<std::pair<std::size_t, std::size_t>, SDL_Texture*> m_tileset_mt_gfx;

		SDL_Palette* m_nes_palette;

		Uint32 HOT_PINK_TRANSPARENT;

		// surface operations
		SDL_Surface* create_sdl_surface(int p_w, int p_h,
			bool p_transparent = false) const;
		void put_nes_pixel(SDL_Surface* srf, int x, int y, byte p_palette_index,
			bool p_transparent = false) const;
		SDL_Texture* surface_to_texture(SDL_Renderer* p_rnd, SDL_Surface* p_srf,
			bool p_destroy_surface = true);

		std::vector<SDL_Texture*> m_icon_overlays;

		SDL_Color uint24_to_SDL_Color(std::size_t l_col) const;
		void delete_texture(SDL_Texture* p_txt);

	public:
		gfx(SDL_Renderer* p_rnd);
		~gfx(void);

		// set NES palette
		void set_nes_palette(const std::vector<std::size_t>& p_palette);

		// image caching operations
		void generate_atlas(SDL_Renderer* p_rnd, const std::vector<klib::NES_tile>& p_tiles, const std::vector<byte>& p_palette);
		void generate_mt_texture(SDL_Renderer* p_rnd, const std::vector<std::vector<byte>>& p_mt_def, std::size_t p_idx, std::size_t p_sub_palette_no);
		void generate_icon_overlays(SDL_Renderer* p_rnd);
		void gen_tileset_txt(SDL_Renderer* p_rnd, std::size_t p_world_no,
			std::size_t p_screen_no,
			const std::vector<std::vector<byte>>& p_palette,
			const std::vector<fe::Metatile>& p_metatiles,
			const std::vector<klib::NES_tile>& p_tiles);
		void save_tileset_bmp(std::size_t p_world_no,
			std::size_t p_screen_no,
			const std::vector<std::vector<byte>>& p_palette,
			const std::vector<fe::Metatile>& p_metatiles,
			const std::vector<klib::NES_tile>& p_tiles,
			const std::string& p_path,
			const std::string& p_filename) const;
		SDL_Surface* gen_tileset_surface(const std::vector<std::vector<byte>>& p_palette,
			const std::vector<fe::Metatile>& p_metatiles,
			const std::vector<klib::NES_tile>& p_tiles) const;
		void save_surface_as_bmp(SDL_Surface* srf,
			const std::string& p_path,
			const std::string& p_filename,
			bool p_destroy_surface = true) const;
		SDL_Surface* load_bmp(const std::string& p_path,
			const std::string& p_filename) const;

		// blitting operations
		void blit(SDL_Renderer* p_rnd, SDL_Texture* p_texture, int p_x, int p_y) const;
		void blit_to_screen(SDL_Renderer* p_rnd, int tile_no, int sub_palette_no, int x, int y) const;
		void draw_pixel_rect_on_screen(SDL_Renderer* p_rnd, SDL_Color p_color, int pixel_x, int pixel_y, int pixel_w, int pixel_h) const;
		void draw_gridlines_on_screen(SDL_Renderer* p_rnd) const;
		void draw_rect_on_screen(SDL_Renderer* p_rnd, SDL_Color p_color, int x, int y, int w, int h) const;
		void draw_sprite_on_screen(SDL_Renderer* p_rnd, std::size_t p_sprite_no, std::size_t p_frame_no, int x, int y) const;
		void draw_icon_overlay(SDL_Renderer* p_rnd, int x, int y, byte block_property) const;
		void draw_door_req(SDL_Renderer* p_rnd, int x, int y, byte p_req) const;

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
		void gen_door_req_gfx(SDL_Renderer* p_rnd,
			const fe::Sprite_gfx_definiton& p_def);
		SDL_Texture* anim_frame_to_texture(SDL_Renderer* p_rnd,
			const fe::AnimationFrame& p_frame,
			const std::vector<klib::NES_tile>& p_tiles,
			const std::vector<std::vector<byte>>& p_palette);
		SDL_Texture* get_tileset_txt(std::size_t p_world_no, std::size_t p_screen_no) const;

		int score_region_vs_palette(SDL_Surface* srf, const std::vector<byte>& p_palette,
			int x, int y, int w, int h) const;
		std::size_t best_subpalette_region_fit(SDL_Surface* srf, const std::vector<std::vector<byte>>& p_palette,
			int x, int y, int w, int h) const;
		fe::GfxResult import_tileset_bmp(const std::vector<klib::NES_tile> p_tiles,
			std::set<std::size_t> p_read_only_idx,
			const std::vector<std::vector<byte>>& p_palette,
			const std::string& p_path,
			const std::string& p_filename) const;
		klib::NES_tile surface_region_to_nes_tile(SDL_Surface* srf,
			const std::vector<byte>& p_palette,
			int x, int y) const;
	};

}

#endif
