#ifndef FE_ROM_MANAGER_H
#define FE_ROM_MANAGER_H

#include "Game.h"
#include "fe_constants.h"
#include "./../common/klib/NES_tile.h"
#include "./../fe/Sprite_definitions.h"
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

using byte = unsigned char;
using PtrDef = std::pair<std::size_t, std::size_t>;

namespace fe {

	class Config;

	struct TileMapPackingResult {
		bool m_result;
		std::map<std::size_t, std::vector<std::size_t>> m_assignments;
		std::vector<std::size_t> m_sizes;
	};

	class ROM_Manager {

		std::vector<byte> build_pointer_table_and_data(
			std::size_t p_rom_loc_ptr_table,
			std::size_t p_ptr_base_rom_offset,
			const std::vector<std::vector<byte>>& p_data) const;

		std::vector<byte> build_pointer_table_and_data_aggressive(
			std::size_t p_rom_loc_ptr_table,
			std::size_t p_ptr_base_rom_offset,
			const std::vector<std::vector<byte>>& p_data) const;

		std::pair<std::vector<byte>, std::vector<byte>> build_pointer_table_and_data_aggressive_decoupled(
			std::size_t p_rom_loc_ptr_table,
			std::size_t p_ptr_base_rom_offset,
			std::size_t p_rom_loc_data,
			const std::vector<std::vector<byte>>& p_data) const;

		template<class T, class U = T>
		std::size_t get_vector_index(const std::vector<T>& p_data, U p_val) const {
			for (std::size_t i{ 0 }; i < p_data.size(); ++i)
				if (p_data[i] == static_cast<T>(p_val))
					return i;
			throw std::runtime_error("No such element");
		}

		// this function generates pointer tables and data offsets for several pieces of data at once,
		// and generates a vector of <data table number> -> {ptr value, data pointed to}
		// it uses global deduplication across all the data
		std::vector<std::vector<std::pair<std::size_t, std::vector<byte>>>> generate_multi_pointer_tables(
			const std::vector<std::vector<std::vector<byte>>>& all_data_sets,
			const std::vector<std::size_t>& pointer_table_offsets,
			std::size_t rom_zero_address,
			const std::vector<std::pair<std::size_t, std::size_t>>& p_available);

		std::size_t get_ptr_to_rom_offset(const std::vector<byte>& p_rom,
			std::size_t p_ptr_offset,
			std::size_t p_zero_addr) const;

	public:
		ROM_Manager(void);

		// encode tilemaps, ensure a packing if found if any is possible
		fe::TileMapPackingResult encode_game_tilemaps(const fe::Config& p_config,
			std::vector<byte>& p_rom, const fe::Game& p_game) const;
		// given an assignment vector and bank no, pack
		std::vector<byte> encode_bank_tilemap_data(const fe::Game& p_game,
			std::size_t p_bank_offset,
			std::size_t p_bank_no,
			const std::vector<std::size_t>& p_worlds) const;
		// recursive function used to find a tilemap packing
		bool pack_tilemaps_recursively(const std::vector<std::size_t>& p_sizes,
			std::size_t p_index,
			std::map<byte, std::size_t>& p_used_bank_bytes,
			std::vector<int>& p_bank_assignments,
			std::size_t p_bank_max_size) const;

		// encoding data with variable locations, needing pointer tables
		std::vector<byte> encode_game_sprite_data_new(const fe::Config& p_config, const fe::Game& p_game) const;
		std::vector<byte> encode_game_metadata_all(const fe::Config& p_config, const fe::Game& p_game) const;
		std::vector<byte> encode_game_otherworld_trans(const fe::Config& p_config, const fe::Game& p_game) const;
		std::vector<byte> encode_game_sameworld_trans(const fe::Config& p_config, const fe::Game& p_game) const;

		// encode in place and return a pair of used size and max size
		std::pair<std::size_t, std::size_t> encode_metadata(const fe::Config& p_config, const fe::Game& p_game, std::vector<byte>& p_rom) const;
		std::pair<std::size_t, std::size_t> encode_sprite_data(const fe::Config& p_config, const fe::Game& p_game, std::vector<byte>& p_rom) const;
		std::pair<std::size_t, std::size_t> encode_transitions(const fe::Config& p_config, const fe::Game& p_game, std::vector<byte>& p_rom) const;
		std::pair<std::size_t, std::size_t> encode_sw_transitions(const fe::Config& p_config, const fe::Game& p_game, std::vector<byte>& p_rom) const;
		std::pair<std::size_t, std::size_t> encode_ow_transitions(const fe::Config& p_config, const fe::Game& p_game, std::vector<byte>& p_rom) const;

		// encoding data in-place using a given address and stride-indexed
		// no need to return anything here as the data is of fixed length
		void encode_static_data(const fe::Config& p_config, const fe::Game& p_game, std::vector<byte>& p_rom) const;

		void encode_palette_data(const fe::Config& p_config, const fe::Game& p_game, std::vector<byte>& p_rom) const;
		void encode_stage_data(const fe::Config& p_config, const fe::Game& p_game, std::vector<byte>& p_rom) const;
		void encode_palette_to_music(const fe::Config& p_config, const fe::Game& p_game, std::vector<byte>& p_rom) const;
		void encode_spawn_locations(const fe::Config& p_config, const fe::Game& p_game, std::vector<byte>& p_rom) const;
		void encode_mattock_animations(const fe::Config& p_config, const fe::Game& p_game, std::vector<byte>& p_rom) const;
		void encode_push_block(const fe::Config& p_config, const fe::Game& p_game, std::vector<byte>& p_rom) const;
		void encode_jump_on_tiles(const fe::Config& p_config, const fe::Game& p_game, std::vector<byte>& p_rom) const;
		void encode_scene_data(const fe::Config& p_config, const fe::Game& p_game, std::vector<byte>& p_rom) const;
		void encode_fog_data(const fe::Config& p_config, const fe::Game& p_game, std::vector<byte>& p_rom) const;
		void encode_chr_data(const fe::Config& p_config, const fe::Game& p_game, std::vector<byte>& p_rom,
			const std::vector<std::size_t> p_tileset_start, const std::vector<std::size_t> p_tileset_count) const;

		// util
		void patch_bytes(const std::vector<byte>& p_source, std::vector<byte>& p_target, std::size_t p_target_offset) const;
		static std::pair<byte, byte> to_uint16_le(std::size_t p_value);
		static std::size_t from_uint16_le(const std::pair<byte, byte>& p_value);

		// gfx
		const std::map<std::size_t, fe::Sprite_gfx_definiton> extract_sprite_data(
			const std::vector<byte>& p_rom) const;
		fe::Sprite_gfx_definiton extract_door_req_gfx(const std::vector<byte>& p_rom) const;
	};

}

#endif
