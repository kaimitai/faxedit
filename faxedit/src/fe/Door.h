#ifndef FE_DOOR_H
#define FE_DOOR_H

#include <string>
#include <utility>
#include <vector>

using byte = unsigned char;

namespace fe {

	enum DoorType { SameWorld, Building, PrevWorld, NextWorld };

	struct Door {

		std::pair<byte, byte> m_coords, m_dest_coords;
		fe::DoorType m_door_type;

		// doors with parameter < 0xfe
		byte m_requirement, m_unknown;

		// building doors with 0x20 <= parameter 0x20 < 0xfe
		byte m_npc_bundle;

		// intra-chunk doors with parameter < 0x20
		byte m_dest_palette_id;

		// for building doors this is the screen id in the buildings chunk
		// for intra-chunk doors this is the screen in in the same chunk
		byte m_dest_screen_id;

		std::pair<byte, byte> byte_to_coords(byte p_coords) const;

		Door(byte p_coords, byte p_param, byte p_dest_coords,
			const std::vector<byte>& p_rom, std::size_t p_door_param_offset,
			byte p_param_value_offset);

		Door(void);

	};

}

#endif
