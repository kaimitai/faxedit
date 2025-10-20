#ifndef FE_CONSTANTS_H
#define FE_CONSTANTS_H

#include <map>
#include <string>
#include <vector>

using byte = unsigned char;

namespace fe {

	namespace c {

		// START - Rework constants while we move the pointer data into the ROM_Manager-class instead of the Game-class

		// the start of the pointer table for each chunks's tilemaps
		// the start of the data (ptr table + data for each chunk) follows immediately after this outer pointer table
		inline const std::vector<std::size_t> PTR_TILEMAPS_BANK_ROM_OFFSET{ 0x10, 0x4010, 0x8010 };

		// a map from chunk no (vector index) to bank number - this defines how we order the chunks in the application
		// if you change this, the remap tables also have to be recalculated
		// the size of this vector also define the number of chunks in the game - which is indeed 8 in the original game and can't be changed easily
		inline const std::vector<std::size_t> CHUNK_TILEMAPS_BANK_IDX{ 0, 0, 0, 1, 1, 2, 2, 2 };

		// the following pointers are on the form { ROM offset for master ptr table, ROM offset considered 0 by the ptrs }
		constexpr std::pair<std::size_t, std::size_t> PTR_SPRITE_DATA{ 0x2c220, 0x24010 };
		constexpr std::pair<std::size_t, std::size_t> PTR_OTHERW_TRANS_TABLE{ 0x3eaac, 0x30010 };
		constexpr std::pair<std::size_t, std::size_t> PTR_SAMEW_TRANS_TABLE{ 0x3ea47, 0x30010 };

		// regular offsets with no associated pointer table that we know of
		constexpr std::size_t OFFSET_SPAWN_LOC_WORLDS{ 0x3ddbd };
		constexpr std::size_t OFFSET_SPAWN_LOC_X_POS{ 0x3ddc5 };
		constexpr std::size_t OFFSET_SPAWN_LOC_Y_POS{ 0x3ddcd };
		constexpr std::size_t OFFSET_SPAWN_LOC_SCREENS{ 0x3dde5 };

		// END - Rework constants while we move the pointer data into the ROM_Manager-class instead of the Game-class

		// pointers to chunk data pointer tables
		// indexed by chunk no (offet address by (2 * chunk no) to get the ptr to the chunk we're interested in)
		constexpr std::size_t PTR_CHUNK_METADATA{ 0xc010 };
		constexpr std::size_t PTR_CHUNK_PALETTES{ 0x2c010 };
		constexpr std::size_t PTR_CHUNK_SPRITE_DATA{ 0x2c220 };
		constexpr std::size_t PTR_CHUNK_DEFAULT_PALETTE_IDX{ 0x3df5c };
		constexpr std::size_t PTR_CHUNK_DOOR_TO_CHUNK{ 0x3e5f7 };
		constexpr std::size_t PTR_CHUNK_DOOR_TO_SCREEN{ 0x3e603 };
		constexpr std::size_t PTR_CHUNK_DOOR_REQUIREMENTS{ 0x3e5eb };
		constexpr std::size_t PTR_CHUNK_INTERCHUNK_TRANSITIONS{ 0x3ea47 };
		constexpr std::size_t PTR_CHUNK_INTRACHUNK_TRANSITIONS{ 0x3eaac };

		// the npc bundles are stored as sprite data for the buildings chunk
		// this is a single value and not a vector, since the bundle data is global
		constexpr std::size_t IDX_CHUNK_NPC_BUNDLES{ 6 };

		// ptrs to the screen data for each of the 8 chunks
		const std::vector<std::size_t> PTR_CHUNK_SCREEN_DATA{ 0x10, 0x12, 0x14, 0x4010, 0x4012, 0x8010, 0x8012, 0x8014 };

		// a map from our chunk indexing to the indexing used by the ROM pointer tables
		inline const std::vector<std::size_t> MAP_CHUNK_IDX{ 0, 3, 1, 2, 6, 4, 5, 7 };

		// a map from our chunk indexes to the world indexes given in the chunk-linker table
		// missing values (town and buildings) are invalid indexes into that table
		inline const std::vector<std::size_t> MAP_CHUNK_LEVELS{ 0, 3, 1, 4, 5, 7 };

		// map to background gfx start locations - treated as immutable
		// when extracting the 256 nes tiles starting at any of these locations, the chunk tilemap indexes will match
		// special care needed for chunk 6 which uses different tilesets depending on screen id
		inline const std::vector<std::size_t> OFFSETS_BG_GFX{
			0xf810,   // eolis
			0x10810,  // mist
			0x11810,  // town
			0x10010,  // road to apolune, towers, springs and screen outside forepaw and apolune
			0x11010,  // branches
			0x13010,  // dartmoor + evil fortress
			0x11e10,  // guru, king and hospital screens
			0x13010,  // dartmoor + evil fortress
			0x12410,  // set used for the shop and building interior screens
			0x12a10   // set used for the training shops
		};

		// a label of all the chunks, in ROM-order
		inline const std::vector<std::string> LABELS_CHUNKS{ "Eolis", "Mist", "Towns", "Overworld", "Branches", "Dartmoor", "Buildings", "Evil Lair" };

		inline const std::map<byte, std::string> LABELS_DOOR_REQS{
			{0x00, "None"},
			{0x01, "Key A"},
			{0x02, "Key K"},
			{0x03, "Key Q"},
			{0x04, "Key J"},
			{0x05, "Key Jo"},
			{0x06, "Ring of Elf"},
			{0x07, "Ring of Dwarf"},
			{0x08, "Demon's Ring"}
		};

		inline const std::map<byte, std::string> LABELS_BLOCK_PROPERTIES{
			{0x00, "Air"},
			{0x01, "Solid"},
			{0x02, "Ladder"},
			{0x03, "Door"},
			{0x04, "Foreground"},
			{0x06, "Pushable"},
			{0x0a, "Transition Ladder"},
			{0x0b, "Return Exit"},
			{0x0c, "Inta-World transition"},
		};

		inline const std::map<byte, std::string> LABELS_BUILDINGS{
			{0x00, "King"},
			{0x01, "Guru"},
			{0x02, "Hospital"},
			{0x03, "Pub"},
			{0x04, "Weapon Shop"},
			{0x05, "Key Shop"},
			{0x06, "House"},
			{0x07, "Meat Shop"},
			{0x08, "Strength Trainer"},
			{0x09, "Magic Trainer"}
		};

		// placeholder until we dynamically parse this data
		inline const std::map<byte, std::string> LABELS_NPC_BUNDLES{
			{0x00, "Eolis Meat Shop"},
			{0x01, "Eolis House"},
			{0x02, "Eolis Guru (spawn #0)"},
			{0x03, "Eolis Key Shop"},
			{0x04, "Eolis Tools Shop"},
			{0x05, "Eolis Magic Shop"},
			{0x06, "Eolis Martial Arts"},
			{0x07, "King (early game)"},
			{0x0b, "Apolune Guru (spawn #1)"},
			{0x10, "Forepaw Guru (spawn #2)"},
			{0x1e, "Mist Guru (spawn #3)"},
			{0x23, "Victim Guru (spawn #4)"},
			{0x2b, "Conflate Guru (spawn #5)"},
			{0x33, "Daybreak Guru (spawn #6)"},
			{0x3c, "Dartmoor Guru (spawn #7)"},
		};

		// which door commands to look for when automatically generating spawn points
		inline const std::vector<byte> SPAWN_POINT_BUILDING_PARAMS
		{ 0x02, 0x0b, 0x10, 0x1e, 0x23, 0x2b, 0x33, 0x3c };

		inline const std::map<byte, std::string> LABELS_SPRITES{
			{0x2d, "Wyvern (Mattock)"},
			{0x4f, "Invisible dialogue on touch"},
			{0x50, "Mattock (25%)"},
		};

		// placeholder until we dynamically parse this data
		inline const std::map<byte, std::string> LABELS_DIALOGUE{
			{0x00, "Welcome message"}
		};

		// make sure this has the same order in enum fe::DoorType
		// used for door type dropdowns in the guid
		inline const char* LABELS_DOOR_TYPES[]{
			"Same World",
			"Buildings",
			"Previous World",
			"Next World"
		};

	}

}

#endif
