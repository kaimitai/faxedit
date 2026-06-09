#include "Config.h"
#include "./xml/Xml_helper.h"
#include <algorithm>
#include <format>
#include <stdexcept>

void fe::Config::load_definitions(const std::string& p_config_xml,
	const std::string& p_config_override_xml) {
	m_region_defs = xml::load_region_defs(p_config_override_xml, false);
	auto base_defs{ xml::load_region_defs(p_config_xml) };
	m_region_defs.insert(end(m_region_defs), begin(base_defs), end(base_defs));
}

void fe::Config::load_config_data(const std::string& p_config_xml,
	const std::string& p_config_override_xml,
	const std::vector<byte>& p_rom) {
	xml::load_configuration(p_config_override_xml, m_region, m_constants,
		m_pointers, m_sets, m_byte_maps, m_bools, p_rom, false);
	xml::load_configuration(p_config_xml, m_region, m_constants,
		m_pointers, m_sets, m_byte_maps, m_bools, p_rom);
}

std::size_t fe::Config::constant(const std::string& p_id) const {
	if (m_constants.find(p_id) == end(m_constants))
		throw std::runtime_error("Constant '" + p_id + "' not found");
	else
		return m_constants.at(p_id);
}

std::size_t fe::Config::constant_or(const std::string& p_id, std::size_t p_default) const {
	if (has_constant(p_id))
		return constant(p_id);
	else
		return p_default;
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

const std::map<byte, std::size_t> fe::Config::bmap_numeric(const std::string& p_id) const {
	std::map<byte, std::size_t> result;
	const auto& l_bmap{ bmap(p_id) };

	for (const auto& kv : l_bmap)
		result.insert(std::make_pair(kv.first, xml::parse_numeric(kv.second)));

	return result;
}

std::map<std::string, byte> fe::Config::bmap_reverse(const std::string& p_id) const {
	std::map<std::string, byte> result;
	const auto& l_bmap{ bmap(p_id) };

	for (const auto& kv : l_bmap)
		result.insert(std::make_pair(kv.second, kv.first));

	return result;
}

std::map<std::size_t, byte> fe::Config::bmap_numeric_reverse(const std::string& p_id) const {
	std::map<std::size_t, byte> result;
	const auto& l_bmap{ bmap(p_id) };

	for (const auto& kv : l_bmap)
		result.insert(std::make_pair(xml::parse_numeric(kv.second), kv.first));

	return result;
}

std::vector<std::string> fe::Config::bmap_as_vec(const std::string& p_id,
	std::size_t p_size) const {
	const auto& l_map{ bmap(p_id) };

	std::vector<std::string> result;

	for (std::size_t i{ 0 }; i < (p_size > 0 ? p_size : 256); ++i) {
		auto iter{ l_map.find(static_cast<byte>(i)) };
		if (iter == end(l_map))
			throw std::runtime_error(std::format("Map with ID '{}' is missing value for index {}",
				p_id, i));
		else
			result.push_back(iter->second);
	}

	return result;
}

std::vector<std::size_t> fe::Config::bmap_as_numeric_vec(const std::string& p_id,
	std::size_t p_size) const {
	const auto vec{ bmap_as_vec(p_id, p_size) };
	std::vector<std::size_t> result;
	for (const auto& str : vec)
		result.push_back(xml::parse_numeric(str));

	return result;
}

std::map<byte, std::vector<byte>> fe::Config::bmap_as_numeric_vectors(const std::string& p_id) const {
	std::map<byte, std::vector<byte>> result;
	const auto btovec{ bmap(p_id) };

	for (const auto& kv : btovec)
		result.insert(std::make_pair(kv.first,
			xml::parse_byte_list(kv.second)));

	return result;
}

bool fe::Config::boolean(const std::string& p_id) const {
	if (m_bools.find(p_id) == end(m_bools))
		throw std::runtime_error(std::format("Configuration Boolean '{}' not found", p_id));
	else
		return m_bools.at(p_id);
}

bool fe::Config::boolean_or(const std::string& p_id, bool p_default) const {
	if (m_bools.find(p_id) == end(m_bools))
		return p_default;
	else
		return m_bools.at(p_id);
}

void fe::Config::determine_region(const std::vector<byte>& p_rom) {
	for (const auto& reg : m_region_defs) {
		if (reg.m_filesize.has_value() && (reg.m_filesize.value() != p_rom.size()))
			continue;
		bool l_match{ true };

		for (const auto& sig : reg.m_defs) {
			if (!fe::xml::is_byte_match(p_rom, sig.first, sig.second)) {
				l_match = false;
				break;
			}
		}

		// we found a region match
		if (l_match) {
			m_region.region = reg.m_name;
			m_region.compatible_regions = reg.m_compatible_regions;
			// no need to keep this in memory anymore
			m_region_defs.clear();
			return;
		}
	}

	throw std::runtime_error("ROM region could not be determined");
}

std::string fe::Config::get_region(void) const {
	return m_region.region;
}

std::vector<std::string> fe::Config::get_region_names(void) const {
	std::vector<std::string> result;

	for (const auto& regdef : m_region_defs)
		result.push_back(regdef.m_name);

	return result;
}

void fe::Config::set_region(const std::string& p_region_name) {
	m_region.region = p_region_name;
	for (const auto& reg : m_region_defs)
		if (reg.m_name == p_region_name)
			m_region.compatible_regions = reg.m_compatible_regions;

	m_region_defs.clear();
}

void fe::Config::clear(void) {
	m_region.region.clear();
	m_region.compatible_regions.clear();
	m_region_defs.clear();
	m_byte_maps.clear();
	m_constants.clear();
	m_pointers.clear();
	m_sets.clear();
	m_bools.clear();
}

bool fe::Config::has_constant(const std::string& p_id) const {
	return m_constants.find(p_id) != end(m_constants);
}

std::string fe::Config::to_string(void) const {
	std::string result{ std::format("Region: '{}'\n", m_region.region) };

	if (!m_region.compatible_regions.empty()) {
		result += std::format("Compatible regions ({}): ", m_region.compatible_regions.size());

		std::vector<std::string> regs{ begin(m_region.compatible_regions), end(m_region.compatible_regions) };
		std::sort(begin(regs), end(regs));

		for (const auto& reg : regs)
			result += std::format("{} ", reg);
	}

	result += "\n--- constants ---\n";
	for (const auto& kv : m_constants)
		result += std::format("{}=${:x}\n", kv.first, kv.second);

	result += "\n--- pointers ---\n";
	for (const auto& kv : m_pointers)
		result += std::format("{}: offset=${:x}, zero=${:x}\n", kv.first,
			kv.second.first, kv.second.second);

	result += "\n--- byte to string maps ---\n";
	for (const auto& kv : m_byte_maps) {
		result += std::format("map name: {}\n", kv.first);
		for (const auto& kkv : kv.second)
			result += std::format("  ${:02x}: '{}'\n", kkv.first, kkv.second);
	}

	result += "\n--- sets ---\n";
	for (const auto& kv : m_sets) {
		result += std::format("{}: ", kv.first);

		for (byte b : kv.second)
			result += std::format("${:02x} ", b);
		result += "\n";
	}

	result += "\n--- booleans ---\n";
	for (const auto& kv : m_bools)
		result += std::format("{}={}\n", kv.first, kv.second);

	return result;
}
