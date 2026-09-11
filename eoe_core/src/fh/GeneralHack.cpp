#include "GeneralHack.h"
#include <algorithm>
#include <format>
#include <set>
#include <stdexcept>
#include "common/klib/Kstring.h"

namespace {
	fh::GeneralHackLib parse_general_hack_type(const std::string& p_type) {
		try {
			return klib::str::parse_enum_ci<fh::GeneralHackLib>(p_type);
		}
		catch (const std::runtime_error&) {
			throw std::runtime_error(std::format("Unknown general hack: '{}'", p_type));
		}
	}

	const std::map<byte, std::set<fh::GeneralHackLib>> GENERAL_HACK_BANKS{
	{ 12, {
		fh::GeneralHackLib::FlexibleItems,
	}},
	{ 14, {
		fh::GeneralHackLib::FastStart, fh::GeneralHackLib::QuestFlagItemDrops,
		fh::GeneralHackLib::BossLockedItems,
	}},
	{ 15, {
		fh::GeneralHackLib::KillSwitch,	fh::GeneralHackLib::SameWorldTransPal2Mus,
		fh::GeneralHackLib::FogRules, fh::GeneralHackLib::DynamicTilesets,
		fh::GeneralHackLib::PoisonPickup, fh::GeneralHackLib::TextSpeed,
		fh::GeneralHackLib::SRAM, fh::GeneralHackLib::BugFixes,
		fh::GeneralHackLib::AtlasDevFrameScheduler,	fh::GeneralHackLib::AtlasDevDayNightCycle,
		fh::GeneralHackLib::AtlasDevInfectedTint,
		fh::GeneralHackLib::AtlasDevTimeOfDay, fh::GeneralHackLib::AtlasDevFallControl,
		fh::GeneralHackLib::AtlasDevJumpControl,
		fh::GeneralHackLib::AtlasDevLadderControl,
		fh::GeneralHackLib::AtlasDevLadderCrown,
		fh::GeneralHackLib::AtlasDevSmartKeys,
	}},
	};

	const std::map<fh::GeneralHackLib, std::set<std::string>> GENERAL_HACK_PARAMS{
		{ fh::GeneralHackLib::FlexibleItems, { "buildings", "price", "selling", "state", "wep_indoors" } },
		{ fh::GeneralHackLib::KillSwitch, {} },
		{ fh::GeneralHackLib::SameWorldTransPal2Mus, {} },
		{ fh::GeneralHackLib::FogRules, { "rules" } },
		{ fh::GeneralHackLib::PoisonPickup, { "item", "sound", "script" }},
		{ fh::GeneralHackLib::TextSpeed, { "mask" }},
		{ fh::GeneralHackLib::AtlasDevFrameScheduler, {} },
		{ fh::GeneralHackLib::AtlasDevDayNightCycle, { "length" } },
		{ fh::GeneralHackLib::AtlasDevInfectedTint, { "colors", "pulse", "armed" } },
		{ fh::GeneralHackLib::AtlasDevTimeOfDay, { "hourlength", "start", "cell" } },
		{ fh::GeneralHackLib::AtlasDevJumpControl, { "coyote", "buffer", "shorthop", "airjumps", "switchable", "armed" } },
		{ fh::GeneralHackLib::AtlasDevLadderControl, { "up", "down", "wingup", "wingdown", "attack", "attackpose", "attackflag" } },
		{ fh::GeneralHackLib::AtlasDevFallControl, { "profile", "curve", "steer", "kind", "boot" } },
		{ fh::GeneralHackLib::AtlasDevLadderCrown, { "mode", "downhold", "align", "roompolicy", "rooms" } },
		{ fh::GeneralHackLib::AtlasDevSmartKeys, { "mode" } },
		{ fh::GeneralHackLib::FastStart, { "gold", "ring_of_elf" } },
		{ fh::GeneralHackLib::QuestFlagItemDrops, { "type" } },
		{ fh::GeneralHackLib::BossLockedItems, { "enemies" } },
		{ fh::GeneralHackLib::DynamicTilesets, { "data", "bank", "addr", "enter_building", "exit_building", "sameworld", "otherworld", "start_screen", "stage_doors"}},
		{ fh::GeneralHackLib::SRAM, { "ranges", "save_gold", "keep_gold", "color", "absolute_spawn" }},
		{ fh::GeneralHackLib::BugFixes, {} },
	};

	void validate_general_hack_params(fh::GeneralHackLib p_type,
		const std::map<std::string, std::string>& p_params) {
		const auto& valid_params{ GENERAL_HACK_PARAMS.at(p_type) };
		for (const auto& [name, value] : p_params) {
			(void)value;
			if (!valid_params.contains(name))
				throw std::runtime_error(std::format(
					"Unknown parameter '{}' for general hack '{}'",
					name, klib::str::enum_to_string(p_type)));
		}
	}

	std::vector<std::string> split_value(const std::string& p_value, char p_delim) {
		std::vector<std::string> result;

		for (const auto& elem : klib::str::split_string(p_value, p_delim))
			result.push_back(klib::str::trim(elem));

		return result;
	}

	std::vector<std::vector<std::string>> split_twice_value(
		const std::string& p_value, char p_delim_outer, char p_delim_inner) {

		std::vector<std::vector<std::string>> result;

		for (const auto& elem : split_value(p_value, p_delim_outer))
			result.push_back(split_value(elem, p_delim_inner));

		return result;
	}

	std::vector<std::pair<byte, std::optional<byte>>> parse_byte_optional_byte(
		const std::string& p_value, const std::string& p_id) {

		std::vector<std::pair<byte, std::optional<byte>>> result;

		for (const auto& vec : split_twice_value(p_value, '+', ':')) {
			if (vec.empty() || vec.size() > 2)
				throw std::runtime_error("invalid parameter format for " + p_id);

			const int first{ klib::str::parse_numeric(vec[0]) };
			if (first < 0 || first > 0xff)
				throw std::runtime_error(std::format(
					"General hack parameter '{}' element '{}' is not a valid byte", p_id, vec[0]));

			std::optional<byte> second;
			if (vec.size() == 2) {
				const int value{ klib::str::parse_numeric(vec[1]) };
				if (value < 0 || value > 0xff)
					throw std::runtime_error(std::format(
						"General hack parameter '{}' element '{}' is not a valid byte", p_id, vec[1]));

				second = static_cast<byte>(value);
			}

			result.emplace_back(static_cast<byte>(first), second);
		}

		return result;
	}

	std::vector<std::pair<word, byte>> parse_word_byte(
		const std::string& p_value, const std::string& p_id) {

		std::vector<std::pair<word, byte>> result;

		for (const auto& vec : split_twice_value(p_value, '+', ':')) {
			if (vec.size() != 2)
				throw std::runtime_error("invalid parameter format for " + p_id);

			const int first{ klib::str::parse_numeric(vec[0]) };
			if (first < 0 || first > 0xffff)
				throw std::runtime_error(std::format(
					"General hack parameter '{}' element '{}' is not a valid word", p_id, vec[0]));

			result.emplace_back(
				static_cast<word>(first),
				klib::str::parse_byte(vec[1]));
		}

		return result;
	}

	std::vector<std::vector<byte>> parse_twice_bytes(
		const std::string& p_value, const std::string& p_id, std::size_t p_inner_size) {

		std::vector<std::vector<byte>> result;

		for (const auto& vec : split_twice_value(p_value, '+', ':')) {
			std::vector<byte> inner_bytes;

			for (const auto& str : vec)
				inner_bytes.push_back(klib::str::parse_byte(str));

			if (p_inner_size != 0 && inner_bytes.size() != p_inner_size)
				throw std::runtime_error(std::format(
					"Hack parameter '{}' expected list with {} elements, but got {}",
					p_id, p_inner_size, inner_bytes.size()));

			result.push_back(std::move(inner_bytes));
		}

		return result;
	}
}

fh::GeneralHack::GeneralHack(const std::string& p_type) :
	type{ parse_general_hack_type(p_type) }
{
	validate_general_hack_params(type, params);
}

fh::GeneralHack::GeneralHack(const std::string& p_type, const std::map<std::string, std::string>& p_params) :
	type{ parse_general_hack_type(p_type) },
	params{ p_params }
{
	validate_general_hack_params(type, params);
}

fh::GeneralHackLib fh::GeneralHack::get_type() const {
	return type;
}

bool fh::GeneralHack::has_param(const std::string& p_id) const {
	return params.contains(p_id);
}

word fh::GeneralHack::get_word(const std::string& p_id) const {
	const int value{ klib::str::parse_numeric(params.at(p_id)) };

	if (value < 0 || value > 0xffff)
		throw std::runtime_error(
			std::format("General hack parameter '{}' is not a valid word", p_id));

	return static_cast<word>(value);
}

byte fh::GeneralHack::get_byte(const std::string& p_id) const {
	const int value{ klib::str::parse_numeric(params.at(p_id)) };

	if (value < 0 || value > 0xff)
		throw std::runtime_error(
			std::format("General hack parameter '{}' is not a valid byte", p_id));

	return static_cast<byte>(value);
}

bool fh::GeneralHack::get_bool(const std::string& p_id) const {
	return klib::str::parse_bool_ci(params.at(p_id));
}

const std::string& fh::GeneralHack::get_string(const std::string& p_id) const {
	return params.at(p_id);
}

std::vector<std::string> fh::GeneralHack::split(const std::string& p_id, char p_delim) const {
	return split_value(get_string(p_id), p_delim);
}

std::vector<std::vector<std::string>> fh::GeneralHack::split_twice(const std::string& p_id,
	char p_delim_outer, char p_delim_inner) const {
	return split_twice_value(get_string(p_id), p_delim_outer, p_delim_inner);
}

std::vector<std::pair<byte, std::optional<byte>>> fh::GeneralHack::split_byte_optional_byte(
	const std::string& p_id) const {
	return parse_byte_optional_byte(get_string(p_id), p_id);
}

std::vector<std::pair<word, byte>> fh::GeneralHack::split_word_byte(const std::string& p_id) const {
	return parse_word_byte(get_string(p_id), p_id);
}

std::vector<std::pair<word, byte>> fh::GeneralHack::split_word_byte(const std::string& p_id,
	const std::string& p_default) const {
	return parse_word_byte(string_or(p_id, p_default), p_id);
}

std::vector<std::vector<byte>> fh::GeneralHack::split_twice_bytes(const std::string& p_id,
	std::size_t p_inner_size) const {
	return parse_twice_bytes(get_string(p_id), p_id, p_inner_size);
}

// default value helpers
word fh::GeneralHack::word_or(const std::string& p_id, word p_default) const {
	return has_param(p_id) ? get_word(p_id) : p_default;
}

byte fh::GeneralHack::byte_or(const std::string& p_id, byte p_default) const {
	return has_param(p_id) ? get_byte(p_id) : p_default;
}

bool fh::GeneralHack::bool_or(const std::string& p_id, bool p_default) const {
	return has_param(p_id) ? get_bool(p_id) : p_default;
}

std::string fh::GeneralHack::string_or(const std::string& p_id, const std::string& p_default) const {
	return has_param(p_id) ? get_string(p_id) : p_default;
}

std::vector<fh::GeneralHack> fh::parse_general_hacks(const std::string& p_str) {
	std::vector<fh::GeneralHack> result;

	std::string entries{ p_str };
	std::replace(entries.begin(), entries.end(), '\n', ',');

	for (const auto& raw_entry : klib::str::split_string(entries, ',')) {
		const std::string entry{ klib::str::trim(raw_entry) };

		if (entry.empty())
			continue;

		const auto tokens{ klib::str::split_whitespace(entry) };

		if (tokens.empty())
			continue;

		std::map<std::string, std::string> params;

		for (std::size_t i{ 1 }; i < tokens.size();) {
			std::string key;
			std::string value;

			const auto eq{ tokens[i].find('=') };

			if (eq != std::string::npos) {
				key = tokens[i].substr(0, eq);
				value = tokens[i].substr(eq + 1);
				++i;

				// foo= value
				if (value.empty() && i < tokens.size())
					value = tokens[i++];
			}
			else {
				key = tokens[i++];

				// foo =bar / foo = bar
				if (i >= tokens.size())
					throw std::runtime_error("Missing value for parameter " + key);

				if (tokens[i] == "=") {
					++i;

					if (i >= tokens.size())
						throw std::runtime_error("Missing value for parameter " + key);

					value = tokens[i++];
				}
				else if (tokens[i].starts_with("=")) {
					value = tokens[i].substr(1);
					++i;
				}
				else {
					throw std::runtime_error("Expected '=' after parameter " + key);
				}
			}

			if (key.empty())
				throw std::runtime_error("Empty general hack parameter name");
			key = klib::str::to_lower(key);

			if (value.empty())
				throw std::runtime_error("Empty value for parameter " + key);

			if (!params.emplace(key, value).second)
				throw std::runtime_error("General hack parameter redefined: " + key);
		}

		result.emplace_back(tokens.front(), params);
	}

	return result;
}

std::vector<fh::GeneralHack> fh::filter_general_hacks(byte p_bank, const std::vector<fh::GeneralHack>& p_hacks) {
	std::vector<GeneralHack> result;

	const auto it{ GENERAL_HACK_BANKS.find(p_bank) };
	if (it == GENERAL_HACK_BANKS.end())
		return result;

	for (const auto& hack : p_hacks)
		if (it->second.contains(hack.get_type()))
			result.push_back(hack);

	return result;
}
