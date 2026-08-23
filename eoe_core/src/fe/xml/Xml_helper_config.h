#ifndef FE_XML_HELPER_CONFIG_H
#define FE_XML_HELPER_CONFIG_H

#include <map>
#include <string>
#include <utility>
#include <vector>
#include "fe/xml/Xml_helper.h"
#include "common/pugixml/pugixml.hpp"
#include "fe/Config.h"

using byte = unsigned char;

namespace fe {

	namespace xml {
		// eoe config
		pugi::xml_document load_config_xml(const std::string& p_xml_file,
			bool p_throw_on_file_not_exists = true);
		std::vector<RegionDefinition> load_region_defs(const pugi::xml_document& p_xml_doc);
		std::vector<RegionDefinition> load_region_defs(const std::string& p_xml_file,
			bool p_throw_on_file_not_exists = true);
		void load_configuration(const std::string& p_config_xml,
			const fe::ConfigRegion& p_region,
			std::map<std::string, std::size_t>& p_constants,
			std::map<std::string, std::pair<std::size_t, std::size_t>>& p_pointers,
			std::map<std::string, std::vector<byte>>& p_sets,
			std::map<std::string, std::map<byte, std::string>>& p_byte_maps,
			std::map<std::string, std::string>& p_strings,
			std::map<std::string, std::map<std::string, std::string>>& p_string_maps,
			std::map<std::string, bool>& p_bools,
			const std::vector<byte>& p_rom,
			bool p_throw_on_file_not_exists = true);
		void load_configuration(const pugi::xml_document& p_xml_doc,
			const fe::ConfigRegion& p_region,
			std::map<std::string, std::size_t>& p_constants,
			std::map<std::string, std::pair<std::size_t, std::size_t>>& p_pointers,
			std::map<std::string, std::vector<byte>>& p_sets,
			std::map<std::string, std::map<byte, std::string>>& p_byte_maps,
			std::map<std::string, std::string>& p_strings,
			std::map<std::string, std::map<std::string, std::string>>& p_string_maps,
			std::map<std::string, bool>& p_bools,
			const std::vector<byte>& p_rom);

		// utility
		bool region_match(const fe::ConfigRegion& current_region, const std::string& region_list,
			bool exact_match_only);
		bool matches_config_region(const pugi::xml_node& p_node,
			const fe::ConfigRegion& p_region);
		bool is_byte_match(const std::vector<byte>& p_rom, std::size_t p_offset,
			const std::vector<byte>& p_vals);
		bool evaluate_bool_condition(const std::vector<byte>& p_rom,
			const std::string& p_condition);
	}

}

#endif
