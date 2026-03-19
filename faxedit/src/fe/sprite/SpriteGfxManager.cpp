#include "SpriteGfxManager.h"
#include "fe_sprite_constants.h"
#include "./../Config.h"
#include "./../ROM_Manager.h"
#include <algorithm>
#include <map>
#include <format>
#include <set>
#include <string>
#include "./../../common/klib/Kfile.h"

void fe::SpriteGfxManager::load_rom(const fe::Config& p_config, const std::vector<byte>& p_rom,
	const fe::ROM_Manager& p_rom_mgr) {

	// set up constants from config
	std::size_t SPRITE_COUNT{ p_config.constant(c::ID_SPRITE_COUNT) };

	// sprite indexes using the common ppu 
	const auto commongfxsprites{ p_config.vset_as_set(c::ID_ABSOLUTE_PPU_IDX_SPRITES) };
	for (byte b : commongfxsprites)
		npc_using_common_gfx.insert(static_cast<std::size_t>(b));

	// frame counts per sprite
	const auto npcframecounts{ p_config.bmap_numeric(c::ID_GFX_NPC_FRAME_COUNT) };
	for (std::size_t i{ 0 }; i < SPRITE_COUNT; ++i)
		if (npcframecounts.find(static_cast<byte>(i)) != end(npcframecounts))
			npc_frame_counts.push_back(npcframecounts.at(static_cast<byte>(i)));
		else
			npc_frame_counts.push_back(1);

	// if the data was loaded from elsewhere (xml) we return after populating the frame to chr-banks mappings
	if (!npc_start_frames.empty()) {
		calculate_all_chr_bank_mappings();
		return;
	}

	// start frame idx per sprite
	const auto npcstartframes{ p_rom_mgr.read_bytes(p_rom,
		p_config.constant(c::ID_GFX_NPC_ANIM_IDX_OFFSET),
		SPRITE_COUNT) };
	for (byte b : npcstartframes)
		npc_start_frames.push_back(static_cast<std::size_t>(b));

	// get the npc sprite banks from bank 6
	const auto SPRITE_CUTOFF{ get_sprite_chr_cutoff(p_config, p_rom) };
	const auto ppu_tile_counts{ p_rom_mgr.read_bytes(p_rom, p_config.constant(c::ID_GFX_NPC_TILE_COUNT_OFFSET), SPRITE_COUNT) };
	const auto bank6_chr_master_ptr{ p_config.pointer(c::ID_GFX_NPC_CHR_BANK6_PTR) };
	std::size_t bank6_chr_ptr_table_start{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, bank6_chr_master_ptr) };

	for (std::size_t i{ 0 }; i < SPRITE_CUTOFF; ++i) {
		if (npc_using_common_gfx.contains(i))
			c_npcs.add_chr_bank({});
		else
			c_npcs.add_chr_bank(extract_chr_tiles(p_rom,
				p_rom_mgr.get_ptr_to_rom_offset(p_rom, bank6_chr_ptr_table_start + 2 * i,
					bank6_chr_master_ptr.second),
				ppu_tile_counts.at(i)));
	}

	// bank 7 - chr-banks for sprites with IDs >= cutoff, and frames of all types
	// 2 bytes: ptr to 
	const auto bank7_chr_master_ptr{ p_config.pointer(c::ID_GFX_NPC_CHR_BANK7_PTR) };
	std::size_t bank7_chr_ptr_table_start{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, bank7_chr_master_ptr) };
	const auto bank7_npc_frame_master_ptr{ p_config.pointer(c::ID_GFX_NPC_ANIM_FRAME_PTR) };
	const auto bank7_player_frame_master_ptr{ p_config.pointer(c::ID_GFX_PLAYER_ANIM_FRAME_PTR) };
	const auto bank7_portrait_frame_master_ptr{ p_config.pointer(c::ID_GFX_PORTRAIT_ANIM_FRAME_PTR) };

	// read in chr-banks for sprites residing in bank 7
	for (std::size_t i{ SPRITE_CUTOFF }; i < SPRITE_COUNT; ++i) {
		if (npc_using_common_gfx.contains(i))
			c_npcs.add_chr_bank({});
		else
			c_npcs.add_chr_bank(extract_chr_tiles(p_rom,
				p_rom_mgr.get_ptr_to_rom_offset(p_rom, bank7_chr_ptr_table_start + 2 * (i - SPRITE_CUTOFF),
					bank7_chr_master_ptr.second),
				ppu_tile_counts.at(i)));
	}

	c_npcs.add_frames(extract_animation_frames(p_rom, p_rom_mgr,
		p_rom_mgr.get_ptr_to_rom_offset(p_rom, bank7_npc_frame_master_ptr), bank7_npc_frame_master_ptr.second));
	c_player.add_frames(extract_animation_frames(p_rom, p_rom_mgr,
		p_rom_mgr.get_ptr_to_rom_offset(p_rom, bank7_player_frame_master_ptr), bank7_player_frame_master_ptr.second));
	c_portraits.add_frames(extract_animation_frames(p_rom, p_rom_mgr,
		p_rom_mgr.get_ptr_to_rom_offset(p_rom, bank7_portrait_frame_master_ptr), bank7_portrait_frame_master_ptr.second));

	// add constant to all frame indexes (given in config, but hard coded in asm for a few frames)
	const auto NPC_LINEAR_TRANSLATE{ p_config.bmap_numeric(c::ID_GFX_NPC_FRAME_IDX_TRANSLATE) };
	for (const auto& kv : NPC_LINEAR_TRANSLATE) {
		std::size_t frame_idx{ npcstartframes.at(kv.first) };
		std::size_t frame_count{ npcframecounts.at(kv.first) };

		for (std::size_t i{ 0 }; i < frame_count; ++i)
			if (frame_idx + i < c_npcs.frames.size())
				normalize_frame(static_cast<byte>(kv.second), c_npcs.frames[frame_idx + i].frame);
	}

	// bank 8 - 8 master ptrs
	// 2 bytes: ptr to player load list ptr tables
	// 2 bytes: ptr to weapons load list ptr tables
	// 2 bytes: ptr to player chr data
	// 2 bytes: ptr to weapons chr data
	// 2 bytes: ptr to common chr data
	// 2 bytes: ptr to shield chr data
	// 2 bytes: ptr to shield load list ptr tables
	// 2 bytes: ptr to portrait load list ptr tables
	// 2 bytes: ptr to portrait chr-data
	const auto bank8_player_ll_ptr{ p_config.pointer(c::ID_GFX_PLAYER_LOOKUP_TABLE_PTR) };
	const auto bank8_weapon_ll_ptr{ p_config.pointer(c::ID_GFX_WEAPON_LOOKUP_TABLE_PTR) };
	const auto bank8_player_chr_ptr{ p_config.pointer(c::ID_GFX_PLAYER_CHR_PTR) };
	const auto bank8_weapon_chr_ptr{ p_config.pointer(c::ID_GFX_WEAPON_CHR_PTR) };
	const auto bank8_common_chr_ptr{ p_config.pointer(c::ID_GFX_COMMON_CHR_PTR) };
	const auto bank8_shield_chr_ptr{ p_config.pointer(c::ID_GFX_SHIELD_CHR_PTR) };
	const auto bank8_shield_ll_ptr{ p_config.pointer(c::ID_GFX_SHIELD_LOOKUP_TABLE_PTR) };
	const auto bank8_portrait_ll_ptr{ p_config.pointer(c::ID_GFX_PORTRAIT_LOOKUP_TABLE_PTR) };
	const auto bank8_portrait_chr_ptr{ p_config.pointer(c::ID_GFX_PORTRAIT_CHR_PTR) };

	const std::size_t bank8_player_ll_offset{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, bank8_player_ll_ptr) };
	const std::size_t bank8_weapon_ll_offset{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, bank8_weapon_ll_ptr) };
	const std::size_t bank8_player_chr_offset{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, bank8_player_chr_ptr) };
	const std::size_t bank8_weapon_chr_offset{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, bank8_weapon_chr_ptr) };
	const std::size_t bank8_common_chr_offset{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, bank8_common_chr_ptr) };
	const std::size_t bank8_shield_chr_offset{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, bank8_shield_chr_ptr) };
	const std::size_t bank8_shield_ll_offset{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, bank8_shield_ll_ptr) };
	const std::size_t bank8_portrait_ll_offset{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, bank8_portrait_ll_ptr) };
	const std::size_t bank8_portrait_chr_offset{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, bank8_portrait_chr_ptr) };

	// let's make a set of all destinations for the master ptrs which we will try to use to deduce chr-bank sizes
	std::set<std::size_t> BANK8_MASTER_PTR_DESTS{
		bank8_player_ll_offset, bank8_weapon_ll_offset, bank8_player_chr_offset, bank8_weapon_chr_offset,
		bank8_common_chr_offset, bank8_shield_chr_offset, bank8_shield_ll_offset, bank8_portrait_ll_offset,
		bank8_portrait_chr_offset
	};

	// sprite chr - many banks (already extracted), plus one common bank we tack on at the end
	c_npcs.add_chr_bank(extract_chr_tiles(p_rom, bank8_common_chr_offset, c::PPU_COMMON_TILE_COUNT), true);

	// player chr - 3 banks: player, weapon, shields
	c_player.add_chr_bank(extract_chr_tiles(p_rom, bank8_player_chr_offset, BANK8_MASTER_PTR_DESTS));
	c_player.add_chr_bank(extract_chr_tiles(p_rom, bank8_weapon_chr_offset, BANK8_MASTER_PTR_DESTS));
	c_player.add_chr_bank(extract_chr_tiles(p_rom, bank8_shield_chr_offset, BANK8_MASTER_PTR_DESTS));

	// portrait chr - 1 bank
	c_portraits.add_chr_bank(extract_chr_tiles(p_rom, bank8_portrait_chr_offset, BANK8_MASTER_PTR_DESTS));

	// finally extract the load lists so we can normalize frames
	const auto PLAYER_LOAD_LIST_SIZES{ p_rom_mgr.read_bytes(p_rom,
		p_config.constant(c::ID_GFX_PLAYER_TILE_COUNT_OFFSET),
		c::PLAYER_TYPE_COUNT) };
	const auto WEAPON_LOAD_LIST_SIZES{ p_rom_mgr.read_bytes(p_rom,
	p_config.constant(c::ID_GFX_WEAPON_TILE_COUNT_OFFSET),
	c::WEAPON_TYPE_COUNT) };

	std::vector<std::vector<byte>> player_load_lists, weapon_load_lists,
		shield_load_lists, portrait_load_lists;

	for (std::size_t i{ 0 }; i < c::PLAYER_FRAME_COUNT; ++i) {
		player_load_lists.push_back(extract_load_list(p_rom,
			p_rom_mgr.get_ptr_to_rom_offset(p_rom, bank8_player_ll_offset + 2 * i, bank8_player_ll_ptr.second),
			static_cast<std::size_t>(PLAYER_LOAD_LIST_SIZES.at(i)) + 1));
	}

	for (std::size_t i{ 0 }; i < c::WEAPON_TYPE_COUNT; ++i) {
		weapon_load_lists.push_back(extract_load_list(p_rom,
			p_rom_mgr.get_ptr_to_rom_offset(p_rom, bank8_weapon_ll_offset + 2 * i, bank8_weapon_ll_ptr.second),
			static_cast<std::size_t>(WEAPON_LOAD_LIST_SIZES.at(i)) + 1));
	}

	for (std::size_t i{ 0 }; i < c::SHIELD_TYPE_COUNT; ++i) {
		shield_load_lists.push_back(extract_load_list(p_rom,
			p_rom_mgr.get_ptr_to_rom_offset(p_rom, bank8_shield_ll_offset + 2 * i, bank8_shield_ll_ptr.second),
			c::SHIELD_PPU_TILE_COUNT + 1));
	}

	for (std::size_t i{ 0 }; i < c_portraits.frames.size() / c::PORTRAIT_FRAME_COUNT; ++i) {
		portrait_load_lists.push_back(extract_load_list(p_rom,
			p_rom_mgr.get_ptr_to_rom_offset(p_rom, bank8_portrait_ll_offset + 2 * i, bank8_portrait_ll_ptr.second),
			255));
	}

	// normalize all player frames relative to the ppu order
	int totalmisses{ 0 };
	for (std::size_t p{ 0 }; p < player_load_lists.size(); ++p)
		for (std::size_t i{ 0 }; i < c::PLAYER_FRAME_COUNT; ++i)
			totalmisses += normalize_frame(player_load_lists[p],
				c_player.frames.at(p * c::PLAYER_FRAME_COUNT + i).frame);

	for (std::size_t i{ 0 }; i < 4; ++i)
		for (std::size_t j{ 0 }; j < 8; ++j) {
			normalize_frame(weapon_load_lists[i], c_player.frames[64 + 8 * i + j].frame);
		}

	// the hand-extend frames come in an unexpected order
	for (std::size_t p{ 0 }; p < 8; ++p)
		totalmisses += normalize_frame(
			player_load_lists[c::ARMOR_ORDER_TO_HAND_EXTEND_ORDER[p]],
			c_player.frames.at(99 + p).frame);

	for (std::size_t p{ 0 }; p < portrait_load_lists.size(); ++p) {
		for (std::size_t i{ 0 }; i < c::PORTRAIT_FRAME_COUNT; ++i) {
			int misses{
				normalize_frame(portrait_load_lists[p], c_portraits.frames.at(p * c::PORTRAIT_FRAME_COUNT + i).frame)
			};
		}
	}

	// extract the player animation state -> shield frame index map
	shield_frame_indexes = p_rom_mgr.read_bytes(p_rom, p_config.constant(c::ID_GFX_SHIELD_FRAME_IDX_OFFSET),
		c::PLAYER_FRAME_COUNT);
	m_shield_load_lists = shield_load_lists;

	calculate_all_chr_bank_mappings();
}

void fe::SpriteGfxManager::calculate_all_chr_bank_mappings(void) {
	calculate_npc_chr_bank_mappings();
	calculate_player_chr_bank_mappings();
	calculate_portrait_chr_bank_mappings();
}

fe::ChrBankImpact fe::SpriteGfxManager::analyze_bank_impact(const SpriteFrameCollection& p_coll,
	std::size_t bank_idx) const {
	std::set<std::size_t> involved_banks{ bank_idx };
	std::vector<std::size_t> involved_frames;

	for (std::size_t i{ 0 }; i < p_coll.frames.size(); ++i) {
		const auto& chrbankidxs{ p_coll.frames[i].chrbanks };
		if (std::find(begin(chrbankidxs), end(chrbankidxs), bank_idx) != end(chrbankidxs)) {
			for (std::size_t bankidx : chrbankidxs)
				involved_banks.insert(bankidx);
			involved_frames.push_back(i);
		}
	}

	const auto& ref = p_coll.banks[*involved_banks.begin()];
	bool all_identical = std::all_of(std::next(involved_banks.begin()), involved_banks.end(),
		[&](auto i) { return p_coll.banks[i] == ref; });

	return fe::ChrBankImpact{
		.frame_indexes = involved_frames,
		.chr_bank_indexes = involved_banks,
		.banks_identical = all_identical
	};
}

// this procedure determines which chr-banks are associated with each frame in the npc pool
void fe::SpriteGfxManager::calculate_npc_chr_bank_mappings(void) {
	auto& frames{ c_npcs.frames };
	const std::size_t SPRITE_COUNT{ npc_start_frames.size() };
	const std::size_t FRAME_COUNT{ frames.size() };
	const std::size_t PPU_BANK_IDX{ SPRITE_COUNT };

	std::vector<std::set<std::size_t>> bank_maps(FRAME_COUNT, std::set<std::size_t>());

	for (std::size_t i{ 0 }; i < SPRITE_COUNT; ++i) {
		if (npc_using_common_gfx.contains(i))
			continue;
		for (std::size_t j{ 0 }; j < npc_frame_counts[i]; ++j) {
			std::size_t frame_no{ npc_start_frames[i] + j };
			if (frame_no < bank_maps.size()) {
				bank_maps[frame_no].insert(i);
			}
		}
	}

	for (std::size_t i{ 0 }; i < frames.size(); ++i)
		if (bank_maps[i].empty())
			frames[i].chrbanks.push_back(PPU_BANK_IDX);
		else
			frames[i].chrbanks = std::vector<std::size_t>(begin(bank_maps[i]), end(bank_maps[i]));
}

void fe::SpriteGfxManager::calculate_player_chr_bank_mappings(void) {
	for (std::size_t i{ 0 }; i < c::PLAYER_TYPE_COUNT * c::PLAYER_FRAME_COUNT; ++i)
		c_player.frames[i].chrbanks = { c::KEY_BANK_ARMOR };
	for (std::size_t i{ c::HAND_EXTEND_FRAME_START }; i < c_player.frames.size(); ++i)
		c_player.frames[i].chrbanks = { c::KEY_BANK_ARMOR };
	for (std::size_t i{ 0 }; i < c::WEAPON_FRAME_COUNT * c::WEAPON_TYPE_COUNT; ++i)
		c_player.frames.at(c::WEAPON_FRAME_START + i).chrbanks = { c::KEY_BANK_WEAPONS };

	for (auto& frame : c_player.frames)
		if (frame.chrbanks.empty())
			frame.chrbanks.push_back(c::KEY_BANK_SHIELDS);
}

void fe::SpriteGfxManager::calculate_portrait_chr_bank_mappings(void) {
	// all portraits use the same chr-bank
	for (std::size_t i{ 0 }; i < c_portraits.frames.size(); ++i)
		c_portraits.frames[i].chrbanks = { c::KEY_BANK_PORTRAITS };
}

std::vector<byte> fe::SpriteGfxManager::extract_load_list(const std::vector<byte>& p_rom,
	std::size_t p_rom_offset, std::size_t p_tile_count) const {
	std::vector<byte> result;

	for (std::size_t i{ 0 }; i < p_tile_count; ++i) {
		byte b{ p_rom.at(p_rom_offset + i) };
		if (b == 0xff)
			break;
		else
			result.push_back(b);
	}

	return result;
}

std::vector<fe::SpriteAnimationFrame> fe::SpriteGfxManager::extract_animation_frames(const std::vector<byte>& p_rom,
	const fe::ROM_Manager& p_rom_mgr, std::size_t p_ptr_table_rom_offset, std::size_t p_zero_addr_rom_offset) const {
	std::vector<fe::SpriteAnimationFrame> result;

	std::size_t frame_count{ p_rom_mgr.get_ptr_table_entry_count(p_rom,
	p_ptr_table_rom_offset, p_zero_addr_rom_offset) };

	for (std::size_t i{ 0 }; i < frame_count; ++i)
		result.push_back(fe::SpriteAnimationFrame(p_rom,
			p_rom_mgr.get_ptr_to_rom_offset(p_rom, p_ptr_table_rom_offset + 2 * i, p_zero_addr_rom_offset)
		));

	return result;
}

std::optional<std::size_t> fe::SpriteGfxManager::pack_animation_frame_data(std::size_t p_ptr_table_rom_offset,
	std::size_t p_zero_addr_rom_offset, std::vector<byte>& p_rom,
	const std::vector<std::vector<byte>>& frames) const {
	fe::GodAllocator allocator;
	return allocator.init_allocate_and_patch_single_table(
		std::make_pair(p_ptr_table_rom_offset, p_zero_addr_rom_offset),
		frames, p_rom);
}

std::optional<std::size_t> fe::SpriteGfxManager::pack_player_frame_data(std::size_t p_ptr_table_rom_offset,
	std::size_t p_zero_addr_rom_offset, std::vector<byte>& p_rom,
	const std::vector<std::vector<byte>>& p_player_load_lists,
	const std::vector<std::vector<byte>>& p_weapons_load_lists,
	const std::vector<std::vector<byte>>& p_shields_load_lists) const {
	std::vector<std::vector<byte>> frame_data;

	// encode the player frames modulo the load lists
	for (std::size_t i{ 0 }; i < c::PLAYER_TYPE_COUNT; ++i)
		for (std::size_t j{ 0 }; j < c::PLAYER_FRAME_COUNT; ++j)
			frame_data.push_back(c_player.frames.at(i * c::PLAYER_FRAME_COUNT + j).frame.to_bytes(
				p_player_load_lists.at(i)
			));

	// encode the weapon frames modulo the load lists
	for (std::size_t i{ 0 }; i < c::WEAPON_TYPE_COUNT; ++i)
		for (std::size_t j{ 0 }; j < c::WEAPON_FRAME_COUNT; ++j)
			frame_data.push_back(c_player.frames.at(c::WEAPON_FRAME_START + i * c::WEAPON_FRAME_COUNT + j).frame.to_bytes(
				p_weapons_load_lists.at(i)
			));

	// encode the shield frames
	// the "empty" shield frame was already encoded as a weapon frame
	// TODO: Generate and use the shield load lists, after making them from virtual frames?
	for (std::size_t i{ 0 }; i < c::SHIELD_FRAME_COUNT - 1; ++i)
		frame_data.push_back(c_player.frames.at(c::SHIELD_FRAME_START + i).frame.to_bytes());;

	// encode the hand-extend frames
	for (std::size_t i{ 0 }; i < c::PLAYER_TYPE_COUNT; ++i)
		frame_data.push_back(c_player.frames.at(c::HAND_EXTEND_FRAME_START + i).frame.to_bytes(
			p_player_load_lists.at(c::ARMOR_ORDER_TO_HAND_EXTEND_ORDER.at(i))
		));
	fe::GodAllocator allocator;
	return allocator.init_allocate_and_patch_single_table(
		std::make_pair(p_ptr_table_rom_offset, p_zero_addr_rom_offset),
		frame_data, p_rom);
}

std::optional<std::size_t> fe::SpriteGfxManager::pack_portrait_frame_data(std::size_t p_ptr_table_rom_offset,
	std::size_t p_zero_addr_rom_offset, std::vector<byte>& p_rom,
	const std::vector<std::vector<byte>>& p_portrait_load_lists) const {
	std::vector<std::vector<byte>> frame_data;

	for (std::size_t i{ 0 }; i < p_portrait_load_lists.size(); ++i) {
		for (std::size_t j{ 0 }; j < c::PORTRAIT_FRAME_COUNT; ++j)
			frame_data.push_back(c_portraits.frames.at(i * c::PORTRAIT_FRAME_COUNT + j).frame.to_bytes(
				p_portrait_load_lists[i]
			));
	}

	fe::GodAllocator allocator;
	return allocator.init_allocate_and_patch_single_table(
		std::make_pair(p_ptr_table_rom_offset, p_zero_addr_rom_offset),
		frame_data, p_rom);
}

// makes frame tile numbers relative to chr bank and not ppu order
// return number of invalid references (for debugging)
int fe::SpriteGfxManager::normalize_frame(const std::vector<byte>& ppu_order,
	fe::SpriteAnimationFrame& frame) {
	int result{ 0 };

	for (std::size_t y{ 0 }; y < frame.tilemap.size(); ++y)
		for (std::size_t x{ 0 }; x < frame.tilemap[y].size(); ++x) {
			auto& tile{ frame.tilemap[y][x] };
			if (tile) {
				if (tile->index < ppu_order.size())
					tile->index = ppu_order[tile->index];
				else {
					tile = std::nullopt;
					++result;
				}
			}
		}

	return result;
}

void fe::SpriteGfxManager::normalize_frame(byte linear_value, fe::SpriteAnimationFrame& frame) {
	for (auto& row : frame.tilemap)
		for (auto& tile : row)
			if (tile)
				tile->index += linear_value;
}

std::vector<klib::NES_tile> fe::SpriteGfxManager::extract_chr_tiles(const std::vector<byte>& p_rom,
	std::size_t p_offset, std::size_t p_count) const {
	std::vector<klib::NES_tile> result;
	for (std::size_t i{ 0 }; i < p_count; ++i)
		result.push_back(klib::NES_tile(p_rom, p_offset + 16 * i));
	return result;
}

// will deduce how many chr-tiles a ptr points to depending on what all other master ptrs in the bank point to
// if can't be deduced - will extract 255 chr-tiles which is the maximum an anim frame can completely reference
std::vector<klib::NES_tile> fe::SpriteGfxManager::extract_chr_tiles(const std::vector<byte>& p_rom,
	std::size_t p_offset, const std::set<std::size_t>& p_all_master_ptr_dests) const {
	auto iter{ std::upper_bound(begin(p_all_master_ptr_dests),
		end(p_all_master_ptr_dests), p_offset) };

	std::size_t l_chr_count{ 255 };
	if (iter != end(p_all_master_ptr_dests))
		l_chr_count = (*iter - p_offset) / 16;

	return extract_chr_tiles(p_rom, p_offset, l_chr_count);
}

std::vector<std::vector<byte>> fe::SpriteGfxManager::calc_portrait_load_lists(void) const {
	std::vector<std::vector<byte>> result;

	for (std::size_t i{ 0 }; i < c_portraits.frames.size(); i += c::PORTRAIT_FRAME_COUNT) {
		std::set<std::size_t> frame_indexes;
		for (std::size_t j{ 0 }; j < c::PORTRAIT_FRAME_COUNT; ++j)
			frame_indexes.insert(i + j);

		result.push_back(calc_load_list(c_portraits, frame_indexes, true));

		// validate load list size
		std::size_t portrait_ppu_tiles{ result.back().size() - 1 };
		if (portrait_ppu_tiles > c::PPU_PORTRAIT_TILE_COUNT)
			throw std::runtime_error(std::format("Portrait {} uses {} tiles, but maximum is {}",
				i / 5, portrait_ppu_tiles, c::PPU_PORTRAIT_TILE_COUNT));
	}

	return result;
}

std::vector<std::vector<byte>> fe::SpriteGfxManager::calc_player_load_lists(std::size_t min_weapon_start,
	std::size_t drasle_start) const {
	std::vector<std::vector<byte>> result;

	for (std::size_t i{ 0 }; i < c::PLAYER_TYPE_COUNT; ++i) {
		std::set<std::size_t> player_frames;

		for (std::size_t j{ 0 }; j < c::PLAYER_FRAME_COUNT; ++j)
			player_frames.insert(i * c::PLAYER_FRAME_COUNT + j);

		// add the arm-extension frame
		player_frames.insert(c::HAND_EXTEND_FRAME_START + c::HAND_EXTEND_ORDER_TO_ARMOR[i]);

		result.push_back(calc_load_list(c_player, player_frames));

		// validate load list size
		std::size_t player_ppu_tiles{ result.back().size() };

		// default max tiles: type < 6 AND we have a shield, then our tile count must stop before
		// the first shield tile
		std::size_t player_max_tiles{ static_cast<std::size_t>(c::PPU_SHIELD_START) };

		// if player type >= 6, we have the dragon slayer and don't need to worry about a shield
		if (i >= 6)
			player_max_tiles = drasle_start;
		else if (i % 2 == 0) {
			player_max_tiles = min_weapon_start; // this is the minimum ppu tile index for all weapons BUT drasle
		}
		// else we have a shield, and then our tile count must be less than shield start, which was our initial value
		if (player_ppu_tiles > player_max_tiles)
			throw std::runtime_error(std::format("Player type {} uses {} tiles, but maximum is {}",
				i, player_ppu_tiles, player_max_tiles));
	}

	return result;
}

std::vector<std::vector<byte>> fe::SpriteGfxManager::calc_weapons_load_lists(const std::vector<std::size_t>& p_weapon_tile_start) const {
	std::vector<std::vector<byte>> result;

	constexpr std::size_t WEAPONS_START_FRAME{ c::PLAYER_TYPE_COUNT * c::PLAYER_FRAME_COUNT };

	for (std::size_t i{ 0 }; i < c::WEAPON_TYPE_COUNT; ++i) {
		std::set<std::size_t> weapon_frames;
		for (std::size_t j{ 0 }; j < c::PLAYER_FRAME_COUNT; ++j)
			weapon_frames.insert(WEAPONS_START_FRAME + i * c::PLAYER_FRAME_COUNT + j);
		result.push_back(calc_load_list(c_player, weapon_frames));

		// validate load list size
		std::size_t weapon_ppu_tiles{ result.back().size() };
		std::size_t weapon_max_tiles{ static_cast<std::size_t>(c::PPU_COMMON_TILE_START) -
			p_weapon_tile_start.at(i) };
		if (weapon_ppu_tiles > weapon_max_tiles)
			throw std::runtime_error(std::format("Weapon {} uses {} tiles, but maximum is {}",
				i, weapon_ppu_tiles, weapon_max_tiles));
	}

	return result;
}

std::vector<byte> fe::SpriteGfxManager::calc_load_list(const fe::SpriteFrameCollection& p_coll,
	std::set<std::size_t> frame_indexes, bool add_ff) const {
	std::map<byte, int> tile_usage;

	for (std::size_t frame_idx : frame_indexes) {
		const auto usagemap{ p_coll.frames.at(frame_idx).frame.get_tile_usage() };
		for (const auto& kv : usagemap)
			tile_usage[kv.first] += kv.second;
	}

	std::vector<byte> result;
	for (const auto& kv : tile_usage)
		result.push_back(kv.first);

	if (add_ff)
		result.push_back(0xff);

	return result;
}

// look for all refs to sprite index cut-off for bank switching when getting chr-tiles - all constants must agree
std::size_t fe::SpriteGfxManager::get_sprite_chr_cutoff(const fe::Config& p_config,
	const std::vector<byte>& p_rom) const {
	std::size_t result{ p_rom.at(p_config.constant(c::ID_NPC_CHR_CUTOFF_REF1)) };

	if (result != p_rom.at(p_config.constant(c::ID_NPC_CHR_CUTOFF_REF2)) ||
		result != p_rom.at(p_config.constant(c::ID_NPC_CHR_CUTOFF_REF3)))
		throw std::runtime_error("Could not unambigously decide npc index cutoff for bank6/7 chr-data");

	return result;
}

fe::SpriteGfxPatchResult fe::SpriteGfxManager::patch_rom(const fe::Config& p_config, std::vector<byte>& p_rom,
	const fe::ROM_Manager& p_rom_mgr) {
	fe::SpriteGfxPatchResult final_result{
		.success = false
	};

	const std::size_t NPC_COUNT{ p_config.constant(c::ID_SPRITE_COUNT) };

	// bank 6: sprite chr-banks
	const auto absolute_idx_sprites{ p_config.vset_as_set(c::ID_ABSOLUTE_PPU_IDX_SPRITES) };
	const auto chr_bank6_master_ptr{ p_config.pointer(c::ID_GFX_NPC_CHR_BANK6_PTR) };
	std::pair<std::size_t, std::size_t> chr_bank6_chr_ptr{
		std::make_pair(chr_bank6_master_ptr.first + 2, chr_bank6_master_ptr.second)
	};

	// patch the master ptr itself to be thorough, although it couldn't reasonably point elsewhere
	p_rom_mgr.patch_ptr(p_rom, chr_bank6_master_ptr.first, 2);

	// try to pack as many contiguous chr-banks as possible from sprite id 0-N
	// store the cutoff index
	for (std::size_t i{ NPC_COUNT }; i != 0; --i) {
		auto cutoffres{ pack_npc_chr_data(chr_bank6_chr_ptr.first, chr_bank6_chr_ptr.second,
			p_rom, 0, i, absolute_idx_sprites) };
		if (cutoffres) {
			final_result.bank6_sprite_cutoff = i;
			final_result.bank6_used = cutoffres.value() - chr_bank6_master_ptr.first;
			break;
		}
	}

	// probably can never happen
	if (!final_result.bank6_sprite_cutoff)
		return final_result;

	// patch all references to the cutoff index
	std::size_t npc_chr_bank6_count{ final_result.bank6_sprite_cutoff.value() };
	p_rom.at(p_config.constant(c::ID_NPC_CHR_CUTOFF_REF1)) = static_cast<byte>(npc_chr_bank6_count);
	p_rom.at(p_config.constant(c::ID_NPC_CHR_CUTOFF_REF2)) = static_cast<byte>(npc_chr_bank6_count);
	p_rom.at(p_config.constant(c::ID_NPC_CHR_CUTOFF_REF3)) = static_cast<byte>(npc_chr_bank6_count);

	/* BANK 7 */

	// bank 7 sprite chr-data
	const auto chr_bank7_master_ptr{ p_config.pointer(c::ID_GFX_NPC_CHR_BANK7_PTR) };
	std::pair<std::size_t, std::size_t> chr_bank7_chr_ptr{
		std::make_pair(chr_bank7_master_ptr.first + 0x0c, chr_bank7_master_ptr.second)
	};
	p_rom_mgr.patch_ptr(p_rom, chr_bank7_master_ptr.first, 0x0c);

	// pack the rest of the sprite chr data in bank 7
	auto bank7chr_result{ pack_npc_chr_data(chr_bank7_chr_ptr.first, chr_bank7_chr_ptr.second,
	p_rom, npc_chr_bank6_count, NPC_COUNT, absolute_idx_sprites) };

	// ensure we could actually pack the chr
	if (!bank7chr_result)
		return final_result;
	else
		final_result.bank7_sprite_chr_used = bank7chr_result.value() - chr_bank7_chr_ptr.first;

	// pack the npc anim frames
	const auto bank7_npc_anim_frame_ptr{ p_config.pointer(c::ID_GFX_NPC_ANIM_FRAME_PTR) };
	p_rom_mgr.patch_ptr(p_rom, bank7_npc_anim_frame_ptr.first,
		bank7chr_result.value() - chr_bank7_chr_ptr.second);

	const auto NPC_LINEAR_TRANSLATE{ p_config.bmap_numeric(c::ID_GFX_NPC_FRAME_IDX_TRANSLATE) };
	std::vector<byte> linear_deltas{ std::vector<byte>(c_npcs.frames.size(), 0) };
	for (const auto& kv : NPC_LINEAR_TRANSLATE) {
		std::size_t frame_idx{ npc_start_frames.at(kv.first) };
		std::size_t frame_count{ npc_frame_counts.at(kv.first) };

		for (std::size_t i{ 0 }; i < frame_count; ++i)
			if (frame_idx + i < linear_deltas.size())
				linear_deltas[frame_idx + i] = static_cast<byte>(kv.second);
	}

	std::vector<std::vector<byte>> npc_frames;
	for (std::size_t i{ 0 }; i < c_npcs.frames.size(); ++i) {
		const auto& frame{ c_npcs.frames[i].frame };
		byte lindelta{ linear_deltas[i] };
		if (lindelta == 0)
			npc_frames.push_back(frame.to_bytes());
		else
			npc_frames.push_back(frame.to_bytes(lindelta));
	}

	auto bank7npcframe_result{
		pack_animation_frame_data(bank7chr_result.value(),
			chr_bank7_master_ptr.second, p_rom, npc_frames)
	};

	if (!bank7npcframe_result)
		return final_result;
	else
		final_result.bank7_npc_anim_frame_used = bank7npcframe_result.value() - bank7chr_result.value();

	// pack the player anim frames
	const auto bank7_player_anim_frame_ptr{ p_config.pointer(c::ID_GFX_PLAYER_ANIM_FRAME_PTR) };
	p_rom_mgr.patch_ptr(p_rom, bank7_player_anim_frame_ptr.first,
		bank7npcframe_result.value() - chr_bank7_chr_ptr.second);

	// we need to find out the ppu index of the weapon tiles to calculate our player tile count limits
	const std::size_t ROM_OFFSET_WEAPON_TILE_PPU_ADDR_OFFSET{
		p_config.constant(c::ID_GFX_WEAPON_TILE_COUNT_OFFSET) + c::WEAPON_TYPE_COUNT
	};

	std::vector<std::size_t> wep_ppu_start;
	for (std::size_t i{ 0 }; i < c::WEAPON_TYPE_COUNT; ++i)
		wep_ppu_start.push_back(
			(static_cast<std::size_t>(p_rom.at(ROM_OFFSET_WEAPON_TILE_PPU_ADDR_OFFSET + 2 * i)) +
				256 * static_cast<std::size_t>(p_rom.at(ROM_OFFSET_WEAPON_TILE_PPU_ADDR_OFFSET + 2 * i + 1))) / 0x10);

	std::size_t min_wep_start_xcept_drasle{
		*std::min_element(begin(wep_ppu_start), begin(wep_ppu_start) + c::WEAPON_TYPE_COUNT - 1)
	};

	const auto weapon_load_lists{ calc_weapons_load_lists(wep_ppu_start) };
	const auto player_load_lists{ calc_player_load_lists(min_wep_start_xcept_drasle, wep_ppu_start.back()) };
	const auto shield_load_lists{ m_shield_load_lists };

	auto bank7playerframe_result{
	pack_player_frame_data(bank7npcframe_result.value(),
		chr_bank7_master_ptr.second, p_rom, player_load_lists, weapon_load_lists, shield_load_lists)
	};

	if (!bank7playerframe_result)
		return final_result;
	else
		final_result.bank7_player_anim_frame_used = bank7playerframe_result.value() - bank7npcframe_result.value();

	// pack the portrait anim frames
	const auto bank7_portrait_anim_frame_ptr{ p_config.pointer(c::ID_GFX_PORTRAIT_ANIM_FRAME_PTR) };
	p_rom_mgr.patch_ptr(p_rom, bank7_portrait_anim_frame_ptr.first,
		bank7playerframe_result.value() - chr_bank7_chr_ptr.second);

	const auto portrait_load_lists{ calc_portrait_load_lists() };
	auto bank7portraitframe_result{
		pack_portrait_frame_data(bank7playerframe_result.value(),
	chr_bank7_master_ptr.second, p_rom, portrait_load_lists)
	};

	if (!bank7portraitframe_result)
		return final_result;
	else
		final_result.bank7_portrait_anim_frame_used = bank7portraitframe_result.value() - bank7playerframe_result.value();

	final_result.bank7_used = bank7portraitframe_result.value() - chr_bank7_master_ptr.first;

	/* BANK 8 */

	const auto PLAYER_LOAD_LIST_PTR{ p_config.pointer(c::ID_GFX_PLAYER_LOOKUP_TABLE_PTR) };
	const auto WEAPONS_LOAD_LIST_PTR{ p_config.pointer(c::ID_GFX_WEAPON_LOOKUP_TABLE_PTR) };
	const auto PLAYER_CHR_PTR{ p_config.pointer(c::ID_GFX_PLAYER_CHR_PTR) };
	const auto WEAPONS_CHR_PTR{ p_config.pointer(c::ID_GFX_WEAPON_CHR_PTR) };
	const auto COMMON_CHR_PTR{ p_config.pointer(c::ID_GFX_COMMON_CHR_PTR) };
	const auto SHIELDS_CHR_PTR{ p_config.pointer(c::ID_GFX_SHIELD_CHR_PTR) };
	const auto SHIELDS_LOAD_LIST_PTR{ p_config.pointer(c::ID_GFX_SHIELD_LOOKUP_TABLE_PTR) };
	const auto PORTRAIT_LOAD_LISTS_PTR{ p_config.pointer(c::ID_GFX_PORTRAIT_LOOKUP_TABLE_PTR) };
	const auto PORTRAIT_CHR_PTR{ p_config.pointer(c::ID_GFX_PORTRAIT_CHR_PTR) };

	// we start packing data after the last master ptr
	fe::GodAllocator allocator;
	std::size_t l_rom_cursor{ PORTRAIT_CHR_PTR.first + 2 };

	// player load lists
	p_rom_mgr.patch_ptr(p_rom, PLAYER_LOAD_LIST_PTR.first, l_rom_cursor - PLAYER_LOAD_LIST_PTR.second);
	const auto player_ll_result{ allocator.init_allocate_and_patch_single_table(l_rom_cursor,
		PLAYER_LOAD_LIST_PTR.second,
		player_load_lists, p_rom) };
	if (!player_ll_result)
		return final_result;
	else
		final_result.bank8_player_load_lists = player_ll_result.value() - l_rom_cursor;

	// weapon load lists
	p_rom_mgr.patch_ptr(p_rom, WEAPONS_LOAD_LIST_PTR.first, player_ll_result.value() - WEAPONS_LOAD_LIST_PTR.second);
	const auto weapons_ll_result{ allocator.init_allocate_and_patch_single_table(player_ll_result.value(),
		WEAPONS_LOAD_LIST_PTR.second,
		weapon_load_lists, p_rom) };
	if (!weapons_ll_result)
		return final_result;
	else
		final_result.bank8_weapons_load_lists = weapons_ll_result.value() - player_ll_result.value();

	// player chr-tiles
	p_rom_mgr.patch_ptr(p_rom, PLAYER_CHR_PTR.first, weapons_ll_result.value() - PLAYER_CHR_PTR.second);
	auto chr_bank_bytes{ flatten_chr_bank(c_player.banks.at(0)) };
	p_rom_mgr.patch_bytes(chr_bank_bytes, p_rom, weapons_ll_result.value());
	l_rom_cursor = weapons_ll_result.value() + chr_bank_bytes.size();
	final_result.bank8_player_chr = l_rom_cursor - weapons_ll_result.value();

	// weapons chr-tiles
	p_rom_mgr.patch_ptr(p_rom, WEAPONS_CHR_PTR.first, l_rom_cursor - WEAPONS_CHR_PTR.second);
	chr_bank_bytes = flatten_chr_bank(c_player.banks.at(1));
	p_rom_mgr.patch_bytes(chr_bank_bytes, p_rom, l_rom_cursor);
	l_rom_cursor += chr_bank_bytes.size();
	final_result.bank8_weapons_chr = chr_bank_bytes.size();

	// common chr-tiles
	p_rom_mgr.patch_ptr(p_rom, COMMON_CHR_PTR.first, l_rom_cursor - COMMON_CHR_PTR.second);
	chr_bank_bytes = flatten_chr_bank(c_npcs.get_chr_bank(c_npcs.banks.size() - 1, true));
	p_rom_mgr.patch_bytes(chr_bank_bytes, p_rom, l_rom_cursor);
	l_rom_cursor += chr_bank_bytes.size();
	final_result.bank8_common_chr = chr_bank_bytes.size();

	// shields chr-tiles
	p_rom_mgr.patch_ptr(p_rom, SHIELDS_CHR_PTR.first, l_rom_cursor - SHIELDS_CHR_PTR.second);
	chr_bank_bytes = flatten_chr_bank(c_player.banks.at(2));
	p_rom_mgr.patch_bytes(chr_bank_bytes, p_rom, l_rom_cursor);
	l_rom_cursor += chr_bank_bytes.size();
	final_result.bank8_shield_chr = chr_bank_bytes.size();

	// shields load lists
	p_rom_mgr.patch_ptr(p_rom, SHIELDS_LOAD_LIST_PTR.first, l_rom_cursor - SHIELDS_LOAD_LIST_PTR.second);
	const auto shields_ll_result{ allocator.init_allocate_and_patch_single_table(l_rom_cursor,
		SHIELDS_LOAD_LIST_PTR.second,
		shield_load_lists, p_rom) };
	if (!shields_ll_result)
		return final_result;
	else
		final_result.bank8_shield_load_lists = shields_ll_result.value() - l_rom_cursor;

	// portrait load lists
	p_rom_mgr.patch_ptr(p_rom, PORTRAIT_LOAD_LISTS_PTR.first, shields_ll_result.value() - PORTRAIT_LOAD_LISTS_PTR.second);
	const auto portrait_ll_result{ allocator.init_allocate_and_patch_single_table(shields_ll_result.value(),
		PORTRAIT_LOAD_LISTS_PTR.second,
		portrait_load_lists, p_rom) };
	if (!portrait_ll_result)
		return final_result;
	else
		final_result.bank8_portrait_load_lists = portrait_ll_result.value() - shields_ll_result.value();

	// portrait chr-tiles
	p_rom_mgr.patch_ptr(p_rom, PORTRAIT_CHR_PTR.first, portrait_ll_result.value() - PORTRAIT_CHR_PTR.second);
	chr_bank_bytes = flatten_chr_bank(c_portraits.banks.at(0));
	p_rom_mgr.patch_bytes(chr_bank_bytes, p_rom, portrait_ll_result.value());
	final_result.bank8_portrait_chr = chr_bank_bytes.size();

	final_result.bank8_used = portrait_ll_result.value() + chr_bank_bytes.size() - PLAYER_LOAD_LIST_PTR.first;

	// patch static data relating to load lists and npc first frame indexes
	const std::size_t NPC_TILE_COUNT_ROM_OFFSET{ p_config.constant(c::ID_GFX_NPC_TILE_COUNT_OFFSET) };
	const std::size_t NPC_FIRST_FRAME_ROM_OFFSET{ p_config.constant(c::ID_GFX_NPC_ANIM_IDX_OFFSET) };
	const std::size_t PPU_TILE_COUNT_PLAYER{ p_config.constant(c::ID_GFX_PLAYER_TILE_COUNT_OFFSET) };
	const std::size_t PPU_TILE_COUNT_WEAPON{ p_config.constant(c::ID_GFX_WEAPON_TILE_COUNT_OFFSET) };

	for (std::size_t i{ 0 }; i < NPC_COUNT; ++i) {
		p_rom.at(NPC_FIRST_FRAME_ROM_OFFSET + i) = static_cast<byte>(npc_start_frames.at(i));
		p_rom.at(NPC_TILE_COUNT_ROM_OFFSET + i) = static_cast<byte>(c_npcs.banks.at(i).size());
	}

	for (std::size_t i{ 0 }; i < player_load_lists.size(); ++i)
		p_rom.at(PPU_TILE_COUNT_PLAYER + i) = static_cast<byte>(player_load_lists[i].size() - 1);
	for (std::size_t i{ 0 }; i < weapon_load_lists.size(); ++i)
		p_rom.at(PPU_TILE_COUNT_WEAPON + i) = static_cast<byte>(weapon_load_lists[i].size() - 1);

	p_rom_mgr.patch_bytes(shield_frame_indexes, p_rom, p_config.constant(c::ID_GFX_SHIELD_FRAME_IDX_OFFSET));

	final_result.success = (final_result.bank6_used.value() <= 0x4000 &&
		final_result.bank7_used.value() <= 0x4000 &&
		final_result.bank8_used.value() <= 0x4000);

	// Write 0xff to all space we claim is unused - Begin
	// TODO: Decide if we want to keep this
	if (final_result.success) {
		std::vector<std::size_t> used_sizes{
			final_result.bank6_used.value(),
			final_result.bank7_used.value(),
			final_result.bank8_used.value()
		};

		for (std::size_t i{ 6 }; i <= 8; ++i) {
			std::size_t rom_offset{ i * 0x4000 + 0x10 };
			std::size_t rom_end_offset{ rom_offset + 0x4000 };
			std::size_t start_idx{ rom_offset + used_sizes[i - 6] };
			for (std::size_t bb{ start_idx }; bb < rom_end_offset; ++bb)
				p_rom.at(bb) = 0xff;
		}
	}
	// Write 0xff to all space we claim is unused - End

	return final_result;
}

void fe::SpriteGfxManager::canonsort_gfx_collection_chr_bank(SpriteFrameCollection& coll, std::size_t p_bank_idx) {
	canonicalize_gfx_collection_bank(coll, p_bank_idx);
	dedup_gfx_collection_bank(coll, p_bank_idx);
	sort_gfx_collection_bank(coll, p_bank_idx);
}

void fe::SpriteGfxManager::canonicalize_gfx_collection_bank(fe::SpriteFrameCollection& coll,
	std::size_t bank_idx) {
	const auto impact{ analyze_bank_impact(coll, bank_idx) };

	if (!impact.banks_identical)
		throw std::runtime_error("Different chr-banks are used for the same animation frame(s)");

	auto chrbank{ coll.banks.at(bank_idx) };

	// 1. canonicalize each tile
	std::vector<klib::CanonChoice> choices(chrbank.size());

	for (std::size_t i{ 0 }; i < chrbank.size(); ++i)
		choices[i] = chrbank[i].canonicalize();

	// 2. Apply the reverse action to each tile instance in each frame
	//    (for flips, inverse == itself, so just toggle those bits)
	for (std::size_t frameidx : impact.frame_indexes) {
		for (auto& row : coll.frames.at(frameidx).frame.tilemap) {
			for (auto& cell : row) {
				if (!cell) continue;

				const byte idx = cell->index;
				if (idx >= choices.size())
					throw std::runtime_error("Frame references CHR index out of range during canonicalization");

				const auto& c = choices[idx];
				if (c.h) cell->h_flip = !cell->h_flip;
				if (c.v) cell->v_flip = !cell->v_flip;
			}
		}
	}

	for (std::size_t bankidx : impact.chr_bank_indexes)
		coll.banks.at(bankidx) = chrbank;
}

void fe::SpriteGfxManager::dedup_gfx_collection_bank(SpriteFrameCollection& coll, std::size_t bank_idx) {
	const auto impact{ analyze_bank_impact(coll, bank_idx) };

	if (!impact.banks_identical)
		throw std::runtime_error("Different chr-banks are used for the same animation frame(s)");

	const auto chrbank{ coll.banks.at(bank_idx) };

	const std::size_t N = chrbank.size();

	std::map<klib::NES_tile, std::size_t> tileToNewIndex;
	std::vector<std::size_t> oldToNew(N, 0);
	std::vector<klib::NES_tile> newBank;
	newBank.reserve(N);

	for (std::size_t i = 0; i < N; ++i) {
		const auto& tile = chrbank[i];

		auto it = tileToNewIndex.find(tile);
		if (it == tileToNewIndex.end()) {
			std::size_t newIdx = newBank.size();
			newBank.push_back(tile);
			tileToNewIndex.emplace(tile, newIdx);
			oldToNew[i] = newIdx;
		}
		else {
			oldToNew[i] = it->second;
		}
	}

	// update frame indices
	for (std::size_t frame_idx : impact.frame_indexes) {
		for (auto& row : coll.frames.at(frame_idx).frame.tilemap) {
			for (auto& cell : row) {
				if (!cell) continue;
				cell->index = static_cast<byte>(oldToNew[cell->index]);
			}
		}
	}

	// 0. clear unused tiles
	std::map<byte, int> tileusage;
	for (std::size_t frame_idx : impact.frame_indexes) {
		const auto frameusage{ coll.frames.at(frame_idx).frame.get_tile_usage() };
		for (const auto& kv : frameusage)
			tileusage[kv.first] += kv.second;
	}

	for (std::size_t i{ 0 }; i < newBank.size(); ++i)
		if (!tileusage.contains(static_cast<byte>(i)))
			newBank[i] = klib::NES_tile();

	for (std::size_t bankidx : impact.chr_bank_indexes)
		coll.banks.at(bankidx) = newBank;
}

void fe::SpriteGfxManager::sort_gfx_collection_bank(SpriteFrameCollection& coll, std::size_t bank_idx) {
	const auto impact{ analyze_bank_impact(coll, bank_idx) };

	if (!impact.banks_identical)
		throw std::runtime_error("Different chr-banks are used for the same animation frame(s)");

	const auto chrbank{ coll.banks.at(bank_idx) };

	// 0) Figure out which old tile indices are actually used
	std::vector<bool> used(chrbank.size(), false);
	for (std::size_t frame_idx : impact.frame_indexes) {
		for (const auto& row : coll.frames.at(frame_idx).frame.tilemap) {
			for (const auto& cell : row) {
				if (!cell) continue;

				const byte idx = cell->index;
				if (idx >= used.size())
					throw std::runtime_error("Frame references CHR index out of range during sort.");

				used[idx] = true;
			}
		}
	}

	// 1) Build sort order of existing tiles
	std::vector<std::size_t> order(chrbank.size());
	for (std::size_t i = 0; i < order.size(); ++i)
		order[i] = i;

	// 2) Sort:
	//    used first,
	//    then non-empty before empty,
	//    then descending among non-empty
	std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
		if (used[a] != used[b])
			return used[a] > used[b]; // used first

		const bool ae = chrbank[a].is_empty();
		const bool be = chrbank[b].is_empty();

		if (ae != be)
			return be; // non-empty first

		if (ae && be)
			return false; // both empty: order doesn't matter

		// descending among non-empty tiles
		return chrbank[b] < chrbank[a];
		});

	// 3) Build old->new index mapping only for used tiles
	std::vector<byte> oldToNew2(chrbank.size(), 0);
	std::size_t newIdx = 0;
	for (std::size_t oldIdx : order) {
		if (!used[oldIdx])
			continue;
		oldToNew2[oldIdx] = static_cast<byte>(newIdx++);
	}

	// 4) Remap all frame indices
	for (std::size_t frame_idx : impact.frame_indexes) {
		for (auto& row : coll.frames.at(frame_idx).frame.tilemap) {
			for (auto& cell : row) {
				if (!cell) continue;

				const byte oldIdx = cell->index;
				if (oldIdx >= oldToNew2.size())
					throw std::runtime_error("Frame references CHR index out of range during sort/remap.");

				cell->index = oldToNew2[oldIdx];
			}
		}
	}

	// 5) Rebuild bank in sorted order, keeping only used tiles
	std::vector<klib::NES_tile> newBank2;
	newBank2.reserve(newIdx);

	for (std::size_t oldIdx : order) {
		if (!used[oldIdx])
			continue;
		newBank2.push_back(chrbank[oldIdx]);
	}

	// 6) Write back to all impacted identical banks
	for (std::size_t impacted_bank_idx : impact.chr_bank_indexes)
		coll.banks.at(impacted_bank_idx) = newBank2;
}

std::optional<std::size_t> fe::SpriteGfxManager::pack_npc_chr_data(std::size_t p_ptr_table_rom_offset,
	std::size_t p_zero_addr_rom_offset, std::vector<byte>& p_rom,
	std::size_t p_sprite_id_start, std::size_t p_sprite_id_end,
	const std::set<byte>& p_ignore_sprite_idxs) const {
	fe::GodAllocator allocator;
	return allocator.init_allocate_and_patch_single_table(
		std::make_pair(p_ptr_table_rom_offset, p_zero_addr_rom_offset),
		get_sprite_chr_banks_as_bytes(p_sprite_id_start, p_sprite_id_end, p_ignore_sprite_idxs),
		p_rom);
}

std::vector<std::vector<byte>> fe::SpriteGfxManager::get_sprite_chr_banks_as_bytes(std::size_t p_sprite_id_start,
	std::size_t p_sprite_id_end, const std::set<byte>& p_ignore_sprite_idxs) const {
	std::vector<std::vector<byte>> result;

	for (std::size_t i{ p_sprite_id_start }; i < p_sprite_id_end; ++i)
		result.push_back(flatten_chr_bank(c_npcs.banks.at(i)));

	return result;
}

std::vector<byte> fe::SpriteGfxManager::flatten_chr_bank(const std::vector<klib::NES_tile>& p_tiles) const {
	std::vector<byte> result;

	for (const auto& tile : p_tiles) {
		const auto tilebytes{ tile.to_bytes() };
		result.insert(end(result), begin(tilebytes), end(tilebytes));
	}

	return result;
}

std::vector<byte> fe::SpriteGfxManager::flatten_common_chr_bank(void) const {
	const auto& commonchr{ c_npcs.banks.back() };
	std::vector<klib::NES_tile> out_tiles;

	for (std::size_t i{ 0 }; i < c::PPU_COMMON_TILE_COUNT; ++i)
		out_tiles.push_back(commonchr.at(c::PPU_COMMON_TILE_START + i));

	return flatten_chr_bank(out_tiles);
}
