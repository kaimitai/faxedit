#ifndef FE_XML_CONSTANTS
#define FE_XML_CONSTANTS

namespace fe {

	namespace xml {

		namespace c {

			// TODO: Enable the non-beta xml format
			// Changes:
			//  - tilemap entries are written as "row" instead of legacy "col"
			//  - hex values are written as $nn instead of 0xnn
			//
			// The xml reader remains backward compatible with both formats
			// Enabling this will cause large xml diffs but no data changes
			constexpr bool USE_XML_NON_BETA_VERSION{ false };

			constexpr const char* HEX_PREFIX{ USE_XML_NON_BETA_VERSION ? "$" : "0x" };

			constexpr char TAG_ROOT[]{ "echoes_of_eolis" };
			constexpr char ATTR_ROOT_VERSION[]{ "version" };
			constexpr char COMMENTS_ROOT[]{ "Faxanadu project file created with Echoes of Eolis (https://github.com/kaimitai/faxedit) " };

			constexpr char TAG_SETTINGS_ROOT[]{ "eoe_settings" };
			constexpr char COMMENTS_SETTINGS_ROOT[]{ "Echoes of Eolis settings file (https://github.com/kaimitai/faxedit) " };
			constexpr char TAG_PARAM[]{ "param" };

			constexpr char SETTINGS_PARAM_FRAME_SCALING[]{ "frame_scaling" };
			constexpr char SETTINGS_PARAM_BANK_SCALING[]{ "bank_scaling" };
			constexpr char SETTINGS_PARAM_TRANSP_TOLERANCE[]{ "transparency_tolerance" };
			constexpr char SETTINGS_PARAM_SPRITE_PALETTE[]{ "sprite_palette" };

			constexpr char SETTINGS_PARAM_PATCH_TILEMAPS[]("patch_tilemaps");
			constexpr char SETTINGS_PARAM_PATCH_SPRITE_DATA[]("patch_sprite_data");
			constexpr char SETTINGS_PARAM_PATCH_METADATA[]("patch_metadata");
			constexpr char SETTINGS_PARAM_PATCH_BANK15_DATA[]("patch_bank15_data");
			constexpr char SETTINGS_PARAM_PATCH_WORLD_CHR_DATA[]("patch_world_chr_data");
			constexpr char SETTINGS_PARAM_PATCH_SPRITE_GFX[]{ "patch_sprite_gfx" };
			constexpr char SETTINGS_PARAM_PATCH_CINEMATICS[]{ "patch_cinematics" };
			constexpr char SETTINGS_PARAM_THROW_ON_CINEMATIC_OVERFLOW[]{ "throw_on_cinematic_overflow" };
			constexpr char SETTINGS_PARAM_PATCH_PALETTES[]{ "patch_palettes" };
			constexpr char SETTINGS_PARAM_PATCH_STAGES[]{ "patch_stages" };
			constexpr char SETTINGS_PARAM_PATCH_MATTOCK[]{ "patch_mattock" };
			constexpr char SETTINGS_PARAM_PATCH_PUSH_BLOCK[]{ "patch_push_blocks" };
			constexpr char SETTINGS_PARAM_PATCH_JUMP_ON[]{ "patch_jump_on" };
			constexpr char SETTINGS_PARAM_PATCH_SCENES[]{ "patch_scenes" };
			constexpr char SETTINGS_PARAM_PATCH_FOG[]{ "patch_fog" };
			constexpr char SETTINGS_PARAM_PATCH_BG_GFX[]{ "patch_bg_gfx" };

			constexpr char SETTINGS_PARAM_SHOW_BLD_SPRITE_SETS[]{ "show_building_sprite_sets" };
			constexpr char SETTINGS_PARAM_SHOW_GRID[]{ "show_grid" };
			constexpr char SETTINGS_PARAM_ANIMATE_SPRITES[]{ "animate_sprites" };
			constexpr char SETTINGS_PARAM_ADJACENT_SCREENS[]{ "adjacent_screens" };
			constexpr char SETTINGS_PARAM_IO_BLOCK_PROPERTY[]{ "icon_overlay_block_property" };
			constexpr char SETTINGS_PARAM_IO_MATTOCK[]{ "icon_overlay_mattock" };
			constexpr char SETTINGS_PARAM_IO_DOOR_REQS[]{ "icon_overlay_door_reqs" };
			constexpr char SETTINGS_PARAM_SHOW_DOOR_PADDING[]{ "show_door_padding_byte" };
			constexpr char SETTINGS_PARAM_ENABLE_CONFIG_DUMP[]{ "enable_config_dump" };
			constexpr char SETTINGS_PARAM_CAM_ZOOM_FACTOR[]{ "camera_zoom_factor" };
			constexpr char SETTINGS_PARAM_BORDER_ALPHA[]{ "screen_border_alpha" };
			constexpr char SETTINGS_PARAM_INVERT_ZOOM[]{ "invert_zoom" };
			constexpr char SETTINGS_PARAM_WARN_TILEMAP_95[]{ "warn_tilemap_95_pct" };
			constexpr char SETTINGS_PARAM_WARN_DOOR_DEST_00[]{ "warn_door_dest_00" };

			constexpr char TAG_STAGES[]{ "stages" };
			constexpr char TAG_STAGE[]{ "stage" };
			constexpr char ATTR_NEXT_STAGE[]{ "next_stage" };
			constexpr char ATTR_PREV_STAGE[]{ "prev_stage" };
			constexpr char ATTR_NEXT_SCREEN[]{ "next_screen" };
			constexpr char ATTR_PREV_SCREEN[]{ "prev_screen" };
			constexpr char ATTR_NEXT_REQ[]{ "next_req" };
			constexpr char ATTR_PREV_REQ[]{ "prev_req" };
			constexpr char ATTR_HP[]{ "hp" };

			constexpr char TAG_PALETTES[]{ "palettes" };
			constexpr char TAG_PALETTE[]{ "palette" };

			constexpr char ATTR_HUD_ATTR_INDEX[]{ "hud_attr_index" };

			constexpr char TAG_HUD_ATTRIBUTES[]{ "hud_attributes" };
			constexpr char TAG_HUD_ATTRIBUTE[]{ "hud_attribute" };

			constexpr char TAG_SPAWN_POINTS[]{ "spawn_points" };
			constexpr char TAG_SPAWN_POINT[]{ "spawn_point" };

			constexpr char TAG_NPC_BUNDLES[]{ "building_params" };
			constexpr char TAG_NPC_BUNDLE[]{ "building_param" };

			constexpr char TAG_PALETTE_TO_MUSIC[]{ "palette_to_music" };
			constexpr char TAG_SLOT[]{ "slot" };

			constexpr char TAG_PUSH_BLOCK[]{ "push_block" };
			constexpr char ATTR_BLOCK_COUNT[]{ "block_count" };
			constexpr char ATTR_DELTA_X[]{ "delta_x" };
			constexpr char ATTR_SOURCE_BLOCK0[]{ "source_0" };
			constexpr char ATTR_SOURCE_BLOCK1[]{ "source_1" };
			constexpr char ATTR_TARGET_BLOCK0[]{ "target_0" };
			constexpr char ATTR_TARGET_BLOCK1[]{ "target_1" };
			constexpr char ATTR_DRAW_BLOCK[]{ "draw_block" };
			constexpr char ATTR_COVER_X[]{ "cover_x" };
			constexpr char ATTR_COVER_Y[]{ "cover_y" };

			constexpr char TAG_JUMP_ON_ANIMATION[]{ "jump_on_animation" };
			constexpr char TAG_FOG[]{ "fog" };

			constexpr char TAG_METATILES[]{ "metatiles" };
			constexpr char TAG_METATILE[]{ "metatile" };

			constexpr char ATTR_MATTOCK_ANIMATION[]{ "mattock_animation" };

			constexpr char TAG_BUILDING_SCENES[]{ "building_scenes" };
			constexpr char TAG_SCENE[]{ "scene" };

			constexpr char TAG_TILESETS[]{ "tilesets" };
			constexpr char TAG_TILESET[]{ "tileset" };
			constexpr char TAG_CHR_TILE[]{ "chr_tile" };
			constexpr char ATTR_PPU_START_IDX[]{ "ppu_start_index" };

			constexpr char TAG_GFX_IMAGES[]{ "gfx_images" };
			constexpr char TAG_GFX_IMAGE[]{ "gfx_image" };
			constexpr char TAG_CHR_BANKS[]{ "chr_banks" };
			constexpr char TAG_CHR_BANK[]{ "chr_bank" };
			constexpr char TAG_GFX_ATTRTABLE[]{ "attribute_table" };

			constexpr char TAG_CHUNKS[]{ "worlds" };
			constexpr char TAG_CHUNK[]{ "world" };

			constexpr char ATTR_DEFAULT_PALETTE[]{ "default_palette" };
			constexpr char ATTR_TILESET[]{ "tileset" };
			constexpr char ATTR_MUSIC[]{ "music" };

			constexpr char ATTR_MT_PROPERTY[]{ "property" };
			constexpr char ATTR_MT_PAL_TL[]{ "pal_tl" };
			constexpr char ATTR_MT_PAL_TR[]{ "pal_tr" };
			constexpr char ATTR_MT_PAL_BL[]{ "pal_bl" };
			constexpr char ATTR_MT_PAL_BR[]{ "pal_br" };

			constexpr char ATTR_CHUNK_ID[]{ "world_no" };
			constexpr char ATTR_SCREEN_ID[]{ "screen_no" };
			constexpr char ATTR_STAGE_ID[]{ "stage_no" };
			constexpr char ATTR_REQUIREMENT[]{ "requirement" };

			constexpr char TAG_TILEMAP[]{ "tilemap" };
			constexpr char TAG_ROW[]{ "row" };
			constexpr char TAG_COL[]{ "col" };
			constexpr const char* TAG_TILEMAP_ROW{ USE_XML_NON_BETA_VERSION ?
				TAG_ROW : TAG_COL };

			constexpr char TAG_SCREENS[]{ "screens" };
			constexpr char TAG_SCREEN[]{ "screen" };


			constexpr char TAG_SPRITES[]{ "sprites" };
			constexpr char TAG_SPRITE[]{ "sprite" };

			constexpr char TAG_DOORS[]{ "doors" };
			constexpr char TAG_DOOR[]{ "door" };

			constexpr char ATTR_SCREEN_ID_LEFT[]{ "screen_id_left" };
			constexpr char ATTR_SCREEN_ID_RIGHT[]{ "screen_id_right" };
			constexpr char ATTR_SCREEN_ID_UP[]{ "screen_id_up" };
			constexpr char ATTR_SCREEN_ID_DOWN[]{ "screen_id_down" };

			constexpr char TAG_SCREEN_INTERCHUNK_TRANSTION[]{ "sameworld_transition" };
			constexpr char TAG_SCREEN_INTRACHUNK_TRANSTION[]{ "otherworld_transition" };

			constexpr char ATTR_NO[]{ "no" };
			constexpr char ATTR_ID[]{ "id" };
			constexpr char ATTR_TEXT_ID[]{ "script_index" };
			constexpr char ATTR_X[]{ "x" };
			constexpr char ATTR_Y[]{ "y" };
			constexpr char ATTR_DEST_X[]{ "destination_x" };
			constexpr char ATTR_DEST_Y[]{ "destination_y" };
			constexpr char ATTR_DEST_SCREEN_NO[]{ "destination_screen_no" };
			constexpr char ATTR_DEST_PALETTE[]{ "destination_palette" };
			constexpr char ATTR_DEST_PARAM_ID[]{ "destination_parameter" };
			constexpr char ATTR_UNKNOWN_BYTE[]{ "unknown_byte" };
			constexpr char ATTR_SPRITE_COMM_BYTE[]{ "sprite_command_byte" };

			constexpr char ATTR_BYTES[]{ "bytes" };
			constexpr char ATTR_ROW[]{ "row" };
			constexpr char ATTR_TYPE[]{ "type" };

			constexpr char VAL_DOOR_TYPE_INTERCHUNK[]{ "sameworld" };
			constexpr char VAL_DOOR_TYPE_BUILDING[]{ "building" };
			constexpr char VAL_DOOR_TYPE_NEXTCHUNK[]{ "next_stage" };
			constexpr char VAL_DOOR_TYPE_PREVCHUNK[]{ "previous_stage" };

			// sprite gfx tags and attributes
			constexpr char TAG_SPRITE_GFX[]{ "sprite_gfx" };
			constexpr char TAG_NPC_START_FRAMES[]{ "npc_start_frames" };
			constexpr char TAG_NPC_UPDATE_HANDLERS[]{ "npc_update_handlers" };
			constexpr char TAG_SHIELD_LOAD_LISTS[]{ "shield_load_lists" };
			constexpr char TAG_LOAD_LIST[]{ "load_list" };
			constexpr char TAG_SHIELD_FRAME_INDEXES[]{ "shield_frame_indexes" };
			constexpr char TAG_FRAMES[]{ "frames" };
			constexpr char TAG_FRAME[]{ "frame" };
			constexpr char ATTR_HANDLER[]{ "handler" };
			constexpr char TAG_TILE[]{ "tile" };
			constexpr char TAG_NPC_GFX[]{ "npc_gfx" };
			constexpr char TAG_PLAYER_GFX[]{ "player_gfx" };
			constexpr char TAG_PORTRAIT_GFX[]{ "portrait_gfx" };

			constexpr char ATTR_NPC_FRAME_NO[]{ "frame_no" };
			constexpr char ATTR_OFFSET_X[]{ "x_offset" };
			constexpr char ATTR_OFFSET_Y[]{ "y_offset" };
			constexpr char ATTR_PIVOT_X[]{ "x_pivot" };

			constexpr char ATTR_CHR_INDEX[]{ "idx" };
			constexpr char ATTR_SUB_PAL[]{ "sp" };
			constexpr char ATTR_HFLIP[]{ "h" };
			constexpr char ATTR_VFLIP[]{ "v" };

			// cinematic tags and attributes
			constexpr char TAG_CINEMATIC[]{ "cinematic" };
			constexpr char TAG_SPRITE_PALETTE_INTRO[]{ "sprite_palette_intro" };
			constexpr char TAG_SPRITE_PALETTE_OUTRO[]{ "sprite_palette_outro" };
			constexpr char TAG_SPRITE_CHR_BANK[]{ "sprite_chr_bank" };
			constexpr char TAG_PLAYER_ANIMATIONS[]{ "player_animations" };
			constexpr char TAG_PLAYER_ANIMATION[]{ "player_animation" };
			constexpr char TAG_DEPTH_STAGES[]{ "depth_stages" };
			constexpr char TAG_DEPTH_STAGE[]{ "depth_stage" };
			constexpr char TAG_WATERFALL[]{ "waterfall" };
			constexpr char TAG_RIPPLES[]{ "ripples" };
			constexpr char TAG_RIPPLE[]{ "ripple" };

			constexpr char ATTR_CUTOFF_Y[]{ "cutoff_y" };
			constexpr char ATTR_VELOCITY_X[]{ "velocity_x" };
			constexpr char ATTR_VELOCITY_Y[]{ "velocity_y" };

			// config file tags
			constexpr char TAG_CONFIG_ROOT[]{ "eoe_config" };

			constexpr char TAG_REGIONS[]{ "regions" };
			constexpr char TAG_REGION[]{ "region" };
			constexpr char TAG_SIGNATURE[]{ "signature" };
			constexpr char TAG_CONSTANTS[]{ "consts" };
			constexpr char TAG_CONSTANT[]{ "const" };

			constexpr char TAG_POINTERS[]{ "pointers" };
			constexpr char TAG_POINTER[]{ "pointer" };
			constexpr char TAG_SETS[]{ "sets" };
			constexpr char TAG_SET[]{ "set" };
			constexpr char TAG_BYTE_TO_STR_MAPS[]{ "byte_to_string_maps" };
			constexpr char TAG_BYTE_TO_STR_MAP[]{ "byte_to_string_map" };
			constexpr char TAG_ENTRY[]{ "entry" };
			constexpr char TAG_BOOLS[]{ "bools" };
			constexpr char TAG_BOOL[]{ "bool" };

			constexpr char ATTR_COMPATIBLE_REGIONS[]{ "compatible_regions" };
			constexpr char ATTR_EXACT_MATCH_ONLY[]{ "exact_match_only" };

			constexpr char ATTR_NAME[]{ "name" };
			constexpr char ATTR_FILE_SIZE[]{ "file_size" };
			constexpr char ATTR_OFFSET[]{ "offset" };
			constexpr char ATTR_VALUES[]{ "values" };
			constexpr char ATTR_VALUE[]{ "value" };
			constexpr char ATTR_CONDITION[]{ "condition" };
			constexpr char ATTR_ZERO_ADDR[]{ "zero_addr" };
			constexpr char ATTR_BYTE[]{ "byte" };
			constexpr char ATTR_STRING[]{ "str" };
		}

	}

}

#endif
