#ifndef FE_XML_CONSTANTS
#define FE_XML_CONSTANTS

namespace fe {

	namespace xml {

		namespace c {

			constexpr char TAG_ROOT[]{ "echoes_of_eolis" };
			constexpr char ATTR_ROOT_VERSION[]{ "version" };
			constexpr char VAL_ROOT_VERSION[]{ "beta-0.1" };
			constexpr char COMMENTS_ROOT[]{ "Faxanadu project file created with Echoes of Eolis (https://github.com/kaimitai/faxedit) " };

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

			constexpr char TAG_SPAWN_POINTS[]{ "spawn_points" };
			constexpr char TAG_SPAWN_POINT[]{ "spawn_point" };

			constexpr char TAG_NPC_BUNDLES[]{ "building_params" };
			constexpr char TAG_NPC_BUNDLE[]{ "building_param" };

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

			constexpr char TAG_METATILES[]{ "metatiles" };
			constexpr char TAG_METATILE[]{ "metatile" };

			constexpr char ATTR_MATTOCK_ANIMATION[]{ "mattock_animation" };

			constexpr char TAG_CHUNKS[]{ "worlds" };
			constexpr char TAG_CHUNK[]{ "world" };

			constexpr char ATTR_DEFAULT_PALETTE[]{ "default_palette" };

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
			constexpr char TAG_COL[]{ "col" };

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
		}

	}

}

#endif
