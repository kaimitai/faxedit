#include "ROM_manager.h"
#include <algorithm>
#include <stdexcept>

#include <format>
#include "Config.h"
#include "./../common/klib/Kfile.h"
#include "./../common/klib/Kstring.h"
#include "fe_constants.h"

// this function will extract all tilemaps and try to pack them across the 3
// tilemap banks, and update the tilemap metadata
fe::TileMapPackingResult fe::ROM_Manager::encode_game_tilemaps(const fe::Config& p_config,
	std::vector<byte>& p_rom, const fe::Game& p_game) const {

	auto l_ptr_tilebamp_bank_rom_offsets{ p_config.bmap_numeric(c::ID_TILEMAP_BANK_OFFSETS) };
	auto l_predef{ p_config.bmap_numeric(c::ID_TILEMAP_TO_PREDEFINED_BANK) };
	std::size_t l_max_bank_size{ p_config.constant(c::ID_WORLD_TILEMAP_MAX_SIZE) };

	// precompute all world tilemaps' total sizes (all ptrs + data)
	auto l_world_tilemap_sizes{ get_world_tilemap_sizes(p_game) };

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

// return the total size each set of world tilemaps take - including ptrs and ptr to ptrs
std::vector<std::size_t> fe::ROM_Manager::get_world_tilemap_sizes(const fe::Game& p_game) const {
	std::vector<std::size_t> result;

	for (std::size_t world{ 0 }; world < 8; ++world) {
		// extract all tilemaps for this world
		std::vector<std::vector<byte>> l_tilemp_data;
		for (const auto& screen : p_game.m_chunks.at(world).m_screens)
			l_tilemp_data.push_back(screen.get_tilemap_bytes());

		// simulate a ptr table + data section generation, and keep the sizes
		auto l_data{ build_pointer_table_and_data(0, 0, l_tilemp_data) };
		// add 2 bytes for the master ptr
		result.push_back(l_data.size() + 2);
	}

	return result;
}

std::size_t fe::ROM_Manager::get_ptr_to_rom_offset(const std::vector<byte>& p_rom,
	std::size_t p_ptr_offset, std::size_t p_zero_addr) const {
	std::size_t l_rel_addr{ from_uint16_le(
		std::make_pair(p_rom.at(p_ptr_offset), p_rom.at(p_ptr_offset + 1))
) };

	return p_zero_addr + l_rel_addr;
}

std::size_t fe::ROM_Manager::get_ptr_to_rom_offset(const std::vector<byte>& p_rom,
	std::pair<std::size_t, std::size_t> p_ptr) const {
	return get_ptr_to_rom_offset(p_rom, p_ptr.first, p_ptr.second);
}

std::size_t fe::ROM_Manager::read_uint16_le(const std::vector<byte>& p_rom, std::size_t p_offset) const {
	return static_cast<std::size_t>(p_rom.at(p_offset)) +
		256 * static_cast<std::size_t>(p_rom.at(p_offset + 1));
}

std::vector<byte> fe::ROM_Manager::read_bytes(const std::vector<byte>& p_rom,
	std::size_t p_offset, std::size_t p_count) const {
	return std::vector<byte>(begin(p_rom) + p_offset, begin(p_rom) + p_offset + p_count);
}

// given ROM offset and zero addr of ptr table start - detect how many ptrs are present
// assumes data follows the ptr table - and that the table has at least one ptr
std::size_t fe::ROM_Manager::get_ptr_table_entry_count(const std::vector<byte>& p_rom,
	std::size_t p_ptr_rom_address, std::size_t p_zero_addr) const {
	std::size_t min_ptr_value{ get_ptr_to_rom_offset(p_rom, p_ptr_rom_address, p_zero_addr) };
	std::size_t cur_rom_offset{ p_ptr_rom_address };
	std::size_t result{ 0 };

	while (cur_rom_offset < min_ptr_value) {
		min_ptr_value = std::min(min_ptr_value, get_ptr_to_rom_offset(p_rom, cur_rom_offset, p_zero_addr));
		cur_rom_offset += 2;
		++result;
	}

	return result;
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

void fe::ROM_Manager::patch_ptr(std::vector<byte>& p_rom, std::size_t p_offset, std::size_t p_ptr_value) const {
	p_rom.at(p_offset) = static_cast<byte>(p_ptr_value % 256);
	p_rom.at(p_offset + 1) = static_cast<byte>(p_ptr_value / 256);
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
	const auto bld_door_sub{ p_config.bmap_numeric(c::ID_DOOR_DEST_INDEX_SUB) };

	std::size_t l_cur_rom_offset{ l_md_ptr.first + 2 + 2 * p_game.m_chunks.size() };

	for (std::size_t c{ 0 }; c < p_game.m_chunks.size(); ++c) {

		std::vector<std::vector<byte>> l_chunk_md;

		const auto& chunk{ p_game.m_chunks[c] };

		// useless pointer to attribute pointer
		// update value before packing
		l_chunk_md.push_back(std::vector<byte>());

		l_chunk_md.push_back(chunk.get_block_property_bytes());
		l_chunk_md.push_back(chunk.get_screen_scroll_bytes());

		auto l_door_data{ chunk.get_door_bytes(c,
			bld_door_sub.contains(static_cast<byte>(c)) ? bld_door_sub.at(static_cast<byte>(c)) : 0
			) };

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

std::pair<std::size_t, std::size_t> fe::ROM_Manager::encode_transitions(const fe::Config& p_config,
	const fe::Game& p_game, std::vector<byte>& p_rom) const {
	constexpr bool PAD_WITH_FF{ false };

	const auto freespace_strs{ p_config.bmap(c::ID_BANK15_FREE_SPACE) };
	std::vector<std::pair<std::size_t, std::size_t>> free_ranges;
	std::size_t total_free_space{ 0 }, total_used_space{ 0 };

	for (const auto& kv : freespace_strs) {
		auto endpoints{ klib::str::split_string(kv.second, ':') };
		if (endpoints.size() != 2)
			throw std::runtime_error(std::format("Invalid ROM range: {}", kv.second));

		std::size_t start_ep{ static_cast<std::size_t>(klib::str::parse_numeric(klib::str::trim(endpoints.at(0)))) };
		std::size_t end_ep{ static_cast<std::size_t>(klib::str::parse_numeric(klib::str::trim(endpoints.at(1)))) };
		if (start_ep >= end_ep)
			throw std::runtime_error(std::format("Invalid ROM range: {}", kv.second));

		free_ranges.push_back(std::make_pair(start_ep, end_ep));
		total_free_space += end_ep - start_ep;
	}

	// pack all transition data
	std::vector<std::vector<std::vector<byte>>> all_trans_data;
	std::vector<std::vector<byte>> l_all_sw_trans_data, l_all_ow_trans_data;
	for (std::size_t i{ 0 }; i < p_game.m_chunks.size(); ++i) {
		l_all_sw_trans_data.push_back(p_game.m_chunks[i].get_sameworld_transition_bytes());
		l_all_ow_trans_data.push_back(p_game.m_chunks[i].get_otherworld_transition_bytes());
	}
	all_trans_data.push_back(l_all_sw_trans_data);
	all_trans_data.push_back(l_all_ow_trans_data);

	const std::vector<Pointer> ptrs{
		p_config.pointer(c::ID_SAMEWORLD_TRANS_PTR),
		p_config.pointer(c::ID_OTHERWORLD_TRANS_PTR)
	};

	fe::GodAllocator allocator;
	const auto allocresult{ allocator.init_and_allocate(ptrs, all_trans_data, free_ranges,
		PAD_WITH_FF) };

	if (!allocresult)
		throw std::runtime_error(std::format("Could not pack all transition data within {} bytes",
			total_free_space));
	else {
		const auto& ptrtablewrites{ allocresult->ptr_table_writes };
		const auto& datawrites{ allocresult->bucket_writes };

		for (const auto& ptrwrite : ptrtablewrites)
			patch_bytes(ptrwrite.data, p_rom, ptrwrite.rom_offset);
		for (const auto& datawrite : datawrites) {
			patch_bytes(datawrite.data, p_rom, datawrite.rom_offset);
			total_used_space += datawrite.data.size();
		}
	}

	return std::make_pair(total_used_space, total_free_space);
}

// all static data patching
void fe::ROM_Manager::encode_static_data(const fe::Config& p_config, const fe::Game& p_game, std::vector<byte>& p_rom) const {
	encode_palette_data(p_config, p_game, p_rom);
	encode_stage_data(p_config, p_game, p_rom);
	encode_spawn_locations(p_config, p_game, p_rom);
	encode_mattock_animations(p_config, p_game, p_rom);
	encode_push_block(p_config, p_game, p_rom);
	encode_jump_on_tiles(p_config, p_game, p_rom);
	encode_scene_data(p_config, p_game, p_rom);
	encode_palette_to_music(p_config, p_game, p_rom);
	encode_fog_data(p_config, p_game, p_rom);

	// TODO: The title screen uses one of the "world palettes" - so make sure this is patched last
	// can we handle this more elegantly?
	p_game.m_gfx_manager.patch_rom(p_rom);
}

void fe::ROM_Manager::encode_chr_data(const fe::Config& p_config,
	const fe::Game& p_game, std::vector<byte>& p_rom) const {

	const std::size_t tileset_count{ p_config.constant(c::ID_WORLD_TILESET_COUNT) };

	// drop all chr tiles in the tileset chr bank
	std::size_t l_chr_wtile_offset{ p_config.constant(c::ID_CHR_WORLD_TILE_OFFSET) };

	std::size_t l_chr_offset{ 0 };
	for (const auto& tileset : p_game.m_tilesets) {
		if (tileset.tiles.size() % 16 != 0 || tileset.start_idx % 16 != 0)
			throw std::runtime_error(std::format("Tileset has chr-tile count of {} and ppu start index {} - but both must be divisible by 16 to fit on ppu pages", tileset.tiles.size(), tileset.start_idx));
		else if (tileset.start_idx + tileset.tiles.size() > 256)
			throw std::runtime_error(std::format("Tileset has chr-tile count of {} and ppu start index {} - but last index must be < 256", tileset.tiles.size(), tileset.start_idx));

		for (const auto& tile : tileset.tiles) {
			if (l_chr_offset >= 0x4000)
				throw std::runtime_error("Chr-tiles across world tilesets exceeds bank size");
			patch_bytes(tile.to_bytes(), p_rom, l_chr_wtile_offset + l_chr_offset);
			l_chr_offset += 16;
		}
	}

	// next patch the ptr table - we know where each ptr should point
	std::size_t l_tileset_to_addr{ p_config.constant(c::ID_WORLD_TILESET_TO_ADDR_OFFSET) };
	// char bank is loaded into cpu mem 0x8000
	std::size_t l_rel_addr{ 0x8000 };

	for (std::size_t i{ 0 }; i < tileset_count; ++i) {
		p_rom.at(l_tileset_to_addr + 2 * i) = static_cast<byte>(l_rel_addr % 256);
		p_rom.at(l_tileset_to_addr + 2 * i + 1) = static_cast<byte>(l_rel_addr / 256);
		l_rel_addr += 16 * p_game.m_tilesets[i].tiles.size();
	}

	// next patch the upper ppu address for each tileset, this depends on the ppu start index
	std::size_t ppu_addr_offset{ l_tileset_to_addr + 2 * tileset_count };
	for (std::size_t i{ 0 }; i < tileset_count; ++i) {
		p_rom.at(ppu_addr_offset + i) = static_cast<byte>(
			0x10 + (p_game.m_tilesets.at(i).start_idx / 0x10)
			);
	}

	// next patch the number of ppu pages for each tileset, this depends on the chr tile count
	std::size_t ppu_numpage_offset{ l_tileset_to_addr + 3 * tileset_count };
	for (std::size_t i{ 0 }; i < tileset_count; ++i) {
		p_rom.at(ppu_numpage_offset + i) = static_cast<byte>(
			p_game.m_tilesets.at(i).tiles.size() / 0x10
			);
	}
}

// patch ROM in place for the palette data
void fe::ROM_Manager::encode_palette_data(const fe::Config& p_config,
	const fe::Game& p_game, std::vector<byte>& p_rom) const {
	std::size_t l_palette_offset{ p_config.constant(c::ID_PALETTE_OFFSET) };
	for (const auto& palette : p_game.m_palettes) {
		patch_bytes(palette, p_rom, l_palette_offset);
		l_palette_offset += 16;
	}

	// we are now at the palette to hud idx offset
	const auto& hud{ p_game.m_hud_attributes };
	for (std::size_t i{ 0 }; i < hud.m_palette_to_hud_idx.size(); ++i)
		p_rom.at(l_palette_offset + i) = hud.m_palette_to_hud_idx[i];

	// and finally patch the hud attributes themselves
	std::size_t l_hud_attr_offset{ p_config.constant(c::ID_HUD_ATTRIBUTE_LOOKUP_OFFSET) };
	for (std::size_t i{ 0 }; i < hud.m_hud_attributes.size(); ++i)
		p_rom.at(l_hud_attr_offset + i) = hud.m_hud_attributes[i].to_byte();
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
	std::size_t l_bscene_start{ p_config.constant(c::ID_BUILDING_SCENE_OFFSET) };
	const auto& bscenes{ p_game.m_building_scenes };

	for (std::size_t i{ 0 }; i < bscenes.size(); ++i) {
		p_rom.at(l_bscene_start + i) = static_cast<byte>(bscenes[i].m_music);
		p_rom.at(l_bscene_start + c::WORLD_BUILDINGS_SCREEN_COUNT + i) = static_cast<byte>(bscenes[i].m_palette);
		p_rom.at(l_bscene_start + 2 * c::WORLD_BUILDINGS_SCREEN_COUNT + i) = static_cast<byte>(bscenes[i].m_tileset);
		p_rom.at(l_bscene_start + 3 * c::WORLD_BUILDINGS_SCREEN_COUNT + i) = bscenes[i].get_pos_as_byte();
	}

	// patch scenes for each world
	std::size_t l_wscene_start{ p_config.constant(c::ID_WORLD_SCENE_OFFSET) };
	const auto& wscenes{ p_game.m_chunks };

	for (std::size_t i{ 0 }; i < wscenes.size(); ++i) {
		p_rom.at(l_wscene_start + i) = static_cast<byte>(wscenes[i].m_scene.m_tileset);
		p_rom.at(l_wscene_start + 8 + i) = static_cast<byte>(wscenes[i].m_scene.m_palette);
		p_rom.at(l_wscene_start + 16 + i) = wscenes[i].m_scene.get_pos_as_byte();
		p_rom.at(l_wscene_start + 24 + i) = static_cast<byte>(wscenes[i].m_scene.m_music);
	}
}

void fe::ROM_Manager::encode_fog_data(const fe::Config& p_config, const fe::Game& p_game, std::vector<byte>& p_rom) const {
	p_rom.at(p_config.constant(c::ID_FOG_WORLD_OFFSET)) = p_game.m_fog.m_world_no;
	p_rom.at(p_config.constant(c::ID_FOG_PALETTE_OFFSET)) = p_game.m_fog.m_palette_no;
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

std::size_t fe::ROM_Manager::get_music_count(const fe::Config& p_config, const std::vector<byte>& p_rom) const {
	auto musicptr{ p_config.pointer(c::ID_MUSIC_PTR) };
	std::size_t result{ 0 };
	std::size_t lowest_target{ 0x10000 };

	for (std::size_t i{ 0 }; ; i += 2) {
		std::size_t next_ptr_addr{ (musicptr.first - musicptr.second) + (i + 2) };
		if (next_ptr_addr > lowest_target + 1)
			break;

		++result;
		std::size_t ptr_addr{ static_cast<std::size_t>(p_rom.at(musicptr.first + i)) +
			256 * static_cast<std::size_t>(p_rom.at(musicptr.first + i + 1)) };
		lowest_target = std::min(lowest_target, ptr_addr);
	}

	return result / 4;
}
