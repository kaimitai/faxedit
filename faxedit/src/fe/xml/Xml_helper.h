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

		std::string join_bytes(const std::vector<byte>& p_bytes, bool p_hex = false);
		std::string byte_to_hex(byte p_byte);
	}

}

#endif
