#ifndef FE_GFX_H
#define FE_GFX_H

#include <./SDL3/SDL.h>
#include <map>
#include <unordered_map>
#include <optional>
#include <utility>
#include <set>
#include <vector>
#include "./../common/klib/NES_tile.h"
#include "./../fe/Sprite_definitions.h"
#include "./../fe/ChrStructures.h"

using byte = unsigned char;
using NES_Palette = std::vector<byte>;

namespace fe {

	struct MetaTileCandidate {
		std::vector<klib::NES_tile> m_tiles;
		std::size_t paletteIndex;

		std::vector<int> m_quad_errors;
		int reuseCount; // how many quadrants already exist
		int rgbError; // visual diff score

		MetaTileCandidate(void) :
			paletteIndex{ 0 },
			reuseCount{ 0 },
			rgbError{ 0 }
		{
		}
	};

	enum ChrDedupMode {
		PalIndex_Eq,     // strict byte equivalence: tiles equal only if raw CHR bitplanes match
		NESPalIndex_Eq,  // NES-faithful: tiles equal if palette indexes resolve identically
		rgb_Eq           // visual equivalence: tiles equal if resolved RGB values match
	};

	class gfx {

		// background tile atlas, tiles will be pulled from here when rendering the background gfx of a screen
		// based on a vector<NES tile> and a NES palette with 4 sub-palettes
		// 256 x 4 tiles, one row of tiles per sub-palette. Total dimensions (256*8 x 4x8) = 1024x32 pixels
		SDL_Texture* m_atlas;
		// texture for holding the screen data
		SDL_Texture* m_screen_texture;

		// cache all metatile definitions
		std::vector<SDL_Texture*> m_metatile_gfx;
		std::map<byte, SDL_Texture*> m_door_req_gfx;
		std::map<std::size_t, std::vector<SDL_Texture*>> m_sprite_gfx;
		std::unordered_map<std::string, SDL_Texture*> m_chr_bank_gfx;
		// map from (world, screen) to SDL_Texture of all its world metatiles as texture
		// use virtual keys for other tilemaps, like the intro, outro and title screens
		std::map<std::size_t, SDL_Texture*> m_tilemap_gfx;
		std::map<std::size_t, ChrTilemap> m_tilemap_import_results;

		SDL_Palette* m_nes_palette;

		SDL_Color m_hot_pink;

		// surface operations
		SDL_Surface* create_sdl_surface(int p_w, int p_h,
			bool p_transparent = false,
			bool p_set_no_colorkey = false) const;
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
		void draw_rect_on_surface(SDL_Surface* p_srf, int x, int y,
			int w, int h, SDL_Color color, int line_width = 1) const;

		static void set_app_icon(SDL_Window* p_window, const unsigned char* p_pixels);

		void gen_bank_chr_gfx(SDL_Renderer* p_rnd, const std::string& p_bank_id,
			const std::vector<fe::ChrGfxTile> tiles,
			const std::set<std::size_t>& p_fixed_idx_tiles = std::set<std::size_t>());
		SDL_Texture* get_bank_chr_gfx(const std::string& p_bank_id) const;

		void gen_sprites(SDL_Renderer* p_rnd,
			const std::map<std::size_t, fe::Sprite_gfx_definiton>& p_defs);
		void gen_door_req_gfx(SDL_Renderer* p_rnd,
			byte p_req_no,
			const std::vector<klib::NES_tile>& p_tiles,
			const std::vector<byte>& p_palette);
		SDL_Texture* anim_frame_to_texture(SDL_Renderer* p_rnd,
			const fe::AnimationFrame& p_frame,
			const std::vector<klib::NES_tile>& p_tiles,
			const std::vector<std::vector<byte>>& p_palette);
		SDL_Texture* get_tileset_txt(std::size_t p_key) const;
		void clear_tileset_textures(void);

		klib::NES_tile surface_region_to_nes_tile(SDL_Surface* srf,
			const std::vector<byte>& p_palette,
			int x, int y) const;
		int get_duplicate_count(const klib::NES_tile& p_q0, const klib::NES_tile& p_q1,
			const klib::NES_tile& p_q2, const klib::NES_tile& p_q3,
			const std::set<std::size_t>& p_reserved_idxs,
			const std::map<klib::NES_tile, std::vector<std::pair<std::size_t, std::size_t>>>& p_generated,
			const std::vector<klib::NES_tile>& p_tiles) const;

		bool has_tilemap_import_result(std::size_t p_key) const;
		ChrTilemap get_tilemap_import_result(std::size_t p_key) const;
		void clear_tilemap_import_result(std::size_t p_key);
		void clear_all_tilemap_import_results(void);

		// re-generate an already imported result under a new palette
		// use: view result under any palette before you commit
		void re_render_tilemap_result(SDL_Renderer* p_rnd,
			std::size_t p_key,
			const std::vector<byte>& p_palette);

		// functions for bmp import
		std::pair<int, int> import_tilemap_bmp(SDL_Renderer* p_rnd,
			std::vector<ChrGfxTile>& p_tiles,
			const std::vector<std::vector<byte>>& p_palette,
			ChrDedupMode p_dedupmode,
			const std::string& p_path,
			const std::string& p_filename,
			std::size_t p_key);

		MetaTileCandidate slice_and_quantize(
			SDL_Surface* p_srf,
			std::size_t mt_x, std::size_t mt_y,
			const std::vector<std::vector<byte>>& p_palette,
			std::size_t p_sub_pal_idx,
			fe::ChrDedupMode p_dedupmode,
			const std::vector<fe::ChrGfxTile>& p_tiles
		) const;

		std::vector<klib::NES_tile> gen_unique_tiles(
			const std::vector<klib::NES_tile>& p_tiles,
			const std::vector<byte>& p_palette,
			fe::ChrDedupMode p_dedupmode
		) const;

		int rgb_space_diff(const klib::NES_tile& p_tile,
			const std::vector<byte>& p_palette,
			SDL_Surface* srf, int x, int y) const;

		fe::MetaTileCandidate collapse_candidates(
			const std::vector<fe::MetaTileCandidate>& cands) const;

		std::size_t allocate_or_reuse_chr(const klib::NES_tile& tile,
			std::vector<ChrGfxTile>& p_tiles,
			std::map<klib::NES_tile, std::vector<std::size_t>>& tileToIndices,
			const std::vector<byte> p_palette,
			fe::ChrDedupMode p_dedupmode) const;

		std::size_t best_substitute_chr_index(
			SDL_Surface* srf,
			int px, int py,
			const std::vector<byte>& subPalette,
			const std::map<klib::NES_tile, std::vector<std::size_t>>& tileToIndices,
			const std::vector<ChrGfxTile>& p_tiles) const;

		std::pair<int, int> mt_to_pixels(std::size_t mt_x,
			std::size_t mt_y, std::size_t quadrant) const;

		std::vector<klib::NES_tile> chrtiletoindex_map_to_vector(
			const std::map<klib::NES_tile, std::vector<std::size_t>>& tileToIndices,
			std::size_t chr_count = 256
		) const;

		bool chr_tile_equivalence(fe::ChrDedupMode p_dedupmode,
			const klib::NES_tile& p_tile_a,
			const klib::NES_tile& p_tile_b,
			const std::vector<byte>& p_palette) const;

		bool rgb_equivalence(const SDL_Color a, const SDL_Color b) const;

		bool is_optional_bmp_region(SDL_Surface* srf,
			std::size_t mt_x, std::size_t mt_y) const;

		// functions for bmp export
		SDL_Surface* gen_tilemap_surface(const fe::ChrTilemap& p_tilemap) const;
		void gen_tilemap_texture(SDL_Renderer* p_rnd, const fe::ChrTilemap& p_tilemap,
			std::size_t p_key);

		void save_tilemap_bmp(const fe::ChrTilemap& p_tilemap,
			const std::string& p_path,
			const std::string& p_filename) const;

		// palette
		const SDL_Palette* get_nes_palette(void) const;
	};

}

#endif
