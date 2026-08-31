#ifndef FE_GAMEGFX_H
#define FE_GAMEGFX_H

#include <map>
#include <set>
#include <vector>
#include "fe/Game.h"
#include "common/klib/Image.h"

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

	// png and file I/O
	std::vector<byte> encode_png(const klib::Image& p_image);
	void save_png_to_file(const klib::Image& p_image, const std::string& p_filename);

	klib::Image decode_png(const std::vector<byte>& p_data);
	klib::Image load_png_from_file(const std::string& p_filename);
}

#endif
