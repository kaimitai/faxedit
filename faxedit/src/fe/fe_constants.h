#ifndef FE_CONSTANTS_H
#define FE_CONSTANTS_H

#include <map>
#include <string>
#include <vector>

using byte = unsigned char;

namespace fe {

	namespace c {

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
		constexpr std::size_t OFFSET_SPAWN_LOC_X_POS{ 0x3ddc5 };
		constexpr std::size_t OFFSET_SPAWN_LOC_Y_POS{ 0x3ddcd };
		constexpr std::size_t OFFSET_SPAWN_LOC_SCREENS{ 0x3dde5 };
		constexpr std::size_t OFFSET_SPAWN_LOC_STAGES{ 0x3dddd };

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
			{0x05, "Jump-On (experimental)"},
			{0x06, "Pushable" },
			{0x0a, "Transition Ladder"},
			{0x0b, "Mattock-breakable"},
			{0x0c, "Other-World/Return"},
			{0x0d, "Other-World (foreground)"},
		};

		// placeholder until we dynamically parse this data
		inline const std::map<byte, std::string> LABELS_SPECIAL_BUNDLES{
			{0x42, "End-Game Sprite Set"}
		};

		// which door commands to look for when automatically generating spawn points
		// TODO: Use the script bytes that set spawn points instead
		inline const std::vector<byte> SPAWN_POINT_BUILDING_PARAMS
		{ 0x02, 0x0b, 0x10, 0x1e, 0x23, 0x2b, 0x33, 0x3c };

		inline const std::vector<std::string> LABELS_BUILDINGS{
			"King Room", "Guru Room", "Hospital", "Pub", "Tool Shop",
			"Key Shop", "House", "Meat Shop", "Martial Arts", "Magic Trainer"
		};

		inline const std::map<byte, std::string> LABELS_PALETTES{
			{0x00, "Eolis"},
			{0x06, "Trunk"},
			{0x07, "Trunk Towers"},
			{0x08, "Branches"},
			{0x0a, "Mist"},
			{0x0b, "Mist Towers"},
			{0x0c, "Dartmoor"},
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

		inline const std::map<byte, std::string> LABELS_SPRITES{
			// {0x00, ""},
			{0x01, "Item: Dropped Bread"},
			{0x02, "Item: Dropped Coin"},
			// {0x03, "TODO: Garbled 3"},
			{0x04, "Enemy: Raiden"},
			{0x05, "Enemy: Necron Aides"},
			{0x06, "Enemy: Zombie"},
			{0x07, "Enemy: Hornet"},
			{0x08, "Enemy: Bihoruda"},
			{0x09, "Enemy: Lilith"},
			// {0x0A, "TODO: Garbled 10"},
			{0x0B, "Enemy: Yuinaru"},
			{0x0C, "Enemy: Snowman"},
			{0x0D, "Enemy: Nash"},
			{0x0E, "Enemy: Fire Giant"},
			{0x0F, "Enemy: Ishiisu"},
			{0x10, "Enemy: Execution Hood"},
			{0x11, "Boss: Rokusutahn"},
			{0x12, "Enemy: Unused #18"},
			{0x13, "Effect: Enemy Death"},
			{0x14, "Effect: Lightning Ball"},
			{0x15, "Enemy: Charron"},
			{0x16, "Enemy: Unused 22"},
			{0x17, "Enemy: Geributa"},
			{0x18, "Enemy: Sugata"},
			{0x19, "Enemy: Grimlock"},
			{0x1A, "Enemy: Giant Bees"},
			{0x1B, "Enemy: Myconid"},
			{0x1C, "Enemy: Naga"},
			{0x1D, "Enemy: Unused #29"},
			{0x1E, "Enemy: Giant Strider"},
			{0x1F, "Enemy: Sir Gawaine"},
			{0x20, "Enemy: Maskman"},
			{0x21, "Enemy: Wolfman"},
			{0x22, "Enemy: Yareeka"},
			{0x23, "Enemy: Magman"},
			{0x24, "Enemy: Unused #36"},
			{0x25, "Enemy: Unused #37"},
			{0x26, "Enemy: Ikeda"},
			{0x27, "Enemy: Unused #39"},
			{0x28, "Enemy: Lamprey"},
			{0x29, "Enemy: Unused #41"},
			{0x2A, "Enemy: Monodron"},
			// {0x2B, "Unused #43"},
			{0x2C, "Enemy: Tamazutsu"},
			{0x2D, "Boss: Ripasheiku"},
			{0x2E, "Boss: Zoradohna"},
			{0x2F, "Boss: Borabohra"},
			{0x30, "Boss: Pakukame"},
			{0x31, "Boss: Zorugeriru"},
			{0x32, "Boss: King Grieve"},
			{0x33, "Boss: Shadow Eura"},
			{0x34, "NPC: Walking man 1"},
			{0x35, "NPC: Unused Blue Lady"},
			{0x36, "NPC: Unused Child"},
			{0x37, "NPC: Armor Salesman"},
			{0x38, "NPC: Martial Arts"},
			{0x39, "NPC: Priest"},
			{0x3A, "NPC: King"},
			{0x3B, "NPC: Magic Teacher / Spring Wise Man"},
			{0x3C, "NPC: Key Salesman"},
			{0x3D, "NPC: Smoking Man"},
			{0x3E, "NPC: Man in Chair"},
			{0x3F, "NPC: Sitting Man 1"},
			{0x40, "NPC: Meat Salesman"},
			{0x41, "NPC: Lady in blue dress with cup"},
			{0x42, "NPC: King's Guard"},
			{0x43, "NPC: Doctor"},
			{0x44, "NPC: Walking Woman 1"},
			{0x45, "NPC: Walking Woman 2"},
			{0x46, "Enemy: Unused eyeball"},
			{0x47, "Enemy: Zozura"},
			{0x48, "Item: Glove"},
			{0x49, "Item: Black Onyx"},
			{0x4A, "Item: Pendant"},
			{0x4B, "Item: Red Potion"},
			{0x4C, "Item: Poison"},
			{0x4D, "Item: Elixir"},
			{0x4E, "Item: Ointment"},
			{0x4F, "Item: Invisible Dialogue"},
			{0x50, "Item: Mattock"},
			// {0x51, "TODO: Garbled #81"},
			{0x52, "Fountain: Final"},
			// {0x53, "TODO: Unknown #83"},
			// {0x54, "TODO: Unknown #84"},
			{0x55, "Item: Wing Boots"},
			{0x56, "Item: Hour Glass"},
			{0x57, "Item: Magical Rod"},
			{0x58, "Item: Battle Suit"},
			{0x59, "Item: Battle Helmet"},
			{0x5A, "Item: Dragon Slayer"},
			{0x5B, "Item: Mattock #2"},
			{0x5C, "Item: Wing Boots (for quest)"},
			{0x5D, "Item: Red Potion #2"},
			{0x5E, "Item: Poison #2"},
			{0x5F, "Item: Glove #2"},
			{0x60, "Item: Ointment #2"},
			{0x61, "Spring: Tower"},
			{0x62, "Spring: Sky"},
			{0x63, "Spring: Dungeon"},
			{0x64, "Effect: Boss Death"}
		};

		// placeholder until we dynamically parse this data
		inline const std::map<byte, std::string> LABELS_SCRIPTS{
			{0x00, "Introduction"},
			{0x01, "See King / Go to Apolune"}
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
