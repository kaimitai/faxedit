#ifndef FE_WORLDVISUALIZER_H
#define FE_WORLDVISUALIZER_H

#include "Game.h"
#include "ChrStructures.h"
#include "./../common/klib/NES_tile.h"
#include "./sprite/SpriteAnimationFrame.h"
#include "./sprite/SpriteGUILoader.h"
#include <map>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using byte = unsigned char;
using PaletteId = std::size_t;
using ScreenId = std::size_t;
using IntPosition = std::pair<int, int>;

namespace fe {

	class WorldVisualizer {

		struct ResolvedScreen {
			IntPosition pos;
			std::size_t palette;
		};

		struct PlacementGraph {

			std::map<IntPosition, ScreenId> placements;

			std::vector<std::vector<std::optional<ScreenId>>> make_grid(void) const;
			bool occupied(IntPosition p) const;
			ScreenId at(IntPosition p) const;
			void place(ScreenId s, IntPosition p);
		};

		void draw_pixel_on_tilemap(GfxTilemap& p_tilemap,
			const GfxPalette& p_palette,
			std::size_t p_sub_palette,
			byte p_sub_palette_index,
			std::size_t p_pixel_x, std::size_t p_pixel_y) const;

		void draw_chr_tile_on_tilemap(GfxTilemap& p_tilemap,
			const GfxPalette& p_palette,
			std::size_t p_sub_palette,
			const klib::NES_tile& p_chr_tile,
			int p_pixel_x, int p_pixel_y,
			bool p_h_flip = false,
			bool p_v_flip = false,
			bool p_sprite = false) const;

		void draw_metatile_on_tilemap(GfxTilemap& p_tilemap,
			const fe::Metatile& p_metatile,
			const std::vector<klib::NES_tile>& p_tiles,
			const GfxPalette& p_palette,
			std::size_t p_mt_x, std::size_t p_mt_y) const;

		void draw_screen_on_tilemap(GfxTilemap& p_tilemap,
			const fe::Screen& p_screen,
			const std::vector<fe::Metatile>& p_metatiles,
			const std::vector<klib::NES_tile>& p_tiles,
			const GfxPalette& p_palette) const;

		void draw_animation_frame_on_tilemap(GfxTilemap& p_tilemap,
			const fe::SpriteAnimationFrame& p_frame,
			int p_pixel_x, int p_pixel_y,
			const std::vector<klib::NES_tile>& p_tiles,
			const GfxPalette& p_palette) const;

		GfxTilemap render_screen(const fe::Game& p_game, std::size_t p_world_no,
			std::size_t p_screen_no,
			std::size_t p_palette_no,
			const fe::SpriteGUILoader& p_sprites) const;

		std::optional<IntPosition> get_sw_trans_offset(const fe::Chunk& p_world,
			std::size_t p_screen_id) const;

		std::vector<std::vector<klib::NES_tile>> tilesets;

	public:
		WorldVisualizer(const std::vector<std::vector<klib::NES_tile>>& p_complete_tilesets);

		fe::WorldVisualization visualize_world(const fe::Game& game, std::size_t world_no,
			ScreenId start_screen, const fe::SpriteGUILoader& p_sprites) const;
	};

}

#endif
