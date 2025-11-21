#include "Config.h"
#include "./xml/Xml_helper.h"
#include <stdexcept>

void fe::Config::load_definitions(const std::string& p_config_xml) {
	m_region_defs = xml::load_region_defs(p_config_xml);
}

void fe::Config::load_config_data(const std::string& p_config_xml) {
	xml::load_configuration(p_config_xml, m_region, m_constants,
		m_pointers, m_sets, m_byte_maps);
}

std::size_t fe::Config::constant(const std::string& p_id) const {
	if (m_constants.find(p_id) == end(m_constants))
		throw std::runtime_error("Constant '" + p_id + "' not found");
	else
		return m_constants.at(p_id);
}

std::pair<std::size_t, std::size_t> fe::Config::pointer(const std::string& p_id) const {
	if (m_pointers.find(p_id) == end(m_pointers))
		throw std::runtime_error("Pointer '" + p_id + "' not found");
	else
		return m_pointers.at(p_id);
}

const std::vector<byte>& fe::Config::vset(const std::string& p_id) const {
	static const std::vector<byte> empty_vec;

	if (m_sets.find(p_id) == end(m_sets))
		return empty_vec;
	else
		return m_sets.at(p_id);
}

std::set<byte> fe::Config::vset_as_set(const std::string& p_id) const {
	std::set<byte> result;
	const auto& vec{ vset(p_id) };

	for (byte b : vec)
		result.insert(b);

	return result;
}

const std::map<byte, std::string>& fe::Config::bmap(const std::string& p_id) const {
	static const std::map<byte, std::string> empty_map;
	
	if (m_byte_maps.find(p_id) == end(m_byte_maps))
		return empty_map;
	else
		return m_byte_maps.at(p_id);
}

std::map<std::string, byte> fe::Config::bmap_reverse(const std::string& p_id) const {
	std::map<std::string, byte> result;
	const auto& l_bmap{ bmap(p_id) };

	for (const auto& kv : l_bmap)
		result.insert(std::make_pair(kv.second, kv.first));

	return result;
}

void fe::Config::determine_region(const std::vector<byte>& p_rom) {
	for (const auto& reg : m_region_defs) {
		if (reg.m_filesize.has_value() && (reg.m_filesize.value() != p_rom.size()))
			continue;
		bool l_match{ true };

		for (const auto& sig : reg.m_defs) {
			if (!is_byte_match(p_rom, sig.first, sig.second)) {
				l_match = false;
				break;
			}
		}

		// we found a region match
		if (l_match) {
			m_region = reg.m_name;
			// no need to keep this in memory anymore
			m_region_defs.clear();
			return;
		}
	}

	throw std::runtime_error("ROM region could not be determined");
}

std::string fe::Config::get_region(void) const {
	return m_region;
}

std::vector<std::string> fe::Config::get_region_names(void) const {
	std::vector<std::string> result;

	for (const auto& regdef : m_region_defs)
		result.push_back(regdef.m_name);

	return result;
}

void fe::Config::set_region(const std::string& p_region_name) {
	m_region = p_region_name;
	m_region_defs.clear();
}

void fe::Config::clear(void) {
	m_region.clear();
	m_region_defs.clear();
	m_byte_maps.clear();
	m_constants.clear();
	m_pointers.clear();
	m_sets.clear();
}

bool fe::Config::is_byte_match(const std::vector<byte>& p_rom, std::size_t p_offset,
	const std::vector<byte>& p_vals) const {
	if (p_rom.size() < (p_offset + p_vals.size()))
		return false;

	for (std::size_t i{ 0 }; i < p_vals.size(); ++i)
		if (p_vals[i] != p_rom[p_offset + i])
			return false;

	return true;
}
