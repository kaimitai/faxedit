#ifndef FH_GENERAL_HACK_H
#define FH_GENERAL_HACK_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

using byte = unsigned char;
using word = uint16_t;

namespace fh {

	enum class GeneralHackLib {
		KillSwitch, SameWorldTransPal2Mus, FastStart
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

		// default value helpers
		word word_or(const std::string& p_id, word p_default) const;
		byte byte_or(const std::string& p_id, byte p_default) const;
		bool bool_or(const std::string& p_id, bool p_default) const;
	};

	std::vector<GeneralHack> parse_general_hacks(const std::string& p_str);
	std::vector<GeneralHack> filter_general_hacks(byte p_bank, const std::vector<GeneralHack>& p_hacks);
}

#endif
