#include "Sprite_definitions.h"
#include <algorithm>

fe::AnimationFrame::AnimationFrame(const std::vector<byte>& p_rom, std::size_t& p_offset) :
	m_w{ static_cast<byte>(p_rom.at(p_offset) % 16 + 1) },
	m_h{ static_cast<byte>(p_rom.at(p_offset) / 16 + 1) },
	m_offset_x{ static_cast<char>(p_rom.at(p_offset + 1)) },
	m_offset_y{ static_cast<char>(p_rom.at(p_offset + 2)) },
	m_hdr_control_byte{ p_rom.at(p_offset + 3) },
	m_disabled{ false }
{
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

fe::AnimationFrame::AnimationFrame(byte p_w, byte p_h, byte p_hdr_byte, char p_x, char p_y, bool p_disabled) :
	m_w{ p_w },
	m_h{ p_h },
	m_hdr_control_byte{ p_hdr_byte },
	m_offset_x{ p_x },
	m_offset_y{ p_y },
	m_disabled{ p_disabled }
{
}

fe::Sprite_gfx_definiton::Sprite_gfx_definiton(
	const std::vector<klib::NES_tile>& p_tiles,
	const std::vector<std::vector<byte>>& p_palette,
	fe::SpriteCategory p_category) :
	m_nes_tiles{ p_tiles },
	m_sprite_palette{ p_palette },
	m_category{ p_category },
	w{ 8 },
	h{ 8 }
{
}

void fe::Sprite_gfx_definiton::add_frame(const fe::AnimationFrame& p_frame) {
	m_frames.push_back(p_frame);
}

void fe::Sprite_gfx_definiton::disable_frame(std::size_t p_frame_no) {
	if (p_frame_no < m_frames.size())
		m_frames[p_frame_no].m_disabled = true;
}

void fe::Sprite_gfx_definiton::calc_bounding_rect(void) {

	for (const auto& frame : m_frames) {
		if (!frame.m_disabled) {
			w = std::max(static_cast<int>(8 * frame.m_w + frame.m_offset_x), w);
			h = std::max(static_cast<int>(8 * frame.m_h + frame.m_offset_y), h);
		}
	}

}

void fe::Sprite_gfx_definiton::add_offsets(byte delta_x, byte delta_y) {
	for (auto& frame : m_frames) {
		frame.m_offset_x += delta_x;
		frame.m_offset_y += delta_y;
	}
}

void fe::Sprite_gfx_definiton::stack_snake(void) {
	std::vector<fe::AnimationFrame> newframes;

	if (m_frames.size() < 2)
		return;
	const auto& body{ m_frames.back() };

	for (std::size_t i{ 0 }; i < m_frames.size() - 1; ++i) {
		m_frames[i].m_offset_y = -48;

		for (std::size_t j{ 0 }; j < 3; ++j)
			for (const auto& row : body.m_tilemap) {
				m_frames[i].m_tilemap.push_back(row);
				m_frames[i].m_h += 1;
			}
	}

	m_frames.pop_back();
}

std::string fe::Sprite_gfx_definiton::SpriteCatToString(fe::SpriteCategory category) {
	switch (category) {
	case SpriteCategory::Enemy: return "Enemy";
	case SpriteCategory::DroppedItem: return "Dropped Item";
	case SpriteCategory::NPC: return "NPC";
	case SpriteCategory::SpecialEffect: return "Special Effect";
	case SpriteCategory::GameTrigger: return "Game Trigger";
	case SpriteCategory::Item: return "Item";
	case SpriteCategory::MagicEffect: return "Magic Effect";
	case SpriteCategory::Boss: return "Boss";
	case SpriteCategory::Glitched: return "Glitch";
	default: return "Unknown";
	}
}
