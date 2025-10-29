#ifndef FE_SCREEN_H
#define FE_SCREEN_H

#include <optional>
#include <vector>
#include "Sprite_set.h"
#include "Door.h"
#include "InterChunkScroll.h"
#include "IntraChunkScroll.h"

using byte = unsigned char;
using Tilemap = std::vector<std::vector<byte>>;

namespace fe {

	struct Screen {

		Tilemap m_tilemap;
		std::vector<fe::Door> m_doors;
		fe::Sprite_set m_sprite_set;
		std::optional<byte> m_scroll_left, m_scroll_right,
			m_scroll_up, m_scroll_down;
		std::optional<fe::InterChunkScroll> m_interchunk_scroll;
		std::optional<fe::IntraChunkScroll> m_intrachunk_scroll;

		// constructor that reads ROM data and extracts it
		Screen(const std::vector<byte>& p_rom, std::size_t p_offset);
		Screen(void);
		// procedure that reads scroll properties from rom
		void set_scroll_properties(const std::vector<byte>& p_rom, std::size_t p_offset);
		void add_sprite(byte p_id, byte p_x, byte p_y);
		void set_sprite_text(std::size_t p_sprite_no, byte p_text);

		std::vector<byte> get_tilemap_bytes(void) const;

	private:
		std::optional<byte> scroll_property_to_opt(byte p_val) const;

	};

}

#endif
