#include "Screen.h"
#include "./../common/klib/Kutil.h"
#include "./../common/klib/Bitreader.h"
#include "./../common/klib/Bitwriter.h"

using byte = unsigned char;

fe::Screen::Screen(const std::vector<byte>& p_rom, std::size_t p_offset) {
	std::vector<byte> l_screen{
std::vector<byte>(13 * 16, 0)
	};

	klib::Bitreader reader(p_offset);

	std::size_t idx{ 0 };

	while (idx < static_cast<std::size_t>(16 * 13)) {
		unsigned int l_control{ reader.read_int(p_rom, 2) };
		byte l_tile_val{ 0 };

		switch (l_control) {

		case 0:
			l_tile_val = l_screen[idx - 1];
			break;

		case 1:
			l_tile_val = l_screen[idx - 16];
			break;

		case 2:
			l_tile_val = l_screen[idx - 17];
			break;

		case 3:
			l_tile_val = static_cast<byte>(reader.read_int(p_rom, 8));
			break;

		default:
			// can't fall through to here
			break;
		}

		l_screen[idx++] = l_tile_val;

	}

	m_tilemap = klib::kutil::flat_vec_to_2d(l_screen, 16);
}

std::optional<std::size_t> fe::Screen::scroll_property_to_opt(byte p_val) const {
	if (p_val == 255)
		return std::nullopt;
	else
		return static_cast<std::size_t>(p_val);
}

void fe::Screen::set_scroll_properties(const std::vector<byte>& p_rom, std::size_t p_offset) {
	m_scroll_left = scroll_property_to_opt(p_rom.at(p_offset));
	m_scroll_right = scroll_property_to_opt(p_rom.at(p_offset + 1));
	m_scroll_up = scroll_property_to_opt(p_rom.at(p_offset + 2));
	m_scroll_down = scroll_property_to_opt(p_rom.at(p_offset + 3));
}

void fe::Screen::add_sprite(byte p_id, byte p_x, byte p_y) {
	m_sprites.push_back(fe::Sprite(p_id, p_x, p_y));
}

void fe::Screen::set_sprite_text(std::size_t p_sprite_no, byte p_text) {
	m_sprites.at(p_sprite_no).set_text(p_text);
}

std::vector<byte> fe::Screen::get_tilemap_bytes(void) const {
	klib::Bitwriter writer;

	auto l_tm_data{ klib::kutil::flatten_2d_vec(m_tilemap) };

	for (std::size_t i{ 0 }; i < l_tm_data.size(); ++i) {
		if (i >= 1 && l_tm_data[i] == l_tm_data[i - 1])
			writer.write_bits(0b00, 2);
		else if (i >= 16 && l_tm_data[i] == l_tm_data[i - 16])
			writer.write_bits(0b01, 2);
		else if (i >= 17 && l_tm_data[i] == l_tm_data[i - 17])
			writer.write_bits(0b10, 2);
		else {
			writer.write_bits(0b11, 2);
			writer.write_bits(l_tm_data[i], 8);
		}

	}

	return writer.get_data();
}

std::vector<byte> fe::Screen::get_sprite_bytes(void) const {
	std::vector<byte> l_result;

	// if there are both sprites with and without text - put the text ones first
	std::vector<std::size_t> l_txt_sprites, l_mute_sprites;

	for (std::size_t s{ 0 }; s < m_sprites.size(); ++s) {
		if (m_sprites[s].m_text_id.has_value())
			l_txt_sprites.push_back(s);
		else
			l_mute_sprites.push_back(s);
	}

	// hypothesis: we don't really need to end the text portion with 0xff if all the sprites use text
	// as the game doesn't look for text bytes beyond the actual screen sprite counts
	for (std::size_t i{ 0 }; i < l_txt_sprites.size(); ++i) {
		const auto& l_sprite{ m_sprites[l_txt_sprites[i]] };
		l_result.push_back(l_sprite.m_id);
		l_result.push_back(l_sprite.m_y * 16 + l_sprite.m_x);
	}

	for (std::size_t i{ 0 }; i < l_mute_sprites.size(); ++i) {
		const auto& l_sprite{ m_sprites[l_mute_sprites[i]] };
		l_result.push_back(l_sprite.m_id);
		l_result.push_back(l_sprite.m_y * 16 + l_sprite.m_x);
	}

	l_result.push_back(0xff);

	// add the text bytes
	for (std::size_t i{ 0 }; i < l_txt_sprites.size(); ++i) {
		const auto& l_sprite{ m_sprites[l_txt_sprites[i]] };
		l_result.push_back(l_sprite.m_text_id.value());
	}

	// is this really necessary if l_txt_sprites.size() == l_mute_sprites.size()
	// the original game seems to omit this sometimes in these cases
	l_result.push_back(0xff);

	return l_result;
}
