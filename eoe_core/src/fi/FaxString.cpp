#include "FaxString.h"
#include "fi_constants.h"
#include <format>
#include <stdexcept>

fi::FaxString::FaxString(const std::string& p_string) :
	m_string{ p_string }
{
}

const std::string& fi::FaxString::get_string(void) const {
	return m_string;
}

std::vector<byte> fi::FaxString::to_bytes(const std::map<std::string, byte>& p_smap) const {

	const auto parse_fallback_token = [](const std::string& token) -> byte {
		if (token.size() < 3 || token.front() != '<' || token.back() != '>')
			throw std::runtime_error("Malformed token: " + token);

		std::string body = token.substr(1, token.size() - 2); // strip < and >

		std::string number;
		int base = 10;

		if (body.starts_with("$")) {
			number = body.substr(1);
			base = 16;
		}
		else {
			number = body;
			if (number.starts_with("0x") || number.starts_with("0X")) {
				number = number.substr(2);
				base = 16;
			}
		}

		try {
			int value = std::stoi(number, nullptr, base);
			if (value < 0 || value > 255)
				throw std::runtime_error("Byte value out of range: " + token);
			return static_cast<byte>(value);
		}
		catch (...) {
			throw std::runtime_error("Invalid numeric token: " + token);
		}
		};

	std::vector<byte> result;
	std::size_t pos = 0;

	while (pos < m_string.size()) {
		if (m_string[pos] == '<') {
			auto end = m_string.find('>', pos);
			if (end == std::string::npos)
				throw std::runtime_error("Unterminated token in string: " + m_string);

			std::string token = m_string.substr(pos, end - pos + 1); // includes < and >
			auto iter = p_smap.find(token);
			if (iter != p_smap.end()) {
				result.push_back(iter->second);
			}
			else {
				result.push_back(parse_fallback_token(token));
			}

			pos = end + 1;
		}
		else {
			std::string ch(1, m_string[pos]);
			auto iter = p_smap.find(ch);
			if (iter == p_smap.end())
				throw std::runtime_error(std::format("Unknown character: '{}'", ch));
			result.push_back(iter->second);
			pos++;
		}
	}

	result.push_back(0xff); // Terminator
	return result;
}
