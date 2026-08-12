#include "AsmReader.h"
#include "fi_constants.h"
#include "Opcode.h"
#include "common/klib/Kstring.h"
#include <algorithm>
#include <format>
#include <cctype>
#include <stdexcept>
#include <set>
#include <string>
#include <utility>

fi::AsmReader::AsmReader(const std::map<byte, fi::Opcode>& p_opcodes) :
	m_opcodes{ p_opcodes }
{
}

void fi::AsmReader::read_asm(const fe::Config& p_config, const std::vector<std::string>& p_asm_code,
	std::size_t script_rg2_offset) {
	auto l_lines{ p_asm_code };
	fi::SectionType currentSection{ fi::SectionType::Defines };

	for (auto& line : l_lines) {
		line = trim(strip_comment(line));

		if (line == c::SECTION_DEFINES) {
			currentSection = fi::SectionType::Defines;
		}
		else if (line == c::SECTION_STRINGS) {
			currentSection = fi::SectionType::Strings;
		}
		else if (line == c::SECTION_SHOPS) {
			currentSection = fi::SectionType::Shops;
		}
		else if (line == c::SECTION_ISCRIPT) {
			currentSection = fi::SectionType::IScript;
		}
		else if (line == c::SECTION_TILEMAP_CHANGES) {
			currentSection = fi::SectionType::TilemapChanges;
		}
		else if (!line.empty())
			m_sections[currentSection].push_back(line);
	}

	parse_section_strings();
	parse_section_defines();
	parse_section_shops();
	parse_section_tilemap_changes();
	parse_section_iscript(p_config, script_rg2_offset);
}

void fi::AsmReader::parse_section_strings(void) {
	if (!m_sections.contains(SectionType::Strings))
		return;

	const auto& lines = m_sections.at(SectionType::Strings);
	std::map<int, std::string> temp;
	int max_index{ 0 };

	for (const auto& line : lines) {

		size_t colon_pos = line.find(':');
		if (colon_pos == std::string::npos) {
			throw std::runtime_error("Malformed string line: " + line);
		}

		int index{
			parse_numeric(line.substr(0, colon_pos))
		};

		size_t quote_start = line.find('"', colon_pos);
		size_t quote_end = line.rfind('"');

		if (quote_start == std::string::npos || quote_end == quote_start) {
			throw std::runtime_error("Malformed string content: " + line);
		}

		std::string value = line.substr(quote_start + 1, quote_end - quote_start - 1);

		if (temp.find(index) != end(temp))
			throw std::runtime_error(std::format("Multiple definitions for string with index {}", index));

		if (index == 0)
			throw std::runtime_error("Invalid string index 0. Strings start from index 1.");
		else
			temp[index] = value;

		max_index = std::max(max_index, index);
	}

	for (const auto& kv : temp)
		m_strings[kv.first] = fi::FaxString(kv.second);
}

void fi::AsmReader::parse_section_defines() {
	if (!m_sections.contains(SectionType::Defines))
		return;

	const auto& lines = m_sections.at(SectionType::Defines);

	for (const auto& line : lines) {
		// Must start with "define "
		if (!line.starts_with("define ")) {
			throw std::runtime_error("Malformed define line: " + line);
		}

		// Strip "define " prefix
		std::string rest = line.substr(7);
		size_t space_pos = rest.find(' ');
		if (space_pos == std::string::npos) {
			throw std::runtime_error("Malformed define line (missing value): " + line);
		}

		std::string key = trim(rest.substr(0, space_pos));
		std::string value_token = trim(rest.substr(space_pos + 1));

		if (key.empty() || value_token.empty()) {
			throw std::runtime_error("Empty key or value in define: " + line);
		}
		if (m_defines.contains(key)) {
			throw std::runtime_error("Duplicate define key: " + key);
		}

		std::size_t value = static_cast<std::size_t>(parse_numeric(value_token));
		m_defines[key] = value;
	}

}

void fi::AsmReader::parse_section_shops() {
	if (!m_sections.contains(SectionType::Shops))
		return;

	const auto& lines = m_sections.at(SectionType::Shops);
	std::map<std::size_t, Shop> result;

	for (const auto& line : lines) {
		size_t colon_pos = line.find(':');
		if (colon_pos == std::string::npos) {
			throw std::runtime_error("Malformed shop line: " + line);
		}

		std::size_t index = static_cast<std::size_t>(parse_numeric(line.substr(0, colon_pos)));
		std::string rest = trim(line.substr(colon_pos + 1));

		Shop shop;
		size_t pos = 0;
		while ((pos = rest.find('(', pos)) != std::string::npos) {
			size_t end = rest.find(')', pos);
			if (end == std::string::npos) {
				throw std::runtime_error("Unclosed item group in shop line: " + line);
			}

			std::string group = trim(rest.substr(pos + 1, end - pos - 1));
			size_t space = group.find(' ');
			if (space == std::string::npos) {
				throw std::runtime_error("Malformed item-price pair: " + group);
			}

			std::string item_token = group.substr(0, space);
			std::string price_token = group.substr(space + 1);

			byte item{
				static_cast<byte>(resolve_token(item_token))
			};

			uint16_t price{
				static_cast<uint16_t>(resolve_token(price_token))
			};

			shop.m_entries.push_back(fi::ShopEntry(item, price));
			pos = end + 1;
		}

		if (shop.m_entries.empty()) {
			throw std::runtime_error(std::format("Shop {} has no items.", index));
		}
		if (result.contains(index)) {
			throw std::runtime_error(std::format("Duplicate shop index {}.", index));
		}

		result[index] = std::move(shop);
	}

	m_shops = std::move(result);
}

void fi::AsmReader::parse_section_tilemap_changes() {
	if (!m_sections.contains(SectionType::TilemapChanges))
		return;

	const auto throw_invalid_line = [](const std::string& line) -> void {
		throw std::runtime_error(std::format("Unexpected line: '{}'", line));
		};

	const auto throw_missing_value = [](const std::string& line,
		const std::optional<byte>& p_world, const std::optional<byte>& p_screen) -> void {

			if (!p_world)
				throw std::runtime_error(std::format("World not known when parsing line: '{}'", line));
			else if (!p_screen)
				throw std::runtime_error(std::format("Screen for world {} not known when parsing line: '{}'", *p_world, line));
		};

	const auto& lines = m_sections.at(SectionType::TilemapChanges);
	std::optional<byte> current_world, current_screen;

	for (const auto& line : lines) {
		auto tokens = klib::str::split_whitespace(line);

		if (tokens.empty())
			continue;

		if (klib::str::str_equals_icase(tokens[0], "world")) {
			if (tokens.size() != 2)
				throw_invalid_line(line);

			current_world = static_cast<byte>(resolve_token(tokens[1]));
			current_screen.reset();

			continue;
		}
		else if (klib::str::str_equals_icase(tokens[0], "screen")) {
			if (tokens.size() != 2)
				throw_invalid_line(line);

			current_screen = static_cast<byte>(resolve_token(tokens[1]));

			continue;
		}
		else if (klib::str::str_equals_icase(tokens[0], "flag")) {
			if (tokens.size() != 2)
				throw_invalid_line(line);

			byte l_flag{ static_cast<byte>(resolve_token(tokens[1])) };

			throw_missing_value(line, current_world, current_screen);

			try {
				m_tilemap_changes.add_screen(current_world.value(), current_screen.value(), l_flag);
			}
			catch (const std::runtime_error& ex) {
				throw std::runtime_error(std::format("Error on line: '{}': {}", line, ex.what()));
			}

			continue;
		}
		else {
			const auto tiletokens{ klib::str::split_string(line, ',') };
			if (tiletokens.size() != 3)
				throw_invalid_line(line);

			throw_missing_value(line, current_world, current_screen);

			byte l_x{ static_cast<byte>(resolve_token(klib::str::trim(tiletokens[0]))) };
			byte l_y{ static_cast<byte>(resolve_token(klib::str::trim(tiletokens[1]))) };
			byte l_id{ static_cast<byte>(resolve_token(klib::str::trim(tiletokens[2]))) };

			try {
				m_tilemap_changes.add_change(current_world.value(), current_screen.value(), l_x, l_y, l_id);
			}
			catch (const std::runtime_error& ex) {
				throw std::runtime_error(std::format("Error on line: '{}': {}", line, ex.what()));
			}
		}
	}
}

std::string fi::AsmReader::strip_comment(const std::string& line) const {
	bool in_string = false;

	for (size_t i{ 0 }; i < line.size(); ++i) {
		if (line[i] == '"')
			in_string = !in_string;

		if (!in_string && line[i] == ';')
			return line.substr(0, i);
	}

	return line;
}

std::string fi::AsmReader::trim(const std::string& line) const {
	size_t start = 0;
	while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start]))) {
		++start;
	}

	size_t end = line.size();
	while (end > start && std::isspace(static_cast<unsigned char>(line[end - 1]))) {
		--end;
	}

	return line.substr(start, end - start);
}

int fi::AsmReader::parse_numeric(const std::string& token) const {
	if (token.empty()) throw std::runtime_error("Empty index token");

	int base = 10;
	std::string digits;
	if (token[0] == '%') {
		base = 2;
		digits = token.substr(1);
	}
	if (token[0] == '$') {
		base = 16;
		digits = token.substr(1);
	}
	else if (token.starts_with("0b") || token.starts_with("0B")) {
		base = 2;
		digits = token.substr(2);
	}
	else if (token.starts_with("0x") || token.starts_with("0X")) {
		base = 16;
		digits = token.substr(2);
	}
	else {
		digits = token;
	}

	size_t pos;

	int value{ 0 };

	try {
		value = std::stoi(digits, &pos, base);
	}
	catch (const std::invalid_argument&) {
		throw std::runtime_error("Invalid numeric token: '" + token + "' - not a valid number or define.");
	}
	catch (const std::out_of_range&) {
		throw std::runtime_error("Numeric token out of range: '" + token + "'");
	}

	if (pos != digits.size()) {
		throw std::runtime_error("Invalid numeric token: " + token);
	}
	return value;
}

std::size_t fi::AsmReader::resolve_token(const std::string& token) const {
	auto it = m_defines.find(token);
	if (it != m_defines.end()) {
		return it->second;
	}
	return static_cast<std::size_t>(parse_numeric(token));
}

bool fi::AsmReader::contains_label(const std::string& p_line) const {
	std::size_t colonPos = p_line.find(':');
	if (colonPos == std::string::npos) return false;

	std::string label = p_line.substr(0, colonPos);
	return !label.empty() && label.find(' ') == std::string::npos;
}

bool fi::AsmReader::is_string_token(const std::string& p_token) const {
	return
		p_token.size() >= 2 &&
		p_token.front() == '"' &&
		p_token.back() == '"';
}

std::string fi::AsmReader::extract_label(const std::string& p_line) const {
	std::size_t colonPos = p_line.find(':');
	return p_line.substr(0, colonPos);
}

bool fi::AsmReader::contains_entrypoint(const std::string& p_line) const {
	return str_begins_with(p_line, c::DIRECTIVE_ENTRYPOINT);
}

std::size_t fi::AsmReader::extract_entrypoint(const std::string& p_line) const {
	std::size_t spacePos = p_line.find(' ');
	if (spacePos == std::string::npos || spacePos + 1 >= p_line.size()) {
		throw std::runtime_error("Missing entrypoint index: " + p_line);
	}

	std::string indexStr = p_line.substr(spacePos + 1);
	indexStr = trim(indexStr);
	return resolve_token(indexStr);
}

bool fi::AsmReader::contains_textbox(const std::string& p_line) const {
	return str_begins_with(p_line, c::PSEUDO_OPCODE_TEXTBOX);
}

byte fi::AsmReader::extract_textbox(const std::string& p_line) const {
	std::size_t spacePos = p_line.find(' ');
	if (spacePos == std::string::npos || spacePos + 1 >= p_line.size()) {
		throw std::runtime_error("Missing textbox value: " + p_line);
	}

	std::string indexStr = p_line.substr(spacePos + 1);
	indexStr = trim(indexStr);
	return static_cast<byte>(resolve_token(indexStr));
}

bool fi::AsmReader::str_begins_with(const std::string& p_line,
	const std::string& p_start) const {
	if (p_start.size() > p_line.size())
		return false;
	else
		for (std::size_t i{ 0 }; i < p_start.size(); ++i)
			if (p_start[i] != p_line[i])
				return false;

	return true;
}

std::vector<std::string> fi::AsmReader::split_whitespace(const std::string& line) const {
	std::vector<std::string> tokens;
	std::size_t i = 0;
	while (i < line.size()) {
		// Skip leading whitespace, there really shouldn't be any
		while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) {
			++i;
		}

		if (i >= line.size()) break;

		// Handle quoted string
		if (line[i] == '"') {
			std::size_t start = i;
			++i;
			while (i < line.size() && line[i] != '"') {
				++i;
			}
			if (i < line.size()) ++i; // include closing quote
			tokens.emplace_back(line.substr(start, i - start));
		}
		else {
			// Handle regular token
			std::size_t start = i;
			while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i]))) {
				++i;
			}
			tokens.emplace_back(line.substr(start, i - start));
		}
	}
	return tokens;
}

std::string fi::AsmReader::to_lower(const std::string& str) {
	std::string result;

	for (char c : str)
		result.push_back(std::tolower(c));

	return result;
}

std::vector<byte> fi::AsmReader::get_string_bytes(const fe::Config& p_config) const {
	std::vector<byte> result;

	const std::map<std::string, byte> l_smap{ p_config.bmap_reverse(c::ID_STRING_CHAR_MAP) };

	for (const auto& kv : m_strings) {
		try {
			const auto& fsbytes{ kv.second.to_bytes(l_smap) };
			result.insert(end(result), begin(fsbytes), end(fsbytes));
		}
		catch (const std::runtime_error& ex) {
			throw std::runtime_error(std::format("Could not generate bytes for string with index {}: {}",
				kv.first, ex.what()));
		}
	}

	return result;
}

std::size_t fi::AsmReader::get_string_count(void) const {
	return m_strings.size();
}

std::map<std::string, int> fi::AsmReader::relocate_strings(
	const std::set<std::string>& p_strings) {
	std::map<int, std::string> new_string_table;
	std::map<std::string, int> result;

	int index{ 1 };
	for (const auto& kv : m_strings)
		new_string_table[kv.first] = kv.second.get_string();

	for (const auto& str : p_strings) {
		// Skip if string already exists (reserved or previously inserted)
		auto it = std::find_if(new_string_table.begin(), new_string_table.end(),
			[&str](const auto& kv) { return kv.second == str; });

		if (it != new_string_table.end())
			continue; // already present, skip

		// Find next available index
		while (new_string_table.find(index) != new_string_table.end())
			++index;

		new_string_table[index] = str;
	}

	// edge-case; highest reserved string index higher than any user idx
	int highest_reserved_idx{
		m_strings.empty() ? 0 : m_strings.rbegin()->first
	};
	// should rarely happen, but we insert empty strings in this case
	for (int i{ 1 }; i <= highest_reserved_idx; ++i)
		if (new_string_table.find(i) == end(new_string_table))
			new_string_table[i] = std::string();

	// turn it into fax-strings
	// also reverse the map we will return
	std::map<int, fi::FaxString> l_strs;
	for (const auto& kv : new_string_table) {
		l_strs.insert(std::make_pair(kv.first, kv.second));
		result.insert(std::make_pair(kv.second, kv.first));
	}
	m_strings = l_strs;

	if (m_strings.size() > 255)
		throw std::runtime_error(
			std::format("Only 255 unique strings can be used, but actual count is {}",
				m_strings.size())
		);

	return result;
}

const fh::TilemapChanges& fi::AsmReader::get_tilemap_changes() const {
	return m_tilemap_changes;
}
