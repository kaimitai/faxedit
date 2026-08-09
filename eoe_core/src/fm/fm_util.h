#ifndef FM_UTIL_H
#define FM_UTIL_H

#include <map>
#include <string>
#include "fe/Config.h"
#include "MusicOpcode.h"
#include "common/klib/Kstring.h"
#include "fm/song/mml_constants.h"

using byte = unsigned char;

namespace fm {

	namespace util {

		std::map<byte, std::string> generate_note_meta_consts(void);
		MusicOpcode decode_opcode_byte(byte b,
			const std::map<byte, fm::MusicOpcode>& p_opcodes);
		std::string byte_to_note(byte p_val, int8_t offset);
		std::string pitch_offset_to_string(int8_t offset);
		std::string sq_control_to_string(byte val);

		bool is_note(const std::string& token);
		byte note_to_byte(const std::string& token, int8_t offset);

		std::pair<int, int> note_string_to_pitch(const std::string& s);
		int mml_constant_to_int(const std::string& name);
		std::string mml_arg_to_string(fm::MmlArgDomain p_domain,
			int p_value);

		std::size_t get_music_count(const fe::Config& p_config, const std::vector<byte>& p_rom);
	}

}

#endif
