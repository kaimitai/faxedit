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

// bg gfx import pipeline
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
