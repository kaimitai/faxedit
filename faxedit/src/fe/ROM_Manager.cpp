#include "ROM_manager.h"
#include <algorithm>

fe::ROM_Manager::ROM_Manager(void) :
	m_chunk_tilemaps_bank_idx{ c::CHUNK_TILEMAPS_BANK_IDX },
	m_ptr_tilemaps_bank_rom_offset{ c::PTR_TILEMAPS_BANK_ROM_OFFSET },
	m_chunk_idx{ c::MAP_CHUNK_IDX },
	m_ptr_sprites{ c::PTR_SPRITE_DATA },
	m_chunk_idx_npc_bundles{ c::IDX_CHUNK_NPC_BUNDLES }
{
}

// this function encodes the game sprite data in the same way as the original game
std::vector<byte> fe::ROM_Manager::encode_game_sprite_data_new(const fe::Game& p_game) const {
	std::vector<std::vector<byte>> l_all_screen_sprite_data;

	std::size_t l_cur_rom_offset{ m_ptr_sprites.first + 2 * p_game.m_chunks.size() };

	for (std::size_t c{ 0 }; c < p_game.m_chunks.size(); ++c) {
		std::vector<std::vector<byte>> l_screen_sprite_data;

		if (m_chunk_idx[c] == m_chunk_idx_npc_bundles) {
			l_screen_sprite_data = p_game.m_npc_bundles;
		}
		else {
			const auto& chunk{ p_game.m_chunks[m_chunk_idx[c]] };

			for (std::size_t s{ 0 }; s < chunk.m_screens.size(); ++s)
				l_screen_sprite_data.push_back(chunk.m_screens[s].get_sprite_bytes());
		}

		auto l_data{ build_pointer_table_and_data(
	l_cur_rom_offset,
	m_ptr_sprites.second,
	l_screen_sprite_data)
		};

		l_cur_rom_offset += l_data.size();

		l_all_screen_sprite_data.push_back(std::move(l_data));
	}

	auto l_all_screens_w_ptr_table{ build_pointer_table_and_data(
m_ptr_sprites.first,
	m_ptr_sprites.second,
	l_all_screen_sprite_data
) };

	return l_all_screens_w_ptr_table;

}

// this function encodes the screen data for a given bank
std::vector<byte> fe::ROM_Manager::encode_bank_screen_data(const fe::Game& p_game, std::size_t p_bank_no) const {

	// extract all chunk nos for this bank
	std::vector<std::size_t> l_chunks_this_bank;
	for (std::size_t i{ 0 }; i < m_chunk_tilemaps_bank_idx.size(); ++i)
		if (p_bank_no == m_chunk_tilemaps_bank_idx[i])
			l_chunks_this_bank.push_back(i);

	// l_chunks_this_bank now holds all the chunk numbers in the correct order - create ROM data for these chunk tilemaps
	std::vector<std::vector<byte>> l_all_screen_data_chunks;
	std::size_t l_cur_rom_offset{ m_ptr_tilemaps_bank_rom_offset.at(p_bank_no) + 2 * l_chunks_this_bank.size() };

	for (std::size_t i{ 0 }; i < l_chunks_this_bank.size(); ++i) {
		std::vector<std::vector<byte>> l_cmpr_scr;

		for (std::size_t s{ 0 }; s < p_game.m_chunks[l_chunks_this_bank[i]].m_screens.size(); ++s)
			l_cmpr_scr.push_back(p_game.m_chunks[l_chunks_this_bank[i]].m_screens[s].get_tilemap_bytes());

		auto l_data{
		build_pointer_table_and_data(
			l_cur_rom_offset,
			m_ptr_tilemaps_bank_rom_offset.at(p_bank_no),
			l_cmpr_scr)
		};

		l_cur_rom_offset += l_data.size();

		l_all_screen_data_chunks.push_back(std::move(l_data));

	}

	auto l_all_screens_w_ptr_table{ build_pointer_table_and_data(
	m_ptr_tilemaps_bank_rom_offset.at(p_bank_no),
		m_ptr_tilemaps_bank_rom_offset.at(p_bank_no),
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

// this function encodes all the sprite data for all chunks and all screens
// it puts all unique sprite data vectors in a "pool" and the pointers for
// all chunks draw from this pool of unique data
// TODO: Inject npc bundles and store it for the buildings chunk
std::vector<byte> fe::ROM_Manager::encode_game_sprite_data(const fe::Game& p_game) const {
	std::vector<byte> l_result;

	// Precompute sizes
	std::size_t master_table_size = p_game.m_chunks.size() * 2;
	std::size_t chunk_table_size = 0;
	for (const auto& chunk : p_game.m_chunks) {
		chunk_table_size += chunk.m_screens.size() * 2;
	}
	std::size_t sprite_data_start_offset = m_ptr_sprites.first + master_table_size + chunk_table_size;

	// Deduplication map
	std::map<std::vector<byte>, std::size_t> unique_data_map;
	std::vector<std::vector<std::size_t>> screen_data_offsets(p_game.m_chunks.size());
	std::vector<byte> sprite_data_blob;

	// Step 1: Deduplicate and collect sprite data
	for (size_t chunk_idx = 0; chunk_idx < p_game.m_chunks.size(); ++chunk_idx) {
		const auto& chunk = p_game.m_chunks[chunk_idx];
		for (const auto& screen : chunk.m_screens) {
			auto data = screen.get_sprite_bytes();
			auto it = unique_data_map.find(data);
			if (it == unique_data_map.end()) {
				std::size_t offset = sprite_data_start_offset + sprite_data_blob.size();
				unique_data_map[data] = offset;
				screen_data_offsets[chunk_idx].push_back(offset);
				sprite_data_blob.insert(sprite_data_blob.end(), data.begin(), data.end());
			}
			else {
				screen_data_offsets[chunk_idx].push_back(it->second);
			}
		}
	}

	// Step 2: Precompute chunk table offsets using m_chunk_idx remapping
	std::vector<std::size_t> chunk_table_offsets(p_game.m_chunks.size());
	std::size_t chunk_table_start = m_ptr_sprites.first + master_table_size;
	std::size_t current_chunk_offset = chunk_table_start;

	for (std::size_t rom_chunk_idx = 0; rom_chunk_idx < p_game.m_chunks.size(); ++rom_chunk_idx) {
		std::size_t logical_chunk_idx = m_chunk_idx[rom_chunk_idx];
		std::size_t table_size = p_game.m_chunks[logical_chunk_idx].m_screens.size() * 2;
		chunk_table_offsets[rom_chunk_idx] = current_chunk_offset;
		current_chunk_offset += table_size;
	}

	// Step 3: Build chunk pointer tables
	std::vector<byte> chunk_pointer_tables;
	for (std::size_t rom_chunk_idx = 0; rom_chunk_idx < p_game.m_chunks.size(); ++rom_chunk_idx) {
		std::size_t logical_chunk_idx = m_chunk_idx[rom_chunk_idx];
		const auto& screen_offsets = screen_data_offsets[logical_chunk_idx];
		for (std::size_t screen_offset : screen_offsets) {
			std::size_t relative = screen_offset - m_ptr_sprites.second;
			chunk_pointer_tables.push_back(relative & 0xFF);
			chunk_pointer_tables.push_back((relative >> 8) & 0xFF);
		}
	}

	// Step 4: Build master pointer table using remapped chunk_table_offsets
	for (std::size_t chunk_offset : chunk_table_offsets) {
		std::size_t relative = chunk_offset - m_ptr_sprites.second;
		l_result.push_back(relative & 0xFF);
		l_result.push_back((relative >> 8) & 0xFF);
	}

	// Step 5: Append chunk pointer tables and sprite data
	l_result.insert(l_result.end(), chunk_pointer_tables.begin(), chunk_pointer_tables.end());
	l_result.insert(l_result.end(), sprite_data_blob.begin(), sprite_data_blob.end());

	return l_result;
}

std::pair<byte, byte> fe::ROM_Manager::to_uint16_le(std::size_t p_value) {
	return std::make_pair(static_cast<byte>(p_value % 0x100),
		static_cast<byte>(p_value / 0x100));
}
