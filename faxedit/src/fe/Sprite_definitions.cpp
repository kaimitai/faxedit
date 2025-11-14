#include "Sprite_definitions.h"

fe::AnimationFrame::AnimationFrame(const std::vector<byte>& p_rom, std::size_t& p_offset) {
	m_w = static_cast<byte>(p_rom.at(p_offset) % 16 + 1);
	m_h = static_cast<byte>(p_rom.at(p_offset) / 16 + 1);
	m_offset_x = p_rom.at(p_offset + 1);
	m_offset_y = p_rom.at(p_offset + 2);
	m_hdr_control_byte = p_rom.at(p_offset + 3);

	p_offset += 4;

	for (std::size_t y{ 0 }; y < m_h; ++y) {

		std::vector<std::optional<std::pair<byte, byte>>> l_row;

		for (std::size_t x{ 0 }; x < m_w; ++x) {
			byte tile_no{ p_rom.at(p_offset++) };

			if (tile_no == 0xff) {
				l_row.push_back(std::nullopt);
			}
			else {
				byte l_attr{ p_rom.at(p_offset++) };
				l_row.push_back(
					std::make_pair(tile_no, l_attr)
				);
			}
		}

		m_tilemap.push_back(l_row);
	}
}

fe::Sprite_gfx_definiton::Sprite_gfx_definiton(const std::vector<klib::NES_tile>& p_tiles,
	const std::vector<std::vector<byte>>& p_palette,
	const fe::AnimationFrame& p_frame) :
	m_nes_tiles{ p_tiles },
	m_sprite_palette{ p_palette },
	m_frame{ p_frame }
{
}
