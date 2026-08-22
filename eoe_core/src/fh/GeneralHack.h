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
		KillSwitch, SameWorldTransPal2Mus
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
		const std::string& get_string(const std::string& p_id) const;
	};

	std::vector<GeneralHack> parse_general_hacks(const std::string& p_str);
	std::vector<GeneralHack> filter_general_hacks(byte p_bank, const std::vector<GeneralHack>& p_hacks);
}

#endif
