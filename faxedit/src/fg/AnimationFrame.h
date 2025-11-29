#ifndef FG_ANIMATIONFRAME_H
#define FG_ANIMATIONFRAME_H

#include <optional>
#include <vector>

using byte = unsigned char;

namespace fg {

	struct AnimationTile {
		byte m_tile_idx, m_sub_palette;
		bool m_vflip, m_hflip;

		AnimationTile(byte p_tile_no, byte p_tile_ctrl);
	};

	struct AnimationFrame {
		int m_w, m_h;
		char m_offset_x, m_offset_y;
		byte m_header;

		std::vector<std::vector<std::optional<AnimationTile>>> m_tilemap;

		AnimationFrame(const std::vector<byte>& p_rom,
			std::size_t p_offset);
	};

}

#endif
