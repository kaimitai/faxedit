#include "Door.h"

fe::Door::Door(byte p_coords, byte p_dest, byte p_dest_coords) :
	m_coords{ byte_to_coords(p_coords) },
	m_dest_coords{ byte_to_coords(p_dest_coords) },
	m_dest{ p_dest }
{}

std::pair<std::size_t, std::size_t> fe::Door::byte_to_coords(byte p_coords) const {
	return std::make_pair(
		static_cast<std::size_t>(p_coords % 16),
		static_cast<std::size_t>(p_coords / 16)
	);
}
