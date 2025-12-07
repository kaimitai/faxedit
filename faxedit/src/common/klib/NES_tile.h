#ifndef KLIB_NES_TILE
#define KLIB_NES_TILE

#include <vector>

using byte = unsigned char;

namespace klib {

	class NES_tile {

		std::vector<std::vector<byte>> m_tile_data;

	public:
		NES_tile(const std::vector<byte>& p_rom_data, std::size_t p_offset);
		NES_tile(void);
		bool operator<(const klib::NES_tile& rhs) const;
		bool operator==(const klib::NES_tile& rhs) const;
		std::vector<byte> to_bytes(void) const;

		std::size_t w(void) const;
		std::size_t h(void) const;
		byte get_color(std::size_t p_x, std::size_t p_y) const;

		void set_color(std::size_t p_x, std::size_t p_y, byte p_pal_idx);
	};

}

#endif
