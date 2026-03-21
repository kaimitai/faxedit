#ifndef FE_SPRITE_CONTANTS_H
#define FE_SPRITE_CONTANTS_H

#include <vector>

using byte = unsigned char;

namespace fe {

	namespace c {

		// this chr-tile is used for zero-hit
		constexpr byte SPRITE_0_HIT_CHR[]{ 0xfc, 0xc6, 0xe6, 0xe6, 0xdc, 0xc0, 0xc0, 0x00, 0xfc,
			0xc6, 0xe6, 0xe6, 0xdc, 0xc0, 0xc0, 0x00 };

		// constants internal to the sprite gfx manager - bank and collection keys
		constexpr std::size_t KEY_COLL_NPCS{ 0 };
		constexpr std::size_t KEY_COLL_PLAYER{ 1 };
		constexpr std::size_t KEY_COLL_PORTRAITS{ 2 };

		constexpr std::size_t KEY_BANK_ARMOR{ 0 };
		constexpr std::size_t KEY_BANK_WEAPONS{ 1 };
		constexpr std::size_t KEY_BANK_SHIELDS{ 2 };
		constexpr std::size_t KEY_BANK_PORTRAITS{ 0 };

		// set of all sprites using absolute ppu idx in their animation frames
		constexpr char ID_ABSOLUTE_PPU_IDX_SPRITES[]{ "common_gfx_sprites" };

		// constants enforced by the game engine
		// player + shield + weapon (TODO: separate out weapon and shield)
		constexpr byte PPU_PLAYER_TILE_START{ 0x00 };
		constexpr byte PPU_PLAYER_TILE_COUNT{ 0x40 }; // includes (optional) shield and weapon
		constexpr byte PPU_SHIELD_START{ 0x30 }; // weapon start is not fixed, must be read from rom
		constexpr byte PPU_COMMON_TILE_START{ PPU_PLAYER_TILE_START + PPU_PLAYER_TILE_COUNT };
		constexpr byte PPU_COMMON_TILE_COUNT{ 0x50 };
		constexpr byte PPU_DYNAMIC_TILE_START{ 0x90 };
		constexpr std::size_t PPU_DYNAMIC_TILE_COUNT{ 0x100 - PPU_DYNAMIC_TILE_START };
		constexpr std::size_t PPU_PORTRAIT_TILE_COUNT{ PPU_DYNAMIC_TILE_COUNT };
		constexpr std::size_t PPU_NPC_TILE_COUNT{ PPU_DYNAMIC_TILE_COUNT };
		constexpr std::size_t SPRITE_0_PPU_IDX{ 0x7f };
		constexpr std::size_t SPRITE_0_PPU_IDX_REL_ZERO{ SPRITE_0_PPU_IDX - PPU_COMMON_TILE_START };

		// immutables
		constexpr std::size_t PLAYER_TYPE_COUNT{ 8 };
		constexpr std::size_t SHIELD_TYPE_COUNT{ 3 }; // excludes the battle helmet
		constexpr std::size_t WEAPON_TYPE_COUNT{ 4 };
		constexpr std::size_t PLAYER_FRAME_COUNT{ 8 };
		constexpr std::size_t WEAPON_FRAME_COUNT{ PLAYER_FRAME_COUNT };
		constexpr std::size_t SHIELD_FRAME_COUNT{ 4 };
		constexpr std::size_t SHIELD_PPU_TILE_COUNT{ 5 };
		constexpr std::size_t PORTRAIT_FRAME_COUNT{ 5 };

		constexpr std::size_t WEAPON_FRAME_START{ PLAYER_TYPE_COUNT * PLAYER_FRAME_COUNT };
		constexpr std::size_t SHIELD_FRAME_START{ WEAPON_FRAME_START + WEAPON_TYPE_COUNT * PLAYER_FRAME_COUNT };
		// shield frames are shared for all shield types, and the empty frame is frame #66 which is also a weapon frame
		constexpr std::size_t HAND_EXTEND_FRAME_START{ SHIELD_FRAME_START + (SHIELD_FRAME_COUNT - 1) };

		// the hand extend frames at the end come in a different order than the player frames
		// armor 0 without shield, armor 1 without shield, ..., armor 0 with shield, armor 1 with shield, ...
		inline const std::vector<std::size_t> ARMOR_ORDER_TO_HAND_EXTEND_ORDER{ 0, 2, 4, 6, 1, 3, 5, 7 };
		inline const std::vector<std::size_t> HAND_EXTEND_ORDER_TO_ARMOR{ 0, 4, 1, 5, 2, 6, 3, 7 };

		// portrait ptrs
		constexpr char ID_GFX_PORTRAIT_LOOKUP_TABLE_PTR[]{ "gfx_portrait_tile_index_table_ptr" };
		constexpr char ID_GFX_PORTRAIT_CHR_PTR[]{ "gfx_portrait_chr_data_ptr" };
		constexpr char ID_GFX_PORTRAIT_ANIM_FRAME_PTR[]{ "gfx_portrait_anim_frame_ptr" };

		// player ptrs
		constexpr char ID_GFX_PLAYER_LOOKUP_TABLE_PTR[]{ "gfx_player_tile_index_table_ptr" };
		constexpr char ID_GFX_PLAYER_CHR_PTR[]{ "gfx_player_chr_data_ptr" };
		constexpr char ID_GFX_WEAPON_LOOKUP_TABLE_PTR[]{ "gfx_weapon_tile_index_table_ptr" };
		constexpr char ID_GFX_WEAPON_CHR_PTR[]{ "gfx_weapon_chr_data_ptr" };
		constexpr char ID_GFX_SHIELD_LOOKUP_TABLE_PTR[]{ "gfx_shield_tile_index_table_ptr" };
		constexpr char ID_GFX_SHIELD_CHR_PTR[]{ "gfx_shield_chr_data_ptr" };
		constexpr char ID_GFX_PLAYER_ANIM_FRAME_PTR[]{ "gfx_player_anim_frame_ptr" };

		// npc ptrs
		// chr-tiles are stored in banks 6 and 7
		constexpr char ID_NPC_CHR_CUTOFF_REF1[]{ "gfx_npc_chr_cutoff_ref1" };
		constexpr char ID_NPC_CHR_CUTOFF_REF2[]{ "gfx_npc_chr_cutoff_ref2" };
		constexpr char ID_NPC_CHR_CUTOFF_REF3[]{ "gfx_npc_chr_cutoff_ref3" };
		constexpr char ID_GFX_NPC_ANIM_FRAME_PTR[]{ "gfx_npc_anim_frame_ptr" };
		constexpr char ID_GFX_NPC_CHR_BANK6_PTR[]{ "gfx_npc_bank6_chr_ptr" };
		constexpr char ID_GFX_NPC_CHR_BANK7_PTR[]{ "gfx_npc_bank7_chr_ptr" };

		// other gfx ptrs
		constexpr char ID_GFX_COMMON_CHR_PTR[]{ "gfx_common_chr_data_ptr" };

		// portrait constants
		constexpr char ID_GFX_PORTRAIT_CHR_TILE_COUNT[]{ "gfx_portrait_chr_tile_count" };

		// player constants
		constexpr char ID_GFX_PLAYER_TILE_COUNT_OFFSET[]{ "gfx_player_tile_count_offset" };
		constexpr char ID_GFX_WEAPON_TILE_COUNT_OFFSET[]{ "gfx_weapon_tile_count_offset" };
		constexpr char ID_GFX_SHIELD_FRAME_IDX_OFFSET[]{ "gfx_shield_frame_idx_offset" };
		constexpr std::size_t SHIELD_LOAD_LIST_COUNT{ 5 };

		// npc constants
		constexpr char ID_GFX_NPC_ANIM_IDX_OFFSET[]{ "gfx_npc_anim_index_offset" };
		constexpr char ID_GFX_NPC_TILE_COUNT_OFFSET[]{ "gfx_npc_ppu_tile_count_offset" };

		// npc maps
		constexpr char ID_GFX_NPC_FRAME_COUNT[]{ "npc_animation_frame_counts" };
		constexpr char ID_GFX_NPC_FRAME_IDX_TRANSLATE[]{ "npc_frame_idx_translate" };
	}

}

#endif
