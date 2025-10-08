#include "NES_tile.h"

klib::NES_tile::NES_tile(const std::vector<byte>& p_rom_data, std::size_t p_offset)
	: m_tile_data { std::vector<std::vector<byte>>(8, std::vector<byte>(8, 0)) }
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

std::size_t klib::NES_tile::w(void) const {
	return m_tile_data.empty() ? 0 : m_tile_data[0].size();
}

std::size_t klib::NES_tile::h(void) const {
	return m_tile_data.size();
}

byte klib::NES_tile::get_color(std::size_t p_x, std::size_t p_y) const {
	return m_tile_data.at(p_y).at(p_x);
}
