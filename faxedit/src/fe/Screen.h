#ifndef FE_SCREEN_H
#define FE_SCREEN_H

#include <optional>
#include <vector>
#include "Sprite.h"
#include "Door.h"

using byte = unsigned char;
using Tilemap = std::vector<std::vector<byte>>;

namespace fe {

	class Screen {

		Tilemap m_tilemap;
		std::vector<fe::Door> m_doors;
		std::vector<fe::Sprite> m_sprites;
		std::optional<std::size_t> m_scroll_left, m_scroll_right,
			m_scroll_up, m_scroll_down;

		std::optional<std::size_t> scroll_property_to_opt(byte p_val) const;

	public:
		// constructor that reads ROM data and extracts it
		Screen(const std::vector<byte>& p_rom, std::size_t p_offset);
		// procedure that reads scroll properties from rom
		void set_scroll_properties(const std::vector<byte>& p_rom, std::size_t p_offset);
		// procedure that adds door
		void add_door(byte p_coords, byte p_dest, byte p_dest_coords);
		void add_sprite(byte p_id, byte p_x, byte p_y);
		void set_sprite_text(std::size_t p_sprite_no, byte p_text);

		bool has_exit_right(void) const;
		bool has_exit_left(void) const;
		bool has_exit_up(void) const;
		bool has_exit_down(void) const;

		std::size_t get_exit_right(void) const;
		std::size_t get_exit_left(void) const;
		std::size_t get_exit_up(void) const;
		std::size_t get_exit_down(void) const;

		// sprites
		std::size_t get_sprite_count(void) const;
		byte get_sprite_id(std::size_t p_sprite_no) const;
		byte get_sprite_x(std::size_t p_sprite_no) const;
		byte get_sprite_y(std::size_t p_sprite_no) const;
		byte get_sprite_text(std::size_t p_sprite_no) const;
		bool has_sprite_text(std::size_t p_sprite_no) const;

		const Tilemap& get_tilemap(void) const;
	};

}

#endif
