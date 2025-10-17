#ifndef FE_ROM_MANAGER_H
#define FE_ROM_MANAGER_H

#include "Game.h"
#include "fe_constants.h"
#include <map>
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

		// pointer variables - check the constants header for descriptions
		std::vector<std::size_t> m_chunk_tilemaps_bank_idx, m_ptr_tilemaps_bank_rom_offset;
		std::pair<std::size_t, std::size_t> m_ptr_sprites;
		std::vector<std::size_t> m_chunk_idx;

		// constant scalars
		std::size_t m_chunk_idx_npc_bundles;

	public:
		ROM_Manager(void);

		std::vector<byte> encode_game_sprite_data_new(const fe::Game& p_game) const;
		std::vector<byte> encode_bank_screen_data(const fe::Game& p_game, std::size_t p_bank_no) const;
		std::vector<byte> encode_game_sprite_data(const fe::Game& p_game) const;
		std::vector<byte> encode_game_metadata(const fe::Game& p_game) const;
		std::vector<byte> encode_game_metadata_all(const fe::Game& p_game) const;
		static std::pair<byte, byte> to_uint16_le(std::size_t p_value);

	};

}

#endif
