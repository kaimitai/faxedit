#ifndef FE_CONSTANTS_H
#define FE_CONSTANTS_H

#include <map>
#include <string>
#include <vector>

using byte = unsigned char;

namespace fe {

	namespace c {

		constexpr char CONFIG_FILE_NAME[]{ "eoe_config.xml" };

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
		constexpr std::pair<std::size_t, std::size_t> PTR_SPRITE_DATA{ 0x2c220, 0x24010 };
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
		constexpr std::size_t PTR_CHUNK_SPRITE_DATA{ 0x2c220 };
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
		constexpr std::size_t SIZE_LIMT_SPRITE_DATA{ 0x30010 - PTR_CHUNK_SPRITE_DATA };
		constexpr std::size_t SIZE_LIMT_METADATA{ 0xf010 - PTR_CHUNK_METADATA };
		constexpr std::size_t SIZE_LIMT_TRANSITION_DATA{ 313 };

		// ptrs to the screen data for each of the 8 chunks
		const std::vector<std::size_t> PTR_CHUNK_SCREEN_DATA{ 0x10, 0x12, 0x14, 0x4010, 0x4012, 0x8010, 0x8012, 0x8014 };

		// map to background gfx start locations - treated as immutable
		// when extracting the 256 nes tiles starting at any of these locations, the chunk tilemap indexes will match
		// special care needed for chunk 6 which uses different tilesets depending on screen id
		inline const std::vector<std::size_t> OFFSETS_BG_GFX{
			0xf810,   // eolis
			0x10010,  // trunk
			0x10810,  // mist
			0x11810,  // town
			0x11e10,  // guru, king and hospital screens
			0x11010,  // branches
			0x13010,  // dartmoor + evil fortress
			0x13010,  // dartmoor + evil fortress
			0x12410,  // set used for the shop and building interior screens
			0x12a10   // set used for the training shops
		};

		// a label of all the chunks, in ROM-order
		inline const std::vector<std::string> LABELS_CHUNKS{ "Eolis", "Trunk", "Mist", "Towns", "Buildings", "Branches", "Dartmoor Castle", "Evil Lair" };

		constexpr std::size_t SPRITE_COUNT{ 101 };
		constexpr std::size_t ISCRIPT_COUNT{ 152 };

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
			{0x05, "Jump-On"},
			{0x06, "Pushable" },
			{0x09, "SW-Transition (fg)" },
			{0x0a, "SW-Transition Ladder"},
			{0x0b, "Unknown (orig. breakable)"},
			{0x0c, "OW/Return"},
			{0x0d, "OW/Return (fg)"},
		};

		// placeholder until we dynamically parse this data
		inline const std::map<byte, std::string> LABELS_SPECIAL_BUNDLES{
			{0x44, "End-Game Sprite Set"}
		};

		inline const std::vector<std::string> LABELS_BUILDINGS{
			"King Room", "Guru Room", "Hospital", "Pub", "Tool Shop",
			"Key Shop", "House", "Meat Shop", "Martial Arts", "Magic Trainer"
		};

		inline const std::map<byte, std::string> LABELS_PALETTES{
			{0x00, "Eolis"},
			{0x06, "Trunk"},
			{0x07, "Trunk Towers"},
			{0x08, "Branches"},
			{0x09, "Branches (green)"},
			{0x0a, "Mist"},
			{0x0b, "Mist Towers"},
			{0x0c, "Dartmoor"},
			{0x0d, "Fraternal Castle"},
			{0x0e, "King Grieve's Room"},
			{0x0f, "Evil Lair"},
			{0x11, LABELS_BUILDINGS.at(0)},
			{0x12, LABELS_BUILDINGS.at(1)},
			{0x13, LABELS_BUILDINGS.at(2)},
			{0x14, LABELS_BUILDINGS.at(3)},
			{0x15, LABELS_BUILDINGS.at(4)},
			{0x16, LABELS_BUILDINGS.at(5)},
			{0x17, LABELS_BUILDINGS.at(6)},
			{0x18, LABELS_BUILDINGS.at(7)},
			{0x19, LABELS_BUILDINGS.at(8)},
			{0x1a, LABELS_BUILDINGS.at(9)},
			{0x1b, "Towns"}
		};

		inline const std::vector<std::string> LABELS_SPRITES{
			"Invisible, No-Damage",
			"Bread",
			"Coin",
			"Zorugeriru Rock",
			"Raiden",
			"Necron Aides",
			"Zombie",
			"Hornet",
			"Bihoruda",
			"Lilith",
			"Magic Effect #1",
			"Yuinaru",
			"Snowman",
			"Nash",
			"Fire Giant",
			"Ishiisu",
			"Execution Hood",
			"Rokusutahn",
			"Unused $12",
			"Enemy Death",
			"Lightning Ball",
			"Charron",
			"Invisble, Stationary",
			"Geributa",
			"Sugata",
			"Grimlock",
			"Giant Bees",
			"Myconid",
			"Naga",
			"Unused $1d",
			"Giant Strider",
			"Sir Gawaine",
			"Maskman",
			"Wolfman",
			"Yareeka",
			"Magman",
			"Unused $24",
			"Invisible, Stationary #2",
			"Ikeda",
			"Unused $27",
			"Lamprey",
			"Invisible, stationary #3",
			"Monodron",
			"Unused $2b",
			"Tamazutsu",
			"Ripasheiku",
			"Zoradohna",
			"Borabohra",
			"Pakukame",
			"Zorugeriru",
			"King Grieve",
			"Shadow Eura",
			"Walking man 1",
			"Blue Lady",
			"Child",
			"Armor Salesman",
			"Martial Arts",
			"Guru",
			"King",
			"Magic Teacher / Wise Man",
			"Key Salesman",
			"Smoking Man",
			"Doctor",
			"Sitting Man 1",
			"Meat Salesman",
			"Lady in blue dress with cup",
			"King's Guard",
			"Man in Chair",
			"Walking Woman",
			"Walking Woman #2",
			"Eyeball",
			"Zozura",
			"Glove",
			"Black Onyx",
			"Pendant",
			"Red Potion",
			"Poison",
			"Elixir",
			"Ointment",
			"Invisible Dialogue",
			"Mattock",
			"Magic Effect #2",
			"Final Fountain",
			"Magic Effect #3",
			"Magic Effect #4",
			"Wing Boots",
			"Hour Glass",
			"Magical Rod",
			"Battle Suit",
			"Battle Helmet",
			"Dragon Slayer",
			"Mattock (for quest)",
			"Wing Boots (for quest)",
			"Red Potion #2",
			"Poison #2",
			"Glove #2",
			"Ointment #2",
			"Tower Spring",
			"Sky Spring",
			"Dungeon Spring",
			"Boss Death"
		};

		inline const std::map<byte, std::string> LABELS_COMMAND_BYTE{
			{0x00, "Final Spring Opening"},
			{0x01, "Boss Room"},
			{0x02, "End-Game Room"}
		};

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
