#ifndef FE_XML_HELPER_H
#define FE_XML_HELPER_H

#include <map>
#include <string>
#include <optional>
#include <utility>
#include <vector>
#include "Xml_constants.h"
#include "./../Game.h"
#include "./../EditorSettings.h"
#include "./../../common/pugixml/pugixml.hpp"
#include "./../../common/pugixml/pugiconfig.hpp"

using byte = unsigned char;

namespace fe {

	struct RegionDefinition {
		std::optional<std::size_t> m_filesize;
		std::string m_name;

		// vector of pairs {ROM offset -> vector of values matching at that offset}
		std::vector<std::pair<std::size_t, std::vector<byte>>> m_defs;
	};

	namespace xml {

		// eoe data
		void save_xml(const std::string p_filepath, const fe::Game& p_game);
		fe::Game load_xml(const std::string p_filepath);

		// eoe settings
		void save_settings_xml(const std::string& p_filepath, const fe::EditorSettings& p_settings);
		void load_settings_xml(const std::string& p_filepath, fe::EditorSettings& p_settings);

		void read_setting_float(pugi::xml_node p_root_node, const std::string& p_param_name,
			float& p_value);
		void read_setting_int(pugi::xml_node p_root_node, const std::string& p_param_name,
			int& p_value);
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

		// eoe config
		std::vector<RegionDefinition> load_region_defs(const std::string& p_xml_file,
			bool p_throw_on_file_not_exists = true);
		void load_configuration(const std::string& p_config_xml,
			const std::string& p_region_name,
			std::map<std::string, std::size_t>& p_constants,
			std::map<std::string, std::pair<std::size_t, std::size_t>>& p_pointers,
			std::map<std::string, std::vector<byte>>& p_sets,
			std::map<std::string, std::map<byte, std::string>>& p_byte_maps,
			bool p_throw_on_file_not_exists = true);

		// utility
		bool region_match(const std::string& current_region, const std::string& region_list);

		std::string join_bytes(const std::vector<byte>& p_bytes, bool p_hex = false);
		std::vector<byte> parse_byte_list(const std::string& input);
		fe::DoorType text_to_doortype(const std::string& p_str);
		std::string trim_whitespace(const std::string& p_value);
		std::vector<std::string> split_bytes(const std::string& p_values);
		std::size_t parse_numeric(const std::string& p_token);
		byte parse_numeric_byte(const std::string& p_token);
		std::string byte_to_hex(byte p_byte);

		// sprite gfx helpers
		void add_sprite_gfx_container(pugi::xml_node p_node, const fe::SpriteFrameCollection& p_coll,
			bool sparsify_last_bank = false);
		void add_chr_bank(pugi::xml_node p_node, std::size_t p_bank_no, const std::vector<klib::NES_tile>& p_tiles);
		void add_frame(pugi::xml_node p_node, std::size_t p_frame_no, const fe::SpriteAnimationFrame& p_frame);

		fe::SpriteFrameCollection read_sprite_gfx_container(pugi::xml_node p_node,
			bool expand_last_bank = false);
		std::vector<klib::NES_tile> read_chr_bank(pugi::xml_node p_node);
		fe::SpriteAnimationFrame read_frame(pugi::xml_node p_node);
	}

}

#endif
