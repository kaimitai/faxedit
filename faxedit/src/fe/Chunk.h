#ifndef FE_CHUNK_H
#define FE_CHUNK_H

#include <utility>
#include <vector>
#include "Screen.h"
#include "Metatile.h"

using byte = unsigned char;
using Tilemap = std::vector<std::vector<byte>>;
using NES_Palette = std::vector<byte>;

namespace fe {

	struct Chunk {

		std::vector<fe::Metatile> m_metatiles;
		std::vector<fe::Screen> m_screens;
		byte m_default_palette_no;
		std::vector<byte> m_mattock_animation;

		std::vector<byte> extract_bytes(const std::vector<byte>& p_rom,
			std::size_t p_offset, std::size_t p_length) const;

		std::vector<byte> get_metatile_quadrant_bytes(std::size_t p_x, std::size_t p_y) const;

	public:
		Chunk(void);

		void decompress_and_add_screen(const std::vector<byte>& p_rom,
			std::size_t p_offset);
		void set_screen_scroll_properties(const std::vector<byte>& p_rom,
			std::size_t p_offset);
		void add_metatiles(const std::vector<byte>& p_rom, std::size_t p_metatile_count,
			std::size_t p_tl_offset, std::size_t p_tr_offset, std::size_t p_bl_offset, std::size_t p_br_offset,
			std::size_t p_attributes_offset, std::size_t p_properties_offset);
		void set_screen_doors(const std::vector<byte>& p_rom,
			std::size_t p_door_offset, std::size_t p_door_param_offset,
			byte p_param_offset);
		void set_default_palette_no(byte p_palette_no);
		void add_screen_sprite(std::size_t p_screen_no, byte p_id, byte p_x, byte p_y);
		void set_screen_sprite_text(std::size_t p_screen_no, std::size_t p_sprite_no, byte p_text_id);

		// ROM data - in the metadata master pointer table order
		std::vector<byte> get_block_property_bytes(void) const;
		std::vector<byte> get_screen_scroll_bytes(void) const;
		std::pair<std::vector<byte>, std::vector<byte>> get_door_bytes(bool p_is_town = false) const;
		std::vector<byte> get_palette_attribute_bytes(void) const;
		std::vector<byte> get_metatile_top_left_bytes(void) const;
		std::vector<byte> get_metatile_top_right_bytes(void) const;
		std::vector<byte> get_metatile_bottom_left_bytes(void) const;
		std::vector<byte> get_metatile_bottom_right_bytes(void) const;

		std::vector<byte> get_sameworld_transition_bytes(void) const;
		std::vector<byte> get_otherworld_transition_bytes(const std::vector<std::size_t>& p_chunk_remap) const;
	};

}

#endif
