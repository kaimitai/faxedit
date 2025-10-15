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

	public:
		ROM_Manager(void) = default;
		std::vector<byte> encode_bank_screen_data(std::size_t p_bank_no, const fe::Game& p_game) const;

		static std::pair<byte, byte> to_uint16_le(std::size_t p_value);
	};

}

#endif
