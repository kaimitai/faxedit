#ifndef FE_SPRITE_CONTANTS_H
#define FE_SPRITE_CONTANTS_H

namespace fe {

	namespace c {


		// portrait ptrs
		constexpr char ID_GFX_PORTRAIT_LOOKUP_TABLE_PTR[]{ "gfx_portrait_tile_index_table_ptr" };
		constexpr char ID_GFX_PORTRAIT_CHR_PTR[]{ "gfx_portrait_chr_data_ptr" };
		constexpr char ID_GFX_PORTRAIT_ANIM_FRAME_PTR[]{ "gfx_portrait_anim_frame_ptr" };

		// player ptrs
		constexpr char ID_GFX_PLAYER_LOOKUP_TABLE_PTR[]{ "gfx_player_tile_index_table_ptr" };
		constexpr char ID_GFX_PLAYER_CHR_PTR[]{ "gfx_player_chr_data_ptr" };
		constexpr char ID_GFX_PLAYER_ANIM_FRAME_PTR[]{ "gfx_player_anim_frame_ptr" };

		// portrait constants
		constexpr char ID_GFX_PORTRAIT_COUNT[]{ "gfx_portrait_count" };
		constexpr char ID_GFX_PORTRAIT_CHR_TILE_COUNT[]{ "gfx_portrait_chr_tile_count" };
		constexpr char ID_GFX_PORTRAIT_TOTAL_FRAME_COUNT[]{ "gfx_portrait_total_frame_count" };

		// player constants
		constexpr char ID_GFX_PLAYER_COUNT[]{ "gfx_player_count" };
		constexpr char ID_GFX_PLAYER_TILE_COUNT_OFFSET[]{ "gfx_player_tile_count_offset" };

	}

}

#endif
