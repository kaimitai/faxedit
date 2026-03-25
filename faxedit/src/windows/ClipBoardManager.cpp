#include "ClipBoardManager.h"
#include "./../fe/xml/Xml_helper.h"
#include <format>
#include <SDL3/SDL.h>

std::string fe::ClipboardManager::get_clipboard_text(void) const {
	char* raw = SDL_GetClipboardText();

	if (!raw)
		throw std::runtime_error("Clipboard empty or unreadable");

	std::string text{ raw };
	SDL_free(raw);
	return text;
}

void fe::ClipboardManager::copy_tilemap(const std::vector<std::vector<byte>>& p_tilemap) const {
	std::string clp_txt{
		std::format("{} w={} h={}\n",
		HEADER_TILEMAP,
		p_tilemap.empty() ? 0 : p_tilemap[0].size(),
		p_tilemap.size())
	};

	for (const auto& bytes : p_tilemap)
		clp_txt += to_hex_string(bytes) + "\n";

	SDL_SetClipboardText(clp_txt.c_str());
}

void fe::ClipboardManager::copy_palette(const std::vector<byte>& p_palette) const {
	std::string clp_txt{ std::format("{}\n", HEADER_PALETTE) };
	clp_txt += to_hex_string(p_palette) + "\n";
	SDL_SetClipboardText(clp_txt.c_str());
}

std::string fe::ClipboardManager::to_hex_string(const std::vector<byte>& p_bytes) const {
	std::string result;

	for (std::size_t i{ 0 }; i < p_bytes.size(); ++i) {
		result += std::format("{:02X}", p_bytes[i]);
		if (i != p_bytes.size() - 1)
			result.push_back(' ');
	}

	return result;
}

std::vector<std::vector<byte>> fe::ClipboardManager::paste_tilemap() const {
	std::vector<std::vector<byte>> result;

	auto lines{ split_lines(get_clipboard_text()) };
	bool header_checked{ false };

	for (const auto& rawline : lines) {
		auto line{ fe::xml::trim_whitespace(rawline) };
		if (line.empty())
			continue;

		const auto tokens{ tokenize(line) };
		if (tokens.empty())
			continue;

		if (!header_checked) {
			validate_header_type(tokens.at(0), fe::ClipBoardType::Tilemap);
			header_checked = true;
			continue;
		}
		std::vector<byte> bytes;
		for (const auto& token : tokens)
			bytes.push_back(parse_hex_byte(token));
		result.push_back(bytes);
	}

	if (!header_checked)
		throw std::runtime_error("Clipboard did not contain a header");

	return result;
}

std::vector<byte> fe::ClipboardManager::paste_palette(void) const {
	std::vector<byte> palette;

	auto lines = split_lines(get_clipboard_text());
	bool header_checked = false;

	for (const auto& rawline : lines) {
		auto line = fe::xml::trim_whitespace(rawline);
		if (line.empty())
			continue;

		const auto tokens = tokenize(line);
		if (tokens.empty())
			continue;

		if (!header_checked) {
			validate_header_type(tokens[0], fe::ClipBoardType::Palette);
			header_checked = true;
			continue;
		}

		// Parse palette bytes
		for (const auto& token : tokens)
			palette.push_back(parse_hex_byte(token));
	}

	if (!header_checked)
		throw std::runtime_error("Clipboard did not contain a palette header");

	if (palette.size() != 16)
		throw std::runtime_error(
			std::format("Palette must contain exactly 16 entries, but clipboard has {}", palette.size())
		);

	// NES palette values must be 0–63
	for (size_t i{ 0 }; i < palette.size(); ++i) {
		if (palette[i] >= 0x40)
			throw std::runtime_error(
				std::format("Palette index {} at position {} is invalid (must be 0-63)",
					palette[i], i)
			);
	}

	return palette;
}

byte fe::ClipboardManager::parse_hex_byte(const std::string& token) const {
	if (token.size() != 2 ||
		!std::isxdigit((unsigned char)token[0]) ||
		!std::isxdigit((unsigned char)token[1]))
		throw std::runtime_error(std::format("Invalid hex byte: {}", token));

	return static_cast<byte>(std::stoi(token, nullptr, 16));
}

std::vector<std::string> fe::ClipboardManager::tokenize(const std::string& text) const {
	std::vector<std::string> tokens;
	std::string current;

	for (char c : text) {
		if (std::isspace(static_cast<unsigned char>(c))) {
			if (!current.empty()) {
				tokens.push_back(fe::xml::trim_whitespace(current));
				current.clear();
			}
		}
		else {
			current.push_back(c);
		}
	}

	if (!current.empty())
		tokens.push_back(fe::xml::trim_whitespace(current));

	return tokens;
}

std::vector<std::string> fe::ClipboardManager::split_lines(const std::string& text) {
	std::vector<std::string> lines;
	std::string current;

	for (char c : text) {
		if (c == '\n') {
			lines.push_back(current);
			current.clear();
		}
		else {
			current.push_back(c);
		}
	}

	if (!current.empty())
		lines.push_back(current);

	return lines;
}

void fe::ClipboardManager::validate_header_type(const std::string& text,
	fe::ClipBoardType p_type) const {
	if (string_to_clp_type(text) != p_type)
		throw std::runtime_error(std::format("Unexpected header '{}' in clipboard data", text));
}

fe::ClipBoardType fe::ClipboardManager::string_to_clp_type(const std::string& token) {
	if (token == HEADER_TILEMAP)
		return fe::ClipBoardType::Tilemap;
	else if (token == HEADER_PALETTE)
		return fe::ClipBoardType::Palette;
	else
		throw std::runtime_error(std::format("Invalid clipboard data type '{}'", token));
}
