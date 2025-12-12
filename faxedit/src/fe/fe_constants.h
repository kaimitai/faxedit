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

		// constant offsets
		constexpr char ID_WORLD_TILEMAP_MD[]{ "world_tilemap_metadata" };
		constexpr char ID_PALETTE_OFFSET[]{ "palette_offset" };
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
		constexpr char ID_PALETTE_TO_MUSIC_OFFSET[]{ "palette_to_music_offset" };
		
		// scene definition start offsets
		constexpr char ID_WORLD_TO_TILESET_OFFSET[]{ "world_to_tileset_offset" };
		constexpr char ID_BUILDING_TO_MUSIC_OFFSET[]{ "building_to_music_offset" };
		// path to mascon parameter offsets
		constexpr char ID_PTM_STAGE_NO_OFFSET[]{ "ptm_stage_no_offset" };
		constexpr char ID_PTM_SCREEN_NO_OFFSET[]{ "ptm_screen_no_offset" };
		constexpr char ID_PTM_BLOCK_COUNT_OFFSET[]{ "ptm_block_count_offset" };
		constexpr char ID_PTM_START_POS_OFFSET[]{ "ptm_start_pos_offset" };
		constexpr char ID_PTM_REPLACE_TILE_OFFSET[]{ "ptm_replace_tile_offset" };
		constexpr char ID_PTM_POS_DELTA_OFFSET[]{ "ptm_pos_delta_offset" };
		constexpr char ID_PTM_TILE_NO_OFFSET[]{ "ptm_tile_no_offset" };
		constexpr char ID_PTM_COVER_POS_OFFSET[]{ "ptm_cover_pos_offset" };

		// gfx definition constants
		constexpr char ID_TITLE_CHR_OFFSET[]{ "title_screen_chr_offset" };
		constexpr char ID_TITLE_CHR_PPU_INDEX[]{ "title_screen_chr_ppu_index" };
		constexpr char ID_TITLE_CHR_PPU_COUNT[]{ "title_screen_chr_ppu_count" };
		constexpr char ID_TITLE_TILEMAP_OFFSET[]{ "title_screen_tilemap_offset" };
		constexpr char ID_TITLE_ATTRIBUTE_OFFSET[]{ "title_screen_attribute_offset" };
		constexpr char ID_TITLE_PALETTE_OFFSET[]{ "title_screen_palette_offset" };
		constexpr char ID_TITLE_TILEMAP_MT_X[]{ "title_screen_mt_x" };
		constexpr char ID_TITLE_TILEMAP_MT_Y[]{ "title_screen_mt_y" };
		constexpr char ID_TITLE_TILEMAP_MT_W[]{ "title_screen_mt_w" };
		constexpr char ID_TITLE_TILEMAP_MT_H[]{ "title_screen_mt_h" };

		constexpr char ID_INTRO_ANIM_CHR_OFFSET[]{ "intro_anim_chr_offset" };
		constexpr char ID_INTRO_ANIM_CHR_PPU_INDEX[]{ "intro_anim_chr_ppu_index" };
		constexpr char ID_INTRO_ANIM_CHR_PPU_COUNT[]{ "intro_anim_chr_ppu_count" };
		constexpr char ID_INTRO_ANIM_TILEMAP_OFFSET[]{ "intro_anim_tilemap_offset" };
		constexpr char ID_INTRO_ANIM_ATTRIBUTE_OFFSET[]{ "intro_anim_attribute_offset" };
		constexpr char ID_INTRO_ANIM_PALETTE_OFFSET[]{ "intro_anim_palette_offset" };
		constexpr char ID_INTRO_ANIM_TILEMAP_MT_X[]{ "intro_anim_mt_x" };
		constexpr char ID_INTRO_ANIM_TILEMAP_MT_Y[]{ "intro_anim_mt_y" };
		constexpr char ID_INTRO_ANIM_TILEMAP_MT_W[]{ "intro_anim_mt_w" };
		constexpr char ID_INTRO_ANIM_TILEMAP_MT_H[]{ "intro_anim_mt_h" };

		constexpr char ID_OUTRO_ANIM_CHR_OFFSET[]{ "outro_anim_chr_offset" };
		constexpr char ID_OUTRO_ANIM_CHR_PPU_INDEX[]{ "outro_anim_chr_ppu_index" };
		constexpr char ID_OUTRO_ANIM_CHR_PPU_COUNT[]{ "outro_anim_chr_ppu_count" };
		constexpr char ID_OUTRO_ANIM_TILEMAP_OFFSET[]{ "outro_anim_tilemap_offset" };
		constexpr char ID_OUTRO_ANIM_ATTRIBUTE_OFFSET[]{ "outro_anim_attribute_offset" };
		constexpr char ID_OUTRO_ANIM_PALETTE_OFFSET[]{ "outro_anim_palette_offset" };
		constexpr char ID_OUTRO_ANIM_TILEMAP_MT_X[]{ "outro_anim_mt_x" };
		constexpr char ID_OUTRO_ANIM_TILEMAP_MT_Y[]{ "outro_anim_mt_y" };
		constexpr char ID_OUTRO_ANIM_TILEMAP_MT_W[]{ "outro_anim_mt_w" };
		constexpr char ID_OUTRO_ANIM_TILEMAP_MT_H[]{ "outro_anim_mt_h" };

		constexpr char ID_ITEM_VSCREEN_CHR_OFFSET[]{ "item_vscreen_chr_offset" };
		constexpr char ID_ITEM_VSCREEN_CHR_PPU_INDEX[]{ "item_vscreen_chr_ppu_index" };
		constexpr char ID_ITEM_VSCREEN_CHR_PPU_COUNT[]{ "item_vscreen_chr_ppu_count" };
		constexpr char ID_ITEM_VSCREEN_TILEMAP_OFFSET[]{ "item_vscreen_tilemap_offset" };
		constexpr char ID_ITEM_VSCREEN_TILEMAP_MT_X[]{ "item_vscreen_mt_x" };
		constexpr char ID_ITEM_VSCREEN_TILEMAP_MT_Y[]{ "item_vscreen_mt_y" };
		constexpr char ID_ITEM_VSCREEN_TILEMAP_MT_W[]{ "item_vscreen_mt_w" };
		constexpr char ID_ITEM_VSCREEN_TILEMAP_MT_H[]{ "item_vscreen_mt_h" };

		// map IDs
		constexpr char ID_WORLD_LABELS[]{ "world_labels" };
		constexpr char ID_SPRITE_LABELS[]{ "sprite_labels" };
		constexpr char ID_BUILDING_LABELS[]{ "building_labels" };
		constexpr char ID_BLOCK_PROP_LABELS[]{ "block_property_labels" };
		constexpr char ID_DOOR_REQ_LABELS[]{ "door_requirement_labels" };
		constexpr char ID_PALETTE_LABELS[]{ "palette_labels" };
		constexpr char ID_TILESET_LABELS[]{ "tileset_labels" };
		constexpr char ID_MUSIC_LABELS[]{ "music_labels" };
		constexpr char ID_SPECIAL_SPRITE_SET_LABELS[]{ "special_sprite_set_labels" };
		constexpr char ID_CMD_BYTE_LABELS[]{ "command_byte_labels" };
		constexpr char ID_TILEMAP_BANK_OFFSETS[]{ "tilemap_bank_offsets" };
		constexpr char ID_TILEMAP_TO_PREDEFINED_BANK[]{ "world_tilemap_to_predefined_bank" };
		constexpr char ID_BG_CHR_ROM_OFFSETS[]{ "bg_chr_rom_offsets" };
		constexpr char ID_NES_PALETTE[]{ "nes_palette" };

		// counts
		constexpr char ID_SPRITE_COUNT[]{ "sprite_count" };
		constexpr char ID_ISCRIPT_COUNT[]{ "iscript_count" };
		constexpr char ID_PALETTE_TO_MUSIC_SLOTS[]{ "palette_to_music_slots" };

		// chr constants
		constexpr char ID_WORLD_TILESET_COUNT[]{ "world_tileset_count" };
		constexpr char ID_CHR_WORLD_TILE_OFFSET[]{ "chr_world_tile_offset" };
		constexpr char ID_CHR_HUD_TILE_OFFSET[]{ "chr_hud_tile_offset" };
		constexpr char ID_WORLD_TILESET_TO_ADDR_OFFSET[]{ "world_tileset_to_addr_offset" };

		// constants not stored in the config xml
		// move them to xml if it becomes necessary
		constexpr std::size_t CHUNK_IDX_TOWNS{ 0x03 };
		constexpr std::size_t CHUNK_IDX_BUILDINGS{ 0x04 };
		constexpr std::size_t WORLD_BUILDINGS_SCREEN_COUNT{ 10 };
		constexpr std::size_t CHR_HUD_TILE_COUNT{ 0x3b };

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
