#include "AnimationFrame.h"

fg::AnimationFrame::AnimationFrame(const std::vector<byte>& p_rom, std::size_t p_offset) :
	m_w{ static_cast<byte>(p_rom.at(p_offset) % 16 + 1) },
	m_h{ static_cast<byte>(p_rom.at(p_offset) / 16 + 1) },
	m_offset_x{ static_cast<char>(p_rom.at(p_offset + 1)) },
	m_offset_y{ static_cast<char>(p_rom.at(p_offset + 2)) },
	m_header{ p_rom.at(p_offset + 3) }
{
	p_offset += 4;

	for (std::size_t y{ 0 }; y < m_h; ++y) {

		std::vector<std::optional<fg::AnimationTile>> l_row;

		for (std::size_t x{ 0 }; x < m_w; ++x) {
			byte tile_no{ p_rom.at(p_offset++) };

			if (tile_no == 0xff) {
				l_row.push_back(std::nullopt);
			}
			else {
				byte l_attr{ p_rom.at(p_offset++) };
				l_row.push_back(
					AnimationTile(tile_no, l_attr)
				);
			}
		}

		m_tilemap.push_back(l_row);
	}
}

fg::AnimationTile::AnimationTile(byte p_tile_no, byte p_tile_ctrl) :
	m_tile_idx{ p_tile_no },
	m_sub_palette{ static_cast<byte>(p_tile_ctrl & 0b11) },
	m_hflip{ static_cast<bool>(p_tile_ctrl & 0x40) },
	m_vflip{ static_cast<bool>(p_tile_ctrl & 0x80) }
{
}
