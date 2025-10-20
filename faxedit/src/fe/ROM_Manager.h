#ifndef FE_ROM_MANAGER_H
#define FE_ROM_MANAGER_H

#include "Game.h"
#include "fe_constants.h"
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

using byte = unsigned char;
using PtrDef = std::pair<std::size_t, std::size_t>;

namespace fe {

	class ROM_Manager {

		std::vector<byte> build_pointer_table_and_data(
			std::size_t p_rom_loc_ptr_table,
			std::size_t p_ptr_base_rom_offset,
			const std::vector<std::vector<byte>>& p_data) const;

		std::vector<byte> build_pointer_table_and_data_aggressive(
			std::size_t p_rom_loc_ptr_table,
			std::size_t p_ptr_base_rom_offset,
			const std::vector<std::vector<byte>>& p_data
		) const;

		// pointer variables - check the constants header for descriptions
		std::vector<std::size_t> m_chunk_tilemaps_bank_idx, m_ptr_tilemaps_bank_rom_offset,
			m_map_chunk_levels;
		std::pair<std::size_t, std::size_t> m_ptr_sprites, m_ptr_sameworld_trans_table,
			m_ptr_otherworld_trans_table;
		std::vector<std::size_t> m_chunk_idx;

		// constant scalars
		std::size_t m_chunk_idx_npc_bundles,
			m_ptr_chunk_door_to_chunk, m_ptr_chunk_door_to_screen,
			m_ptr_chunk_door_reqs;

		template<class T, class U = T>
		std::size_t get_vector_index(const std::vector<T>& p_data, U p_val) const {
			for (std::size_t i{ 0 }; i < p_data.size(); ++i)
				if (p_data[i] == static_cast<T>(p_val))
					return i;
			throw std::runtime_error("No such element");
		}

	public:
		ROM_Manager(void);

		// encoding data with variable locations, needing pointer tables
		std::vector<byte> encode_game_sprite_data_new(const fe::Game& p_game) const;
		std::vector<byte> encode_bank_screen_data(const fe::Game& p_game, std::size_t p_bank_no) const;
		std::vector<byte> encode_game_sprite_data(const fe::Game& p_game) const;
		std::vector<byte> encode_game_metadata(const fe::Game& p_game) const;
		std::vector<byte> encode_game_metadata_all(const fe::Game& p_game) const;
		std::vector<byte> encode_game_otherworld_trans(const fe::Game& p_game) const;
		std::vector<byte> encode_game_sameworld_trans(const fe::Game& p_game) const;

		// encoding data in-place using a given address and stride-indexed
		// no need to return anything here as the data is of fixed length
		void encode_chunk_door_data(const fe::Game& p_game, std::vector<byte>& p_rom) const;
		void encode_spawn_locations(const fe::Game& p_game, std::vector<byte>& p_rom) const;

		// util
		static std::pair<byte, byte> to_uint16_le(std::size_t p_value);
	};

}

#endif
