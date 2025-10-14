#include "Door.h"
#include <stdexcept>

fe::Door::Door(byte p_coords, byte p_param, byte p_dest_coords,
	const std::vector<byte>& p_rom, std::size_t p_door_param_offset,
	byte p_param_value_offset) :
	m_coords{ byte_to_coords(p_coords) },
	m_dest_coords{ byte_to_coords(p_dest_coords) }
{
	if (p_param < p_param_value_offset)
		throw std::runtime_error("Invalid door parameters");

	byte l_param{ static_cast<byte>(p_param - p_param_value_offset) };

	if (l_param == 0xff) {
		m_door_type = fe::DoorType::NextWorld;
	}
	else if (l_param == 0xfe) {
		m_door_type = fe::DoorType::PrevWorld;
	}
	else {
		std::size_t l_param_idx{ static_cast<std::size_t>(l_param) };
		std::size_t l_param_offset{ p_door_param_offset + 4 * l_param_idx };

		m_requirement = p_rom.at(l_param_offset + 2);
		m_unknown = p_rom.at(l_param_offset + 3);

		if (p_param >= 0x20) {
			m_door_type = fe::DoorType::Building;
			m_npc_bundle = p_rom.at(l_param_offset);
			m_dest_screen_id = p_rom.at(l_param_offset + 1);
		}
		else {
			m_door_type = fe::DoorType::SameWorld;

			m_dest_screen_id = p_rom.at(l_param_offset);
			m_dest_palette_id = p_rom.at(l_param_offset + 1);
		}

	}

}

std::pair<std::size_t, std::size_t> fe::Door::byte_to_coords(byte p_coords) const {
	return std::make_pair(
		static_cast<std::size_t>(p_coords % 16),
		static_cast<std::size_t>(p_coords / 16)
	);
}
