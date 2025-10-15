#include "ROM_manager.h"
#include <algorithm>

// this function encodes the screen data for a given bank
std::vector<byte> fe::ROM_Manager::encode_bank_screen_data(std::size_t p_bank_no, const fe::Game& p_game) const {

	

	std::vector<std::vector<byte>> l_all_screen_data_chunks;
	std::size_t l_cur_rom_offset{ p_game.m_ptr_chunk_screen_data[0] + 6 };

	for (std::size_t i{ 0 }; i < 3; ++i) {
		std::vector<std::vector<byte>> l_cmpr_scr;

		for (std::size_t s{ 0 }; s < p_game.m_chunks[i].m_screens.size(); ++s)
			l_cmpr_scr.push_back(p_game.m_chunks[i].m_screens[s].get_tilemap_bytes());

		auto l_data{
		build_pointer_table_and_data(
			l_cur_rom_offset,
			0x10,
			l_cmpr_scr)
		};

		l_cur_rom_offset += l_data.size();

		l_all_screen_data_chunks.push_back(std::move(l_data));

	}

	auto l_all_screens_w_ptr_table{ build_pointer_table_and_data(
	p_game.m_ptr_chunk_screen_data[0],
		0x10,
		l_all_screen_data_chunks
	) };

	return l_all_screens_w_ptr_table;
}

// this function takes a vector of contiguous data elements
// a pointer table is generated, pointing to the data given
// which starts immediately after the pointer table
// duplicate entries are stored once, and pointed to from several ptr table entries
std::vector<byte> fe::ROM_Manager::build_pointer_table_and_data(
	std::size_t p_rom_loc_ptr_table,
	std::size_t p_ptr_base_rom_offset,
	const std::vector<std::vector<byte>>& p_data) const {

	std::vector<std::size_t> l_ptr_table;
	std::vector<std::vector<byte>> l_inserted_data;
	std::vector<std::size_t> l_inserted_offsets;
	std::vector<byte> l_rom_data;

	for (const auto& l_data_chunk : p_data) {
		std::size_t l_rom_offset{ 0 };
		bool l_found{ false };

		// linearly search for duplicates - ok since our data is short and few
		for (std::size_t i{ 0 }; i < l_inserted_data.size(); ++i) {
			if (l_inserted_data[i] == l_data_chunk) {
				l_rom_offset = l_inserted_offsets[i];
				l_found = true;
				break;
			}
		}

		if (!l_found) {
			l_rom_offset = p_rom_loc_ptr_table + 2 * p_data.size() + l_rom_data.size();
			l_inserted_data.push_back(l_data_chunk);
			l_inserted_offsets.push_back(l_rom_offset);
			l_rom_data.insert(end(l_rom_data), begin(l_data_chunk), end(l_data_chunk));
		}

		std::size_t l_ptr_value{ l_rom_offset - p_ptr_base_rom_offset };
		l_ptr_table.push_back(l_ptr_value);
	}

	std::vector<byte> l_result;

	// generate pointer table
	for (std::size_t l_ptr : l_ptr_table) {
		auto l_ptr_bytes{ to_uint16_le(l_ptr) };
		l_result.push_back(l_ptr_bytes.first);
		l_result.push_back(l_ptr_bytes.second);
	}

	// append the data it points to
	l_result.insert(end(l_result), begin(l_rom_data), end(l_rom_data));

	return l_result;
}

std::pair<byte, byte> fe::ROM_Manager::to_uint16_le(std::size_t p_value) {
	return std::make_pair(static_cast<byte>(p_value % 0x100),
		static_cast<byte>(p_value / 0x100));
}
