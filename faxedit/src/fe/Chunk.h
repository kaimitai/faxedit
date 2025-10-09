#ifndef FE_CHUNK_H
#define FE_CHUNK_H

#include <vector>
#include "Screen.h"

using byte = unsigned char;
using Metatile = std::vector<std::vector<byte>>;
using Tilemap = std::vector<std::vector<byte>>;

namespace fe {

	class Chunk {

		std::vector<Metatile> m_metatiles;
		std::vector<fe::Screen> m_screens;
		std::vector<byte> m_block_properties;

		std::vector<byte> extract_bytes(const std::vector<byte>& p_rom,
			std::size_t p_offset, std::size_t p_length) const;

	public:
		Chunk(void) = default;

		void decompress_and_add_screen(const std::vector<byte>& p_rom,
			std::size_t p_offset);
		void set_block_properties(const std::vector<byte>& p_rom,
			std::size_t p_offset);
		void set_screen_scroll_properties(const std::vector<byte>& p_rom,
			std::size_t p_offset);
		void set_tsa_data(const std::vector<byte>& p_rom, std::size_t p_tl_offset,
			std::size_t p_tr_offset, std::size_t p_bl_offset, std::size_t p_br_offset);
		void set_screen_doors(const std::vector<byte>& p_rom,
			std::size_t p_offset, std::size_t p_end_offset);

		const Metatile& get_metatile(std::size_t p_metatile_no) const;
		std::size_t get_screen_count(void) const;

		const Tilemap& get_screen_tilemap(std::size_t p_screen_no) const;
	};

}

#endif
