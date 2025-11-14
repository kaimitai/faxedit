#ifndef FE_SPRITE_DEFINITIONS_H
#define FE_SPRITE_DEFINITIONS_H

#include <optional>
#include <utility>
#include <vector>
#include "./../common/klib/NES_tile.h"

using byte = unsigned char;

namespace fe {

	struct AnimationFrame {

		byte m_w, m_h, m_offset_x, m_offset_y, m_hdr_control_byte;

		std::vector<std::vector<std::optional<std::pair<byte, byte>>>>
			m_tilemap;

		AnimationFrame(const std::vector<byte>& p_rom_data, std::size_t& p_offset);
	};

	struct Sprite_gfx_definiton {
		std::vector<klib::NES_tile> m_nes_tiles;
		std::vector<std::vector<byte>> m_sprite_palette;
		fe::AnimationFrame m_frame;

		Sprite_gfx_definiton(const std::vector<klib::NES_tile>& p_tiles,
			const std::vector<std::vector<byte>>& m_palette,
			const fe::AnimationFrame& p_frame);
	};

}

#endif
