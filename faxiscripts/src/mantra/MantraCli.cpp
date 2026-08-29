#include "MantraCli.h"
#include "MantraCli_constants.h"
#include "mantra/Mantra.h"
#include "common/klib/Kstring.h"
#include <algorithm>
#include <format>
#include <iostream>
#include <stdexcept>

fman::MantraCli::MantraCli(int argc, char** argv) :
	m_terse{ false },
	m_spawn_count{ 8 }
{
	int l_idx{ 2 };

	while (l_idx < argc)
		parse_argument(l_idx, argc, argv);
}

bool fman::MantraCli::is_terse(void) const {
	return m_terse;
}

void fman::MantraCli::parse_argument(int& p_idx, int argc, char** argv) {
	std::string l_arg{ argv[p_idx] };

	if (l_arg == cli::OPT_TERSE) {
		m_terse = true;
		++p_idx;
	}
	else if (l_arg == cli::OPT_SPAWN_COUNT) {
		if (p_idx >= argc - 1)
			throw std::runtime_error("Missing value for spawn count (-sc)");
		else {
			m_spawn_count = klib::str::parse_numeric(argv[p_idx + 1]);
			p_idx += 2;
		}
	}
	else {
		validate_argument(p_idx, argc, argv);
		std::string l_arg_value{ argv[p_idx + 1] };

		if (l_arg == cli::OPT_MANTRA)
			m_mantra = l_arg_value;
		else
			m_arguments[l_arg] = decode_param_values(l_arg, split_by_comma(l_arg_value));

		p_idx += 2;
	}
}

void fman::MantraCli::validate_argument(int p_idx, int argc, char** argv) const {
	std::string l_arg{ argv[p_idx] };

	if (!is_valid_argument(l_arg))
		throw std::runtime_error("Invalid argument " + l_arg);
	else if (p_idx + 1 >= argc)
		throw std::runtime_error("Missing argument value for argument " + l_arg);
}

std::vector<int> fman::MantraCli::decode_param_values(const std::string& p_type_key, const std::vector<std::string>& p_values) const {
	std::vector<int> result;

	for (const auto& v : p_values)
		result.push_back(static_cast<int>(get_index(p_type_key, v)));

	return result;
}

std::vector<std::string> fman::MantraCli::split_by_comma(const std::string& input) const {
	std::vector<std::string> result;

	if (input.empty())
		return result;

	std::size_t start = 0, end;

	while ((end = input.find(',', start)) != std::string::npos) {
		if (end > start) { // Only add non-empty substrings
			result.push_back(input.substr(start, end - start));
		}
		start = end + 1;
	}

	// Add the last substring if it's not empty
	if (start < input.size()) {
		result.push_back(input.substr(start));
	}

	return result;
}

std::size_t fman::MantraCli::get_index(const std::string& p_type_key, const std::string& p_needle) const {
	// 1. If the user typed a number, use it directly
	if (!p_needle.empty() && std::all_of(p_needle.begin(), p_needle.end(), ::isdigit) &&
		p_type_key == cli::OPT_LOCATION) {
		std::size_t locval{ static_cast<std::size_t>(std::stoul(p_needle)) };

		if (locval > c::MAX_LOCATION)
			throw std::runtime_error("Invalid location: " + std::to_string(locval));

		return locval;
	}

	auto l_matches{ find_matches(p_needle, cli::DECODE_VALUES.at(decode_type_key(p_type_key))) };

	if (l_matches.size() == 1)
		return l_matches[0].first;
	if (l_matches.empty())
		throw std::runtime_error("No match for parameter " + p_type_key + " and value " + p_needle);
	else
		throw std::runtime_error("Ambigous value for parameter " + p_type_key + " and value " + p_needle + ". Matches: "
			+ join_with_commas(l_matches));
}

fman::Mantra fman::MantraCli::get_mantra(void) const {
	fman::Mantra l_result{ m_mantra.empty() ?
		fman::Mantra(m_spawn_count) :
		fman::Mantra(m_mantra, m_spawn_count)
	};

	for (const auto& kv : m_arguments) {

		// equipped
		if (kv.first == cli::OPT_EQ_WEAPON)
			l_result.set_eq_weapon(kv.second.at(0));
		else if (kv.first == cli::OPT_EQ_ARMOR)
			l_result.set_eq_armor(kv.second.at(0));
		else if (kv.first == cli::OPT_EQ_SHIELD)
			l_result.set_eq_shield(kv.second.at(0));
		else if (kv.first == cli::OPT_EQ_MAGIC)
			l_result.set_eq_magic(kv.second.at(0));
		else if (kv.first == cli::OPT_EQ_ITEM)
			l_result.set_eq_item(kv.second.at(0));
		// stored
		else if (kv.first == cli::OPT_ST_WEAPON)
			for (int n : kv.second)
				l_result.add_stored_weapon(n);
		else if (kv.first == cli::OPT_ST_ARMOR)
			for (int n : kv.second)
				l_result.add_stored_armor(n);
		else if (kv.first == cli::OPT_ST_SHIELD)
			for (int n : kv.second)
				l_result.add_stored_shield(n);
		else if (kv.first == cli::OPT_ST_MAGIC)
			for (int n : kv.second)
				l_result.add_stored_magic(n);
		else if (kv.first == cli::OPT_ST_ITEM)
			for (int n : kv.second)
				l_result.add_stored_item(n);
		// rank, location
		else if (kv.first == cli::OPT_RANK)
			l_result.set_rank(kv.second.at(0));
		else if (kv.first == cli::OPT_LOCATION)
			l_result.set_location(kv.second.at(0));
		// special items, gamestate flags
		else if (kv.first == cli::OPT_SPECIAL)
			for (int n : kv.second)
				l_result.set_special_item(static_cast<std::size_t>(n), true);
		else if (kv.first == cli::OPT_GAMESTATE_FLAGS)
			for (int n : kv.second)
				l_result.set_gamestate_flag(static_cast<std::size_t>(n), true);
	}

	return l_result;
}

bool fman::MantraCli::is_match(const std::string& p_needle, const std::string& p_str) const {
	if (p_needle.size() > p_str.size())
		return false;
	else {
		for (std::size_t i{ 0 }; i < p_needle.size(); ++i)
			if (p_needle[i] != p_str[i])
				return false;

		return true;
	}
}

std::vector<std::pair<std::size_t, std::string>> fman::MantraCli::find_matches(const std::string& p_needle,
	const std::vector<std::string>& p_haystack) const {

	std::vector<std::pair<std::size_t, std::string>> l_result;

	for (std::size_t i{ 0 }; i < p_haystack.size(); ++i)
		if (is_match(p_needle, p_haystack[i]))
			l_result.push_back(std::make_pair(i, p_haystack[i]));

	return l_result;
}

std::string fman::MantraCli::join_with_commas(const std::vector<std::string>& p_strings) const {
	std::string l_result;

	for (size_t i{ 0 }; i < p_strings.size(); ++i) {
		l_result += p_strings[i];
		if (i != p_strings.size() - 1) {
			l_result += ", ";
		}
	}
	return l_result;
}

std::string fman::MantraCli::join_with_commas(const std::vector<std::pair<std::size_t, std::string>>& p_pairs) const {
	std::vector<std::string> l_strings;
	for (const auto& kv : p_pairs)
		l_strings.push_back(kv.second);

	return join_with_commas(l_strings);
}

// equipped and stored use the same IDs
// so only lookup by one of them
std::string fman::MantraCli::decode_type_key(const std::string& p_type_key) const {
	if (p_type_key == cli::OPT_EQ_WEAPON)
		return cli::OPT_ST_WEAPON;
	else if (p_type_key == cli::OPT_EQ_ARMOR)
		return cli::OPT_ST_ARMOR;
	else if (p_type_key == cli::OPT_EQ_SHIELD)
		return cli::OPT_ST_SHIELD;
	else if (p_type_key == cli::OPT_EQ_MAGIC)
		return cli::OPT_ST_MAGIC;
	else if (p_type_key == cli::OPT_EQ_ITEM)
		return cli::OPT_ST_ITEM;
	else
		return p_type_key;
}

bool fman::MantraCli::is_valid_argument(const std::string& p_type_key) const {
	return p_type_key == cli::OPT_TERSE ||
		p_type_key == cli::OPT_MANTRA ||
		cli::DECODE_VALUES.find(decode_type_key(p_type_key)) != end(cli::DECODE_VALUES);
}
