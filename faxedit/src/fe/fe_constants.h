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
		constexpr char ID_SPRITE_PTR[]{ "sprite_ptr" };

		// end offsets
		constexpr char ID_SPRITE_DATA_END[]{ "sprite_data_end" };

		// map IDs
		constexpr char ID_WORLD_LABELS[]{ "world_labels" };
		constexpr char ID_SPRITE_LABELS[]{ "sprite_labels" };
		constexpr char ID_BUILDING_LABELS[]{ "building_labels" };
		constexpr char ID_BLOCK_PROP_LABELS[]{ "block_property_labels" };
		constexpr char ID_DOOR_REQ_LABELS[]{ "door_requirement_labels" };
		constexpr char ID_PALETTE_LABELS[]{ "palette_labels" };
		constexpr char ID_SPECIAL_SPRITE_SET_LABELS[]{ "special_sprite_set_labels" };
		constexpr char ID_CMD_BYTE_LABELS[]{ "command_byte_labels" };
		constexpr char ID_BG_CHR_ROM_OFFSETS[]{ "bg_chr_rom_offsets" };

		// counts
		constexpr char ID_SPRITE_COUNT[]{ "sprite_count" };
		constexpr char ID_ISCRIPT_COUNT[]{ "iscript_count" };

		// the start of the pointer table for each chunks's tilemaps
		// the start of the data (ptr table + data for each chunk) follows immediately after this outer pointer table
		inline const std::vector<std::size_t> PTR_TILEMAPS_BANK_ROM_OFFSET{ 0x10, 0x4010, 0x8010 };
		// game order tilemaps {{bank, chunk idx} -> game world idx}
		inline const std::vector<std::vector<std::size_t>> MAP_BANK_TO_WORLD_TILEMAPS{
			{0, 2, 3},
			   {1, 5},
			{6, 4, 7} };

		// chunks with special meaning in some contexts
		constexpr std::size_t CHUNK_IDX_TOWNS{ 0x03 };
		constexpr std::size_t CHUNK_IDX_BUILDINGS{ 0x04 };

		// the following pointers are on the form { ROM offset for master ptr table, ROM offset considered 0 by the ptrs }
		constexpr std::pair<std::size_t, std::size_t> PTR_OTHERW_TRANS_TABLE{ 0x3eaac, 0x30010 };
		constexpr std::pair<std::size_t, std::size_t> PTR_SAMEW_TRANS_TABLE{ 0x3ea47, 0x30010 };

		// regular offsets with no associated pointer table that we know of
		constexpr std::size_t OFFSET_MATTOCK_ANIMATIONS{ 0x3c69f };
		constexpr std::size_t OFFSET_SPAWN_LOC_WORLDS{ 0x3ddbd };
		constexpr std::size_t OFFSET_SPAWN_LOC_X_POS{ OFFSET_SPAWN_LOC_WORLDS + 8 };
		constexpr std::size_t OFFSET_SPAWN_LOC_Y_POS{ OFFSET_SPAWN_LOC_X_POS + 8 };
		constexpr std::size_t OFFSET_SPAWN_LOC_BPM{ OFFSET_SPAWN_LOC_Y_POS + 8 };
		constexpr std::size_t OFFSET_SPAWN_LOC_STAGES{ OFFSET_SPAWN_LOC_BPM + 8 };
		constexpr std::size_t OFFSET_SPAWN_LOC_SCREENS{ OFFSET_SPAWN_LOC_STAGES + 8 };

		// jump-on metatile animation
		constexpr std::size_t OFFSET_JUMP_ON_ANIMATION{ 0x3d6ff };

		// in game constants used as parameters to assembly instructions

		// path-to-mascon "line-drawing"
		constexpr std::size_t OFFSET_PTM_STAGE_NO{ 0x3d782 };
		constexpr std::size_t OFFSET_PTM_SCREEN_NO{ 0x3d788 };
		constexpr std::size_t OFFSET_PTM_BLOCK_COUNT{ 0x3d745 };
		constexpr std::size_t OFFSET_PTM_START_POS{ 0x3d749 };
		// 4 bytes from this location: what tiles to replace the pushable blocks with,
		// and then which 2 tiles will appear where the blockes were pushed to
		constexpr std::size_t OFFSET_PTM_REPLACE_TILE_NOS{ 0x3d778 };
		constexpr std::size_t OFFSET_PTM_POS_DELTA{ 0x3d7b3 };
		constexpr std::size_t OFFSET_PTM_TILE_NO{ 0x3d7bf };
		// the position of the fountain cover
		// is used when you re-enter the screen after having already pushed
		constexpr std::size_t OFFSET_PTM_COVER_POS{ 0x3ef8a };

		// END - Rework constants while we move the pointer data into the ROM_Manager-class instead of the Game-class

		// pointers to chunk data pointer tables
		// indexed by chunk no (offet address by (2 * chunk no) to get the ptr to the chunk we're interested in)
		constexpr std::size_t PTR_CHUNK_METADATA{ 0xc010 };
		constexpr std::size_t PTR_CHUNK_PALETTES{ 0x2c010 };
		constexpr std::size_t PTR_CHUNK_DEFAULT_PALETTE_IDX{ 0x3df5c };
		constexpr std::size_t PTR_CHUNK_INTERCHUNK_TRANSITIONS{ 0x3ea47 };
		constexpr std::size_t PTR_CHUNK_INTRACHUNK_TRANSITIONS{ 0x3eaac };

		// stage metadata offsets
		constexpr std::size_t OFFSET_STAGE_CONNECTIONS{ 0x3e5f7 };
		constexpr std::size_t OFFSET_STAGE_SCREENS{ 0x3e603 };
		constexpr std::size_t OFFSET_STAGE_REQUIREMENTS{ 0x3e5eb };
		constexpr std::size_t OFFSET_STAGE_TO_WORLD{ 0x3db0e };
		constexpr std::size_t OFFSET_GAME_START_POS{ 0x3deff };
		constexpr std::size_t OFFSET_GAME_START_SCREEN{ 0x3dedb };
		constexpr std::size_t OFFSET_GAME_START_HP{ 0x3debf };

		// dynamic size limits
		// TODO: Check, this could be totally wrong
		inline const std::vector<std::size_t> SIZE_LIMITS_BANK_TILEMAPS{
			0x4000, 0x4000, 0x4000
		};

		constexpr std::size_t SIZE_LIMT_METADATA{ 0xf010 - PTR_CHUNK_METADATA };
		constexpr std::size_t SIZE_LIMT_TRANSITION_DATA{ 313 };

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
