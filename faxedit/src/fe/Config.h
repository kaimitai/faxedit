#ifndef FE_CONFIG_H
#define FE_CONFIG_H

#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include "./xml/Xml_helper.h"

using byte = unsigned char;

namespace fe {

	struct ConfigRegion {
		std::string region;
		std::unordered_set<std::string> compatible_regions;
	};

	class Config {
		std::vector<RegionDefinition> m_region_defs;
		ConfigRegion m_region;

		bool is_byte_match(const std::vector<byte>& p_rom, std::size_t p_offset,
			const std::vector<byte>& p_vals) const;

		// actual config data
		std::map<std::string, std::size_t> m_constants;
		std::map<std::string, std::pair<std::size_t, std::size_t>> m_pointers;
		std::map<std::string, std::vector<byte>> m_sets;
		std::map<std::string, std::map<byte, std::string>> m_byte_maps;
		std::map<std::string, bool> m_bools;

	public:
		Config(void) = default;
		std::string to_string(void) const;

		std::string get_region(void) const;
		std::vector<std::string> get_region_names(void) const;
		void set_region(const std::string& p_region_name);
		void clear(void);

		bool has_constant(const std::string& p_id) const;

		// first, load all definitions from xml
		void load_definitions(const std::string& p_config_xml, const std::string& p_config_override_xml);
		// then, determine the region for our ROM
		void determine_region(const std::vector<byte>& p_rom);
		// finally load all the data for our region
		void load_config_data(const std::string& p_config_xml, const std::string& p_config_override_xml);

		std::size_t constant(const std::string& p_id) const;
		std::size_t constant_or(const std::string& p_id, std::size_t p_default) const;
		std::pair<std::size_t, std::size_t> pointer(const std::string& p_id) const;
		const std::vector<byte>& vset(const std::string& p_id) const;
		std::set<byte> vset_as_set(const std::string& p_id) const;
		const std::map<byte, std::string>& bmap(const std::string& p_id) const;
		const std::map<byte, std::size_t> bmap_numeric(const std::string& p_id) const;
		std::map<std::string, byte> bmap_reverse(const std::string& p_id) const;
		std::map<std::size_t, byte> bmap_numeric_reverse(const std::string& p_id) const;
		std::vector<std::string> bmap_as_vec(const std::string& p_id, std::size_t p_size) const;
		std::vector<std::size_t> bmap_as_numeric_vec(const std::string& p_id, std::size_t p_size) const;
		std::map<byte, std::vector<byte>> bmap_as_numeric_vectors(const std::string& p_id) const;
		bool boolean(const std::string& p_id) const;
		bool boolean_or(const std::string& p_id, bool p_default) const;
	};

}

#endif
