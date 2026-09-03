#ifndef FE_GAMEGFX_H
#define FE_GAMEGFX_H

#include <map>
#include <set>
#include <utility>
#include <vector>
#include "fe/Game.h"
#include "common/klib/Image.h"
#include "fe/ChrStructures.h"

namespace fe {
	class Config;
}

namespace fe::game::gfx {

	void remap_fog_chr_tiles(const fe::Config& p_config, fe::Game& p_game,
		std::size_t p_tileset_no, const std::vector<byte>& p_desired_indexes);
	fe::ChrReorderResult remap_fog_chr_tiles(const fe::Tileset& p_tileset,
		const std::set<byte>& p_reserved_indexes, const std::vector<byte>& p_desired_indexes);

	void apply_tileset_chr_reorder(fe::Game& p_game, std::size_t p_tileset_no,
		const fe::ChrReorderResult& p_result);

	void reindex_metatile(fe::Metatile& p_metatile, const std::map<byte, byte>& p_old_to_new);
	void reindex_metatiles(std::vector<fe::Metatile>& p_metatiles, const std::map<byte, byte>& p_old_to_new);
	void reindex_metatiles(std::vector<fe::Metatile>& p_metatiles, const std::set<std::size_t>& p_metatile_indexes,
		const std::map<byte, byte>& p_old_to_new);

	void reindex_tileset_metatiles(fe::Game& p_game, std::size_t p_tileset_no,
		const fe::ChrReorderResult& p_result);

	std::map<byte, byte> get_chr_ppu_reindex_map(const fe::Tileset& p_tileset,
		const fe::ChrReorderResult& p_result);
	void set_reordered_chr_tiles(fe::Tileset& p_tileset, const fe::ChrReorderResult& p_result);
	fe::ChrReorderResult remap_chr_tiles(const std::vector<klib::NES_tile>& p_tiles,
		const std::map<std::size_t, std::size_t>& p_old_to_new);

	std::set<std::size_t> get_worlds_using_tileset(const Game& p_game, std::size_t p_tileset_no);
	std::set<std::size_t> get_building_metatiles_using_tileset(const Game& p_game,
		std::size_t p_tileset_no);
	std::set<std::size_t> get_chr_idxs_used_by_other_worlds(const Game& p_game, std::size_t p_tileset_no,
		std::size_t p_exclude_world);
	std::set<std::size_t> get_chr_idxs_used_by_other_buildings(const Game& p_game,
		std::size_t p_tileset_no, std::size_t p_world_no, std::size_t p_exclude_screen);
	std::set<std::size_t> get_reserved_chr_idxs(const Config& p_config,
		const Game& p_game, std::size_t p_world_no, std::size_t p_screen_no = 0);

	// png encode/decode and file I/O
	std::vector<byte> encode_png(const klib::Image& p_image);
	void save_png_to_file(const klib::Image& p_image, const std::string& p_filename);

	klib::Image decode_png(const std::vector<byte>& p_data);
	klib::Image load_png_from_file(const std::string& p_filename);

	// gfx helpers
	std::vector<klib::RGB> parse_nes_palette(const fe::Config& p_config);
	const klib::RGB& get_hot_pink(void);
	void draw_nes_tile_on_image(klib::Image& p_image,
		int p_dst_x, int p_dst_y, const klib::NES_tile& p_tile,
		const std::vector<byte>& p_palette, const std::vector<klib::RGB>& p_nes_palette,
		bool p_transparent = false, bool p_h_flip = false, bool p_v_flip = false);
	klib::Image gen_tilemap_image(const fe::ChrTilemap& p_tilemap,
		const std::vector<klib::RGB>& p_nes_palette);
	std::set<std::size_t> gen_metatile_usage(const fe::Game& p_game,
		std::size_t p_world_no, std::size_t p_screen_no,
		std::size_t p_total_metatile_count);
	std::vector<std::vector<byte>> flat_pal_to_2d_pal(const std::vector<byte>& pal);
	std::vector<klib::NES_tile> gen_world_tileset(const fe::Game& p_game,
		const fe::Config& p_config, std::size_t p_tileset_no);
	std::vector<std::vector<klib::NES_tile>> gen_world_tilesets(const fe::Game& p_game,
		const fe::Config& p_config);
	fe::ChrTilemap get_world_mt_tilemap(const fe::Game& p_game, const fe::Config& p_config,
		std::size_t p_world_no, std::size_t p_screen_no = 0);
	fe::ChrTilemap get_world_mt_tilemap(const fe::Game& p_game,
		const fe::WorldTilesetGfxDef& p_def);

	// background gfx import preparation
	fe::WorldTilesetGfxDef get_world_tileset_gfx_def(const Config& p_config,
		const Game& p_game, std::size_t p_world_no, std::size_t p_screen_no = 0);
	WorldTilesetGfxDef get_custom_world_tileset_gfx_def(const Config& p_config,
		const Game& p_game, std::size_t p_world_no, std::size_t p_tileset_no,
		std::size_t p_palette_no, std::size_t p_metatile_start, std::size_t p_metatile_end,
		std::size_t p_chr_tile_start, std::size_t p_chr_tile_end);

	// background gfx import commit
	void apply_world_tileset_gfx(fe::Game& p_game, const WorldTilesetGfxDef& p_def,
		const fe::ChrTilemap& p_result);

	// background gfx import pipeline
	fe::TilemapImportResult import_tilemap_image(const klib::Image& p_image,
		std::vector<ChrGfxTile>& p_tiles,
		const std::vector<std::vector<byte>>& p_palette,
		const std::vector<klib::RGB>& p_nes_palette,
		ChrDedupMode p_dedupmode);
	klib::NES_tile image_region_to_nes_tile(const klib::Image& p_image,
		const std::vector<klib::RGB>& p_nes_palette, const std::vector<byte>& p_palette,
		int p_x, int p_y);
	int rgb_space_diff(const klib::NES_tile& p_tile,
		const std::vector<klib::RGB>& p_nes_palette, const std::vector<byte>& p_palette,
		const klib::Image& p_image, int p_x, int p_y);
	bool rgb_equivalence(const klib::RGB& p_col_a, const klib::RGB& p_col_b);
	bool chr_tile_equivalence(fe::ChrDedupMode p_dedupmode,
		const klib::NES_tile& p_tile_a, const klib::NES_tile& p_tile_b,
		const std::vector<klib::RGB>& p_nes_palette, const std::vector<byte>& p_palette);
	bool chr_tile_equivalence(fe::ChrDedupMode p_dedupmode,
		const klib::NES_tile& p_tile_a, const klib::NES_tile& p_tile_b,
		const std::vector<klib::RGB>& p_nes_palette, const std::vector<byte>& p_palette);
	std::vector<klib::NES_tile> gen_unique_tiles(const std::vector<klib::NES_tile>& p_tiles,
		const std::vector<klib::RGB>& p_nes_palette, const std::vector<byte>& p_palette,
		fe::ChrDedupMode p_dedupmode);
	fe::MetaTileCandidate slice_and_quantize(
		const klib::Image& p_image,
		const std::vector<klib::RGB>& p_nes_palette,
		std::size_t mt_x, std::size_t mt_y,
		const std::vector<std::vector<byte>>& p_palette,
		std::size_t p_sub_pal_idx,
		fe::ChrDedupMode p_dedupmode,
		const std::vector<fe::ChrGfxTile>& p_tiles);
	std::pair<int, int> mt_to_pixels(std::size_t p_mt_x, std::size_t p_mt_y,
		std::size_t p_quadrant);
	bool is_optional_image_region(const klib::Image& p_image,
		std::size_t p_mt_x, std::size_t p_mt_y);
	fe::MetaTileCandidate collapse_candidates(const std::vector<fe::MetaTileCandidate>& p_candidates);
	std::size_t allocate_or_reuse_chr(const klib::NES_tile& tile,
		std::vector<ChrGfxTile>& p_tiles,
		std::map<klib::NES_tile, std::vector<std::size_t>>& tileToIndices,
		const std::vector<klib::RGB>& p_nes_palette,
		const std::vector<byte>& p_palette,
		fe::ChrDedupMode p_dedupmode);
	std::size_t best_substitute_chr_index(const klib::Image& p_image,
		int p_x, int p_y, const std::vector<klib::RGB>& p_nes_palette,
		const std::vector<byte>& p_sub_palette,
		const std::map<klib::NES_tile, std::vector<std::size_t>>& p_tile_to_indices,
		const std::vector<ChrGfxTile>& p_tiles);
	std::vector<klib::NES_tile> chrtiletoindex_map_to_vector(
		const std::map<klib::NES_tile, std::vector<std::size_t>>& p_tile_to_indices,
		std::size_t p_chr_count);

}

#endif
