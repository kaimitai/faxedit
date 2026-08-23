#ifndef FE_XML_HELPER_H
#define FE_XML_HELPER_H

#include <map>
#include <string>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>
#include "Xml_constants.h"
#include "./../../common/pugixml/pugixml.hpp"
#include "./../../common/pugixml/pugiconfig.hpp"

using byte = unsigned char;

namespace fe {

	namespace xml {

		pugi::xml_document load_xml_file(const std::string& p_filepath);
		void save_xml_file(const pugi::xml_document& p_doc, const std::string& p_filepath);

		std::string join_bytes(const std::vector<byte>& p_bytes, bool p_hex = false);
		std::vector<byte> parse_byte_list(const std::string& input);
		std::string trim_whitespace(const std::string& p_value);
		std::vector<std::string> split_bytes(const std::string& p_values);
		std::vector<std::string> split_csv(const std::string& p_values);
		std::size_t parse_numeric(const std::string& p_token);
		byte parse_numeric_byte(const std::string& p_token);
		std::string byte_to_hex(byte p_byte);
	}

}

#endif
