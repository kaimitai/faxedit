#ifndef FE_CONSTANTS_H
#define FE_CONSTANTS_H

#include <map>
#include <string>
#include <vector>

using byte = unsigned char;

namespace fe {

	namespace c {

		constexpr char CONFIG_FILE_NAME[]{ "eoe_config.xml" };

		// pointers
		constexpr char ID_METADATA_PTR[]{ "metadata_ptr" };
		constexpr char ID_SPRITE_PTR[]{ "sprite_ptr" };
		constexpr char ID_SAMEWORLD_TRANS_PTR[]{ "sameworld_trans_ptr" };
		constexpr char ID_OTHERWORLD_TRANS_PTR[]{ "otherworld_trans_ptr" };

		// gfx pointers
		constexpr char ID_GFX_PLAYER_LOOKUP_TABLE_PTR[]{ "gfx_player_tile_index_table_ptr" };
		constexpr char ID_GFX_PLAYER_CHR_PTR[]{ "gfx_player_chr_data_ptr" };
		constexpr char ID_GFX_PLAYER_ANIM_FRAME_PTR[]{ "gfx_player_anim_frame_ptr" };

		constexpr char ID_GFX_PORTRAIT_LOOKUP_TABLE_PTR[]{ "gfx_portrait_tile_index_table_ptr" };
		constexpr char ID_GFX_PORTRAIT_CHR_PTR[]{ "gfx_portrait_chr_data_ptr" };
		constexpr char ID_GFX_PORTRAIT_ANIM_FRAME_PTR[]{ "gfx_portrait_anim_frame_ptr" };

		// start and end offsets
		constexpr char ID_METADATA_END[]{ "metadata_end" };
		constexpr char ID_SPRITE_DATA_END[]{ "sprite_data_end" };
		// combined transition data
		constexpr char ID_TRANS_DATA_START[]{ "transition_data_start" };
		constexpr char ID_TRANS_DATA_END[]{ "transition_data_end" };
		// individual transition data; if used, the data start is immediately
		// following each pointer table
		constexpr char ID_SW_TRANS_DATA_END[]{ "sameworld_trans_data_end" };
		constexpr char ID_OW_TRANS_DATA_END[]{ "otherworld_trans_data_end" };

		// constant sizes
		constexpr char ID_WORLD_TILEMAP_MAX_SIZE[]{ "world_tilemap_max_size" };

		// gfx constants
		constexpr char ID_GFX_PLAYER_COUNT[]{ "gfx_player_count" };
		constexpr char ID_GFX_PLAYER_CHR_TILE_COUNT[]{ "gfx_player_chr_tile_count" };
		constexpr char ID_GFX_PLAYER_TOTAL_FRAME_COUNT[]{ "gfx_player_total_frame_count" };

		constexpr char ID_GFX_PORTRAIT_COUNT[]{ "gfx_portrait_count" };
		constexpr char ID_GFX_PORTRAIT_CHR_TILE_COUNT[]{ "gfx_portrait_chr_tile_count" };
		constexpr char ID_GFX_PORTRAIT_TOTAL_FRAME_COUNT[]{ "gfx_portrait_total_frame_count" };
		constexpr char ID_GFX_SPRITE_PAL_OFFSET[]{ "gfx_sprite_palette_offset" };
		constexpr char ID_GFX_BUILDING_SPRITE_PAL_OFFSET[]{ "gfx_building_sprite_palette_offset" };

		// constant offsets
		constexpr char ID_WORLD_TILEMAP_MD[]{ "world_tilemap_metadata" };
		constexpr char ID_PALETTE_OFFSET[]{ "palette_offset" };
		constexpr char ID_DEFAULT_PALETTE_OFFSET[]{ "default_palette_offset" };
		constexpr char ID_SPAWN_LOC_DATA_START[]{ "spawn_loc_data_start" };
		constexpr char ID_STAGE_CONN_OFFSET[]{ "stage_conn_offset" };
		constexpr char ID_STAGE_SCREEN_OFFSET[]{ "stage_screen_offset" };
		constexpr char ID_STAGE_REQ_OFFSET[]{ "stage_req_offset" };
		constexpr char ID_STAGE_TO_WORLD_OFFSET[]{ "stage_to_world_offset" };
		constexpr char ID_GAME_START_POS_OFFSET[]{ "game_start_pos_offset" };
		constexpr char ID_GAME_START_SCREEN_OFFSET[]{ "game_start_screen_offset" };
		constexpr char ID_GAME_START_HP_OFFSET[]{ "game_start_hp_offset" };
		constexpr char ID_MATTOCK_ANIM_OFFSET[]{ "mattock_animations_offset" };
		constexpr char ID_JUMP_ON_ANIM_OFFSET[]{ "jump_on_animation_offset" };
		// path to mascon parameter offsets
		constexpr char ID_PTM_STAGE_NO_OFFSET[]{ "ptm_stage_no_offset" };
		constexpr char ID_PTM_SCREEN_NO_OFFSET[]{ "ptm_screen_no_offset" };
		constexpr char ID_PTM_BLOCK_COUNT_OFFSET[]{ "ptm_block_count_offset" };
		constexpr char ID_PTM_START_POS_OFFSET[]{ "ptm_start_pos_offset" };
		constexpr char ID_PTM_REPLACE_TILE_OFFSET[]{ "ptm_replace_tile_offset" };
		constexpr char ID_PTM_POS_DELTA_OFFSET[]{ "ptm_pos_delta_offset" };
		constexpr char ID_PTM_TILE_NO_OFFSET[]{ "ptm_tile_no_offset" };
		constexpr char ID_PTM_COVER_POS_OFFSET[]{ "ptm_cover_pos_offset" };

		// map IDs
		constexpr char ID_WORLD_LABELS[]{ "world_labels" };
		constexpr char ID_SPRITE_LABELS[]{ "sprite_labels" };
		constexpr char ID_BUILDING_LABELS[]{ "building_labels" };
		constexpr char ID_BLOCK_PROP_LABELS[]{ "block_property_labels" };
		constexpr char ID_DOOR_REQ_LABELS[]{ "door_requirement_labels" };
		constexpr char ID_PALETTE_LABELS[]{ "palette_labels" };
		constexpr char ID_SPECIAL_SPRITE_SET_LABELS[]{ "special_sprite_set_labels" };
		constexpr char ID_CMD_BYTE_LABELS[]{ "command_byte_labels" };
		constexpr char ID_TILEMAP_BANK_OFFSETS[]{ "tilemap_bank_offsets" };
		constexpr char ID_TILEMAP_TO_PREDEFINED_BANK[]{ "world_tilemap_to_predefined_bank" };
		constexpr char ID_BG_CHR_ROM_OFFSETS[]{ "bg_chr_rom_offsets" };
		constexpr char ID_NES_PALETTE[]{ "nes_palette" };

		// counts
		constexpr char ID_SPRITE_COUNT[]{ "sprite_count" };
		constexpr char ID_ISCRIPT_COUNT[]{ "iscript_count" };


		// chunks with special meaning in some contexts
		constexpr std::size_t CHUNK_IDX_TOWNS{ 0x03 };
		constexpr std::size_t CHUNK_IDX_BUILDINGS{ 0x04 };

		// make sure this has the same order in enum fe::DoorType
		// used for door type dropdowns in the guid
		inline const char* LABELS_DOOR_TYPES[]{
			"Same World",
			"Building",
			"Previous Stage",
			"Next Stage"
		};

	}

}

#endif
