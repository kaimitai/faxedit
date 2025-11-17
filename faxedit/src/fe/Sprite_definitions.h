#ifndef FE_SPRITE_DEFINITIONS_H
#define FE_SPRITE_DEFINITIONS_H

#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "./../common/klib/NES_tile.h"

using byte = unsigned char;

namespace fe {

	enum class SpriteCategory : byte {
		Enemy = 0,
		DroppedItem = 1,
		NPC = 2,
		SpecialEffect = 3,
		GameTrigger = 4,
		Item = 5,
		MagicEffect = 6,
		Boss = 7,
		// not a real in-game category but we use this in the GUI
		Glitched = 8
	};

	struct AnimationFrame {

		byte m_w, m_h, m_hdr_control_byte;
		char m_offset_x, m_offset_y;
		bool m_disabled;

		std::vector<std::vector<std::optional<std::pair<byte, byte>>>>
			m_tilemap;

		AnimationFrame(const std::vector<byte>& p_rom_data, std::size_t& p_offset);
		AnimationFrame(byte p_w, byte p_h, byte p_hdr_byte, char p_x, char p_y, bool p_disabled);
	};

	struct Sprite_gfx_definiton {
		std::vector<klib::NES_tile> m_nes_tiles;
		std::vector<std::vector<byte>> m_sprite_palette;
		std::vector<fe::AnimationFrame> m_frames;
		fe::SpriteCategory m_category;
		int w, h;

		Sprite_gfx_definiton(const std::vector<klib::NES_tile>& p_tiles,
			const std::vector<std::vector<byte>>& m_palette,
			fe::SpriteCategory p_category);

		void add_frame(const fe::AnimationFrame& p_frame);
		void disable_frame(std::size_t p_frame_no);
		void calc_bounding_rect(void);
		void add_offsets(byte delta_x, byte delta_y = 0);
		// special function for the unused snake boss creature
		void stack_snake(void);

		static std::string SpriteCatToString(fe::SpriteCategory category);
	};

}

#endif
