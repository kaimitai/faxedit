#ifndef FE_PALETTEMUSICMAP_H
#define FE_PALETTEMUSICMAP_H

#include <optional>
#include <vector>

using byte = unsigned char;

namespace fe {

	struct PaletteMusicSlot {
		byte m_palette, m_music;

		PaletteMusicSlot(byte p_palette, byte p_music) :
			m_palette{ p_palette },
			m_music{ p_music }
		{
		}
	};

	struct PaletteMusicMap {

		std::vector<fe::PaletteMusicSlot> m_slots;

		std::optional<byte> get_music(byte p_palette_no) const {
			for (const auto& slot : m_slots)
				if (slot.m_palette == p_palette_no)
					return slot.m_music;

			return std::nullopt;
		}

		void set_slot_palette(std::size_t p_index, byte p_palette_no) {
			m_slots.at(p_index).m_palette = p_palette_no;
		}

		void set_slot_music(std::size_t p_index, byte p_music_no) {
			m_slots.at(p_index).m_music = p_music_no;
		}
	};

}

#endif
