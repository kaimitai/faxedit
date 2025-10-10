#ifndef FE_CHUNK_H
#define FE_CHUNK_H

#include <vector>
#include "Screen.h"
#include "Metatile.h"

using byte = unsigned char;
using Tilemap = std::vector<std::vector<byte>>;
using NES_Palette = std::vector<byte>;

namespace fe {

	class Chunk {

		std::vector<fe::Metatile> m_metatiles;
		std::vector<fe::Screen> m_screens;
		std::vector<byte> m_block_properties;
		std::vector<std::vector<byte>> m_palette_attributes;
		byte m_default_palette_no;

		std::vector<byte> extract_bytes(const std::vector<byte>& p_rom,
			std::size_t p_offset, std::size_t p_length) const;

	public:
		Chunk(void) = default;

		void decompress_and_add_screen(const std::vector<byte>& p_rom,
			std::size_t p_offset);
		void set_block_properties(const std::vector<byte>& p_rom,
			std::size_t p_offset, std::size_t p_metatile_count);
		void set_screen_scroll_properties(const std::vector<byte>& p_rom,
			std::size_t p_offset);
		void add_metatiles(const std::vector<byte>& p_rom, std::size_t p_tl_offset,
			std::size_t p_tr_offset, std::size_t p_bl_offset, std::size_t p_br_offset,
			std::size_t p_attributes_offset, std::size_t p_metatile_count);
		void set_screen_doors(const std::vector<byte>& p_rom,
			std::size_t p_offset, std::size_t p_end_offset);
		void set_default_palette_no(byte p_palette_no);

		std::size_t get_metatile_count(void) const;
		const fe::Metatile& get_metatile(std::size_t p_metatile_no) const;
		std::size_t get_screen_count(void) const;
		byte get_palette_attribute(std::size_t p_x, std::size_t p_y) const;
		byte get_default_palette_no(void) const;

		const Tilemap& get_screen_tilemap(std::size_t p_screen_no) const;
	};

}

#endif
