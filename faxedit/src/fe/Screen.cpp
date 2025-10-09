#include "Screen.h"
#include "./../common/klib/Kutil.h"
#include "./../common/klib/Bitreader.h"

using byte = unsigned char;

fe::Screen::Screen(const std::vector<byte>& p_rom, std::size_t p_offset) {
	std::vector<byte> l_screen{
std::vector<byte>(13 * 16, 0)
	};

	klib::Bitreader reader(p_offset);

	std::size_t idx{ 0 };

	while (idx < (16 * 13)) {
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

void fe::Screen::add_door(byte p_coords, byte p_dest, byte p_dest_coords) {
	m_doors.push_back(fe::Door(p_coords, p_dest, p_dest_coords));
}

const std::vector<std::vector<byte>>& fe::Screen::get_tilemap(void) const {
	return m_tilemap;
}
