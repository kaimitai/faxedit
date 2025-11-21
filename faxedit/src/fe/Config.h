#ifndef FE_CONFIG_H
#define FE_CONFIG_H

#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "./xml/Xml_helper.h"

using byte = unsigned char;

namespace fe {

	class Config {
		std::vector<RegionDefinition> m_region_defs;
		std::string m_region;

		bool is_byte_match(const std::vector<byte>& p_rom, std::size_t p_offset,
			const std::vector<byte>& p_vals) const;

		// actual config data
		std::map<std::string, std::size_t> m_constants;
		std::map<std::string, std::pair<std::size_t, std::size_t>> m_pointers;
		std::map<std::string, std::vector<byte>> m_sets;
		std::map<std::string, std::map<byte, std::string>> m_byte_maps;

	public:
		Config(void) = default;
		std::string get_region(void) const;
		std::vector<std::string> get_region_names(void) const;
		void set_region(const std::string& p_region_name);
		void clear(void);

		// first, load all definitions from xml
		void load_definitions(const std::string& p_config_xml);
		// then, determine the region for our ROM
		void determine_region(const std::vector<byte>& p_rom);
		// finally load all the data for our region
		void load_config_data(const std::string& p_config_xml);

		std::size_t constant(const std::string& p_id) const;
		std::pair<std::size_t, std::size_t> pointer(const std::string& p_id) const;
		const std::vector<byte>& vset(const std::string& p_id) const;
		std::set<byte> vset_as_set(const std::string& p_id) const;
		const std::map<byte, std::string>& bmap(const std::string& p_id) const;
		std::map<std::string, byte> bmap_reverse(const std::string& p_id) const;
		std::vector<std::string> bmap_as_vec(const std::string& p_id, std::size_t p_size) const;
		std::vector<std::size_t> bmap_as_numeric_vec(const std::string& p_id, std::size_t p_size) const;
	};

}

#endif
