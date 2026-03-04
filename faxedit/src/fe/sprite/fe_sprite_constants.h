#ifndef FE_SPRITE_CONTANTS_H
#define FE_SPRITE_CONTANTS_H

using byte = unsigned char;

namespace fe {

	namespace c {

		// set of all sprites using absolute ppu idx in their animation frames
		constexpr char ID_ABSOLUTE_PPU_IDX_SPRITES[]{ "common_gfx_sprites" };

		// constants enforced by the game engine
		// player + shield + weapon (TODO: separate out weapon and shield)
		constexpr byte PPU_PLAYER_TILE_START{ 0x00 };
		constexpr byte PPU_PLAYER_TILE_COUNT{ 0x40 };
		constexpr byte PPU_COMMON_TILE_START{ PPU_PLAYER_TILE_START + PPU_PLAYER_TILE_COUNT };
		constexpr byte PPU_COMMON_TILE_COUNT{ 0x50 };
		constexpr byte PPU_DYNAMIC_TILE_START{ 0x90 };
		constexpr std::size_t PPU_DYNAMIC_TILE_COUNT{ 0x100 - PPU_DYNAMIC_TILE_START };

		// portrait ptrs
		constexpr char ID_GFX_PORTRAIT_LOOKUP_TABLE_PTR[]{ "gfx_portrait_tile_index_table_ptr" };
		constexpr char ID_GFX_PORTRAIT_CHR_PTR[]{ "gfx_portrait_chr_data_ptr" };
		constexpr char ID_GFX_PORTRAIT_ANIM_FRAME_PTR[]{ "gfx_portrait_anim_frame_ptr" };

		// player ptrs
		constexpr char ID_GFX_PLAYER_LOOKUP_TABLE_PTR[]{ "gfx_player_tile_index_table_ptr" };
		constexpr char ID_GFX_PLAYER_CHR_PTR[]{ "gfx_player_chr_data_ptr" };
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
		constexpr char ID_GFX_PORTRAIT_FRAME_COUNT[]{ "gfx_portrait_frame_count" };
		constexpr char ID_GFX_PORTRAIT_CHR_TILE_COUNT[]{ "gfx_portrait_chr_tile_count" };
		constexpr char ID_GFX_PORTRAIT_TOTAL_FRAME_COUNT[]{ "gfx_portrait_total_frame_count" };

		// player constants
		constexpr char ID_GFX_PLAYER_COUNT[]{ "gfx_player_count" };
		constexpr char ID_GFX_PLAYER_TILE_COUNT_OFFSET[]{ "gfx_player_tile_count_offset" };

		// npc constants
		constexpr char ID_GFX_NPC_ANIM_IDX_OFFSET[]{ "gfx_npc_anim_index_offset" };
		constexpr char ID_GFX_NPC_TILE_COUNT_OFFSET[]{ "gfx_npc_ppu_tile_count_offset" };

		// npc maps
		constexpr char ID_GFX_NPC_FRAME_COUNT[]{ "npc_animation_frame_counts" };
	}

}

#endif
