#ifndef FE_PALETTEMUSICMAP_H
#define FE_PALETTEMUSICMAP_H

#include <optional>
#include <vector>

using byte = unsigned char;

namespace fe {

	struct PaletteMusicSlot {
		byte m_palette, m_music;

		PaletteMusicSlot(byte p_palette, byte p_music);
	};

	struct PaletteMusicMap {

		std::vector<fe::PaletteMusicSlot> m_slots;

		std::optional<byte> get_music(byte p_palette_no) const;
		void set_slot_palette(std::size_t p_index, byte p_palette_no);
		void set_slot_music(std::size_t p_index, byte p_music_no);
		void add_slot(void);
		void delete_slot(std::size_t p_slot_no);

		std::vector<byte> get_palette_bytes(void) const;
		std::vector<byte> get_music_bytes(void) const;
		std::size_t get_slot_count(void) const;
	};

}

#endif
