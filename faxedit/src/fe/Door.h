#ifndef FE_DOOR_H
#define FE_DOOR_H

#include <utility>
#include <vector>

using byte = unsigned char;

namespace fe {

	class Door {

		std::pair<std::size_t, std::size_t> m_coords, m_dest_coords;
		std::size_t m_dest;

		std::pair<std::size_t, std::size_t> byte_to_coords(byte p_coords) const;

	public:
		Door(byte p_coords, byte p_dest, byte p_dest_coords);

	};

}

#endif
