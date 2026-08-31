#include "game_gfx.h"
#include "fe/fe_constants.h"
#include "fe/Config.h"
#include "common/lodepng.h"
#include "common/klib/Kfile.h"
#include <algorithm>
#include <format>
#include <numeric>
#include <stdexcept>

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
