#include "NES_tile.h"
#include <stdexcept>

klib::NES_tile::NES_tile(const std::vector<byte>& p_rom_data, std::size_t p_offset)
	: m_tile_data{ std::vector<std::vector<byte>>(8, std::vector<byte>(8, 0)) }
{
	for (int y = 0; y < 8; ++y) {
		// bitplanes are 8 bytes apart
		byte b0 = p_rom_data[p_offset + y];        // plane 0
		byte b1 = p_rom_data[p_offset + 8 + y];    // plane 1

		for (int x = 0; x < 8; ++x) {
			int bit = 7 - x;
			byte low = (b0 >> bit) & 1;
			byte high = (b1 >> bit) & 1;
			m_tile_data[y][x] = (high << 1) | low;        // palette index 0–3
		}
	}

}

klib::NES_tile::NES_tile(void) :
	m_tile_data{ std::vector<std::vector<byte>>(8, std::vector<byte>(8, 0)) }
{
}

bool klib::NES_tile::operator<(const klib::NES_tile& rhs) const {
    return m_tile_data < rhs.m_tile_data;
}

bool klib::NES_tile::operator==(const klib::NES_tile& rhs) const {
    return m_tile_data == rhs.m_tile_data;
}

std::size_t klib::NES_tile::w(void) const {
	return m_tile_data.empty() ? 0 : m_tile_data[0].size();
}

std::size_t klib::NES_tile::h(void) const {
	return m_tile_data.size();
}

byte klib::NES_tile::get_color(std::size_t p_x, std::size_t p_y) const {
	return m_tile_data.at(p_y).at(p_x);
}

void klib::NES_tile::set_color(std::size_t p_x, std::size_t p_y, byte p_pal_idx) {
	m_tile_data.at(p_y).at(p_x) = p_pal_idx;
}

std::vector<byte> klib::NES_tile::to_bytes() const {
    if (m_tile_data.size() != 8 || m_tile_data[0].size() != 8) {
        throw std::runtime_error("NES_tile must be 8x8");
    }

    std::vector<byte> out(16, 0);

    for (int row = 0; row < 8; ++row) {
        byte plane0 = 0;
        byte plane1 = 0;

        for (int col = 0; col < 8; ++col) {
            byte val = m_tile_data[row][col] & 0x03; // ensure 0..3
            byte bit = 1 << (7 - col);          // NES packs MSB leftmost

            if (val & 0x01) plane0 |= bit;      // low bit
            if (val & 0x02) plane1 |= bit;      // high bit
        }

        out[static_cast<std::size_t>(row)] = plane0; // first 8 bytes = plane 0
        out[static_cast<std::size_t>(row + 8)] = plane1; // next 8 bytes = plane 1
    }

    return out;
}
