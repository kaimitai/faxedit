#include "ROM_manager.h"
#include <algorithm>
#include <stdexcept>

#include <format>
#include "Config.h"
#include "./../common/klib/Kfile.h"
#include "fe_constants.h"

fe::ROM_Manager::ROM_Manager(void) {
}

// this function will extract all tilemaps and try to pack them across the 3
// tilemap banks, and update the tilemap metadata
fe::TileMapPackingResult fe::ROM_Manager::encode_game_tilemaps(const fe::Config& p_config,
	std::vector<byte>& p_rom, const fe::Game& p_game) const {

	auto l_ptr_tilebamp_bank_rom_offsets{ p_config.bmap_numeric(c::ID_TILEMAP_BANK_OFFSETS) };
	auto l_predef{ p_config.bmap_numeric(c::ID_TILEMAP_TO_PREDEFINED_BANK) };
	std::size_t l_max_bank_size{ p_config.constant(c::ID_WORLD_TILEMAP_MAX_SIZE) };

	// calculate all world tilemap data total size (all ptrs + data)

	// initialize the size with 2, as each world ptr consumes 2 bytes
	std::vector<std::size_t> l_world_tilemap_sizes(8, 2);

	for (std::size_t world{ 0 }; world < 8; ++world) {
		// keep a set of all unique tilemaps in case we have duplicates
		std::set<std::vector<byte>> l_unique_tilemaps;

		for (const auto& screen : p_game.m_chunks.at(world).m_screens) {
			const auto l_scr_tilemap{ screen.get_tilemap_bytes() };

			// add the screen tilemap size plus the two pointers it consumes
			if (l_unique_tilemaps.find(l_scr_tilemap) == end(l_unique_tilemaps)) {
				l_world_tilemap_sizes[world] += 2 + l_scr_tilemap.size();
				l_unique_tilemaps.insert(l_scr_tilemap);
			}
			else
				l_world_tilemap_sizes[world] += 2;
		}
	}

	// we now have all the total sizes, let us recurse and assign everything
	std::vector<int> l_assignments(8, -1);
	std::map<byte, std::size_t> l_used_sizes;

	// initialize the used sizes map with the banks we will use
	for (const auto& kv : l_ptr_tilebamp_bank_rom_offsets)
		l_used_sizes[kv.first] = 0;

	// let us add the predefined assignments
	for (const auto& kv : l_predef) {
		l_used_sizes[static_cast<byte>(kv.second)] += l_world_tilemap_sizes[kv.first];
		l_assignments[kv.first] = static_cast<int>(kv.second);
	}

	// ensure we did not already cross the size limits
	// with the pre-defined placements
	bool l_predef_within_limits{ true };
	for (const auto& kv : l_used_sizes)
		if (kv.second > l_max_bank_size)
			l_predef_within_limits = false;

	// let us try to pack everything
	bool l_success{ l_predef_within_limits &&
		pack_tilemaps_recursively(l_world_tilemap_sizes, 0,
		l_used_sizes, l_assignments, l_max_bank_size) };

	std::map<std::size_t, std::vector<std::size_t>> l_assign_map;

	// we found a possible packing, execute it
	if (l_success) {
		// turn our vector of assignments into a map from bank no
		// to vector of all world nos living here
		for (std::size_t w{ 0 }; w < l_assignments.size(); ++w)
			l_assign_map[static_cast<std::size_t>(l_assignments[w])].push_back(w);

		for (const auto& kv : l_assign_map) {

			auto l_bank_tm_data{ encode_bank_tilemap_data(p_game,
				l_ptr_tilebamp_bank_rom_offsets.at(static_cast<byte>(kv.first)),
				kv.first, kv.second) };

			patch_bytes(l_bank_tm_data, p_rom,
				l_ptr_tilebamp_bank_rom_offsets.at(static_cast<byte>(kv.first)));
		}

		// and patch the world to bank/ptr metadata in ROM as well
		std::size_t lc_md_world_to_bank{ p_config.constant(c::ID_WORLD_TILEMAP_MD) };
		std::size_t lc_md_world_to_ptr{ lc_md_world_to_bank + 8 };

		std::vector<std::size_t> l_md_world_to_ptr(8, 0);
		std::map<std::size_t, std::size_t> l_md_bank_to_ptr;

		for (const auto& kv : l_assign_map) {
			byte l_ptr_no{ 0 };
			for (std::size_t i{ 0 }; i < kv.second.size(); ++i) {
				p_rom.at(lc_md_world_to_bank + kv.second[i]) = static_cast<byte>(kv.first);
				p_rom.at(lc_md_world_to_ptr + kv.second[i]) = l_ptr_no++;
			}
		}
	}

	return fe::TileMapPackingResult(l_success,
		l_assign_map, l_world_tilemap_sizes);
}

std::vector<byte> fe::ROM_Manager::encode_bank_tilemap_data(const fe::Game& p_game,
	std::size_t p_bank_offset,
	std::size_t p_bank_no,
	const std::vector<std::size_t>& p_worlds) const {

	// offset + 2 bytes per world pointer
	std::size_t l_cur_rom_offset{
		p_bank_offset +
		p_worlds.size() * 2 };

	std::vector<std::vector<byte>> l_all_screen_data_chunks;

	for (std::size_t i{ 0 }; i < p_worlds.size(); ++i) {
		std::vector<std::vector<byte>> l_cmpr_scr;

		for (std::size_t s{ 0 }; s < p_game.m_chunks[p_worlds[i]].m_screens.size(); ++s)
			l_cmpr_scr.push_back(p_game.m_chunks[p_worlds[i]].m_screens[s].get_tilemap_bytes());

		auto l_data{
		build_pointer_table_and_data(
			l_cur_rom_offset,
			p_bank_offset,
			l_cmpr_scr)
		};

		l_cur_rom_offset += l_data.size();

		l_all_screen_data_chunks.push_back(std::move(l_data));

	}

	// ptr table start and zero addr are the same here since the ptr table
	// start at the very beginning of each bank
	auto l_all_screens_w_ptr_table{ build_pointer_table_and_data(
		p_bank_offset,
		p_bank_offset,
		l_all_screen_data_chunks
	) };

	return l_all_screens_w_ptr_table;
}

bool fe::ROM_Manager::pack_tilemaps_recursively(const std::vector<std::size_t>& p_sizes,
	std::size_t p_index,
	std::map<byte, std::size_t>& p_used_bank_bytes,
	std::vector<int>& p_bank_assignments,
	std::size_t p_bank_max_size) const {

	if (p_index == p_sizes.size())
		return true;
	else if (p_bank_assignments[p_index] != -1) {
		return pack_tilemaps_recursively(p_sizes, p_index + 1,
			p_used_bank_bytes,
			p_bank_assignments,
			p_bank_max_size);
	}
	else {

		for (auto& kv : p_used_bank_bytes) {
			if (kv.second + p_sizes[p_index] <= p_bank_max_size) {
				p_bank_assignments[p_index] = kv.first;
				kv.second += p_sizes[p_index];
				if (pack_tilemaps_recursively(p_sizes, p_index + 1, p_used_bank_bytes,
					p_bank_assignments, p_bank_max_size))
					return true;
				else {
					kv.second -= p_sizes[p_index];
					p_bank_assignments[p_index] = -1;
				}
			}
		}

	}

	return false;

}

std::size_t fe::ROM_Manager::get_ptr_to_rom_offset(const std::vector<byte>& p_rom,
	std::size_t p_ptr_offset, std::size_t p_zero_addr) const {
	std::size_t l_rel_addr{ from_uint16_le(
		std::make_pair(p_rom.at(p_ptr_offset), p_rom.at(p_ptr_offset + 1))
) };

	return p_zero_addr + l_rel_addr;
}

// this function encodes the game sprite data in the same way as the original game
std::vector<byte> fe::ROM_Manager::encode_game_sprite_data_new(const fe::Config& p_config,
	const fe::Game& p_game) const {
	std::vector<std::vector<byte>> l_all_screen_sprite_data;

	auto l_sprite_ptr{ p_config.pointer(c::ID_SPRITE_PTR) };

	std::size_t l_cur_rom_offset{ l_sprite_ptr.first + 2 * p_game.m_chunks.size() };

	for (std::size_t c{ 0 }; c < p_game.m_chunks.size(); ++c) {
		std::vector<std::vector<byte>> l_screen_sprite_data;

		if (c == c::CHUNK_IDX_BUILDINGS) {
			for (const auto& bundle : p_game.m_npc_bundles)
				l_screen_sprite_data.push_back(bundle.get_bytes());
		}
		else {
			const auto& chunk{ p_game.m_chunks[c] };

			for (std::size_t s{ 0 }; s < chunk.m_screens.size(); ++s)
				l_screen_sprite_data.push_back(chunk.m_screens[s].m_sprite_set.get_bytes());
		}

		auto l_data{ build_pointer_table_and_data(
	l_cur_rom_offset,
	l_sprite_ptr.second,
	l_screen_sprite_data)
		};

		l_cur_rom_offset += l_data.size();

		l_all_screen_sprite_data.push_back(std::move(l_data));
	}

	auto l_all_screens_w_ptr_table{ build_pointer_table_and_data(
l_sprite_ptr.first,
	l_sprite_ptr.second,
	l_all_screen_sprite_data
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

std::vector<byte> fe::ROM_Manager::encode_game_metadata_all(const fe::Config& p_config,
	const fe::Game& p_game) const {
	std::vector<std::vector<byte>> l_all_chunk_meta_data;
	auto l_md_ptr{ p_config.pointer(c::ID_METADATA_PTR) };

	std::size_t l_cur_rom_offset{ l_md_ptr.first + 2 + 2 * p_game.m_chunks.size() };

	for (std::size_t c{ 0 }; c < p_game.m_chunks.size(); ++c) {

		std::vector<std::vector<byte>> l_chunk_md;

		const auto& chunk{ p_game.m_chunks[c] };

		// useless pointer to attribute pointer
		// update value before packing
		l_chunk_md.push_back(std::vector<byte>());

		l_chunk_md.push_back(chunk.get_block_property_bytes());
		l_chunk_md.push_back(chunk.get_screen_scroll_bytes());

		auto l_door_data{ chunk.get_door_bytes(c == c::CHUNK_IDX_TOWNS) };

		// door data followed by door destination table
		l_chunk_md.push_back(l_door_data.first);
		l_chunk_md.push_back(l_door_data.second);

		l_chunk_md.push_back(chunk.get_palette_attribute_bytes());

		l_chunk_md.push_back(chunk.get_metatile_top_left_bytes());
		l_chunk_md.push_back(chunk.get_metatile_top_right_bytes());
		l_chunk_md.push_back(chunk.get_metatile_bottom_left_bytes());
		l_chunk_md.push_back(chunk.get_metatile_bottom_right_bytes());

		auto l_data{ build_pointer_table_and_data(
					l_cur_rom_offset, l_md_ptr.first, l_chunk_md) };

		auto l_attr_ptr{ to_uint16_le(l_cur_rom_offset + 10 - l_md_ptr.first) };

		l_data.at(0) = l_attr_ptr.first;
		l_data.at(1) = l_attr_ptr.second;

		l_cur_rom_offset += l_data.size();

		l_all_chunk_meta_data.push_back(std::move(l_data));
	}

	auto l_all_chunks_w_ptr_table{ build_pointer_table_and_data(
		l_md_ptr.first + 2, l_md_ptr.second, l_all_chunk_meta_data) };

	return l_all_chunks_w_ptr_table;
}

std::pair<std::size_t, std::size_t> fe::ROM_Manager::encode_metadata(const fe::Config& p_config,
	const fe::Game& p_game, std::vector<byte>& p_rom) const {

	// add 2 to the ptr since we actually don't encode the first two bytes
	auto l_md_ptr{ p_config.pointer(c::ID_METADATA_PTR) };
	std::size_t l_md_end{ p_config.constant(c::ID_METADATA_END) };
	std::size_t l_md_size{ l_md_end - (l_md_ptr.first + 2) };

	auto l_metadata{ encode_game_metadata_all(p_config, p_game) };

	if (l_metadata.size() <= l_md_size)
		patch_bytes(l_metadata, p_rom, l_md_ptr.first + 2);
	return std::make_pair(l_metadata.size(), l_md_size);
}

std::pair<std::size_t, std::size_t> fe::ROM_Manager::encode_sprite_data(const fe::Config& p_config,
	const fe::Game& p_game, std::vector<byte>& p_rom) const {
	auto l_sprite_data{ encode_game_sprite_data_new(p_config, p_game) };

	auto l_sprite_ptr{ p_config.pointer(c::ID_SPRITE_PTR) };
	std::size_t l_sprite_data_size{
		p_config.constant(c::ID_SPRITE_DATA_END) -
		l_sprite_ptr.first
	};

	if (l_sprite_data.size() <= l_sprite_data_size)
		patch_bytes(l_sprite_data, p_rom, l_sprite_ptr.first);
	return std::make_pair(l_sprite_data.size(), l_sprite_data_size);
}

// call this if you want to pack both transition types together in free space
std::pair<std::size_t, std::size_t> fe::ROM_Manager::encode_transitions(const fe::Config& p_config,
	const fe::Game& p_game, std::vector<byte>& p_rom) const {
	std::size_t l_free_space_offset{ p_config.constant(c::ID_TRANS_DATA_START) };
	std::size_t l_free_space_size{ p_config.constant(c::ID_TRANS_DATA_END) -
	l_free_space_offset };

	std::vector<std::vector<byte>> l_all_sw_trans_data, l_all_ow_trans_data;

	for (std::size_t i{ 0 }; i < p_game.m_chunks.size(); ++i) {
		l_all_sw_trans_data.push_back(p_game.m_chunks[i].get_sameworld_transition_bytes());
		l_all_ow_trans_data.push_back(p_game.m_chunks[i].get_otherworld_transition_bytes());
	}

	// generate same-world ptr table and data
	auto l_sameworld_ptr{ p_config.pointer(c::ID_SAMEWORLD_TRANS_PTR) };
	auto l_sw_trans_encoded{ build_pointer_table_and_data_aggressive_decoupled(
		l_sameworld_ptr.first,
		l_sameworld_ptr.second,
		l_free_space_offset, l_all_sw_trans_data) };

	// generate other-world ptr table and data
	// let this data start immediately after the same-world data
	auto l_otherworld_ptr{ p_config.pointer(c::ID_OTHERWORLD_TRANS_PTR) };
	auto l_ow_trans_encoded{ build_pointer_table_and_data_aggressive_decoupled(
	l_otherworld_ptr.first,
	l_otherworld_ptr.second,
	l_free_space_offset + l_sw_trans_encoded.second.size(), l_all_ow_trans_data) };

	std::size_t l_total_size{ l_sw_trans_encoded.second.size() + l_ow_trans_encoded.second.size() };

	if (l_total_size <= l_free_space_size) {
		patch_bytes(l_sw_trans_encoded.first, p_rom, l_sameworld_ptr.first);
		patch_bytes(l_sw_trans_encoded.second, p_rom, l_free_space_offset);
		patch_bytes(l_ow_trans_encoded.first, p_rom, l_otherworld_ptr.first);
		patch_bytes(l_ow_trans_encoded.second, p_rom, l_free_space_offset + l_sw_trans_encoded.second.size());
	}

	return std::make_pair(l_total_size, l_free_space_size);
}

// call this if you want to pack same-world transitions in the original location
std::pair<std::size_t, std::size_t> fe::ROM_Manager::encode_sw_transitions(const fe::Config& p_config,
	const fe::Game& p_game, std::vector<byte>& p_rom) const {
	auto l_sw_trans_ptr{ p_config.pointer(c::ID_SAMEWORLD_TRANS_PTR) };
	std::size_t l_sw_data_size{ p_config.constant(c::ID_SW_TRANS_DATA_END) -
	l_sw_trans_ptr.first };

	const auto l_sw_trans_data{ encode_game_sameworld_trans(p_config, p_game) };

	if (l_sw_trans_data.size() <= l_sw_data_size)
		patch_bytes(l_sw_trans_data, p_rom, l_sw_trans_ptr.first);
	return std::make_pair(l_sw_trans_data.size(), l_sw_data_size);
}

// call this if you want to pack other-world transitions in the original location
std::pair<std::size_t, std::size_t> fe::ROM_Manager::encode_ow_transitions(const fe::Config& p_config,
	const fe::Game& p_game, std::vector<byte>& p_rom) const {
	auto l_ow_trans_ptr{ p_config.pointer(c::ID_OTHERWORLD_TRANS_PTR) };
	std::size_t l_ow_data_size{ p_config.constant(c::ID_OW_TRANS_DATA_END) -
	l_ow_trans_ptr.first };

	const auto l_ow_trans_data{ encode_game_otherworld_trans(p_config, p_game) };

	if (l_ow_trans_data.size() <= l_ow_data_size)
		patch_bytes(l_ow_trans_data, p_rom, l_ow_trans_ptr.first);
	return std::make_pair(l_ow_trans_data.size(), l_ow_data_size);
}

// all static data patching
void fe::ROM_Manager::encode_static_data(const fe::Config& p_config, const fe::Game& p_game, std::vector<byte>& p_rom) const {
	encode_stage_data(p_config, p_game, p_rom);
	encode_spawn_locations(p_config, p_game, p_rom);
	encode_mattock_animations(p_config, p_game, p_rom);
	encode_push_block(p_config, p_game, p_rom);
	encode_jump_on_tiles(p_config, p_game, p_rom);
	encode_scene_data(p_config, p_game, p_rom);
	encode_palette_to_music(p_config, p_game, p_rom);
}

void fe::ROM_Manager::encode_chr_data(const fe::Config& p_config,
	const fe::Game& p_game, std::vector<byte>& p_rom,
	const std::vector<std::size_t> p_tileset_start, const std::vector<std::size_t> p_tileset_count) const {

	std::size_t l_chr_wtile_offset{ p_config.constant(c::ID_CHR_WORLD_TILE_OFFSET) };
	std::size_t l_tileset_to_addr{ p_config.constant(c::ID_WORLD_TILESET_TO_ADDR_OFFSET) };

	for (std::size_t i{ 0 }; i < p_game.m_tilesets.size(); ++i) {
		auto l_local_addr_lo{ l_tileset_to_addr + 2 * i };
		auto l_local_addr_hi{ l_tileset_to_addr + 2 * i + 1 };
		std::size_t l_local_addr{
			256 * static_cast<std::size_t>(p_rom.at(l_local_addr_hi)) +
			static_cast<std::size_t>(p_rom.at(l_local_addr_lo)) +
			l_chr_wtile_offset - 0x8000
		};

		const auto& chrtiles{ p_game.m_tilesets[i] };

		for (std::size_t j{ p_tileset_start.at(i) };
			j < p_tileset_start.at(i) + p_tileset_count.at(i); ++j) {
			const auto chr_bytes{ chrtiles.at(j).to_bytes() };

			patch_bytes(chr_bytes, p_rom, l_local_addr);
			l_local_addr += 16;
		}

	}
}

// patch ROM in place for the stage data
void fe::ROM_Manager::encode_stage_data(const fe::Config& p_config, const fe::Game& p_game, std::vector<byte>& p_rom) const {
	const auto& l_stages{ p_game.m_stages };

	// the stage data itself
	for (std::size_t i{ 0 }; i < 6; ++i) {
		const auto& l_stage{ l_stages.m_stages[i] };

		p_rom.at(p_config.constant(c::ID_STAGE_CONN_OFFSET) + 2 * i) = static_cast<byte>(l_stage.m_prev_stage);
		p_rom.at(p_config.constant(c::ID_STAGE_CONN_OFFSET) + 2 * i + 1) = static_cast<byte>(l_stage.m_next_stage);
		p_rom.at(p_config.constant(c::ID_STAGE_SCREEN_OFFSET) + 2 * i) = static_cast<byte>(l_stage.m_prev_screen);
		p_rom.at(p_config.constant(c::ID_STAGE_SCREEN_OFFSET) + 2 * i + 1) = static_cast<byte>(l_stage.m_next_screen);
		p_rom.at(p_config.constant(c::ID_STAGE_REQ_OFFSET) + 2 * i) = static_cast<byte>(l_stage.m_prev_requirement);
		p_rom.at(p_config.constant(c::ID_STAGE_REQ_OFFSET) + 2 * i + 1) = static_cast<byte>(l_stage.m_next_requirement);
		p_rom.at(p_config.constant(c::ID_STAGE_TO_WORLD_OFFSET) + i) = static_cast<byte>(l_stage.m_world_id);
	}

	// start data 
	p_rom.at(p_config.constant(c::ID_GAME_START_SCREEN_OFFSET)) = static_cast<byte>(l_stages.m_start_screen);
	p_rom.at(p_config.constant(c::ID_GAME_START_POS_OFFSET)) = l_stages.m_start_x + (l_stages.m_start_y << 4);
	p_rom.at(p_config.constant(c::ID_GAME_START_HP_OFFSET)) = l_stages.m_start_hp;
}

std::vector<byte> fe::ROM_Manager::encode_game_sameworld_trans(const fe::Config& p_config,
	const fe::Game& p_game) const {
	auto l_sw_ptr{ p_config.pointer(c::ID_SAMEWORLD_TRANS_PTR) };

	std::vector<std::vector<byte>> l_all_sw_trans_data;

	for (std::size_t i{ 0 }; i < p_game.m_chunks.size(); ++i) {
		l_all_sw_trans_data.push_back(
			p_game.m_chunks[i].get_sameworld_transition_bytes()
		);
	}

	return build_pointer_table_and_data_aggressive(l_sw_ptr.first,
		l_sw_ptr.second, l_all_sw_trans_data);
}


std::vector<byte> fe::ROM_Manager::encode_game_otherworld_trans(const fe::Config& p_config,
	const fe::Game& p_game) const {
	auto l_ow_ptr{ p_config.pointer(c::ID_OTHERWORLD_TRANS_PTR) };
	std::vector<std::vector<byte>> l_all_ow_trans_data;

	for (std::size_t i{ 0 }; i < p_game.m_chunks.size(); ++i) {
		l_all_ow_trans_data.push_back(
			p_game.m_chunks[i].get_otherworld_transition_bytes()
		);
	}

	return build_pointer_table_and_data_aggressive(l_ow_ptr.first,
		l_ow_ptr.second, l_all_ow_trans_data);
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

void fe::ROM_Manager::encode_spawn_locations(const fe::Config& p_config, const fe::Game& p_game, std::vector<byte>& p_rom) const {
	std::size_t l_spawn_loc_data_start{ p_config.constant(c::ID_SPAWN_LOC_DATA_START) };

	for (std::size_t i{ 0 }; i < 8; ++i) {
		const auto& l_sl{ p_game.m_spawn_locations.at(i) };

		p_rom.at(l_spawn_loc_data_start + i) = static_cast<byte>(l_sl.m_world);
		p_rom.at(l_spawn_loc_data_start + static_cast<std::size_t>(5 * 8) + i) = l_sl.m_screen;
		p_rom.at(l_spawn_loc_data_start + 8 + i) = l_sl.m_x << 4;
		p_rom.at(l_spawn_loc_data_start + static_cast<std::size_t>(2 * 8) + i) = l_sl.m_y << 4;
		p_rom.at(l_spawn_loc_data_start + static_cast<std::size_t>(4 * 8) + i) = l_sl.m_stage;
		p_rom.at(l_spawn_loc_data_start + static_cast<std::size_t>(3 * 8) + i) = l_sl.m_sprite_set;
	}
}

void fe::ROM_Manager::encode_mattock_animations(const fe::Config& p_config,
	const fe::Game& p_game, std::vector<byte>& p_rom) const {

	std::size_t l_mattock_anim_offset{ p_config.constant(c::ID_MATTOCK_ANIM_OFFSET) };
	for (std::size_t i{ 0 }; i < 8; ++i) {
		for (std::size_t m{ 0 }; m < 4; ++m)
			p_rom.at(l_mattock_anim_offset + 4 * i + m) =
			p_game.m_chunks.at(i).m_mattock_animation.at(m);
	}
}

void fe::ROM_Manager::encode_push_block(const fe::Config& p_config,
	const fe::Game& p_game, std::vector<byte>& p_rom) const {
	const auto& l_pb{ p_game.m_push_block };

	p_rom.at(p_config.constant(c::ID_PTM_STAGE_NO_OFFSET)) = l_pb.m_stage;
	p_rom.at(p_config.constant(c::ID_PTM_SCREEN_NO_OFFSET)) = l_pb.m_screen;
	p_rom.at(p_config.constant(c::ID_PTM_START_POS_OFFSET)) = (l_pb.m_y << 4) + l_pb.m_x;
	p_rom.at(p_config.constant(c::ID_PTM_POS_DELTA_OFFSET)) = l_pb.m_pos_delta;
	p_rom.at(p_config.constant(c::ID_PTM_REPLACE_TILE_OFFSET)) = l_pb.m_source_0;
	p_rom.at(p_config.constant(c::ID_PTM_REPLACE_TILE_OFFSET) + 1) = l_pb.m_source_1;
	p_rom.at(p_config.constant(c::ID_PTM_REPLACE_TILE_OFFSET) + 2) = l_pb.m_target_0;
	p_rom.at(p_config.constant(c::ID_PTM_REPLACE_TILE_OFFSET) + 3) = l_pb.m_target_1;
	p_rom.at(p_config.constant(c::ID_PTM_TILE_NO_OFFSET)) = l_pb.m_draw_block;
	p_rom.at(p_config.constant(c::ID_PTM_BLOCK_COUNT_OFFSET)) = l_pb.m_block_count;
	p_rom.at(p_config.constant(c::ID_PTM_COVER_POS_OFFSET)) = (l_pb.m_cover_y << 4) + l_pb.m_cover_x;
}

void fe::ROM_Manager::encode_jump_on_tiles(const fe::Config& p_config,
	const fe::Game& p_game, std::vector<byte>& p_rom) const {
	std::size_t l_jump_on_anim_offset{ p_config.constant(c::ID_JUMP_ON_ANIM_OFFSET) };
	for (std::size_t i{ 0 }; i < p_game.m_jump_on_animation.size() && i < 4; ++i)
		p_rom.at(l_jump_on_anim_offset + i) = p_game.m_jump_on_animation[i];
}

void fe::ROM_Manager::encode_scene_data(const fe::Config& p_config, const fe::Game& p_game,
	std::vector<byte>& p_rom) const {

	// patch building screen scenes
	std::size_t l_bscene_start{ p_config.constant(c::ID_BUILDING_TO_MUSIC_OFFSET) };
	const auto& bscenes{ p_game.m_building_scenes };

	for (std::size_t i{ 0 }; i < bscenes.size(); ++i) {
		p_rom.at(l_bscene_start + i) = static_cast<byte>(bscenes[i].m_music);
		p_rom.at(l_bscene_start + c::WORLD_BUILDINGS_SCREEN_COUNT + i) = static_cast<byte>(bscenes[i].m_palette);
		p_rom.at(l_bscene_start + 2 * c::WORLD_BUILDINGS_SCREEN_COUNT + i) = static_cast<byte>(bscenes[i].m_tileset);
		p_rom.at(l_bscene_start + 3 * c::WORLD_BUILDINGS_SCREEN_COUNT + i) = bscenes[i].get_pos_as_byte();
	}

	// patch scenes for each world
	std::size_t l_wscene_start{ p_config.constant(c::ID_WORLD_TO_TILESET_OFFSET) };
	const auto& wscenes{ p_game.m_chunks };

	for (std::size_t i{ 0 }; i < wscenes.size(); ++i) {
		p_rom.at(l_wscene_start + i) = static_cast<byte>(wscenes[i].m_scene.m_tileset);
		p_rom.at(l_wscene_start + 8 + i) = static_cast<byte>(wscenes[i].m_scene.m_palette);
		p_rom.at(l_wscene_start + 16 + i) = wscenes[i].m_scene.get_pos_as_byte();
		p_rom.at(l_wscene_start + 24 + i) = static_cast<byte>(wscenes[i].m_scene.m_music);
	}
}

void fe::ROM_Manager::encode_palette_to_music(const fe::Config& p_config,
	const fe::Game& p_game, std::vector<byte>& p_rom) const {
	auto l_slots{ p_config.constant(c::ID_PALETTE_TO_MUSIC_SLOTS) };
	auto l_slot_offset{ p_config.constant(c::ID_PALETTE_TO_MUSIC_OFFSET) };

	const auto& pal_to_mus{ p_game.m_pal_to_music.m_slots };

	for (std::size_t i{ 0 }; i < pal_to_mus.size(); ++i) {
		p_rom.at(l_slot_offset + i) = pal_to_mus[i].m_palette;
		p_rom.at(l_slot_offset + l_slots + i) = pal_to_mus[i].m_music;
	}
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

/*
static void replace_with_scantily_clad_woman(std::vector<byte>& rom_data,
	std::size_t sprite_id) {
	constexpr std::size_t SPRITE_PPU_TILE_COUNT_OFFSET{ 0x3ce2b };
	// offset to first chr of scw
	constexpr std::size_t CHR_SCW_OFFSET{ 0x19852 };
	// map from sprite no to anim frame ptr in the table below
	constexpr std::size_t SPRITE_PHASE_IDX_OFFSET{ 0x38caf };
	// ptr table for animation frames
	constexpr std::size_t SPRITE_PHASE_PTR_OFFSET{ 0x1d046 };
	// chr data in bank 7
	constexpr std::size_t SPRITE_DATA_BANK_7_PTR{ 0x1c01c };
	// zero-addr of bank 7
	constexpr std::size_t SPRITE_DATA_BANK7_0ADDR{ 0x1c010 };

	// set the ppu tile count for the sprite to be 11 - the number of tiles
	// used in this animation
	rom_data[SPRITE_PPU_TILE_COUNT_OFFSET + sprite_id] = 12;

	// copy chr to our sprite which is assumed to be in bank7 (as all s-frame npc's are)
	// copy 11 tiles to target
	const std::size_t chr_target_address = fe::ROM_Manager::from_uint16_le(std::make_pair(
		rom_data.at(SPRITE_DATA_BANK_7_PTR + 2 * (sprite_id - 55)),
		rom_data.at(SPRITE_DATA_BANK_7_PTR + 2 * (sprite_id - 55) + 1)
	)) + SPRITE_DATA_BANK7_0ADDR;

	for (std::size_t i{ 0 }; i < 12 * 16; ++i) {
		rom_data.at(chr_target_address + i) = rom_data.at(CHR_SCW_OFFSET + i);
	}

	// update the 2 animations of the npc to render the tiles in the correct order
	std::size_t ptr_idx{ rom_data[SPRITE_PHASE_IDX_OFFSET + sprite_id] };

	// extract ptr address and then the bank rom offset
	std::size_t anim_ptr{
	fe::ROM_Manager::from_uint16_le(std::make_pair(
			rom_data.at(SPRITE_PHASE_PTR_OFFSET + 2 * ptr_idx),
			rom_data.at(SPRITE_PHASE_PTR_OFFSET + 2 * ptr_idx + 1))) +
			SPRITE_DATA_BANK7_0ADDR
	};

	for (std::size_t i{ 0 }; i < 8; ++i)
		rom_data.at(anim_ptr + 4 + 2 * i) = static_cast<byte>(i);
	anim_ptr += 20;
	for (std::size_t i{ 0 }; i < 7; ++i)
		rom_data.at(anim_ptr + 4 + 2 * i) = static_cast<byte>(i > 3 ? i + 4 : i);

	rom_data.at(anim_ptr + 18) = 0xff;
	rom_data.at(anim_ptr + 19) = 0xff;

	klib::file::write_bytes_to_file(rom_data, "c:/temp/lollers.nes");
}
*/

// gfx functions
const std::map<std::size_t, fe::Sprite_gfx_definiton> fe::ROM_Manager::extract_sprite_data(
	const std::vector<byte>& p_rom
) const {
	auto romrom{ p_rom };
	constexpr std::size_t SPRITE_DATA_BANK6{ 0x18082 };
	constexpr std::size_t SPRITE_DATA_BANK6_END{ 0x1be22 };
	constexpr std::size_t SPRITE_DATA_BANK7{ 0x1c076 };
	constexpr std::size_t SPRITE_DATA_BANK7_END{ 0x1d046 };
	constexpr std::size_t SPRITE_DATA_BANK6_0ADDR{ 0x18010 };
	constexpr std::size_t SPRITE_DATA_BANK_6_PTR{ 0x18012 };
	constexpr std::size_t SPRITE_DATA_BANK_6_PTR_COUNT{ 55 };
	constexpr std::size_t SPRITE_DATA_BANK7_0ADDR{ 0x1c010 };
	constexpr std::size_t SPRITE_DATA_BANK_7_PTR{ 0x1c01c };
	constexpr std::size_t SPRITE_DATA_BANK_7_PTR_COUNT{ 45 };
	constexpr std::size_t SPRITE_PPU_TILE_COUNT_OFFSET{ 0x3ce2b };
	constexpr std::size_t SPRITE_PHASE_IDX_OFFSET{ 0x38caf };
	constexpr std::size_t SPRITE_PALETTE_OFFSET{ 0x2c1d0 };
	constexpr std::size_t SPRITE_TOTAL_COUNT{ SPRITE_DATA_BANK_6_PTR_COUNT + SPRITE_DATA_BANK_7_PTR_COUNT };
	constexpr std::size_t SPRITE_CATEGORY_OFFSET{ 0x3b554 };

	const std::size_t PPU_STATIC_GFX_OFFSET{ 0x20814 };

	const auto sprite_rom_offset = [&p_rom](std::size_t p_sprite_no) -> std::size_t {
		const bool is_bank7 = p_sprite_no >= SPRITE_DATA_BANK_6_PTR_COUNT;
		const std::size_t bank_offset = is_bank7 ? SPRITE_DATA_BANK_7_PTR : SPRITE_DATA_BANK_6_PTR;
		const std::size_t chr_base = is_bank7 ? SPRITE_DATA_BANK7_0ADDR : SPRITE_DATA_BANK6_0ADDR;

		const std::size_t ptr_index = is_bank7 ? p_sprite_no - SPRITE_DATA_BANK_6_PTR_COUNT : p_sprite_no;
		const std::size_t ptr_offset = bank_offset + 2 * ptr_index;

		const std::size_t ptr_value = from_uint16_le(std::make_pair(
			p_rom.at(ptr_offset),
			p_rom.at(ptr_offset + 1)
		));

		return chr_base + ptr_value;
		};

	const auto use_ppu_tiles = [](std::size_t p_sprite_no,
		const std::vector<fe::SpriteCategory>& p_cats) -> bool {
			const std::set<std::size_t> SPRING_SPRITE_NOS{ 0x52,97,98,99 };

			auto spr_cat{ p_cats[p_sprite_no] };

			return spr_cat == SpriteCategory::DroppedItem ||
				spr_cat == SpriteCategory::GameTrigger ||
				(spr_cat == SpriteCategory::MagicEffect &&
					p_sprite_no != 0x0a) ||
				(spr_cat == SpriteCategory::SpecialEffect &&
					SPRING_SPRITE_NOS.find(p_sprite_no) == end(SPRING_SPRITE_NOS)
					);
		};

	// extract offsets to all animation frames
	std::vector<std::size_t> phase_offsets;
	constexpr std::size_t SPRITE_PHASE_COUNT{ 252 };
	constexpr std::size_t SPRITE_PHASE_PTR_OFFSET{ 0x1d046 };

	for (std::size_t i{ 0 }; i < SPRITE_PHASE_COUNT; ++i) {
		phase_offsets.push_back(from_uint16_le(std::make_pair(
			p_rom.at(SPRITE_PHASE_PTR_OFFSET + 2 * i),
			p_rom.at(SPRITE_PHASE_PTR_OFFSET + 2 * i + 1))) +
			SPRITE_DATA_BANK7_0ADDR
		);
	}

	// extract sprite categories
	std::vector<fe::SpriteCategory> spr_cats;
	for (std::size_t i{ 0 }; i <= SPRITE_TOTAL_COUNT; ++i)
		spr_cats.push_back(fe::SpriteCategory(
			p_rom.at(SPRITE_CATEGORY_OFFSET + i)
		));

	// TEST - get animation frame counts per sprite - BEGIN

	// animation frame count per sprite
	std::vector<std::size_t> sprite_anim_frame_cnt;
	// get map of animation ptr table index -> vector of all sprites using this idx
	// keep this for dump to file
	std::map<std::size_t, std::vector<std::size_t>> anim_ptr_to_sprite_ids;

	// intermediate calculations
	{
		// get map of animation ptr table index -> vector of all sprites using this idx
		for (std::size_t i{ 0 }; i < SPRITE_TOTAL_COUNT + 1; ++i)
			anim_ptr_to_sprite_ids[p_rom.at(SPRITE_PHASE_IDX_OFFSET + i)].push_back(i);

		// map sprite id -> anim frame count
		std::map<std::size_t, std::size_t> sprite_anim_frame_cnt_map;

		// loop over the map and assign frame counts to each sprite, assuming that
		// the frames are not scattered across several sprites
		// it will be hard to verify dynamically as the offset tables are all stored
		// within each sprite's update handler

		for (auto it = anim_ptr_to_sprite_ids.begin(); it != anim_ptr_to_sprite_ids.end(); ++it) {
			std::size_t currentKey = it->first;
			std::size_t nextKey;

			auto nextIt = std::next(it);
			if (nextIt != anim_ptr_to_sprite_ids.end()) {
				nextKey = nextIt->first;
			}
			else {
				nextKey = SPRITE_PHASE_COUNT; // End of animation pointer table
			}

			std::size_t dist = nextKey - currentKey;

			for (auto spr_id : it->second) {
				sprite_anim_frame_cnt_map[spr_id] = dist;
			}
		}

		// turn it into a vector
		for (const auto& kv : sprite_anim_frame_cnt_map)
			sprite_anim_frame_cnt.push_back(kv.second);
	}

	/*
	// let's parse all animation frames and dump them too
	std::vector<fe::AnimationFrame> dumpframes;
	for (std::size_t i{ 0 }; i < 252; ++i) {
		std::size_t phase_addr{ phase_offsets.at(i) };
		dumpframes.push_back(fe::AnimationFrame(p_rom, phase_addr));
	}

	// dump all data
	std::string fileout;
	for (std::size_t i{ 0 }; i < dumpframes.size(); ++i) {
		fileout += std::format("\n\nAnimation frame: {}\n", i);
		auto aiter{ anim_ptr_to_sprite_ids.find(i) };
		if (aiter != end(anim_ptr_to_sprite_ids)) {
			//fileout += "Entry point for sprites:";
			for (auto sprno : aiter->second)
				fileout += std::format(" ; ${:02x}: {} ({})\n",
					sprno,
					c::LABELS_SPRITES.find(sprno) != end(c::LABELS_SPRITES) ?
					c::LABELS_SPRITES.at(sprno) : "Unknown",
					fe::Sprite_gfx_definiton::SpriteCatToString(spr_cats.at(sprno))
				);
			fileout += "\n";
		}

		const auto& animz{ dumpframes[i] };
		fileout += std::format("Header: w={}, h={}, x={}, y={}, control=${:02x}\n",
			animz.m_w, animz.m_h,
			static_cast<int>(animz.m_offset_x),
			static_cast<int>(animz.m_offset_y),
			animz.m_hdr_control_byte
		);

		for (const auto& col : animz.m_tilemap) {
			fileout += "\n";
			for (const auto& row : col) {
				if (row.has_value()) {
					fileout += std::format("({:02} ${:02x})",
						row.value().first, row.value().second);
				}
				else
					fileout += "(______)";
			}
		}
	}

	klib::file::write_string_to_file(fileout, "animdump.txt");
	*/
	// TEST - get animation frame counts per sprite - BEGIN

	// extract the palette first
	std::vector<std::vector<byte>> l_sprite_palette;
	for (std::size_t i{ 0 }; i < 4; ++i) {
		std::vector<byte> sub_palette;
		for (std::size_t j{ 0 }; j < 4; ++j)
			sub_palette.push_back(p_rom.at(SPRITE_PALETTE_OFFSET + 4 * i + j));
		l_sprite_palette.push_back(sub_palette);
	}

	// extract static ppu tiles
	std::vector<klib::NES_tile> static_ppu_tiles;
	for (std::size_t i{ 0 }; i < 256; ++i)
		static_ppu_tiles.push_back(klib::NES_tile(p_rom,
			PPU_STATIC_GFX_OFFSET + 16 * i));

	std::map<std::size_t, fe::Sprite_gfx_definiton> result;

	for (std::size_t sprite_no{ 0 }; sprite_no <= SPRITE_TOTAL_COUNT; ++sprite_no) {
		// map tile count and ptr for sprite 3 (Zorugeriru rock) to Zorugeriru
		std::size_t tile_count{ p_rom.at(SPRITE_PPU_TILE_COUNT_OFFSET +
			(sprite_no == 3 ? 49 : sprite_no)) };
		std::size_t l_ptr{ sprite_rom_offset(sprite_no == 3 ? 49 : sprite_no) };

		std::vector<klib::NES_tile> tiles;

		if (use_ppu_tiles(sprite_no, spr_cats)) {
			tiles = static_ppu_tiles;
			l_ptr = PPU_STATIC_GFX_OFFSET;
		}
		else {
			for (std::size_t i{ 0 }; i < tile_count; ++i)
				tiles.push_back(klib::NES_tile(p_rom, l_ptr + 16 * i));
		}

		// find the phase definition index
		std::size_t phase_def_no{ p_rom.at(SPRITE_PHASE_IDX_OFFSET + sprite_no) };
		// get the frames from the animation start address itself
		std::size_t phase_addr{ phase_offsets.at(phase_def_no) };

		fe::Sprite_gfx_definiton gfxdef(tiles, l_sprite_palette, spr_cats[sprite_no]);

		for (std::size_t i{ 0 }; i < (spr_cats[sprite_no] == fe::SpriteCategory::Item ? 1 :
			sprite_anim_frame_cnt[sprite_no]); ++i) {
			gfxdef.add_frame(fe::AnimationFrame(p_rom, phase_addr));
		}
		result.insert(std::make_pair(sprite_no, gfxdef));
	}

	// some hard codings we can't easily get dynamically as the sprite update
	// handlers in the game are not modularized

	// remove the "unused woman" I discovered while working on animation frames
	// hidden within the maskman chr data and animation frames
	result.at(32).disable_frame(2);
	result.at(32).disable_frame(3);

	// lilith with garbage frames at the end
	result.at(9).disable_frame(2);
	result.at(9).disable_frame(3);
	result.at(9).disable_frame(4);

	// Zorugeriru rocks minus Zorugeriru
	result.at(3).disable_frame(0);
	result.at(3).disable_frame(1);
	result.at(3).disable_frame(2);

	// Zorugeriru minus rocks
	result.at(49).disable_frame(3);
	result.at(49).disable_frame(4);
	result.at(49).disable_frame(5);

	// mattocks
	result.at(80).disable_frame(1);
	result.at(80).disable_frame(2);
	result.at(80).disable_frame(3);
	result.at(80).disable_frame(4);

	result.at(91).disable_frame(1);
	result.at(91).disable_frame(2);
	result.at(91).disable_frame(3);
	result.at(91).disable_frame(4);

	// remove first frame of all death effects
	result.at(19).disable_frame(4);
	result.at(20).disable_frame(4);
	result.at(100).disable_frame(4);

	// add deltas for the springs to they look aligned
	// probably hard coded in the game
	result.at(0x52).add_offsets(8);
	result.at(0x61).add_offsets(8);
	result.at(0x62).add_offsets(8);
	result.at(0x63).add_offsets(8);

	// fix the unused snake boss
	result.at(18).stack_snake();

	// mark the glitched entries
	// magic effects and Zorugeriru rock
	result.at(3).m_category = fe::SpriteCategory::Glitched;
	result.at(10).m_category = fe::SpriteCategory::Glitched;
	result.at(81).m_category = fe::SpriteCategory::Glitched;
	result.at(83).m_category = fe::SpriteCategory::Glitched;
	result.at(84).m_category = fe::SpriteCategory::Glitched;

	for (auto& kv : result)
		kv.second.calc_bounding_rect();

	return result;
}

fe::Sprite_gfx_definiton fe::ROM_Manager::extract_door_req_gfx(
	const std::vector<byte>& p_rom) const {

	constexpr std::size_t REQ_GFX_OFFSET{ 0x28550 };
	constexpr std::size_t SPRITE_PALETTE_OFFSET{ 0x2c1d0 };

	std::vector<klib::NES_tile> req_tiles;

	// extract all gfx we need
	for (std::size_t i{ 0 }; i < 115; ++i)
		req_tiles.push_back(klib::NES_tile(p_rom, REQ_GFX_OFFSET + 16 * i));

	// extract palettes
	std::vector<std::vector<byte>> l_sprite_palette;
	for (std::size_t i{ 0 }; i < 4; ++i) {
		std::vector<byte> sub_palette;
		for (std::size_t j{ 0 }; j < 4; ++j)
			sub_palette.push_back(p_rom.at(SPRITE_PALETTE_OFFSET + 4 * i + j));
		l_sprite_palette.push_back(sub_palette);
	}

	fe::Sprite_gfx_definiton result(req_tiles, l_sprite_palette, fe::SpriteCategory::Item);

	fe::AnimationFrame PATTERN_FRAME(2, 2, 0, 0, 4, false);

	fe::AnimationFrame key_a{ PATTERN_FRAME };
	key_a.m_tilemap = {
		{std::make_pair(34, 3), std::make_pair(35,3)},
		{std::make_pair(36, 3), std::make_pair(37,3)}
	};

	fe::AnimationFrame key_k{ PATTERN_FRAME };
	key_k.m_tilemap = {
		{std::make_pair(38, 3), std::make_pair(35,3)},
		{std::make_pair(39, 3), std::make_pair(37,3)}
	};

	fe::AnimationFrame key_q{ PATTERN_FRAME };
	key_q.m_tilemap = {
		{std::make_pair(109, 3), std::make_pair(35,3)},
		{std::make_pair(110, 3), std::make_pair(37,3)}
	};

	fe::AnimationFrame key_j{ PATTERN_FRAME };
	key_j.m_tilemap = {
		{std::make_pair(111, 3), std::make_pair(35,3)},
		{std::make_pair(112, 3), std::make_pair(37,3)}
	};

	fe::AnimationFrame key_jo{ PATTERN_FRAME };
	key_jo.m_tilemap = {
		{std::make_pair(113, 3), std::make_pair(35,3)},
		{std::make_pair(114, 3), std::make_pair(37,3)}
	};

	fe::AnimationFrame ring_elf{ PATTERN_FRAME };
	ring_elf.m_tilemap = {
		{std::make_pair(6, 1), std::make_pair(7,1)},
		{std::make_pair(8, 1), std::make_pair(9,1)}
	};

	fe::AnimationFrame ring_dwarf{ PATTERN_FRAME };
	ring_dwarf.m_tilemap = {
		{std::make_pair(2, 0), std::make_pair(3,0)},
		{std::make_pair(8, 0), std::make_pair(9,0)}
	};

	fe::AnimationFrame ring_demon{ PATTERN_FRAME };
	ring_demon.m_tilemap = {
		{std::make_pair(0, 3), std::make_pair(1,3)},
		{std::make_pair(8, 3), std::make_pair(9,3)}
	};

	result.add_frame(key_a);
	result.add_frame(key_k);
	result.add_frame(key_q);
	result.add_frame(key_j);
	result.add_frame(key_jo);
	result.add_frame(ring_elf);
	result.add_frame(ring_dwarf);
	result.add_frame(ring_demon);

	return result;
}
