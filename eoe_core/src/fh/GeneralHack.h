#ifndef FH_GENERAL_HACK_H
#define FH_GENERAL_HACK_H

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using byte = unsigned char;
using word = uint16_t;

namespace fh {

	enum class GeneralHackLib {
		FlexibleItems, DynamicTilesets,
		KillSwitch, SameWorldTransPal2Mus, FogRules,
		AtlasDevFrameScheduler, AtlasDevDayNightCycle, AtlasDevInfectedTint,
		FastStart, QuestFlagItemDrops, BossLockedItems
	};

	class GeneralHack {
		GeneralHackLib type;
		std::map<std::string, std::string> params;

	public:
		GeneralHack(const std::string& p_type);
		GeneralHack(const std::string& p_type,
			const std::map<std::string, std::string>& p_params);

		GeneralHackLib get_type(void) const;
		bool has_param(const std::string& p_id) const;

		word get_word(const std::string& p_id) const;
		byte get_byte(const std::string& p_id) const;
		bool get_bool(const std::string& p_id) const;
		const std::string& get_string(const std::string& p_id) const;

		// soecialized helpers
		std::vector<std::string> split(const std::string& p_id, char p_delim = '+') const;
		std::vector<std::vector<std::string>> split_twice(const std::string& p_id,
			char p_delim_outer = '+', char p_delim_inner = ':') const;
		std::vector<std::pair<byte, std::optional<byte>>> split_byte_optional_byte(
			const std::string& p_id) const;
		std::vector<std::vector<byte>> split_twice_bytes(const std::string& p_id,
			std::size_t inner_size = 0) const;

		// default value helpers
		word word_or(const std::string& p_id, word p_default) const;
		byte byte_or(const std::string& p_id, byte p_default) const;
		bool bool_or(const std::string& p_id, bool p_default) const;
		std::string string_or(const std::string& p_id, const std::string& p_default) const;
	};

	std::vector<GeneralHack> parse_general_hacks(const std::string& p_str);
	std::vector<GeneralHack> filter_general_hacks(byte p_bank, const std::vector<GeneralHack>& p_hacks);
}

#endif
