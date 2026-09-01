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

	// png encode/decode and file I/O
	std::vector<byte> encode_png(const klib::Image& p_image);
	void save_png_to_file(const klib::Image& p_image, const std::string& p_filename);

	klib::Image decode_png(const std::vector<byte>& p_data);
	klib::Image load_png_from_file(const std::string& p_filename);

	// helpers
	std::vector<klib::RGB> parse_nes_palette(const fe::Config& p_config);

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
