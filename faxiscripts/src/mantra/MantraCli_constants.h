#ifndef FM_CLI_CONSTANTS_H
#define FM_CLI_CONSTANTS_H

#include <map>
#include <string>
#include <vector>

namespace fman {

	namespace cli {

		constexpr char OPT_TERSE[]{ "-t" };
		constexpr char OPT_MANTRA[]{ "-m" };
		constexpr char OPT_SPAWN_COUNT[]{ "-sc" };

		constexpr char OPT_RANK[]{ "-r" };
		constexpr char OPT_LOCATION[]{ "-l" };
		constexpr char OPT_EQ_WEAPON[]{ "-ew" };
		constexpr char OPT_EQ_ARMOR[]{ "-ea" };
		constexpr char OPT_EQ_SHIELD[]{ "-es" };
		constexpr char OPT_EQ_MAGIC[]{ "-em" };
		constexpr char OPT_EQ_ITEM[]{ "-ei" };
		constexpr char OPT_ST_WEAPON[]{ "-sw" };
		constexpr char OPT_ST_ARMOR[]{ "-sa" };
		constexpr char OPT_ST_SHIELD[]{ "-ss" };
		constexpr char OPT_ST_MAGIC[]{ "-sm" };
		constexpr char OPT_ST_ITEM[]{ "-si" };

		constexpr char OPT_SPECIAL[]{ "-s" };
		constexpr char OPT_GAMESTATE_FLAGS[]{ "-g" };

		inline const std::map<std::string, std::vector<std::string>> DECODE_VALUES = {
			{OPT_RANK, {"novice", "aspirant", "battler", "fighter", "adept", "chevalier", "veteran", "warrior", "swordman", "hero", "soldier", "myrmidon", "champion", "superhero", "paladin", "lord"}},
			{OPT_LOCATION,  {"eolis", "apolune", "forepaw", "mascon", "victim", "conflate", "daybreak", "dartmoor"}},
			{OPT_ST_WEAPON, {"handdagger", "longsword", "giantblade", "dragonslayer"}},
			{OPT_ST_ARMOR, {"leatherarmor", "studdedmail", "fullplate", "battlesuit"}},
			{OPT_ST_SHIELD, {"smallshield", "largeshield", "magicshield", "battlehelm"}},
			{OPT_ST_MAGIC, {"deluge", "thunder", "fire", "death", "tilte", "elfring", "rubyring", "dworfring"}},
			{OPT_ST_ITEM, {"elfring", "rubyring", "dworfring", "demonsring", "ace", "king", "queen", "jack", "joker", "mattock", "rod", "crystal", "lamp", "hourglass", "book", "wingboots", "redpotion", "blackpotion", "elixir", "pendant", "blackonix", "firecrystal", "g0", "g1", "g2", "g3", "g4", "g5", "g6", "g7", "g8", "g9"}},
			{OPT_SPECIAL, {"elfring", "rubyring", "dworfring", "demonsring", "elixir", "magicalrod", "pendant", "blackonix"}},
			{OPT_GAMESTATE_FLAGS, {"u1", "u2", "masconpath", "mattock", "wingboots", "dungeonspring", "skyspring", "towerspring"}}
		};

	}
}

#endif
