#ifndef FE_XML_CONSTANTS
#define FE_XML_CONSTANTS

namespace fe {

	namespace xml {

		namespace c {

			constexpr char TAG_ROOT[]{ "echoes_of_eolis" };
			constexpr char ATTR_ROOT_VERSION[]{ "version" };
			constexpr char VAL_ROOT_VERSION[]{ "beta-0.1" };
			constexpr char COMMENTS_ROOT[]{ "Faxanadu project file created with Echoes of Eolis (https://github.com/kaimitai/faxedit) " };

			constexpr char TAG_PALETTES[]{ "palettes" };
			constexpr char TAG_PALETTE[]{ "palette" };

			constexpr char TAG_NPC_BUNDLES[]{ "npc_bundles" };
			constexpr char TAG_NPC_BUNDLE[]{ "npc_bundle" };

			constexpr char TAG_METATILES[]{ "metatiles" };
			constexpr char TAG_METATILE[]{ "metatile" };

			constexpr char TAG_CHUNKS[]{ "chunks" };
			constexpr char TAG_CHUNK[]{ "chunk" };

			constexpr char ATTR_DEFAULT_PALETTE[]{ "default_palette" };

			constexpr char ATTR_MT_PROPERTY[]{ "property" };
			constexpr char ATTR_MT_PAL_TL[]{ "pal_top_left" };
			constexpr char ATTR_MT_PAL_TR[]{ "pal_top_right" };
			constexpr char ATTR_MT_PAL_BL[]{ "pal_bottom_left" };
			constexpr char ATTR_MT_PAL_BR[]{ "pal_bottom_right" };

			constexpr char TAG_CHUNK_DOOR_CONN[]{ "chunk_door_connections" };
			constexpr char TAG_NEXT_CHUNK[]{ "next_chunk" };
			constexpr char TAG_PREV_CHUNK[]{ "previous_chunk" };

			constexpr char ATTR_CHUNK_ID[]{ "chunk_no" };
			constexpr char ATTR_SCREEN_ID[]{ "screen_no" };
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

			constexpr char TAG_SCREEN_INTERCHUNK_TRANSTION[]{ "inter_chunk_transition" };
			constexpr char TAG_SCREEN_INTRACHUNK_TRANSTION[]{ "intra_chunk_transition" };

			constexpr char ATTR_NO[]{ "no" };
			constexpr char ATTR_ID[]{ "id" };
			constexpr char ATTR_TEXT_ID[]{ "text_id" };
			constexpr char ATTR_X[]{ "x" };
			constexpr char ATTR_Y[]{ "y" };
			constexpr char ATTR_DEST_X[]{ "destination_x" };
			constexpr char ATTR_DEST_Y[]{ "destination_y" };
			constexpr char ATTR_DEST_SCREEN_NO[]{ "destination_screen_no" };
			constexpr char ATTR_DEST_PALETTE[]{ "destination_palette" };
			constexpr char ATTR_DEST_PARAM_ID[]{ "destination_parameter" };
			constexpr char ATTR_UNKNOWN_BYTE[]{ "unknown_byte" };
			constexpr char ATTR_SPRITE_COMM_BYTE[]{ "sprite_command_byte" };
			constexpr char ATTR_UNKNOWN_SPR_BYTES[]{ "unknown_sprite_bytes" };
			constexpr char ATTR_BYTES[]{ "bytes" };
			constexpr char ATTR_ROW[]{ "row" };
			constexpr char ATTR_TYPE[]{ "type" };

			constexpr char VAL_DOOR_TYPE_INTERCHUNK[]{ "inter_chunk" };
			constexpr char VAL_DOOR_TYPE_BUILDING[]{ "building" };
			constexpr char VAL_DOOR_TYPE_NEXTCHUNK[]{ "next_chunk" };
			constexpr char VAL_DOOR_TYPE_PREVCHUNK[]{ "previous_chunk" };
		}

	}

}

#endif
