#ifndef FM_CONSTANTS_H
#define FM_CONSTANTS_H

#include <array>
#include <cstddef>
#include <string_view>

namespace fman {

	namespace c {

		// default mantra (starter mantra)
		constexpr char DEFAULT_MANTRA[]{ "rFAAAAQAAA" };
		constexpr int CHECKSUM_MASK{ 0xff };

		// how many bits of information does one character in the mantra represent
		constexpr int BITS_PER_CHAR{ 6 };

		// how many bits of the mantra are reserved for the checksum
		constexpr std::size_t BITS_CHECKSUM{ 0 };
		constexpr std::size_t BITS_CHECKSUM_LENGTH{ 8 };
		// how many bits are reserved for the mantra character count
		constexpr std::size_t BITS_CHAR_COUNT{ BITS_CHECKSUM + BITS_CHECKSUM_LENGTH };
		constexpr std::size_t BITS_CHAR_COUNT_LENGTH{ 5 };
		// bits for storing location
		constexpr std::size_t BITS_LOCATION{ BITS_CHAR_COUNT + BITS_CHAR_COUNT_LENGTH };
		constexpr std::size_t BITS_LOCATION_LENGTH{ 3 };
		// bits for storing rank
		constexpr std::size_t BITS_RANK{ BITS_LOCATION + BITS_LOCATION_LENGTH };
		constexpr std::size_t BITS_RANK_LENGTH{ 4 };
		// bits for storing special items
		constexpr std::size_t BITS_SPECIAL_ITEMS{ BITS_RANK + BITS_RANK_LENGTH };
		constexpr std::size_t BITS_SPECIAL_ITEMS_LENGTH{ 8 };
		// bits for storing game state
		constexpr std::size_t BITS_GAMESTATE_FLAGS{ BITS_SPECIAL_ITEMS + BITS_SPECIAL_ITEMS_LENGTH };
		constexpr std::size_t BITS_GAMESTATE_FLAGS_LENGTH{ 8 };

		// bits for storing equipped gear
		constexpr std::size_t BITS_EQ_WEAPONS{ BITS_GAMESTATE_FLAGS + BITS_GAMESTATE_FLAGS_LENGTH };

		// equipped flag (single bool)
		constexpr std::size_t BITS_EQ_ITEM_FLAG_LENGTH{ 1 };

		// bit-length of IDs per item type
		constexpr std::size_t BITS_WEAPON_ID{ 2 };
		constexpr std::size_t BITS_ARMOR_ID{ 2 };
		constexpr std::size_t BITS_SHIELD_ID{ 2 };
		constexpr std::size_t BITS_MAGIC_ID{ 3 };
		constexpr std::size_t BITS_ITEM_ID{ 5 };

		// bit-length of counts per item type
		constexpr std::size_t BITS_WEAPON_COUNT{ 3 };
		constexpr std::size_t BITS_ARMOR_COUNT{ 3 };
		constexpr std::size_t BITS_SHIELD_COUNT{ 3 };
		constexpr std::size_t BITS_MAGIC_COUNT{ 3 };
		constexpr std::size_t BITS_ITEM_COUNT{ 4 };

		// boundaries for item IDs by type
		constexpr int MAX_RANK{ 15 };
		constexpr int MAX_LOCATION{ 255 };

		constexpr int MAX_WEAPON_ID{ (1 << BITS_WEAPON_ID) - 1 };
		constexpr std::size_t MAX_CAP_WEAPONS{ 4 };

		constexpr int MAX_ARMOR_ID{ (1 << BITS_ARMOR_ID) - 1 };
		constexpr std::size_t MAX_CAP_ARMORS{ 4 };

		constexpr int MAX_SHIELD_ID{ (1 << BITS_SHIELD_ID) - 1 };
		constexpr std::size_t MAX_CAP_SHIELDS{ 4 };

		constexpr int MAX_MAGIC_ID{ (1 << BITS_MAGIC_ID) - 1 };
		constexpr std::size_t MAX_CAP_MAGICS{ 4 };

		constexpr int MAX_ITEM_ID{ (1 << BITS_ITEM_ID) - 1 };
		constexpr std::size_t MAX_CAP_ITEMS{ 8 };

		// named constants for weapons
		constexpr int WEAPON_HAND_DAGGER{ 0 };
		constexpr int WEAPON_LONG_SWORD{ 1 };
		constexpr int WEAPON_GIANT_BLADE{ 2 };
		constexpr int WEAPON_DRAGON_SLAYER{ 3 };

		// named constants for armors
		constexpr int ARMOR_LEATHER_ARMOR{ 0 };
		constexpr int ARMOR_STUDDED_MAIL{ 1 };
		constexpr int ARMOR_FULL_PLATE{ 2 };
		constexpr int ARMOR_BATTLE_SUIT{ 3 };

		// named constants for shields
		constexpr int SHIELD_SMALL_SHIELD{ 0 };
		constexpr int SHIELD_LARGE_SHIELD{ 1 };
		constexpr int SHIELD_MAGIC_SHIELD{ 2 };
		constexpr int SHIELD_BATTLE_HELM{ 3 };

		// named constants for magics
		constexpr int MAGIC_DELUGE{ 0 };
		constexpr int MAGIC_THUNDER{ 1 };
		constexpr int MAGIC_FIRE{ 2 };
		constexpr int MAGIC_DEATH{ 3 };
		constexpr int MAGIC_TILTE{ 4 };
		constexpr int MAGIC_RING_OF_ELF{ 5 };
		constexpr int MAGIC_RUBY_RING{ 6 };
		constexpr int MAGIC_RING_OF_DWORF{ 7 };

		// named constants for items
		constexpr int ITEM_RING_OF_ELF{ 0 };
		constexpr int ITEM_RUBY_RING{ 1 };
		constexpr int ITEM_RING_OF_DWORF{ 2 };
		constexpr int ITEM_DEMONS_RING{ 3 };
		constexpr int ITEM_KEY_A{ 4 };
		constexpr int ITEM_KEY_K{ 5 };
		constexpr int ITEM_KEY_Q{ 6 };
		constexpr int ITEM_KEY_J{ 7 };
		constexpr int ITEM_KEY_JO{ 8 };
		constexpr int ITEM_MATTOCK{ 9 };
		constexpr int ITEM_MAGICAL_ROD{ 10 };
		constexpr int ITEM_CRYSTAL{ 11 };
		constexpr int ITEM_LAMP{ 12 };
		constexpr int ITEM_HOUR_GLASS{ 13 };
		constexpr int ITEM_BOOK{ 14 };
		constexpr int ITEM_WING_BOOTS{ 15 };
		constexpr int ITEM_RED_POTION{ 16 };
		constexpr int ITEM_BLACK_POTION{ 17 };
		constexpr int ITEM_ELIXIR{ 18 };
		constexpr int ITEM_PENDANT{ 19 };
		constexpr int ITEM_BLACK_ONIX{ 20 };
		constexpr int ITEM_FIRE_CRYSTAL{ 21 };
		constexpr int ITEM_GLITCH_22{ 22 };
		constexpr int ITEM_GLITCH_23{ 23 };
		constexpr int ITEM_GLITCH_24{ 24 };
		constexpr int ITEM_GLITCH_25{ 25 };
		constexpr int ITEM_GLITCH_26{ 26 };
		constexpr int ITEM_GLITCH_27{ 27 };
		constexpr int ITEM_GLITCH_28{ 28 };
		constexpr int ITEM_GLITCH_29{ 29 };
		constexpr int ITEM_GLITCH_30{ 30 };
		constexpr int ITEM_GLITCH_31{ 31 };

		// named constants for locations
		constexpr int LOCATION_EOLIS{ 0 };
		constexpr int LOCATION_APOLUNE{ 1 };
		constexpr int LOCATION_FOREPAW{ 2 };
		constexpr int LOCATION_MASCON{ 3 };
		constexpr int LOCATION_VICTIM{ 4 };
		constexpr int LOCATION_CONFLATE{ 5 };
		constexpr int LOCATION_DAYBREAK{ 6 };
		constexpr int LOCATION_DARTMOOR{ 7 };

		// named constants for ranks
		constexpr int RANK_NOVICE{ 0 };
		constexpr int RANK_ASPIRANT{ 1 };
		constexpr int RANK_BATTLER{ 2 };
		constexpr int RANK_FIGHTER{ 3 };
		constexpr int RANK_ADEPT{ 4 };
		constexpr int RANK_CHEVALIER{ 5 };
		constexpr int RANK_VETERAN{ 6 };
		constexpr int RANK_WARRIOR{ 7 };
		constexpr int RANK_SWORDMAN{ 8 };
		constexpr int RANK_HERO{ 9 };
		constexpr int RANK_SOLDIER{ 10 };
		constexpr int RANK_MYRMIDON{ 11 };
		constexpr int RANK_CHAMPION{ 12 };
		constexpr int RANK_SUPERHERO{ 13 };
		constexpr int RANK_PALADIN{ 14 };
		constexpr int RANK_LORD{ 15 };

		// named constants for special item indexes
		constexpr std::size_t SPECIAL_ITEM_RING_OF_ELF{ 0 };
		constexpr std::size_t SPECIAL_ITEM_RUBY_RING{ 1 };
		constexpr std::size_t SPECIAL_ITEM_RING_OF_DWORF{ 2 };
		constexpr std::size_t SPECIAL_ITEM_DEMONS_RING{ 3 };
		constexpr std::size_t SPECIAL_ITEM_ELIXIR{ 4 };
		constexpr std::size_t SPECIAL_ITEM_MAGICAL_ROD{ 5 };
		constexpr std::size_t SPECIAL_ITEM_PENDANT{ 6 };
		constexpr std::size_t SPECIAL_ITEM_BLACK_ONIX{ 7 };

		// named constants for gamestate indexes
		constexpr std::size_t GAMESTATE_UNKNOWN_0{ 0 };
		constexpr std::size_t GAMESTATE_UNKNOWN_1{ 1 };
		constexpr std::size_t GAMESTATE_PATH_TO_MASCON{ 2 };
		constexpr std::size_t GAMESTATE_WYVERN_MATTOCK{ 3 };
		constexpr std::size_t GAMESTATE_STONE_DROPPER_WING_BOOTS{ 4 };
		constexpr std::size_t GAMESTATE_SPRING_DUNGEON{ 5 };
		constexpr std::size_t GAMESTATE_SPRING_SKY{ 6 };
		constexpr std::size_t GAMESTATE_SPRING_TOWER{ 7 };

		// labels
		constexpr std::array<std::string_view, 8> LABELS_SPECIAL_ITEMS{
			"Ring of Elf", "Ring of Ruby", "Ring of Dworf", "Demon's Ring",
			"Elixir", "Magical Rod", "Pendant", "Black Onix"
		};

		constexpr std::array<std::string_view, 8> LABELS_GAMESTATE_FLAGS{
			"Unknown 1", "Unknown 2", "Path to Mascon", "Wyvern Mattock",
			"Stone Dropper Wing Boots", "Dungeon Spring", "Sky Spring", "Tower Spring"
		};

		constexpr std::array<std::string_view, 32> LABELS_ITEMS{
			"Ring of Elf", "Ruby Ring", "Ring of Dworf", "Demon's Ring", "Key A", "Key K", "Key Q", "Key J",
			"Key Jo", "Mattock", "Rod", "Crystal", "Lamp", "Hour Glass", "Book", "Wing Boots",
			"Red Potion", "Black Potion", "Elixir", "Pendant", "Black Onix", "Fire Crystal", "Glitch #22", "Glitch #23",
			"Glitch #24", "Glitch #25", "Glitch #26", "Glitch #27", "Glitch #28", "Glitch #29", "Glitch #30", "Glitch #31"
		};

		constexpr std::array<std::string_view, 4> LABELS_WEAPONS{
			"Hand Dagger", "Long Sword", "Giant Blade", "Dragon Slayer"
		};

		constexpr std::array<std::string_view, 4> LABELS_ARMORS{
			"Leather Armor", "Studded Mail", "Full Plate", "Battle Suit"
		};

		constexpr std::array<std::string_view, 4> LABELS_SHIELDS{
			"Small Shield", "Large Shield", "Magic Shield", "Battle Helm"
		};

		constexpr std::array<std::string_view, 8> LABELS_MAGICS{
			"Deluge", "Thunder", "Fire", "Death",
			"Tilte", "Ring of Elf", "Ruby Ring", "Ring of Dworf"
		};

		constexpr std::array<std::string_view, 8> LABELS_LOCATIONS{
			"Eolis", "Apolune", "Forepaw", "Mascon",
			"Victim", "Conflate", "Daybreak", "Dartmoor"
		};

		constexpr std::array<std::string_view, 16> LABELS_RANKS{
			"Novice", "Aspirant", "Battler", "Fighter", "Adept", "Chevalier", "Veteran", "Warrior",
			"Swordman", "Hero", "Soldier", "Myrmidon", "Champion", "Superhero", "Paladin", "Lord"
		};
	}

}

#endif
