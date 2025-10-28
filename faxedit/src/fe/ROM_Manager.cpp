#include "ROM_manager.h"
#include <algorithm>
#include <stdexcept>

fe::ROM_Manager::ROM_Manager(void) :
	m_chunk_tilemaps_bank_idx{ c::CHUNK_TILEMAPS_BANK_IDX },
	m_ptr_tilemaps_bank_rom_offset{ c::PTR_TILEMAPS_BANK_ROM_OFFSET },
	m_chunk_idx{ c::MAP_CHUNK_IDX },
	m_ptr_sprites{ c::PTR_SPRITE_DATA },
	m_ptr_chunk_door_to_chunk{ c::OFFSET_STAGE_CONNECTIONS },
	m_ptr_chunk_door_to_screen{ c::OFFSET_STAGE_SCREENS },
	m_ptr_chunk_door_reqs{ c::OFFSET_STAGE_REQUIREMENTS },
	m_ptr_otherworld_trans_table{ c::PTR_OTHERW_TRANS_TABLE },
	m_ptr_sameworld_trans_table{ c::PTR_SAMEW_TRANS_TABLE }
{
}

std::size_t fe::ROM_Manager::get_ptr_to_rom_offset(const std::vector<byte>& p_rom,
	std::size_t p_ptr_offset, std::size_t p_zero_addr) const {
	std::size_t l_rel_addr{ from_uint16_le(
		std::make_pair(p_rom.at(p_ptr_offset), p_rom.at(p_ptr_offset + 1))
) };

	return p_zero_addr + l_rel_addr;
}

// TODO: Extract sprite metadata from the ROM itself
/*
void fe::ROM_Manager::extract_sprite_data(const std::vector<byte>& p_rom) {
	constexpr std::size_t OFFSET_SPRITE_HSIZES{ 0x3b4e1 };
	constexpr std::size_t OFFSET_SPRITE_VSIZES{ 0x3b4e8 };
	constexpr std::size_t OFFSET_SPRITE_SIZE_IDX{ 0x3b4ef };
	constexpr std::size_t OFFSET_SPRITE_TILE_COUNTS{ 0x3ce2b };

	constexpr std::size_t PTR_TABLE_SPRITE_DATA_0{ 0x18010 };
	constexpr std::size_t PTR_TABLE_SPRITE_DATA_1{ 0x1c010 };

	constexpr std::size_t SPRITE_COUNT{ 101 };

	std::vector<byte> l_hsizes;
	std::vector<byte> l_vsizes;
	std::vector<byte> l_size_idx;
	std::vector<byte> l_tile_cnt;

	for (std::size_t i{ 0 }; i < 7; ++i) {
		l_hsizes.push_back((p_rom.at(OFFSET_SPRITE_HSIZES + i) + 1) >> 3);
		l_vsizes.push_back((p_rom.at(OFFSET_SPRITE_VSIZES + i) + 1) >> 3);
	}

	for (std::size_t i{ 0 }; i < SPRITE_COUNT; ++i) {
		l_tile_cnt.push_back(p_rom.at(OFFSET_SPRITE_TILE_COUNTS + i));
		l_size_idx.push_back(p_rom.at(OFFSET_SPRITE_SIZE_IDX + i));
	}

	std::vector<std::vector<byte>> l_all_spr_tiles;

	// first 0x37 sprites data is in one bank
	// the rest in another
	for (std::size_t i{ 0 }; i < 101; ++i) {
		std::size_t idx{ i >= 0x37 ? i - 0x37 : i };

		std::size_t l_data_offset{
	get_ptr_to_rom_offset(p_rom,
		2 * idx + (i >= 0x37 ? PTR_TABLE_SPRITE_DATA_1 : PTR_TABLE_SPRITE_DATA_0),
		2 * idx + (i >= 0x37 ? PTR_TABLE_SPRITE_DATA_1 : PTR_TABLE_SPRITE_DATA_0))
		};

		std::vector<byte> l_spr_tiles;
		for (std::size_t j{ 0 }; j < l_tile_cnt.at(i); ++j) {
			l_spr_tiles.push_back(p_rom.at(l_data_offset + j));
		}

		l_all_spr_tiles.push_back(l_spr_tiles);
	}
}
*/
// this function encodes the game sprite data in the same way as the original game
std::vector<byte> fe::ROM_Manager::encode_game_sprite_data_new(const fe::Game& p_game) const {
	std::vector<std::vector<byte>> l_all_screen_sprite_data;

	std::size_t l_cur_rom_offset{ m_ptr_sprites.first + 2 * p_game.m_chunks.size() };

	for (std::size_t c{ 0 }; c < p_game.m_chunks.size(); ++c) {
		std::vector<std::vector<byte>> l_screen_sprite_data;

		if (m_chunk_idx[c] == c::CHUNK_IDX_BUILDINGS) {
			for (const auto& bundle : p_game.m_npc_bundles)
				l_screen_sprite_data.push_back(bundle.get_bytes());
		}
		else {
			const auto& chunk{ p_game.m_chunks[m_chunk_idx[c]] };

			for (std::size_t s{ 0 }; s < chunk.m_screens.size(); ++s)
				l_screen_sprite_data.push_back(chunk.m_screens[s].m_sprite_set.get_bytes());
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
			auto data = screen.m_sprite_set.get_bytes();
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

void fe::ROM_Manager::patch_bytes(const std::vector<byte>& p_source, std::vector<byte>& p_target, std::size_t p_target_offset) const {
	std::copy(begin(p_source), end(p_source), begin(p_target) + p_target_offset);
}

std::pair<byte, byte> fe::ROM_Manager::to_uint16_le(std::size_t p_value) {
	return std::make_pair(static_cast<byte>(p_value % 0x100),
		static_cast<byte>(p_value / 0x100));
}

std::size_t fe::ROM_Manager::from_uint16_le(const std::pair<byte, byte>& p_value) {
	return static_cast<std::size_t>(p_value.first) +
		256 * static_cast<std::size_t>(p_value.second);
}

std::vector<byte> fe::ROM_Manager::encode_game_metadata(const fe::Game& p_game) const {

	std::vector<std::vector<byte>> l_chunk_md;

	// for some reason there is a pointer to the attribute pointer for all chunks
	// it just points 10 bytes forward in each case, but we need it for alignment
	// we will update the value of this pointer as we go along, and in the end
	// we pack all metadata for all chunks in a huge blob with a master
	// table at the bottom

	std::size_t l_current_rom_offset{ 0xc022 };

	for (std::size_t i{ 0 }; i <= 0; ++i) {
		const auto& chunk{ p_game.m_chunks[i] };

		// add an empty vector here to get a 2 byte space where the ptr to attr-ptr will be
		l_chunk_md.push_back(std::vector<byte>());

		l_chunk_md.push_back(chunk.get_block_property_bytes());
		l_chunk_md.push_back(chunk.get_screen_scroll_bytes());

		auto l_door_data{ chunk.get_door_bytes() };

		// door data followed by door destination table
		l_chunk_md.push_back(l_door_data.first);
		l_chunk_md.push_back(l_door_data.second);

		l_chunk_md.push_back(chunk.get_palette_attribute_bytes());

		l_chunk_md.push_back(chunk.get_metatile_top_left_bytes());
		l_chunk_md.push_back(chunk.get_metatile_top_right_bytes());
		l_chunk_md.push_back(chunk.get_metatile_bottom_left_bytes());
		l_chunk_md.push_back(chunk.get_metatile_bottom_right_bytes());
	}

	auto l_packed_chunk_md{ build_pointer_table_and_data(l_current_rom_offset, 0xc010, l_chunk_md) };

	auto l_attr_ptr{ to_uint16_le(l_current_rom_offset + 10 - 0xc010) };

	l_packed_chunk_md.at(0) = l_attr_ptr.first;
	l_packed_chunk_md.at(1) = l_attr_ptr.second;

	return l_packed_chunk_md;
}

std::vector<byte> fe::ROM_Manager::encode_game_metadata_all(const fe::Game& p_game) const {
	std::vector<std::vector<byte>> l_all_chunk_meta_data;

	std::size_t l_cur_rom_offset{ 0xc012 + 2 * p_game.m_chunks.size() };

	for (std::size_t c{ 0 }; c < p_game.m_chunks.size(); ++c) {
		std::size_t l_true_chunk_no{ m_chunk_idx[c] };

		std::vector<std::vector<byte>> l_chunk_md;

		const auto& chunk{ p_game.m_chunks[l_true_chunk_no] };

		// useless pointer to attribute pointer
		// update value before packing
		l_chunk_md.push_back(std::vector<byte>());

		l_chunk_md.push_back(chunk.get_block_property_bytes());
		l_chunk_md.push_back(chunk.get_screen_scroll_bytes());

		auto l_door_data{ chunk.get_door_bytes(l_true_chunk_no == 2) };

		// door data followed by door destination table
		l_chunk_md.push_back(l_door_data.first);
		l_chunk_md.push_back(l_door_data.second);

		l_chunk_md.push_back(chunk.get_palette_attribute_bytes());

		l_chunk_md.push_back(chunk.get_metatile_top_left_bytes());
		l_chunk_md.push_back(chunk.get_metatile_top_right_bytes());
		l_chunk_md.push_back(chunk.get_metatile_bottom_left_bytes());
		l_chunk_md.push_back(chunk.get_metatile_bottom_right_bytes());

		auto l_data{ build_pointer_table_and_data(
					l_cur_rom_offset, 0xc010, l_chunk_md) };

		auto l_attr_ptr{ to_uint16_le(l_cur_rom_offset + 10 - 0xc010) };

		l_data.at(0) = l_attr_ptr.first;
		l_data.at(1) = l_attr_ptr.second;

		l_cur_rom_offset += l_data.size();

		l_all_chunk_meta_data.push_back(std::move(l_data));
	}

	auto l_all_chunks_w_ptr_table{ build_pointer_table_and_data(
		0xc012, 0xc010, l_all_chunk_meta_data) };

	return l_all_chunks_w_ptr_table;
}

std::pair<std::size_t, std::size_t> fe::ROM_Manager::encode_bank_tilemaps(const fe::Game& p_game,
	std::vector<byte>& p_rom, std::size_t p_bank_no) const {
	auto l_bank_screen_data{ encode_bank_screen_data(p_game, p_bank_no) };

	if (l_bank_screen_data.size() <= c::SIZE_LIMITS_BANK_TILEMAPS.at(p_bank_no))
		patch_bytes(l_bank_screen_data, p_rom, c::PTR_TILEMAPS_BANK_ROM_OFFSET.at(p_bank_no));

	return std::make_pair(l_bank_screen_data.size(), c::SIZE_LIMITS_BANK_TILEMAPS.at(p_bank_no));
}

std::pair<std::size_t, std::size_t> fe::ROM_Manager::encode_metadata(const fe::Game& p_game, std::vector<byte>& p_rom) const {
	auto l_metadata{ encode_game_metadata_all(p_game) };
	if (l_metadata.size() <= c::SIZE_LIMT_METADATA)
		patch_bytes(l_metadata, p_rom, c::PTR_CHUNK_METADATA + 2);
	return std::make_pair(l_metadata.size(), c::SIZE_LIMT_METADATA);
}

std::pair<std::size_t, std::size_t> fe::ROM_Manager::encode_sprite_data(const fe::Game& p_game, std::vector<byte>& p_rom) const {
	auto l_sprite_data{ encode_game_sprite_data_new(p_game) };
	if (l_sprite_data.size() <= c::SIZE_LIMT_SPRITE_DATA)
		patch_bytes(l_sprite_data, p_rom, c::PTR_CHUNK_SPRITE_DATA);
	return std::make_pair(l_sprite_data.size(), c::SIZE_LIMT_SPRITE_DATA);
}

std::pair<std::size_t, std::size_t> fe::ROM_Manager::encode_transitions(const fe::Game& p_game, std::vector<byte>& p_rom) const {
	constexpr std::size_t FREE_SPACE_OFFSET{ 0x3fced };
	constexpr std::size_t FREE_SPACE_SIZE{ 313 };

	std::vector<std::vector<byte>> l_all_sw_trans_data, l_all_ow_trans_data;

	for (std::size_t i{ 0 }; i < p_game.m_chunks.size(); ++i) {
		l_all_sw_trans_data.push_back(p_game.m_chunks[m_chunk_idx.at(i)].get_sameworld_transition_bytes());
		l_all_ow_trans_data.push_back(p_game.m_chunks[m_chunk_idx.at(i)].get_otherworld_transition_bytes(m_chunk_idx));
	}

	// generate same-world ptr table and data
	auto l_sw_trans_encoded{ build_pointer_table_and_data_aggressive_decoupled(
		m_ptr_sameworld_trans_table.first,
		m_ptr_sameworld_trans_table.second,
		FREE_SPACE_OFFSET, l_all_sw_trans_data) };

	// std::size_t l_ow_data_rel_offset{ FREE_SPACE_SIZE + l_sw_trans_encoded.second.size() };

	// generate other-world ptr table and data
	// let this data start immediately after the same-world data
	auto l_ow_trans_encoded{ build_pointer_table_and_data_aggressive_decoupled(
	m_ptr_otherworld_trans_table.first,
	m_ptr_otherworld_trans_table.second,
	FREE_SPACE_OFFSET + l_sw_trans_encoded.second.size(), l_all_ow_trans_data) };

	std::size_t l_total_size{ l_sw_trans_encoded.second.size() + l_ow_trans_encoded.second.size() };

	if (l_total_size <= FREE_SPACE_SIZE) {
		patch_bytes(l_sw_trans_encoded.first, p_rom, m_ptr_sameworld_trans_table.first);
		patch_bytes(l_sw_trans_encoded.second, p_rom, FREE_SPACE_OFFSET);
		patch_bytes(l_ow_trans_encoded.first, p_rom, m_ptr_otherworld_trans_table.first);
		patch_bytes(l_ow_trans_encoded.second, p_rom, FREE_SPACE_OFFSET + l_sw_trans_encoded.second.size());
	}

	return std::make_pair(l_total_size, FREE_SPACE_SIZE);
}

// all static data patching
void fe::ROM_Manager::encode_static_data(const fe::Game& p_game, std::vector<byte>& p_rom) const {
	encode_chunk_palette_no(p_game, p_rom);
	encode_stage_data(p_game, p_rom);
	encode_spawn_locations(p_game, p_rom);
	encode_mattock_animations(p_game, p_rom);
	encode_push_block(p_game, p_rom);
}

void fe::ROM_Manager::encode_chunk_palette_no(const fe::Game& p_game, std::vector<byte>& p_rom) const {
	for (std::size_t i{ 0 }; i < 8; ++i) {
		p_rom.at(c::PTR_CHUNK_DEFAULT_PALETTE_IDX + get_vector_index(m_chunk_idx, i)) =
			p_game.m_chunks.at(i).m_default_palette_no;
	}
}

// patch ROM in place for the stage data
void fe::ROM_Manager::encode_stage_data(const fe::Game& p_game, std::vector<byte>& p_rom) const {
	const auto& l_stages{ p_game.m_stages };

	// the stage data itself
	for (std::size_t i{ 0 }; i < 6; ++i) {
		const auto& l_stage{ l_stages.m_stages[i] };

		p_rom.at(c::OFFSET_STAGE_CONNECTIONS + 2 * i) = static_cast<byte>(l_stage.m_prev_stage);
		p_rom.at(c::OFFSET_STAGE_CONNECTIONS + 2 * i + 1) = static_cast<byte>(l_stage.m_next_stage);
		p_rom.at(c::OFFSET_STAGE_SCREENS + 2 * i) = static_cast<byte>(l_stage.m_prev_screen);
		p_rom.at(c::OFFSET_STAGE_SCREENS + 2 * i + 1) = static_cast<byte>(l_stage.m_next_screen);
		p_rom.at(c::OFFSET_STAGE_REQUIREMENTS + 2 * i) = static_cast<byte>(l_stage.m_prev_requirement);
		p_rom.at(c::OFFSET_STAGE_REQUIREMENTS + 2 * i + 1) = static_cast<byte>(l_stage.m_next_requirement);
		p_rom.at(c::OFFSET_STAGE_TO_WORLD + i) = static_cast<byte>(get_vector_index(c::MAP_CHUNK_IDX, l_stage.m_world_id));
	}

	// start data 
	p_rom.at(c::OFFSET_GAME_START_SCREEN) = static_cast<byte>(l_stages.m_start_screen);
	p_rom.at(c::OFFSET_GAME_START_POS) = l_stages.m_start_x + (l_stages.m_start_y << 4);
	p_rom.at(c::OFFSET_GAME_START_HP) = l_stages.m_start_hp;
}

std::vector<byte> fe::ROM_Manager::encode_game_sameworld_trans(const fe::Game& p_game) const {
	std::vector<std::vector<byte>> l_all_sw_trans_data;

	for (std::size_t i{ 0 }; i < p_game.m_chunks.size(); ++i) {
		l_all_sw_trans_data.push_back(
			p_game.m_chunks[m_chunk_idx.at(i)].get_sameworld_transition_bytes()
		);
	}

	return build_pointer_table_and_data_aggressive(m_ptr_sameworld_trans_table.first,
		m_ptr_sameworld_trans_table.second, l_all_sw_trans_data);
}


std::vector<byte> fe::ROM_Manager::encode_game_otherworld_trans(const fe::Game& p_game) const {
	std::vector<std::vector<byte>> l_all_ow_trans_data;

	for (std::size_t i{ 0 }; i < p_game.m_chunks.size(); ++i) {
		l_all_ow_trans_data.push_back(
			p_game.m_chunks[m_chunk_idx.at(i)].get_otherworld_transition_bytes(m_chunk_idx)
		);
	}

	return build_pointer_table_and_data_aggressive(m_ptr_otherworld_trans_table.first,
		m_ptr_otherworld_trans_table.second, l_all_ow_trans_data);
}

// creates the ptr table and data as one contiguous blob
std::vector<byte> fe::ROM_Manager::build_pointer_table_and_data_aggressive(
	std::size_t p_rom_loc_ptr_table,
	std::size_t p_ptr_base_rom_offset,
	const std::vector<std::vector<byte>>& p_data) const {
	auto l_data{ build_pointer_table_and_data_aggressive_decoupled(p_rom_loc_ptr_table, p_ptr_base_rom_offset,
		p_rom_loc_ptr_table + 2 * p_data.size(), p_data) };

	std::vector<byte> l_result(l_data.first);
	l_result.insert(end(l_result), begin(l_data.second), end(l_data.second));

	return l_result;
}

// look for deduplication opportunities by putting the largest data elements
// first, and then see if any of the smaller are contiguous sub-data of any previous
// TODO: Make it even more aggressive by using pointer-bytes made so far
// as data (pointer destinations), if possible
std::pair<std::vector<byte>, std::vector<byte>> fe::ROM_Manager::build_pointer_table_and_data_aggressive_decoupled(
	std::size_t p_rom_loc_ptr_table,
	std::size_t p_ptr_base_rom_offset,
	std::size_t p_rom_loc_data,
	const std::vector<std::vector<byte>>& p_data) const
{
	struct Entry {
		const std::vector<byte>* data;
		std::size_t original_index;
	};

	// Step 1: Sort input blocks by descending size for better deduplication
	std::vector<Entry> sorted;
	sorted.reserve(p_data.size());
	for (std::size_t i = 0; i < p_data.size(); ++i)
		sorted.push_back({ &p_data[i], i });

	std::sort(sorted.begin(), sorted.end(), [](const Entry& a, const Entry& b) {
		return a.data->size() > b.data->size();
		});

	// Step 2: Build deduplicated data section and track pointer targets
	std::vector<byte> data_section;
	std::vector<std::size_t> pointers(p_data.size());

	for (const auto& entry : sorted) {
		const auto& vec = *entry.data;
		std::size_t offset = std::string::npos;

		// Brute-force search for subvector match in already placed data
		for (std::size_t i = 0; i + vec.size() <= data_section.size(); ++i) {
			if (std::equal(vec.begin(), vec.end(), data_section.begin() + i)) {
				offset = i;
				break;
			}
		}

		// If no match found, append new data
		if (offset == std::string::npos) {
			offset = data_section.size();
			data_section.insert(data_section.end(), vec.begin(), vec.end());
		}

		// Compute ROM address of the data block
		std::size_t rom_offset = p_rom_loc_data + offset;
		pointers[entry.original_index] = rom_offset - p_ptr_base_rom_offset;
	}

	// Step 3: Build pointer table (2 bytes per pointer, little-endian)
	std::vector<byte> pointer_table(p_data.size() * 2);
	for (std::size_t i = 0; i < p_data.size(); ++i) {
		std::size_t ptr = pointers[i];
		pointer_table[i * 2] = static_cast<byte>(ptr & 0xFF);       // Low byte
		pointer_table[i * 2 + 1] = static_cast<byte>((ptr >> 8) & 0xFF); // High byte
	}

	// Step 4: Return pointer table and data section separately
	return { pointer_table, data_section };
}

void fe::ROM_Manager::encode_spawn_locations(const fe::Game& p_game, std::vector<byte>& p_rom) const {

	for (std::size_t i{ 0 }; i < 8; ++i) {
		const auto& l_sl{ p_game.m_spawn_locations.at(i) };

		p_rom.at(c::OFFSET_SPAWN_LOC_WORLDS + i) = static_cast<byte>(get_vector_index(m_chunk_idx, l_sl.m_world));
		p_rom.at(c::OFFSET_SPAWN_LOC_SCREENS + i) = l_sl.m_screen;
		p_rom.at(c::OFFSET_SPAWN_LOC_X_POS + i) = l_sl.m_x << 4;
		p_rom.at(c::OFFSET_SPAWN_LOC_Y_POS + i) = l_sl.m_y << 4;
		p_rom.at(c::OFFSET_SPAWN_LOC_STAGES + i) = l_sl.m_stage;
	}
}

void fe::ROM_Manager::encode_mattock_animations(const fe::Game& p_game, std::vector<byte>& p_rom) const {
	for (std::size_t i{ 0 }; i < 8; ++i) {
		std::size_t l_true_chunk{ get_vector_index(m_chunk_idx, i) };

		for (std::size_t m{ 0 }; m < 4; ++m)
			p_rom.at(c::OFFSET_MATTOCK_ANIMATIONS + 4 * l_true_chunk + m) =
			p_game.m_chunks.at(i).m_mattock_animation.at(m);
	}
}

void fe::ROM_Manager::encode_push_block(const fe::Game& p_game, std::vector<byte>& p_rom) const {
	const auto& l_pb{ p_game.m_push_block };

	p_rom.at(c::OFFSET_PTM_STAGE_NO) = l_pb.m_stage;
	p_rom.at(c::OFFSET_PTM_SCREEN_NO) = l_pb.m_screen;
	p_rom.at(c::OFFSET_PTM_START_POS) = (l_pb.m_y << 4) + l_pb.m_x;
	p_rom.at(c::OFFSET_PTM_POS_DELTA) = l_pb.m_pos_delta;
	p_rom.at(c::OFFSET_PTM_REPLACE_TILE_NOS) = l_pb.m_source_0;
	p_rom.at(c::OFFSET_PTM_REPLACE_TILE_NOS + 1) = l_pb.m_source_1;
	p_rom.at(c::OFFSET_PTM_REPLACE_TILE_NOS + 2) = l_pb.m_target_0;
	p_rom.at(c::OFFSET_PTM_REPLACE_TILE_NOS + 3) = l_pb.m_target_1;
	p_rom.at(c::OFFSET_PTM_TILE_NO) = l_pb.m_draw_block;
	p_rom.at(c::OFFSET_PTM_BLOCK_COUNT) = l_pb.m_block_count;
	p_rom.at(c::OFFSET_PTM_COVER_POS) = (l_pb.m_cover_y << 4) + l_pb.m_cover_x;
}

// this function generates pointer tables and data offsets for several pieces of data at once,
// and generates a vector of <data table number> -> {ptr value, data pointed to}
// it uses global deduplication across all the data
// TODO: Test and actually use this
std::vector<std::vector<std::pair<std::size_t, std::vector<byte>>>> fe::ROM_Manager::generate_multi_pointer_tables(
	const std::vector<std::vector<std::vector<byte>>>& all_data_sets,
	const std::vector<std::size_t>& pointer_table_offsets,
	std::size_t rom_zero_address,
	const std::vector<std::pair<std::size_t, std::size_t>>& p_available
) {

	// Internal structure to track placed data and its ROM address
	struct PlacedBlock {
		std::size_t rom_address;       // ROM offset where this block starts
		std::vector<byte> data;        // Actual data placed at that address
	};

	if (all_data_sets.size() != pointer_table_offsets.size()) {
		throw std::runtime_error("Mismatch between number of data sets and pointer table offsets.");
	}

	// Flatten all blocks with metadata and sort by descending size
	struct BlockInfo {
		std::size_t table_index;
		std::size_t block_index;
		std::vector<byte> data;
	};

	std::vector<BlockInfo> all_blocks;
	for (std::size_t table_idx = 0; table_idx < all_data_sets.size(); ++table_idx) {
		const auto& table = all_data_sets[table_idx];
		for (std::size_t block_idx = 0; block_idx < table.size(); ++block_idx) {
			all_blocks.push_back({ table_idx, block_idx, table[block_idx] });
		}
	}

	std::sort(all_blocks.begin(), all_blocks.end(), [](const BlockInfo& a, const BlockInfo& b) {
		return a.data.size() > b.data.size(); // Largest blocks first
		});

	// Result structure: one vector per pointer table
	std::vector<std::vector<std::pair<std::size_t, std::vector<byte>>>> result(all_data_sets.size());

	// Track available chunks
	std::vector<std::pair<std::size_t, std::size_t>> available_chunks = p_available;

	// Track all placed blocks for subvector matching
	std::vector<PlacedBlock> placed_blocks;

	for (const auto& block_info : all_blocks) {
		const auto& data = block_info.data;

		// Try to find a subvector match in previously placed blocks
		bool matched = false;
		for (const auto& placed : placed_blocks) {
			auto it = std::search(placed.data.begin(), placed.data.end(), data.begin(), data.end());
			if (it != placed.data.end()) {
				// Match found — compute offset and pointer address
				std::size_t offset_in_block = std::distance(placed.data.begin(), it);
				std::size_t rom_address = placed.rom_address + offset_in_block;
				std::size_t pointer_address = rom_address - rom_zero_address;

				// Record pointer and empty data (since it's reused)
				result[block_info.table_index].emplace_back(pointer_address, std::vector<byte>{});
				matched = true;
				break;
			}
		}

		if (matched) continue;

		// No match found — allocate full block in available space
		bool placed = false;
		for (auto& chunk : available_chunks) {
			std::size_t& chunk_offset = chunk.first;
			std::size_t& chunk_remaining = chunk.second;

			if (chunk_remaining >= data.size()) {
				std::size_t rom_address = chunk_offset;
				std::size_t pointer_address = rom_address - rom_zero_address;

				// Record placement
				result[block_info.table_index].emplace_back(pointer_address, data);
				placed_blocks.push_back({ rom_address, data });

				// Update chunk
				chunk_offset += data.size();
				chunk_remaining -= data.size();

				placed = true;
				break;
			}
		}

		if (!placed) {
			throw std::runtime_error("Not enough space to place all data blocks.");
		}
	}

	return result;
}
