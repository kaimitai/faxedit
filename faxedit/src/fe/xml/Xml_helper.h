#ifndef FE_XML_HELPER_H
#define FE_XML_HELPER_H

#include <string>
#include "./../Game.h"
#include "./../../common/pugixml/pugixml.hpp"
#include "./../../common/pugixml/pugiconfig.hpp"

using byte = unsigned char;

namespace fe {

	namespace xml {

		void save_xml(const std::string p_filepath, const fe::Game& p_game);
		fe::Game load_xml(const std::string p_filepath);

		std::string join_bytes(const std::vector<byte>& p_bytes, bool p_hex = false);
		std::vector<byte> parse_byte_list(const std::string& input);

		fe::DoorType text_to_doortype(const std::string& p_str);

		std::string trim_whitespace(const std::string& p_value);
		std::vector<std::string> split_bytes(const std::string& p_values);
		byte parse_numeric_byte(const std::string& p_token);
		std::string byte_to_hex(byte p_byte);
	}

}

#endif
