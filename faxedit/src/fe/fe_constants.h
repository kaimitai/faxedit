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

		constexpr std::size_t SPRITE_COUNT{ 101 };

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
			{0x0b, "Unknown (originally breakable)"},
			{0x0c, "Other-World/Return"},
			{0x0d, "Other-World (foreground)"},
		};

		// placeholder until we dynamically parse this data
		inline const std::map<byte, std::string> LABELS_SPECIAL_BUNDLES{
			{0x44, "End-Game Sprite Set"}
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
			{0x3E, "NPC: Doctor"},
			{0x3F, "NPC: Sitting Man 1"},
			{0x40, "NPC: Meat Salesman"},
			{0x41, "NPC: Lady in blue dress with cup"},
			{0x42, "NPC: King's Guard"},
			{0x43, "NPC: Man in Chair"},
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
			{0x4F, "Item: Invisible Dialogue Trigger"},
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
			{0x00, "Dia: Introduction"},
			{0x01, "Dia: See king right away"},
			{0x02, "Game: Mark of Jack"},
			{0x03, "Dia: This is Eolis"},
			{0x04, "Eolis Guru: Ring of Elf (spawn #0)"},
			{0x05, "Dia: The last well is almost dry"},
			{0x06, "Dia: Dwarfs already in town"},
			{0x07, "Dia: King is Waiting"},
			{0x08, "Gift: 1500 Golds"},
			{0x09, "Dia: A meteorite fell"},
			{0x0a, "Dia: Listen to People"},
			{0x0b, "Eolis Meat Shop"},
			{0x0c, "Eolis Key Shop"},
			{0x0d, "Eolis Tools Shop"},
			{0x0e, "Eolis Magic Shop"},
			{0x0f, "Eolis Martial Arts"},
			{0x10, "Dia: Welcome to Apolune"},
			{0x11, "Dia: Dwarfs in Tower of Trunk"},
			{0x12, "Dia: You should have shield"},
			{0x13, "Dia: We blocked the passage"},
			{0x14, "Game: You need a key"},
			{0x15, "Apolune Hospital"},
			{0x16, "Nurse: Are you hurt?"},
			{0x17, "Dia: Did you learn mantra?"},
			{0x18, "Dia: You can carry 8 items"},
			{0x19, "Apolune Tools Shop"},
			{0x1a, "Apolune Key Shop"},
			{0x1b, "(glitched dialogue box)"},
			{0x1c, "Apolune Guru (spawn #1)"},
			{0x1d, "Trunk Secret Tool Shop"},
			// {0x1e, ""},
			{0x1f, "Game: Don't have negative thoughts"},
			{0x20, "Dia: This is Forepaw"},
			{0x21, "Dia: Water from 3 springs"},
			{0x22, "Dia: Fountain in the sky"},
			{0x23, "Nurse: Fountain in tower of Fortress"},
			{0x24, "Gift: Joker Key (with spring active)"},
			{0x25, "Quest: Sky Spring"},
			{0x26, "Quest: Trunk Spring"},
			{0x27, "Quest: Dungeon Spring"},
			{0x28, "Forepaw Tools Shop"},
			{0x29, "Forepaw Guru (spawn #2)"},
			{0x2a, "Forepaw Hospital"},
			{0x2b, "Forepaw Key Shop"},
			{0x2c, "Forepaw Meat Shop"},
			// {0x2d, ""},
			// {0x2e, ""},
			// {0x2f, ""},
			{0x30, "Dia: This is Mascon"},
			{0x31, "Dia: Meteorite becomes poison"},
			{0x32, "Dia: Stores outside town"},
			{0x33, "Dia: Metorite at tower of Suffer"},
			{0x34, "Dia: Town covered in mist"},
			{0x35, "Dia: Tower of Suffer soon"},
			{0x36, "Dia: Dwarfs chanting"},
			{0x37, "Nurse: Hour Glass info"},
			{0x38, "Mascon Hospital"},
			{0x39, "Mascon Tools Shop"},
			{0x3a, "Mascon Meat Shop"},
			{0x3b, "Mascon Key Shop"},
			{0x3c, "Mist Secret Tools Shop"},
			{0x3d, "Mist Guru (spawn #3)"},
			// {0x3e, ""},
			// {0x3f, ""},
			{0x40, "Dia: Welcome to Victim"},
			{0x41, "Dia: Magic of offence is effective"},
			{0x42, "Dia: Magic hall outside town"},
			{0x43, "Dia. Door to Capital below is open"},
			{0x44, "Dia: Looks like I'm lost"},
			{0x45, "Dia: Magic of justice or destruction"},
			{0x46, "Gift: Ace Key (with Onyx)"},
			{0x47, "Victim Hosptial"},
			{0x48, "Victim Tools Shop"},
			{0x49, "Victim Meat Shop"},
			{0x4a, "Victim Key Shop"},
			{0x4b, "Fire Magic Shop"},
			{0x4c, "Victim Guru (spawn #4)"},
			// {0x4d, ""},
			// {0x4e, ""},
			// {0x4f, ""},
			{0x50, "Dia: This is Conflate"},
			{0x51, "Dia: The town is surrounded"},
			{0x52, "Dia: I am a guardian of the Guru"},
			{0x53, "Dia: The town used to prosper"},
			{0x54, "Dia: I used to go to town of Dwarfs"},
			{0x55, "Conflate Guru (spawn #5)"},
			{0x56, "Conflate Hospital"},
			{0x57, "Conflate Tools Shop"},
			{0x58, "Conflate Meat Shop"},
			// {0x59, ""},
			// {0x5a, ""},
			// {0x5b, ""},
			// {0x5c, ""},
			// {0x5d, ""},
			// {0x5e, ""},
			// {0x5f, "" },
			{ 0x60, "Dia: Daybreak is at the border" },
			{ 0x61, "Dia: Look for Battle Suit" },
			{ 0x62, "Dia: You are the famous warrior" },
			{ 0x63, "Nurse: When did you last bath?" },
			{ 0x64, "Dia: Did you get the Battle Suit?" },
			{ 0x65, "Dia: Is this your first visit?" },
			{ 0x66, "Daybreak Tools Shop" },
			{ 0x67, "Daybreak Meat Shop" },
			{ 0x68, "Daybreak Key Shop" },
			{ 0x69, "Daybreak Guru (spawn #6)" },
			// { 0x6a, "" },
			// { 0x6b, "" },
			// { 0x6c, "" },
			// { 0x6d, "" },
			// { 0x6e, "" },
			// { 0x6f, "" },
			{ 0x70, "Dia: Fraternal below Dartmoor" },
			{ 0x71, "Dia: Dragon Slayer in the hands of dwarf king" },
			{ 0x72, "Dia: See Guru in Fraternal Castle" },
			{ 0x73, "Dia: Search for Guru in castle" },
			{ 0x74, "Fraternal Castle Guru" },
			{ 0x75, "Dartmoor Tools Shop" },
			{ 0x76, "Dartmoor Meat Shop" },
			{ 0x77, "Dartmoor Key Shop" },
			{ 0x78, "Dartmoor Guru (spawn #7)" },
			{ 0x79, "End-game dialogue and ending" },
			{ 0x7a, "Dartmoor Hospital" },
			{ 0x7b, "Game: Mark of Queen by the key hole" },
			{ 0x7c, "Game: Mark of King by the key hole" },
			{ 0x7d, "Game: Mark of Ace by the key hole" },
			{ 0x7e, "Game: Mark of Joker by the key hole" },
			{ 0x7f, "Game: Do you need a ring to open door?" },
			{ 0x80, "Game: I've used Red Potion" },
			{ 0x81, "Game: I've used Mattock" },
			{ 0x82, "Game: I've used Hour Glass" },
			{ 0x83, "Game: I've used Wing Boots" },
			{ 0x84, "Game: I've used Key" },
			{ 0x85, "Game: I've used Elixir" },
			{ 0x86, "Game: I'm Holding Elixir" },
			{ 0x87, "Game: I'm holding Red Potion" },
			{ 0x88, "Game: I'm holding Mattock" },
			{ 0x89, "Game: I'm holding Wing Boots" },
			{ 0x8a, "Game: I'm holding Hour Glass" },
			{ 0x8b, "Game: I've got the Battle Suit" },
			{ 0x8c, "Game: I've got the Battle Helmet" },
			{ 0x8d, "Game: I've got the Dragon Slayer" },
			{ 0x8e, "Game: I've got the Black Onyx" },
			{ 0x8f, "Game: I've got the Pendant" },
			{ 0x90, "Game: I've got the Magical Rod" },
			{ 0x91, "Game: I've touched Poison" },
			{ 0x92, "Game: The glove increases offensive power" },
			{ 0x93, "Game: The power of the Glove is gone" },
			{ 0x94, "Game: I am free from injury cuz ointment" },
			{ 0x95, "Game: The power of the ointment is gone" },
			{ 0x96, "Game: The power of the Wing Boots is gone" },
			{ 0x97, "Game: The power of the Hour Glass is gone" }
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
