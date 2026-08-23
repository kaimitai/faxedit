#ifndef FE_XML_HELPER_SETTINGS_H
#define FE_XML_HELPER_SETTINGS_H

#include "fe/EditorSettings.h"
#include "fe/xml/Xml_helper.h"
#include "common/pugixml/pugixml.hpp"
#include "Xml_constants.h"
#include <string>

using byte = unsigned char;

namespace fe {

	namespace xml {

		// eoe settings
		void save_settings_xml(const std::string& p_filepath, const fe::EditorSettings& p_settings);
		void load_settings_xml(const std::string& p_filepath, fe::EditorSettings& p_settings);

		void read_setting_float(pugi::xml_node p_root_node, const std::string& p_param_name,
			float& p_value);
		void read_setting_int(pugi::xml_node p_root_node, const std::string& p_param_name,
			int& p_value);
		void read_setting_byte(pugi::xml_node p_root_node, const std::string& p_param_name,
			byte& p_value);
		void read_setting_uint(pugi::xml_node p_root_node, const std::string& p_param_name,
			std::size_t& p_value);
		void read_setting_bool(pugi::xml_node p_root_node, const std::string& p_param_name,
			bool& p_value);
		pugi::xml_attribute find_settings_param_attr(pugi::xml_node p_root_node, const std::string p_param);
		template<class T>
		void add_setting(pugi::xml_node p_node, const std::string& p_param_name,
			T p_value) {
			auto n_keyval{ p_node.append_child(c::TAG_PARAM) };
			n_keyval.append_attribute(c::ATTR_NAME);
			n_keyval.attribute(c::ATTR_NAME).set_value(p_param_name);
			n_keyval.append_attribute(c::ATTR_VALUE);
			n_keyval.attribute(c::ATTR_VALUE).set_value(p_value);
		}

	}

}

#endif
