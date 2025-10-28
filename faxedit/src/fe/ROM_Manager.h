#ifndef FE_ROM_MANAGER_H
#define FE_ROM_MANAGER_H

#include "Game.h"
#include "fe_constants.h"
#include "./../common/klib/NES_tile.h"
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

using byte = unsigned char;
using PtrDef = std::pair<std::size_t, std::size_t>;

namespace fe {

	class ROM_Manager {
		
		std::vector<std::vector<std::vector<klib::NES_tile>>> m_sprite_tiles;

		std::vector<byte> build_pointer_table_and_data(
			std::size_t p_rom_loc_ptr_table,
			std::size_t p_ptr_base_rom_offset,
			const std::vector<std::vector<byte>>& p_data) const;

		std::vector<byte> build_pointer_table_and_data_aggressive(
			std::size_t p_rom_loc_ptr_table,
			std::size_t p_ptr_base_rom_offset,
			const std::vector<std::vector<byte>>& p_data) const;

		std::pair<std::vector<byte>, std::vector<byte>>  build_pointer_table_and_data_aggressive_decoupled(
			std::size_t p_rom_loc_ptr_table,
			std::size_t p_ptr_base_rom_offset,
			std::size_t p_rom_loc_data,
			const std::vector<std::vector<byte>>& p_data) const;

		// pointer variables - check the constants header for descriptions
		std::vector<std::size_t> m_chunk_tilemaps_bank_idx, m_ptr_tilemaps_bank_rom_offset;
		std::pair<std::size_t, std::size_t> m_ptr_sprites, m_ptr_sameworld_trans_table,
			m_ptr_otherworld_trans_table;

		// constant scalars
		std::size_t m_ptr_chunk_door_to_chunk, m_ptr_chunk_door_to_screen,
			m_ptr_chunk_door_reqs;

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

		// void extract_sprite_data(const std::vector<byte>& p_rom);

		// encoding data with variable locations, needing pointer tables
		std::vector<byte> encode_game_sprite_data_new(const fe::Game& p_game) const;
		std::vector<byte> encode_bank_screen_data(const fe::Game& p_game, std::size_t p_bank_no) const;
		std::vector<byte> encode_game_sprite_data(const fe::Game& p_game) const;
		std::vector<byte> encode_game_metadata(const fe::Game& p_game) const;
		std::vector<byte> encode_game_metadata_all(const fe::Game& p_game) const;
		std::vector<byte> encode_game_otherworld_trans(const fe::Game& p_game) const;
		std::vector<byte> encode_game_sameworld_trans(const fe::Game& p_game) const;

		// encode in place and return a pair of used size and max size
		std::pair<std::size_t, std::size_t> encode_bank_tilemaps(const fe::Game& p_game, std::vector<byte>& p_rom, std::size_t p_bank_no) const;
		std::pair<std::size_t, std::size_t> encode_metadata(const fe::Game& p_game, std::vector<byte>& p_rom) const;
		std::pair<std::size_t, std::size_t> encode_sprite_data(const fe::Game& p_game, std::vector<byte>& p_rom) const;
		std::pair<std::size_t, std::size_t> encode_transitions(const fe::Game& p_game, std::vector<byte>& p_rom) const;

		// encoding data in-place using a given address and stride-indexed
		// no need to return anything here as the data is of fixed length
		void encode_static_data(const fe::Game& p_game, std::vector<byte>& p_rom) const;

		void encode_chunk_palette_no(const fe::Game& p_game, std::vector<byte>& p_rom) const;
		void encode_stage_data(const fe::Game& p_game, std::vector<byte>& p_rom) const;
		void encode_spawn_locations(const fe::Game& p_game, std::vector<byte>& p_rom) const;
		void encode_mattock_animations(const fe::Game& p_game, std::vector<byte>& p_rom) const;
		void encode_push_block(const fe::Game& p_game, std::vector<byte>& p_rom) const;

		// util
		void patch_bytes(const std::vector<byte>& p_source, std::vector<byte>& p_target, std::size_t p_target_offset) const;
		static std::pair<byte, byte> to_uint16_le(std::size_t p_value);
		static std::size_t from_uint16_le(const std::pair<byte, byte>& p_value);
	};

}

#endif
