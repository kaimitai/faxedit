#ifndef FE_SPRITEGFXMANAGER_H
#define FE_SPRITEGFXMANAGER_H

#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "SpriteAnimationFrame.h"
#include "SpriteFrameCollection.h"
#include "./../../common/klib/NES_tile.h"

using byte = unsigned char;
using ChrBank = std::vector<klib::NES_tile>;

namespace fe {

	class Config;
	class ROM_Manager;

	struct ChrBankImpact {
		std::vector<std::size_t> frame_indexes;
		std::set<std::size_t> chr_bank_indexes;
		bool banks_identical;
	};

	struct SpriteGfxPatchResult {
		bool success;
		std::optional<std::size_t> bank6_used, bank6_sprite_cutoff,
			bank7_sprite_chr_used,
			bank7_npc_anim_frame_used,
			bank7_player_anim_frame_used,
			bank7_portrait_anim_frame_used,
			bank7_used,
			bank8_player_load_lists,
			bank8_weapons_load_lists,
			bank8_player_chr,
			bank8_weapons_chr,
			bank8_common_chr,
			bank8_shield_chr,
			bank8_shield_load_lists,
			bank8_portrait_load_lists,
			bank8_portrait_chr,
			bank8_used;
	};

	struct SpriteGfxManager {

		SpriteFrameCollection c_portraits, c_player, c_npcs;
		std::vector<std::vector<byte>> m_shield_load_lists;
		std::vector<std::size_t> npc_start_frames;
		std::vector<std::size_t> npc_frame_counts;
		std::set<std::size_t> npc_using_common_gfx;

		void calculate_npc_chr_bank_mappings(void);
		void calculate_player_chr_bank_mappings(void);
		void calculate_portrait_chr_bank_mappings(void);
		void calculate_all_chr_bank_mappings(void);

		// logic functions for modifications
		ChrBankImpact analyze_bank_impact(const SpriteFrameCollection& coll, std::size_t bank_idx) const;

		// calculating functions
		std::size_t get_sprite_chr_cutoff(const fe::Config& p_config, const std::vector<byte>& p_rom) const;
		void canonsort_everything(void);
		void canonsort_gfx_collection_chr_bank(SpriteFrameCollection& coll, std::size_t p_bank_idx);
		void canonicalize_gfx_collection_bank(SpriteFrameCollection& coll, std::size_t bank_idx);
		void dedup_gfx_collection_bank(SpriteFrameCollection& coll, std::size_t bank_idx);
		void sort_gfx_collection_bank(SpriteFrameCollection& coll, std::size_t bank_idx);

		int normalize_frame(const std::vector<byte>& ppu_order, fe::SpriteAnimationFrame& frame);

		// animation frame helpers
		std::vector<fe::SpriteAnimationFrame> extract_animation_frames(const std::vector<byte>& p_rom,
			const fe::ROM_Manager& p_rom_mgr, std::size_t p_ptr_table_rom_offset, std::size_t p_zero_addr_rom_offset) const;
		std::optional<std::size_t> pack_animation_frame_data(std::size_t p_ptr_table_rom_offset,
			std::size_t p_zero_addr_rom_offset, std::vector<byte>& p_rom,
			const std::vector<std::vector<byte>>& frames) const;
		std::optional<std::size_t> pack_player_frame_data(std::size_t p_ptr_table_rom_offset,
			std::size_t p_zero_addr_rom_offset, std::vector<byte>& p_rom,
			const std::vector<std::vector<byte>>& p_player_load_lists,
			const std::vector<std::vector<byte>>& p_weapons_load_lists,
			const std::vector<std::vector<byte>>& p_shields_load_lists) const;
		std::optional<std::size_t> pack_portrait_frame_data(std::size_t p_ptr_table_rom_offset,
			std::size_t p_zero_addr_rom_offset, std::vector<byte>& p_rom,
			const std::vector<std::vector<byte>>& p_portrait_load_lists) const;

		// load list helpers
		std::vector<byte> extract_load_list(const std::vector<byte>& p_rom,
			std::size_t p_ptr_table_rom_offset, std::size_t p_tile_count) const;
		std::vector<std::vector<byte>> calc_portrait_load_lists(void) const;
		std::vector<std::vector<byte>> calc_player_load_lists(void) const;
		std::vector<std::vector<byte>> calc_weapons_load_lists(void) const;
		std::vector<byte> calc_load_list(const fe::SpriteFrameCollection& p_coll,
			std::set<std::size_t> frame_indexes,
			bool add_ff = false) const;

		// chr data helpers
		std::optional<std::size_t> pack_npc_chr_data(std::size_t p_ptr_table_rom_offset,
			std::size_t p_zero_addr_rom_offset, std::vector<byte>& p_rom,
			std::size_t p_sprite_id_start, std::size_t p_sprite_id_end,
			const std::set<byte>& p_ignore_sprite_idxs) const;
		std::vector<std::vector<byte>> get_sprite_chr_banks_as_bytes(std::size_t p_sprite_id_start,
			std::size_t p_sprite_id_end, const std::set<byte>& p_ignore_sprite_idxs) const;
		std::vector<byte> flatten_chr_bank(const std::vector<klib::NES_tile>& p_tiles) const;
		std::vector<byte> flatten_common_chr_bank(void) const;
		std::vector<klib::NES_tile> extract_chr_tiles(const std::vector<byte>& p_rom,
			std::size_t p_offset, std::size_t p_count) const;
		std::vector<klib::NES_tile> extract_chr_tiles(const std::vector<byte>& p_rom,
			std::size_t p_offset, const std::set<std::size_t>& p_all_master_ptr_dests) const;

	public:
		SpriteGfxManager(void) = default;

		void load_rom(const fe::Config& p_config, const std::vector<byte>& p_rom,
			const fe::ROM_Manager& p_rom_mgr);
		fe::SpriteGfxPatchResult patch_rom(const fe::Config& p_config, std::vector<byte>& p_rom,
			const fe::ROM_Manager& p_rom_mgr);
	};

}

#endif
