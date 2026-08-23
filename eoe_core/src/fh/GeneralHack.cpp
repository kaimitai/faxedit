#include "GeneralHack.h"
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
	{ 14, {
		fh::GeneralHackLib::FastStart,
	}},
	{ 15, {
		fh::GeneralHackLib::KillSwitch,	fh::GeneralHackLib::SameWorldTransPal2Mus,
	}},
	};
}

fh::GeneralHack::GeneralHack(const std::string& p_type) :
	type{ parse_general_hack_type(p_type) }
{
}

fh::GeneralHack::GeneralHack(const std::string& p_type, const std::map<std::string, std::string>& p_params) :
	type{ parse_general_hack_type(p_type) },
	params{ p_params }
{
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

std::vector<fh::GeneralHack> fh::parse_general_hacks(const std::string& p_str) {
	std::vector<fh::GeneralHack> result;

	for (const auto& raw_entry : klib::str::split_string(p_str, ',')) {
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
