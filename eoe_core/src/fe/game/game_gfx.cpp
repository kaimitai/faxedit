#include "game_gfx.h"
#include "fe/fe_constants.h"
#include "fe/Config.h"
#include "common/lodepng.h"
#include "common/klib/Kfile.h"
#include <algorithm>
#include <format>
#include <numeric>
#include <stdexcept>

namespace {
	klib::RGB g_hot_pink{ 0xff, 0x69, 0xb4 };
}

// re-arrange a tileset so the requested CHR tiles occupy the configured fog indexes.
void fe::game::gfx::remap_fog_chr_tiles(const fe::Config& p_config, fe::Game& p_game,
	std::size_t p_tileset_no, const std::vector<byte>& p_desired_indexes) {

	const auto reserved_indexes{ p_config.vset_as_set(c::ID_FOG_RESERVED_CHR_IDXS) };

	const auto result{ remap_fog_chr_tiles(p_game.m_tilesets.at(p_tileset_no),	reserved_indexes,
			p_desired_indexes) };

	apply_tileset_chr_reorder(p_game, p_tileset_no, result);
}

// re-arrange a tileset so the requested CHR tiles occupy the reserved fog indexes.
fe::ChrReorderResult fe::game::gfx::remap_fog_chr_tiles(const fe::Tileset& p_tileset,
	const std::set<byte>& p_reserved_indexes, const std::vector<byte>& p_desired_indexes) {

	if (p_reserved_indexes.size() != p_desired_indexes.size() ||
		std::set<byte>(begin(p_desired_indexes), end(p_desired_indexes)).size() != p_desired_indexes.size())
		throw std::runtime_error("Invalid fog CHR tile indexes");

	std::vector<std::size_t> tiles(p_tileset.tiles.size());
	std::iota(begin(tiles), end(tiles), 0);

	auto reserved{ begin(p_reserved_indexes) };
	for (std::size_t desired : p_desired_indexes) {
		const std::size_t reserved_idx{ *reserved++ };

		if (reserved_idx < p_tileset.start_idx || reserved_idx >= p_tileset.end_index())
			throw std::runtime_error("Reserved fog CHR tile index outside tileset");

		if (desired < p_tileset.start_idx || desired >= p_tileset.end_index())
			throw std::runtime_error("Fog CHR tile index outside tileset");

		const std::size_t dst{ reserved_idx - p_tileset.start_idx };
		const std::size_t wanted{ desired - p_tileset.start_idx };
		const auto src{ std::find(begin(tiles), end(tiles), wanted) };

		std::iter_swap(begin(tiles) + dst, src);
	}

	std::map<std::size_t, std::size_t> old_to_new;
	for (std::size_t i{ 0 }; i < tiles.size(); ++i)
		old_to_new[tiles[i]] = i;

	return remap_chr_tiles(p_tileset.tiles, old_to_new);
}

// apply a CHR reorder to a tileset and update all associated metatile references
void fe::game::gfx::apply_tileset_chr_reorder(fe::Game& p_game, std::size_t p_tileset_no,
	const fe::ChrReorderResult& p_result) {
	auto& tileset{ p_game.m_tilesets.at(p_tileset_no) };

	if (tileset.tiles.size() != p_result.tiles.size() ||
		tileset.tiles.size() != p_result.idx_old_to_new.size())
		throw std::runtime_error("CHR reorder result does not match tileset size");

	reindex_tileset_metatiles(p_game, p_tileset_no, p_result);
	set_reordered_chr_tiles(tileset, p_result);
}

void fe::game::gfx::reindex_metatile(fe::Metatile& p_metatile, const std::map<byte, byte>& p_old_to_new) {
	for (auto& row : p_metatile.m_tilemap)
		for (auto& chr_idx : row)
			if (auto it{ p_old_to_new.find(chr_idx) }; it != p_old_to_new.end())
				chr_idx = it->second;
}

void fe::game::gfx::reindex_metatiles(std::vector<fe::Metatile>& p_metatiles,
	const std::map<byte, byte>& p_old_to_new) {
	for (auto& mt : p_metatiles)
		reindex_metatile(mt, p_old_to_new);
}

void fe::game::gfx::reindex_metatiles(std::vector<fe::Metatile>& p_metatiles,
	const std::set<std::size_t>& p_metatile_indexes, const std::map<byte, byte>& p_old_to_new) {
	for (std::size_t mt_idx : p_metatile_indexes)
		reindex_metatile(p_metatiles.at(mt_idx), p_old_to_new);
}

void fe::game::gfx::set_reordered_chr_tiles(fe::Tileset& p_tileset, const fe::ChrReorderResult& p_result) {
	if (p_tileset.tiles.size() != p_result.tiles.size())
		throw std::runtime_error("CHR reorder tile count mismatch");
	if (p_tileset.tiles.size() != p_result.idx_old_to_new.size())
		throw std::runtime_error("CHR reorder index count mismatch");
	p_tileset.tiles = p_result.tiles;
}

// re-index metatile CHR references for all worlds and building screens using a tileset
void fe::game::gfx::reindex_tileset_metatiles(fe::Game& p_game, std::size_t p_tileset_no,
	const fe::ChrReorderResult& p_result) {
	const auto& tileset{ p_game.m_tilesets.at(p_tileset_no) };
	const auto ppu_map{ get_chr_ppu_reindex_map(tileset, p_result) };

	// normal worlds using this tileset
	for (std::size_t world : get_worlds_using_tileset(p_game, p_tileset_no))
		reindex_metatiles(p_game.m_chunks.at(world).m_metatiles, ppu_map);

	// building metatiles used by screens using this tileset
	const auto building_mts{ get_building_metatiles_using_tileset(p_game, p_tileset_no) };

	reindex_metatiles(p_game.m_chunks.at(c::CHUNK_IDX_BUILDINGS).m_metatiles,
		building_mts, ppu_map);
}

// Convert a local CHR old-to-new index mapping to absolute PPU CHR indexes
std::map<byte, byte> fe::game::gfx::get_chr_ppu_reindex_map(const fe::Tileset& p_tileset,
	const fe::ChrReorderResult& p_result) {
	if (p_result.idx_old_to_new.size() != p_tileset.tiles.size())
		throw std::runtime_error("CHR reorder index count does not match tileset");

	std::map<byte, byte> result;

	for (std::size_t old_idx{ 0 }; old_idx < p_result.idx_old_to_new.size(); ++old_idx) {
		const std::size_t old_ppu{ p_tileset.start_idx + old_idx };
		const std::size_t new_ppu{ p_tileset.start_idx + p_result.idx_old_to_new[old_idx] };

		if (old_ppu > 0xff || new_ppu > 0xff)
			throw std::runtime_error("CHR reorder PPU index out of range");

		if (old_ppu != new_ppu)
			result.emplace(static_cast<byte>(old_ppu), static_cast<byte>(new_ppu));
	}

	return result;
}

// Reorder CHR tiles using a partial old-index -> new-index mapping.
// Unspecified indexes remain unchanged. The mapping must resolve to a valid
// permutation: every tile is preserved and every destination is used exactly once.
// Returns the reordered tiles and a complete old-index -> new-index mapping.
fe::ChrReorderResult fe::game::gfx::remap_chr_tiles(
	const std::vector<klib::NES_tile>& p_tiles,
	const std::map<std::size_t, std::size_t>& p_old_to_new) {

	fe::ChrReorderResult result;
	const std::size_t tile_count{ p_tiles.size() };

	result.tiles = p_tiles;
	result.idx_old_to_new.resize(tile_count);

	// start with identity mapping
	for (std::size_t i{ 0 }; i < tile_count; ++i)
		result.idx_old_to_new[i] = i;

	std::set<std::size_t> destinations;

	// validate and install requested mapping
	for (const auto& [old_idx, new_idx] : p_old_to_new) {
		if (old_idx >= tile_count || new_idx >= tile_count)
			throw std::runtime_error("CHR remap index out of range");

		if (!destinations.insert(new_idx).second)
			throw std::runtime_error("CHR remap contains duplicate destination index");

		result.idx_old_to_new[old_idx] = new_idx;
	}

	// ensure the resulting complete mapping is a permutation
	destinations.clear();

	for (std::size_t new_idx : result.idx_old_to_new) {
		if (!destinations.insert(new_idx).second)
			throw std::runtime_error("CHR remap does not form a valid permutation");
	}

	// move each tile from its old position to its new position
	for (std::size_t old_idx{ 0 }; old_idx < tile_count; ++old_idx)
		result.tiles[result.idx_old_to_new[old_idx]] = p_tiles[old_idx];

	return result;
}

std::set<std::size_t> fe::game::gfx::get_worlds_using_tileset(const fe::Game& p_game, std::size_t p_tileset_no) {
	std::set<std::size_t> result;
	for (std::size_t i{ 0 }; i < p_game.m_chunks.size(); ++i) {
		if (i != fe::c::CHUNK_IDX_BUILDINGS &&
			p_game.get_default_tileset_no(i, 0) == p_tileset_no)
			result.insert(i);
	}
	return result;
}

std::set<std::size_t> fe::game::gfx::get_building_metatiles_using_tileset(const fe::Game& p_game,
	std::size_t p_tileset_no) {
	std::set<std::size_t> result;
	const auto& screens{ p_game.m_chunks.at(c::CHUNK_IDX_BUILDINGS).m_screens };

	for (std::size_t i{ 0 }; i < screens.size(); ++i) {
		if (p_game.get_default_tileset_no(c::CHUNK_IDX_BUILDINGS, i) != p_tileset_no)
			continue;

		for (const auto& row : screens[i].m_tilemap)
			for (byte mt_idx : row)
				result.insert(static_cast<std::size_t>(mt_idx));
	}

	return result;
}

std::set<std::size_t> fe::game::gfx::get_chr_idxs_used_by_other_worlds(const fe::Game& p_game,
	std::size_t p_tileset_no, std::size_t p_exclude_world) {
	std::set<std::size_t> result;

	// let us reserve chr indexes which are used by metatile
	// definitions for other worlds using this tileset so we don't make
	// any changes to them, while ignoring buildings
	for (std::size_t i{ 0 }; i < p_game.m_chunks.size(); ++i) {
		if (i == c::CHUNK_IDX_BUILDINGS ||
			i == p_exclude_world ||
			p_game.get_default_tileset_no(i, 0) != p_tileset_no)
			continue;

		const auto& other_mts{ p_game.m_chunks[i].m_metatiles };

		for (const auto& omt : other_mts)
			for (const auto& row : omt.m_tilemap)
				for (byte b : row)
					result.insert(static_cast<std::size_t>(b));
	}

	return result;
}

std::set<std::size_t> fe::game::gfx::get_chr_idxs_used_by_other_buildings(const fe::Game& p_game,
	std::size_t p_tileset_no, std::size_t p_world_no, std::size_t p_exclude_screen) {
	std::set<std::size_t> result;

	std::set<std::size_t> l_used_mts;

	for (std::size_t i{ 0 }; i < p_game.get_building_screen_count(); ++i) {
		if ((p_world_no == c::CHUNK_IDX_BUILDINGS && i == p_exclude_screen) ||
			p_game.get_default_tileset_no(c::CHUNK_IDX_BUILDINGS, i) != p_tileset_no)
			continue;
		// we have a buildings screen with the tileset we want to protect
		const auto used{ gen_metatile_usage(p_game, c::CHUNK_IDX_BUILDINGS, i, 0) };
		l_used_mts.insert(used.begin(), used.end());
	}

	const auto& building_mts{ p_game.m_chunks.at(c::CHUNK_IDX_BUILDINGS).m_metatiles };

	for (std::size_t mt_idx : l_used_mts)
		for (const auto& row : building_mts.at(mt_idx).m_tilemap)
			for (byte b : row)
				result.insert(static_cast<std::size_t>(b));

	return result;
}

std::set<std::size_t> fe::game::gfx::get_reserved_chr_idxs(const Config& p_config,
	const Game& p_game, std::size_t p_world_no, std::size_t p_screen_no) {
	std::set<std::size_t> result;

	const std::size_t tileset_no{ p_game.get_default_tileset_no(p_world_no, p_screen_no) };
	const auto& tileset{ p_game.m_tilesets.at(tileset_no) };

	// reserve everything outside this tileset's range
	for (std::size_t i{ 0 }; i < tileset.start_idx; ++i)
		result.insert(i);
	for (std::size_t i{ tileset.end_index() }; i < 256; ++i)
		result.insert(i);

	// reserve fog CHR
	if (p_world_no == p_game.m_fog.m_world_no) {
		const auto fogtiles{ p_config.vset_as_set(c::ID_FOG_RESERVED_CHR_IDXS) };
		result.insert(fogtiles.begin(), fogtiles.end());
	}

	// reserve CHR used by other consumers of this tileset
	const auto world_idxs{ get_chr_idxs_used_by_other_worlds(p_game, tileset_no, p_world_no) };
	result.insert(world_idxs.begin(), world_idxs.end());
	const auto building_idxs{ get_chr_idxs_used_by_other_buildings(
		p_game, tileset_no, p_world_no, p_screen_no) };
	result.insert(building_idxs.begin(), building_idxs.end());

	return result;
}

// file IO
std::vector<byte> fe::game::gfx::encode_png(const klib::Image& p_image) {
	std::vector<byte> rgb;
	rgb.reserve(p_image.width() * p_image.height() * 3);

	for (const auto& px : p_image.pixels()) {
		rgb.push_back(px.r);
		rgb.push_back(px.g);
		rgb.push_back(px.b);
	}

	std::vector<byte> result;

	const auto err{
		lodepng::encode(result, rgb,
			static_cast<unsigned>(p_image.width()),
			static_cast<unsigned>(p_image.height()),
			LCT_RGB)
	};

	if (err)
		throw std::runtime_error(std::format("PNG encode failed: {}", lodepng_error_text(err)));

	return result;
}

void fe::game::gfx::save_png_to_file(const klib::Image& p_image, const std::string& p_filename) {
	klib::file::write_bytes_to_file(encode_png(p_image), p_filename);
}

klib::Image fe::game::gfx::decode_png(const std::vector<byte>& p_data) {
	std::vector<byte> rgb;
	unsigned width{};
	unsigned height{};

	const auto err{ lodepng::decode(rgb, width, height,	p_data,	LCT_RGB) };

	if (err)
		throw std::runtime_error(std::format("PNG decode failed: {}", lodepng_error_text(err)));

	klib::Image result{ width, height };

	for (std::size_t y{ 0 }; y < height; ++y) {
		for (std::size_t x{ 0 }; x < width; ++x) {
			const std::size_t idx{ 3 * (y * width + x) };

			result.at(x, y) = {
				rgb[idx],
				rgb[idx + 1],
				rgb[idx + 2]
			};
		}
	}

	return result;
}

klib::Image fe::game::gfx::load_png_from_file(
	const std::string& p_filename) {
	return decode_png(klib::file::read_file_as_bytes(p_filename));
}

// helpers
std::vector<klib::RGB> fe::game::gfx::parse_nes_palette(const fe::Config& p_config) {
	std::vector<klib::RGB> result;
	const auto nespalvec{ p_config.bmap_as_numeric_vec(fe::c::ID_NES_PALETTE, 64) };

	for (std::size_t rgb : nespalvec)
		result.push_back(klib::RGB{
			.r = static_cast<byte>((rgb >> 16) & 0xff),
			.g = static_cast<byte>((rgb >> 8) & 0xff),
			.b = static_cast<byte>(rgb & 0xff)
			});

	return result;
}

const klib::RGB& fe::game::gfx::get_hot_pink(void) {
	return g_hot_pink;
}

void fe::game::gfx::draw_nes_tile_on_image(klib::Image& p_image,
	int p_dst_x, int p_dst_y, const klib::NES_tile& p_tile,
	const std::vector<byte>& p_palette, const std::vector<klib::RGB>& p_nes_palette,
	bool p_transparent, bool p_h_flip, bool p_v_flip) {

	for (int y = 0; y < 8; ++y) {
		for (int x = 0; x < 8; ++x) {
			int src_x = p_h_flip ? 7 - x : x;
			int src_y = p_v_flip ? 7 - y : y;

			byte color = p_tile.get_color(src_x, src_y);

			if (p_transparent && color == 0)
				continue;

			byte palette_index = p_palette.at(color);
			p_image.at(p_dst_x + x, p_dst_y + y) = p_nes_palette.at(palette_index);
		}
	}
}

klib::Image fe::game::gfx::gen_tilemap_image(const fe::ChrTilemap& p_tilemap,
	const std::vector<klib::RGB>& p_nes_palette) {

	const auto& tilemap{ p_tilemap.m_tilemap };
	const auto& chrtiles{ p_tilemap.m_tiles };
	const auto& pals{ p_tilemap.m_palette };

	int width{ static_cast<int>(tilemap.empty() ? 0 : 16 * tilemap[0].size()) };
	int height{ 16 * static_cast<int>(tilemap.size()) };

	klib::Image result(width, height, g_hot_pink);

	for (std::size_t j{ 0 }; j < tilemap.size(); ++j)
		for (std::size_t i{ 0 }; i < tilemap[j].size(); ++i) {
			if (tilemap[j][i].has_value()) {

				for (std::size_t q{ 0 }; q < 4; ++q) {
					auto pxpos{ mt_to_pixels(i, j, q) };

					draw_nes_tile_on_image(
						result,
						pxpos.first, pxpos.second,
						chrtiles.at(tilemap[j][i]->m_idxs[q]),
						pals.at(tilemap[j][i]->m_palette),
						p_nes_palette,
						false, false, false);
				}
			}
		}

	return result;
}

std::set<std::size_t> fe::game::gfx::gen_metatile_usage(const fe::Game& p_game,
	std::size_t p_world_no, std::size_t p_screen_no,
	std::size_t p_total_metatile_count) {
	std::set<std::size_t> result;

	// for the buildings world, look at actual metatile usage for all screens using this tileset
	if (p_world_no == c::CHUNK_IDX_BUILDINGS) {
		std::size_t l_tileset_no{ p_game.get_default_tileset_no(p_world_no, p_screen_no) };

		for (std::size_t s{ 0 }; s < p_game.get_building_screen_count(); ++s)
			if (p_game.get_default_tileset_no(p_world_no, s) == l_tileset_no) {
				const auto& scr{ p_game.m_chunks.at(c::CHUNK_IDX_BUILDINGS).m_screens.at(s) };

				for (std::size_t j{ 0 }; j < 13; ++j)
					for (std::size_t i{ 0 }; i < 16; ++i)
						result.insert(scr.get_mt_at_pos(i, j));
			}
	}
	else {
		// non-buildings world
		for (std::size_t i{ 0 }; i < p_total_metatile_count; ++i)
			result.insert(i);
	}

	return result;
}

std::vector<std::vector<byte>> fe::game::gfx::flat_pal_to_2d_pal(const std::vector<byte>& pal) {
	std::vector<std::vector<byte>> result;

	for (std::size_t j{ 0 }; j < 4; ++j) {
		std::vector<byte> l_subpal;
		for (std::size_t i{ 0 }; i < 4; ++i)
			l_subpal.push_back(pal.at(4 * j + i));
		result.push_back(l_subpal);
	}

	return result;
}

// 1) extract hud tiles (ppu index 0-59)
// 2) inject empty tiles until we hit the world-specific tileset index
// 3) inject the world-specific tileset
// 4) inject empty tiles until we have 256 chr-tiles in total
std::vector<klib::NES_tile> fe::game::gfx::gen_world_tileset(const fe::Game& p_game,
	const fe::Config& p_config, std::size_t p_tileset_no) {

	const auto& wtileset{ p_game.m_tilesets.at(p_tileset_no) };
	std::vector<klib::NES_tile> result{ p_game.get_hud_chr_tiles(p_config) };

	while (result.size() < wtileset.start_idx)
		result.push_back(klib::NES_tile());

	for (const auto& wtile : wtileset.tiles)
		result.push_back(wtile);

	while (result.size() < 256)
		result.push_back(klib::NES_tile());

	return result;
}

std::vector<std::vector<klib::NES_tile>> fe::game::gfx::gen_world_tilesets(const fe::Game& p_game,
	const fe::Config& p_config) {
	std::vector<std::vector<klib::NES_tile>> result;
	for (std::size_t i{ 0 }; i < p_game.m_tilesets.size(); ++i)
		result.push_back(gen_world_tileset(p_game, p_config, i));
	return result;
}

fe::ChrTilemap fe::game::gfx::get_world_mt_tilemap(const fe::Game& p_game,
	const fe::WorldTilesetGfxDef& p_def) {
	fe::ChrTilemap result;

	// the bmp metatile-width should match the metatile picker
	const std::size_t lc_metatile_width{ 10 };
	const auto& mts{ p_game.m_chunks.at(p_def.world_no).m_metatiles };

	// set palette
	result.m_palette = p_def.palette;

	// generate tilemap
	std::vector<std::optional<fe::ChrMetaTile>> resrow;
	for (std::size_t i{ 0 }; i < mts.size(); ++i) {
		if (!p_def.writable_metatile_idxs.contains(i))
			continue;

		fe::ChrMetaTile tile;
		tile.m_palette = mts[i].get_palette_attribute(0, 0);

		for (const auto& col : mts[i].m_tilemap)
			for (byte b : col)
				tile.m_idxs.push_back(static_cast<std::size_t>(b));

		resrow.push_back(tile);

		if (resrow.size() % lc_metatile_width == 0) {
			result.m_tilemap.push_back(resrow);
			resrow.clear();
		}
	}

	if (!resrow.empty()) {
		while (resrow.size() % 16 != 0)
			resrow.push_back(std::nullopt);
		result.m_tilemap.push_back(resrow);
	}

	// get raw CHR tiles from definition
	for (const auto& chr : p_def.chr_tiles)
		result.m_tiles.push_back(chr.m_tile);

	return result;
}

// world tileset/metatile gfx definition
fe::WorldTilesetGfxDef fe::game::gfx::get_world_tileset_gfx_def(const Config& p_config,
	const Game& p_game, std::size_t p_world_no, std::size_t p_screen_no) {
	WorldTilesetGfxDef result;

	result.world_no = p_world_no;
	result.tileset_no = p_game.get_default_tileset_no(p_world_no, p_screen_no);
	result.writable_metatile_idxs = gen_metatile_usage(p_game, p_world_no, p_screen_no,
		p_game.m_chunks.at(p_world_no).m_metatiles.size());
	result.palette = flat_pal_to_2d_pal(p_game.m_palettes.at(
		p_game.get_default_palette_no(p_world_no, p_screen_no)));

	const auto reserved{ get_reserved_chr_idxs(p_config, p_game, p_world_no, p_screen_no) };
	const auto chrtiles{ gen_world_tileset(p_game, p_config, result.tileset_no) };

	const auto& tileset{ p_game.m_tilesets.at(result.tileset_no) };
	const std::size_t tileset_end{ tileset.end_index() };

	for (std::size_t i{ 0 }; i < chrtiles.size(); ++i)
		result.chr_tiles.emplace_back(
			chrtiles[i],
			reserved.contains(i),
			(i >= tileset.start_idx && i < tileset_end) ||
			i < c::CHR_HUD_TILE_COUNT);

	return result;
}

fe::WorldTilesetGfxDef fe::game::gfx::get_custom_world_tileset_gfx_def(const Config& p_config,
	const Game& p_game, std::size_t p_world_no, std::size_t p_tileset_no,
	std::size_t p_palette_no, std::size_t p_metatile_start, std::size_t p_metatile_end) {
	WorldTilesetGfxDef result;

	result.world_no = p_world_no;
	result.tileset_no = p_tileset_no;
	result.palette = flat_pal_to_2d_pal(p_game.m_palettes.at(p_palette_no));

	const auto& mts{ p_game.m_chunks.at(p_world_no).m_metatiles };
	if (p_metatile_start >= p_metatile_end || p_metatile_end > mts.size())
		throw std::runtime_error("invalid metatile range");
	for (std::size_t i{ p_metatile_start }; i < p_metatile_end; ++i)
		result.writable_metatile_idxs.insert(i);

	const auto chrtiles{ gen_world_tileset(p_game, p_config, p_tileset_no) };
	const auto& tileset{ p_game.m_tilesets.at(p_tileset_no) };
	
	for (std::size_t i{ 0 }; i < chrtiles.size(); ++i) {
		const bool in_hud{ i < c::CHR_HUD_TILE_COUNT };
		const bool in_tileset{
			i >= tileset.start_idx && i < tileset.end_index()
		};

		result.chr_tiles.emplace_back(
			chrtiles[i],
			in_hud,
			in_hud || in_tileset);
	}

	return result;
}

// commits
void fe::game::gfx::apply_world_tileset_gfx(fe::Game& p_game, const fe::WorldTilesetGfxDef& p_def,
	const fe::ChrTilemap& p_result) {

	// update tileset CHR
	auto& tileset{ p_game.m_tilesets.at(p_def.tileset_no) };

	for (std::size_t i{ 0 }; i < tileset.tiles.size(); ++i)
		tileset.tiles.at(i) = p_result.m_tiles.at(tileset.start_idx + i);

	// update writable metatiles
	auto& mts{ p_game.m_chunks.at(p_def.world_no).m_metatiles };
	const auto& restm{ p_result.m_tilemap };
	auto allowediter{ p_def.writable_metatile_idxs.begin() };

	for (std::size_t j{ 0 }; j < restm.size(); ++j)
		for (std::size_t i{ 0 }; i < restm[j].size(); ++i) {
			if (allowediter == p_def.writable_metatile_idxs.end())
				break;

			const std::size_t mtno{ *allowediter };

			if (restm[j][i].has_value() && mtno < mts.size()) {
				auto& umt{ mts.at(mtno) };
				auto& umt_tm{ umt.m_tilemap };
				const auto& idxs{ restm[j][i]->m_idxs };

				umt.m_attr_tl = static_cast<byte>(restm[j][i]->m_palette);
				umt.m_attr_tr = static_cast<byte>(restm[j][i]->m_palette);
				umt.m_attr_bl = static_cast<byte>(restm[j][i]->m_palette);
				umt.m_attr_br = static_cast<byte>(restm[j][i]->m_palette);

				umt_tm.at(0).at(0) = static_cast<byte>(idxs.at(0));
				umt_tm.at(0).at(1) = static_cast<byte>(idxs.at(1));
				umt_tm.at(1).at(0) = static_cast<byte>(idxs.at(2));
				umt_tm.at(1).at(1) = static_cast<byte>(idxs.at(3));

				++allowediter;
			}
		}
}

// bg gfx import pipeline
fe::TilemapImportResult fe::game::gfx::import_tilemap_image(
	const klib::Image& p_image,
	std::vector<ChrGfxTile>& p_tiles,
	const std::vector<std::vector<byte>>& p_palette,
	const std::vector<klib::RGB>& p_nes_palette,
	ChrDedupMode p_dedupmode) {

	// return values - did we have room to spare or did we overflow?
	fe::TilemapImportResult result{};

	// enforce multiples of 16
	if ((p_image.width() % 16) != 0 || (p_image.height() % 16) != 0)
		throw std::runtime_error("Image dimensions must be multiples of 16");

	std::size_t mt_w{ p_image.width() / 16 };
	std::size_t mt_h{ p_image.height() / 16 };

	// the tilemap, containing concrete chr tiles
	std::vector<std::vector<std::optional<ChrMetaTile>>> l_final_tilemap(
		mt_h, std::vector<std::optional<ChrMetaTile>>(mt_w));

	// the candidate tilemap, also containing concrete chr tiles
	// but we don't know if we can use all of them before we execute
	std::vector<std::vector<std::optional<MetaTileCandidate>>> l_tilemap(
		mt_h, std::vector<std::optional<MetaTileCandidate>>(mt_w));

	for (std::size_t j{ 0 }; j < mt_h; ++j)
		for (std::size_t i{ 0 }; i < mt_w; ++i) {
			if (is_optional_image_region(p_image, i, j))
				l_tilemap[j][i] = std::nullopt;
			else {
				std::vector<fe::MetaTileCandidate> l_cands;
				for (std::size_t pal{ 0 }; pal < 4; ++pal)
					l_cands.push_back(slice_and_quantize(
						p_image, p_nes_palette, i, j, p_palette, pal, p_dedupmode, p_tiles
					));

				l_tilemap[j][i] = collapse_candidates(l_cands);
			}
		}

	// we now have the full tilemap with concrete chr tiles
	// and pre-calculated rgb-errors vs the image
	// we now emit chr tiles to our generated tilemap
	std::map<klib::NES_tile, std::vector<std::size_t>> tileToIndices;

	// fill out all read-only and unusable tiles
	for (std::size_t i{ 0 }; i < p_tiles.size(); ++i)
		if (!p_tiles[i].m_allowed || p_tiles[i].m_readonly)
			tileToIndices[p_tiles[i].m_tile].push_back(i);

	for (std::size_t j{ 0 }; j < mt_h; ++j)
		for (std::size_t i{ 0 }; i < mt_w; ++i)
			if (l_tilemap[j][i].has_value()) {
				l_final_tilemap[j][i] = fe::ChrMetaTile();
				l_final_tilemap[j][i]->m_palette = l_tilemap[j][i]->paletteIndex;

				for (std::size_t t{ 0 }; t < 4; ++t) {
					std::size_t idx{ allocate_or_reuse_chr(l_tilemap[j][i]->m_tiles[t],
					p_tiles, tileToIndices,
					p_nes_palette,
					p_palette[l_tilemap[j][i]->paletteIndex],
					p_dedupmode) };

					if (idx < 256)
						l_final_tilemap[j][i]->m_idxs.push_back(idx);
					else {
						auto pxpos{ mt_to_pixels(i, j, t) };

						l_final_tilemap[j][i]->m_idxs.push_back(
							best_substitute_chr_index(p_image, pxpos.first, pxpos.second,
								p_nes_palette,
								p_palette.at(l_final_tilemap[j][i]->m_palette),
								tileToIndices, p_tiles)
						);

						++result.overflowChrCount;
					}
				}
			}
			else
				l_final_tilemap[j][i] = std::nullopt;

	// if we have leftover chr space, set it to the empty tile
	std::vector<std::size_t> spareindices;

	for (std::size_t i{ 0 }; i < 256; ++i) {
		bool l_found{ false };
		for (const auto& kv : tileToIndices)
			for (std::size_t tidx : kv.second)
				if (i == tidx)
					l_found = true;

		if (!l_found)
			spareindices.push_back(i);
	}

	result.leftoverChrCount = static_cast<int>(spareindices.size());

	if (!spareindices.empty()) {
		klib::NES_tile l_empty;
		auto iter{ tileToIndices.find(l_empty) };

		if (iter != end(tileToIndices)) {
			for (std::size_t i : spareindices)
				iter->second.push_back(i);
		}
		else {
			tileToIndices.insert(std::make_pair(l_empty, spareindices));
		}
	}

	// finally done ... update result map and texture
	result.tilemap = fe::ChrTilemap(l_final_tilemap,
		chrtiletoindex_map_to_vector(tileToIndices, 256),
		p_palette);
	result.image = gen_tilemap_image(result.tilemap, p_nes_palette);

	return result;
}

klib::NES_tile fe::game::gfx::image_region_to_nes_tile(const klib::Image& p_image,
	const std::vector<klib::RGB>& p_nes_palette, const std::vector<byte>& p_palette,
	int p_x, int p_y) {
	if (p_x < 0 || p_y < 0 ||
		p_x + 8 > static_cast<int>(p_image.width()) ||
		p_y + 8 > static_cast<int>(p_image.height()))
		throw std::runtime_error("image region out of bounds");

	klib::NES_tile result;

	for (int yy{ 0 }; yy < 8; ++yy) {
		for (int xx{ 0 }; xx < 8; ++xx) {
			const auto& pixel{ p_image.at(static_cast<std::size_t>(p_x + xx),
				static_cast<std::size_t>(p_y + yy)) };

			// Find closest of the 4 palette colors
			int bestIdx{ 0 };
			int bestDist{ std::numeric_limits<int>::max() };
			for (int i{ 0 }; i < 4; ++i) {
				const auto& pal_col{ p_nes_palette.at(p_palette.at(i)) };

				const int dr{ int(pixel.r) - int(pal_col.r) };
				const int dg{ int(pixel.g) - int(pal_col.g) };
				const int db{ int(pixel.b) - int(pal_col.b) };
				const int dist{ dr * dr + dg * dg + db * db };

				if (dist < bestDist) {
					bestDist = dist;
					bestIdx = i; // 0..3
				}
			}

			result.set_color(static_cast<std::size_t>(xx),
				static_cast<std::size_t>(yy),
				static_cast<byte>(bestIdx)
			);
		}
	}

	return result;
}

int fe::game::gfx::rgb_space_diff(const klib::NES_tile& p_tile,
	const std::vector<klib::RGB>& p_nes_palette, const std::vector<byte>& p_palette,
	const klib::Image& p_image, int p_x, int p_y) {
	int result{ 0 };

	for (int j{ 0 }; j < 8; ++j)
		for (int i{ 0 }; i < 8; ++i) {
			const auto& pixel{ p_image.at(static_cast<std::size_t>(p_x + i),
					static_cast<std::size_t>(p_y + j)) };

			const byte palIndex{ p_palette.at(p_tile.get_color(i, j)) };
			const auto& tile_pixel_col{ p_nes_palette.at(palIndex) };

			const int rr{ static_cast<int>(pixel.r) - static_cast<int>(tile_pixel_col.r) };
			const int gg{ static_cast<int>(pixel.g) - static_cast<int>(tile_pixel_col.g) };
			const int bb{ static_cast<int>(pixel.b) - static_cast<int>(tile_pixel_col.b) };

			result += rr * rr + gg * gg + bb * bb;
		}

	return result;
}

bool fe::game::gfx::rgb_equivalence(
	const klib::RGB& p_col_a, const klib::RGB& p_col_b) {
	return p_col_a == p_col_b;
}

bool fe::game::gfx::chr_tile_equivalence(fe::ChrDedupMode p_dedupmode,
	const klib::NES_tile& p_tile_a, const klib::NES_tile& p_tile_b,
	const std::vector<klib::RGB>& p_nes_palette, const std::vector<byte>& p_palette) {

	if (p_dedupmode == fe::ChrDedupMode::PalIndex_Eq)
		return p_tile_a == p_tile_b;
	else {
		for (std::size_t j{ 0 }; j < 8; ++j)
			for (std::size_t i{ 0 }; i < 8; ++i) {
				std::size_t inda{ p_palette[p_tile_a.get_color(i, j)] };
				std::size_t indb{ p_palette[p_tile_b.get_color(i, j)] };

				if (p_dedupmode == fe::ChrDedupMode::NESPalIndex_Eq) {
					if (inda != indb)
						return false;
				}
				else if (!rgb_equivalence(
					p_nes_palette.at(inda),
					p_nes_palette.at(indb)))
					return false;
			}
	}

	return true;
}

std::vector<klib::NES_tile> fe::game::gfx::gen_unique_tiles(const std::vector<klib::NES_tile>& p_tiles,
	const std::vector<klib::RGB>& p_nes_palette, const std::vector<byte>& p_palette,
	fe::ChrDedupMode p_dedupmode) {
	std::vector<klib::NES_tile> result;

	for (const auto& tile : p_tiles) {
		bool l_unique{ true };
		for (const auto& utile : result)
			if (chr_tile_equivalence(p_dedupmode, tile, utile, p_nes_palette, p_palette)) {
				l_unique = false;
				break;
			}
		if (l_unique)
			result.push_back(tile);
	}

	return result;
}

fe::MetaTileCandidate fe::game::gfx::slice_and_quantize(
	const klib::Image& p_image,
	const std::vector<klib::RGB>& p_nes_palette,
	std::size_t mt_x, std::size_t mt_y,
	const std::vector<std::vector<byte>>& p_palette,
	std::size_t p_sub_pal_idx,
	fe::ChrDedupMode p_dedupmode,
	const std::vector<fe::ChrGfxTile>& p_tiles) {

	fe::MetaTileCandidate result;
	result.paletteIndex = p_sub_pal_idx;
	result.rgbError = 0;

	int px{ 16 * static_cast<int>(mt_x) };
	int py{ 16 * static_cast<int>(mt_y) };

	result.m_tiles.push_back(
		image_region_to_nes_tile(p_image, p_nes_palette, p_palette[p_sub_pal_idx], px, py));
	result.m_tiles.push_back(
		image_region_to_nes_tile(p_image, p_nes_palette, p_palette[p_sub_pal_idx], px + 8, py));
	result.m_tiles.push_back(
		image_region_to_nes_tile(p_image, p_nes_palette, p_palette[p_sub_pal_idx], px, py + 8));
	result.m_tiles.push_back(
		image_region_to_nes_tile(p_image, p_nes_palette, p_palette[p_sub_pal_idx], px + 8, py + 8));

	for (std::size_t i{ 0 }; i < 4; ++i) {
		const auto& tile = result.m_tiles[i];

		// compute per-tile error
		int err = rgb_space_diff(tile,
			p_nes_palette,
			p_palette[p_sub_pal_idx],
			p_image,
			px + static_cast<int>(i % 2) * 8,
			py + static_cast<int>(i / 2) * 8);

		result.m_quad_errors.push_back(err);
		result.rgbError += err;
	}

	auto l_unique_tiles{ gen_unique_tiles(result.m_tiles,
		p_nes_palette, p_palette[p_sub_pal_idx], p_dedupmode) };

	for (const auto& tile : l_unique_tiles)
		for (const auto& globaltile : p_tiles)
			if (globaltile.m_allowed &&
				chr_tile_equivalence(p_dedupmode,
					globaltile.m_tile, tile, p_nes_palette,
					p_palette[p_sub_pal_idx])) {
				result.reuseCount++;
				break;
			}

	return result;
}

std::pair<int, int> fe::game::gfx::mt_to_pixels(std::size_t p_mt_x, std::size_t p_mt_y,
	std::size_t p_quadrant) {

	int px{ 16 * static_cast<int>(p_mt_x) };
	int py{ 16 * static_cast<int>(p_mt_y) };

	if (p_quadrant == 1 || p_quadrant == 3)
		px += 8;

	if (p_quadrant == 2 || p_quadrant == 3)
		py += 8;

	return std::make_pair(px, py);
}

bool fe::game::gfx::is_optional_image_region(const klib::Image& p_image,
	std::size_t p_mt_x, std::size_t p_mt_y) {

	const auto l_pxpos{ mt_to_pixels(p_mt_x, p_mt_y, 0) };

	for (int j{ 0 }; j < 16; ++j)
		for (int i{ 0 }; i < 16; ++i) {
			const auto& pixel{
				p_image.at(
					static_cast<std::size_t>(l_pxpos.first + i),
					static_cast<std::size_t>(l_pxpos.second + j))
			};

			if (pixel != g_hot_pink)
				return false;
		}

	return true;
}

fe::MetaTileCandidate fe::game::gfx::collapse_candidates(
	const std::vector<fe::MetaTileCandidate>& p_candidates) {
	fe::MetaTileCandidate best;
	bool first = true;

	for (const auto& cand : p_candidates) {
		if (first ||
			cand.rgbError < best.rgbError ||
			(cand.rgbError == best.rgbError && cand.reuseCount > best.reuseCount) ||
			(cand.rgbError == best.rgbError && cand.reuseCount == best.reuseCount &&
				cand.paletteIndex < best.paletteIndex)) {
			best = cand;
			first = false;
		}
	}

	return best;
}

std::size_t fe::game::gfx::allocate_or_reuse_chr(
	const klib::NES_tile& tile,
	std::vector<ChrGfxTile>& p_tiles,
	std::map<klib::NES_tile, std::vector<std::size_t>>& tileToIndices,
	const std::vector<klib::RGB>& p_nes_palette,
	const std::vector<byte>& p_palette,
	fe::ChrDedupMode p_dedupmode) {

	// Deduplication: if tile already exists, reuse any allowed index
	for (auto& [candidateTile, indices] : tileToIndices) {
		if (chr_tile_equivalence(p_dedupmode, tile, candidateTile,
			p_nes_palette, p_palette)) {
			for (std::size_t idx : indices) {
				if (p_tiles[idx].m_allowed) {
					return idx;
				}
			}
		}
	}

	// Pass 1: search p_tiles for a mutable exact match (allowed && !readonly && slot holds tile)
	// We DON'T pre-seed mutable slots in tileToIndices, so we must scan p_tiles here.
	for (std::size_t idx{ 0 }; idx < p_tiles.size(); ++idx) {
		if (p_tiles[idx].m_allowed && !p_tiles[idx].m_readonly &&
			chr_tile_equivalence(p_dedupmode,
				p_tiles[idx].m_tile, tile,
				p_nes_palette, p_palette)) {
			p_tiles[idx].m_readonly = true;
			tileToIndices[tile].push_back(idx);
			return idx;
		}
	}

	// Allocation: find a writable slot (allowed && !readOnly)
	for (std::size_t idx{ 0 }; idx < p_tiles.size(); ++idx) {
		if (p_tiles[idx].m_allowed && !p_tiles[idx].m_readonly) {
			p_tiles[idx].m_readonly = true;
			tileToIndices[tile].push_back(idx);
			return idx;
		}
	}

	// No usable slot available
	return 256;
}

std::size_t fe::game::gfx::best_substitute_chr_index(const klib::Image& p_image,
	int p_x, int p_y, const std::vector<klib::RGB>& p_nes_palette,
	const std::vector<byte>& p_sub_palette,
	const std::map<klib::NES_tile, std::vector<std::size_t>>& p_tile_to_indices,
	const std::vector<ChrGfxTile>& p_tiles) {

	std::size_t bestChrIdx = 0;
	int bestErr = std::numeric_limits<int>::max();

	// Iterate over committed tiles (authoritative content)
	for (const auto& [candidateTile, indices] : p_tile_to_indices) {
		// Compute error IN CONTEXT for the candidate’s CHR content
		int err = rgb_space_diff(candidateTile, p_nes_palette,
			p_sub_palette, p_image, p_x, p_y);

		if (err >= bestErr)
			continue;

		// Among its recorded indices, choose an allowed one to emit
		// Prefer the smallest allowed index for determinism.
		std::size_t chosenIdx = std::numeric_limits<std::size_t>::max();
		for (std::size_t idx : indices)
			if (idx < p_tiles.size() && p_tiles[idx].m_allowed)
				chosenIdx = std::min(chosenIdx, idx);

		if (chosenIdx == std::numeric_limits<std::size_t>::max()) {
			// This map entry has no allowed indices (all disallowed) -> skip
			continue;
		}

		// Better match found -> record
		bestErr = err;
		bestChrIdx = chosenIdx;
	}

	return bestChrIdx; // Guaranteed to be one of the committed indices
}

std::vector<klib::NES_tile> fe::game::gfx::chrtiletoindex_map_to_vector(
	const std::map<klib::NES_tile, std::vector<std::size_t>>& p_tile_to_indices,
	std::size_t p_chr_count) {
	std::vector<klib::NES_tile> result(p_chr_count, klib::NES_tile());

	for (const auto& kv : p_tile_to_indices)
		for (std::size_t idx : kv.second)
			if (idx >= p_chr_count)
				throw std::runtime_error("invalid chr tile index");
			else
				result[idx] = kv.first;

	return result;
}
