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

	class Config;

	struct ScriptSemanticInfo {
		std::vector<byte> gifts;
		std::unordered_set<byte> shop_items;
	};

	struct WorldVisualizationOptions {
		// screen selection
		bool skip_unreferenced_screens{ true };

		// sprite rendering
		bool draw_enemies{ true };
		bool draw_npcs{ true };
		bool draw_items{ true };
		bool draw_other{ true };
		bool use_last_anim_frame{ true };

		// semantic overlays
		bool show_gifts{ true };
		bool show_shops{ true };

		// graph overlays
		bool show_door_labels{ true };
		bool show_door_requirements{ true };

		// integral parameters
		std::size_t sameworld_trans_tolerance{ 0 };
	};

	class WorldVisualizer {

		enum class DrawCommandType { Number, Item };

		struct DrawCommand {
			int x;
			int y;

			DrawCommandType type;
			std::size_t param;
			byte param_palette{ 0 };
		};

		struct ResolvedScreen {
			IntPosition pos;
			std::size_t palette;
		};

		struct BuildingKey {
			ScreenId screen;
			std::size_t sprite_set;

			auto operator<=>(const BuildingKey&) const = default;
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
			const fe::SpriteGUILoader& p_sprites,
			const fe::WorldVisualizationOptions& p_options) const;

		GfxTilemap render_screen(const fe::Game& p_game, std::size_t p_world_no,
			std::size_t p_screen_no,
			std::size_t p_palette_no,
			const fe::Sprite_set& p_sprite_set,
			const fe::SpriteGUILoader& p_sprites,
			const fe::WorldVisualizationOptions& p_options) const;

		void draw_number_on_tilemap(GfxTilemap& p_tilemap, std::size_t p_number,
			const std::vector<klib::NES_tile>& p_alphanumeric, byte p_palette,
			int x, int y, bool p_add_one = true) const;

		void draw_item_on_tilemap(GfxTilemap& p_tilemap, const fe::Game& p_game,
			std::size_t p_item, int x, int y) const;

		std::optional<IntPosition> get_sw_trans_offset(const fe::Chunk& p_world,
			std::size_t p_screen_id, std::size_t p_tolerance) const;

		std::vector<std::vector<klib::NES_tile>> tilesets;
		std::unordered_map<byte, fe::ScriptSemanticInfo> scripts;

		static constexpr std::size_t BUILDING_GRAPH_WIDTH{ 4 };
		static constexpr std::size_t BUILDING_SCREEN_IDX_OFFSET{ 0x100 };

		static const inline std::vector<GfxPalette> DRAW_COMMAND_PALETTES{
			{ 0x0f, 0x30, 0x30, 0x30 }, // door labels
			{ 0x06, 0x20, 0x20, 0x20 }, // door destination labels
			{ 0x0f, 0x28, 0x28, 0x28 }  // (door to building) labels
		};

		void maybe_add_door_requirement_item_draw(
			const fe::Config& p_config, std::unordered_map<ScreenId, std::vector<DrawCommand>>& p_draw_map,
			ScreenId p_screen, int p_x, int p_y, byte p_requirement) const;

		std::optional<byte> normalize_item_id(const fe::Config& p_config, byte script_item_id) const;

		void maybe_emit_script_item_draws(const fe::Config& p_config, ScreenId p_screen,
			const fe::Sprite_set& p_sprite_set,
			std::unordered_map<ScreenId, std::vector<DrawCommand>>& p_draw_map,
			bool p_show_gifts, bool p_show_shops) const;

	public:
		WorldVisualizer(const std::vector<std::vector<klib::NES_tile>>& p_complete_tilesets,
			const std::unordered_map<byte, fe::ScriptSemanticInfo>& p_scripts);

		fe::WorldVisualization visualize_world(const fe::Config& p_config,
			const fe::Game& game, std::size_t world_no,
			ScreenId start_screen, const fe::SpriteGUILoader& p_sprites,
			const fe::WorldVisualizationOptions& p_options) const;
	};

}

#endif
