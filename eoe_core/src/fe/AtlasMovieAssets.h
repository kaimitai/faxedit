#ifndef FE_ATLASMOVIEASSETS_H
#define FE_ATLASMOVIEASSETS_H

#include "AtlasMovieBundle.h"
#include "Game.h"
#include "common/klib/NES_tile.h"
#include "sprite/SpriteAnimationFrame.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fe::atlas_movie {

	struct ImportedRoom {
		std::vector<byte> chr, nametable, palette;
	};

	struct AtlasMovieRomSpan {
		std::size_t offset{ 0 }, bytes{ 0 };
		std::string label;
	};

	std::size_t rom_offset(byte p_bank, std::uint16_t p_cpu);
	std::vector<byte> asset_bytes(const std::vector<byte>& p_rom,
		const AtlasMovieAsset& p_asset);
	const AtlasMovieAsset* find_asset(const AtlasMovie& p_movie,
		AtlasMovieAssetKind p_kind);
	const AtlasMovieImport* find_import(const AtlasMovie& p_movie,
		AtlasMovieImportKind p_kind);
	void replace_import(AtlasMovie& p_movie, AtlasMovieImport p_import);

	std::vector<klib::NES_tile> decode_chr(const std::vector<byte>& p_bytes);
	std::vector<byte> encode_chr(const std::vector<klib::NES_tile>& p_tiles);
	std::vector<SpriteAnimationFrame> decode_movie_frames(
		const std::vector<byte>& p_rom, const AtlasMovie& p_movie);
	std::vector<byte> encode_metasprite_library(
		const std::vector<SpriteAnimationFrame>& p_frames);
	std::vector<byte> resolved_asset_bytes(const std::vector<byte>& p_rom,
		const AtlasMovie& p_movie, AtlasMovieAssetKind p_kind);
	std::vector<klib::NES_tile> movie_sprite_tiles(
		const std::vector<byte>& p_rom, const AtlasMovie& p_movie);
	std::vector<AtlasMovieRomSpan> movie_rom_source_spans(
		const std::vector<byte>& p_rom, const AtlasMovieBundle& p_bundle);
	void reject_movie_source_overlaps(const std::vector<byte>& p_rom,
		const AtlasMovieBundle& p_bundle,
		const std::vector<AtlasMovieRomSpan>& p_writes,
		const std::string& p_owner);
	void validate_movie_oam_budget(const std::vector<byte>& p_rom,
		const AtlasMovieBundle& p_bundle);
	std::optional<std::size_t> movie_background_tile(
		const AtlasMovie& p_movie, byte p_nametable_tile);

	ImportedRoom convert_game_room(const Game& p_game,
		const std::vector<std::vector<klib::NES_tile>>& p_tilesets,
		std::size_t p_world, std::size_t p_screen,
		const std::vector<byte>& p_sprite_palette);

}

#endif
