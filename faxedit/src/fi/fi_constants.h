#ifndef FI_CONSTANTS_H
#define FI_CONSTANTS_H

#include <cstddef>
#include <map>
#include <set>
#include <string>

using byte = unsigned char;

namespace fi {

	namespace c {

		constexpr std::size_t ISCRIPT_ADDR_LO{ 0x31f7b };
		constexpr std::size_t ISCRIPT_ADDR_HI{ 0x32013 };
		constexpr std::size_t ISCRIPT_COUNT{ 152 };
		constexpr std::size_t ISCRIPT_DATA_START{ ISCRIPT_ADDR_LO + 2 * ISCRIPT_COUNT };
		constexpr std::size_t ISCRIPT_PTR_ZERO_ADDR{ 0x28010 };
		// known constant: if the script data size becomes bigger than this
		// we spill into unrelated data
		// these offsets are relative to the data. after script entrypoiny
		// ptr table the first address is 0
		constexpr std::size_t ISCRIPT_DATA_SIZE_REGION_1{ 0x62d };
		// 0x32d9b is a ROM offset at the end of the bank with free space
		constexpr std::size_t ISCRIPT_DATA_ROM_OFFSET_REGION_2{ 0x32d9b };
		constexpr std::size_t ISCRIPT_DATA_OFFSET_REGION_2{
			ISCRIPT_DATA_ROM_OFFSET_REGION_2 - ISCRIPT_DATA_START };
		// we use extra space until th end of bank
		constexpr std::size_t ISCRIPT_DATA_SIZE_REGION_2{
			0x34010 - ISCRIPT_DATA_ROM_OFFSET_REGION_2 };

		constexpr std::size_t OFFSET_STRINGS{ 0x34310 };
		constexpr std::size_t SIZE_STRINGS{ 0x30ba };

		constexpr char SECTION_DEFINES[]{ "[defines]" };
		constexpr char SECTION_STRINGS[]{ "[reserved_strings]" };
		constexpr char SECTION_SHOPS[]{ "[shops]" };
		constexpr char SECTION_ISCRIPT[]{ "[iscript]" };
		constexpr char DIRECTIVE_ENTRYPOINT[]{ ".entrypoint" };
		constexpr char PSEUDO_OPCODE_TEXTBOX[]{ ".textbox" };

		inline const std::map<byte, std::string> DEFINES_ITEMS{
			{0x00, "WEAPON_HAND_DAGGER"},
			{0x01, "WEAPON_LONG_SWORD"},
			{0x02, "WEAPON_GIANT_BLADE"},
			{0x03, "WEAPON_DRAGON_SLAYER"},
			{0x20, "ARMOR_LEATHER"},
			{0x21, "ARMOR_STUDDED_MAIL"},
			{0x22, "ARMOR_FULL_PLATE"},
			{0x23, "ARMOR_BATTLE_SUIT"},
			{0x40, "SHIELD_SMALL"},
			{0x41, "SHIELD_LARGE"},
			{0x42, "SHIELD_MAGIC"},
			{0x43, "SHIELD_BATTLE_HELMET"},
			{0x60, "MAGIC_DELUGE"},
			{0x61, "MAGIC_THUNDER"},
			{0x62, "MAGIC_FIRE"},
			{0x63, "MAGIC_DEATH"},
			{0x64, "MAGIC_TILTE"},
			{0x80, "SPECIAL_RING_OF_ELF"},
			{0x81, "SPECIAL_RING_OF_RUBY"},
			{0x82, "SPECIAL_RING_OF_DWORF"},
			{0x83, "SPECIAL_DEMONS_RING"},
			{0x84, "KEY_A"},
			{0x85, "KEY_K"},
			{0x86, "KEY_Q"},
			{0x87, "KEY_J"},
			{0x88, "KEY_JO"},
			{0x89, "MATTOCK"},
			{0x8A, "SPECIAL_MAGICAL_ROD"},
			{0x8B, "UNUSED_CRYSTAL"},
			{0x8C, "UNUSED_LAMP"},
			{0x8D, "ITEM_HOUR_GLASS"},
			{0x8E, "UNUSED_BOOK"},
			{0x8F, "ITEM_WING_BOOTS"},
			{0x90, "ITEM_RED_POTION"},
			{0x91, "ITEM_POISON"},
			{0x92, "SPECIAL_ELIXIR"},
			{0x93, "SPECIAL_PENDANT"},
			{0x94, "SPECIAL_BLACK_ONYX"},
			{0x95, "SPECIAL_FIRE_CRYSTAL"}
		};

		inline const std::map<byte, std::string> DEFINES_TEXTBOX{
			{0x00, "GENERIC"},
			{0x80, "KING"},
			{0x81, "GURU"},
			{0x82, "MARTIAL_ARTIST"},
			{0x83, "MAGICIAN"},
			{0x84, "DOCTOR"},
			{0x85, "NURSE"},
			{0x86, "PINK_SHIRT"},
			{0x87, "SMOKER"},
			{0x88, "MEAT_SALESMAN"},
			{0x89, "TOOLS_SALESMAN"},
			{0x8A, "KEY_SALESMAN"}
		};

		inline const std::map<byte, std::string> DEFINES_RANKS{
			{0x00, "RANK_NOVICE"},
			{0x01, "RANK_ASPIRANT"},
			{0x02, "RANK_BATTLER"},
			{0x03, "RANK_FIGHTER"},
			{0x04, "RANK_ADEPT"},
			{0x05, "RANK_CHEVALIER"},
			{0x06, "RANK_VETERAN"},
			{0x07, "RANK_WARRIOR"},
			{0x08, "RANK_SWORDMAN"},
			{0x09, "RANK_HERO"},
			{0x0A, "RANK_SOLDIER"},
			{0x0B, "RANK_MYRMIDON"},
			{0x0C, "RANK_CHAMPION"},
			{0x0D, "RANK_SUPERHERO"},
			{0x0E, "RANK_PALADIN"},
			{0x0F, "RANK_LORD"}
		};

		inline const std::map<byte, std::string> DEFINES_QUESTS{
			{0x01, "QUEST_TOWER_SPRING"}, // 0000 0001
			{0x02, "QUEST_SKY_SPRING"},   // 0000 0010
			{0x03, "QUEST_DUNGEON_SPRING"}, // 0000 0100
			// clean exploit of game code - these values when used
			// as byte-indexes in the quest flag table land on exact powers of two
			{0x4f, "QUEST_WYVERN_MATTOCK"}, // 0001 0000
			{0x04, "QUEST_PATH_TO_MASCON"}, // 0010 0000
			// compound "quests" using the remaining bits
			// won't overlap with any clean power of 2
			{0x24, "QUEST_EXTRA"},  // 0100 1000
		};

		// we're being very explicit - we're keeping bit equivalence on read and write
		inline const std::map<byte, std::string> FAXSTRING_CHARS{
			{'A', "A"}, {'B', "B"}, {'C', "C"}, {'D', "D"}, {'E', "E"}, {'F', "F"}, {'G', "G"}, {'H', "H"}, {'I', "I"}, {'J', "J"}, {'K', "K"}, {'L', "L"}, {'M', "M"}, {'N', "N"}, {'O', "O"}, {'P', "P"}, {'Q', "Q"}, {'R', "R"}, {'S', "S"}, {'T', "T"}, {'U', "U"}, {'V', "V"}, {'W', "W"}, {'X', "X"}, {'Y', "Y"}, {'Z', "Z"},
			{'a', "a"}, {'b', "b"}, {'c', "c"}, {'d', "d"}, {'e', "e"}, {'f', "f"}, {'g', "g"}, {'h', "h"}, {'i', "i"}, {'j', "j"}, {'k', "k"}, {'l', "l"}, {'m', "m"}, {'n', "n"}, {'o', "o"}, {'p', "p"}, {'q', "q"}, {'r', "r"}, {'s', "s"}, {'t', "t"}, {'u', "u"}, {'v', "v"}, {'w', "w"}, {'x', "x"}, {'y', "y"}, {'z', "z"},
			{'0', "0"}, {'1', "1"}, {'2', "2"}, {'3', "3"}, {'4', "4"}, {'5', "5"}, {'6', "6"}, {'7', "7"}, {'8', "8"}, {'9', "9"},
			// printable chars
			{'.', "."}, {'?', "?"},{'\'', "'"},{',', ","},{'!', "!"},{'-', "-"},{0x5f,"_"},
			// glyphs
			{0x60, "<block>"},{0x7b, "<short_bar_right>"},{0x7c, "<long_bar>"},{0x7d, "<arrow_right>"},{0x7e, "<arrow_left>"},{0x7f, "<short_bar_left>"},
			// special chars
			{0xfb, "<title>"}, { 0xfc, "<p>" }, {0xfd, " "}, {0xfe, "<n>"},
			// escape char
			{'\"', "<q>"}
		};

		// string indexes which are used directly by the script engine
		// these strings should not be relocated by the assembler
		inline const std::set<int> RESERVED_STRING_IDX{
			3,  // This is not enough Golds.
			6,  // You can't carry any more.
			16, // Come here to buy. Come here to sell.
			18, // You have nothing to buy.
			19, // Which one would like to sell?
			20  // What would you like?
		};

		constexpr byte OPCODE_SET_SPAWN{ 0x06 };

	}

}

#endif
