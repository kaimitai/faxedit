#ifndef FE_GAME_H
#define FE_GAME_H

#include <map>
#include <vector>
#include "./../common/klib/NES_tile.h"
#include "Chunk.h"

using byte = unsigned char;
using Tilemap = std::vector<std::vector<byte>>;
using NES_Palette = std::vector<byte>;

namespace fe {

	class Game {

		std::vector<std::vector<klib::NES_tile>> m_tilesets;
		std::vector<fe::Chunk> m_chunks;
		std::vector<byte> m_rom_data;
		std::vector<NES_Palette> m_palettes;

		std::size_t get_pointer_address(const std::vector<byte>& p_rom,
			std::size_t p_offset, std::size_t p_relative_offset = 0) const;
		std::vector<std::size_t> get_screen_pointers(const std::vector<byte>& p_rom,
			const std::vector<std::size_t>& p_offsets,
			std::size_t p_chunk_no) const;
		void set_various(const std::vector<byte>& p_rom, std::size_t p_chunk_no, std::size_t pt_to_various);
		void set_sprites(const std::vector<byte>& p_rom, std::size_t p_chunk_no,
			std::size_t pt_to_sprites);

	public:
		Game(const std::vector<byte>& p_rom_data);
		const std::vector<byte>& get_rom_data(void) const;
		const std::size_t get_tileset_count(void) const;
		const std::vector<klib::NES_tile>& get_tileset(std::size_t p_tileset_no) const;

		std::size_t get_chunk_count(void) const;
		byte get_chunk_default_palette_no(std::size_t p_chunk_no) const;
		std::size_t get_screen_count(std::size_t p_chunk_no) const;
		std::size_t get_metatile_count(std::size_t p_chunk_no) const;
		byte get_metatile_property(std::size_t p_chunk_no, std::size_t p_metatile_no) const;
		const std::vector<NES_Palette>& get_palettes(void) const;

		const Metatile& get_metatile(std::size_t p_chunk_no, std::size_t p_metatile_no) const;
		const Tilemap& get_screen_tilemap(std::size_t p_chunk_no, std::size_t p_screen_no) const;

		bool has_screen_exit_right(std::size_t p_chunk_no, std::size_t p_screen_no) const;
		bool has_screen_exit_left(std::size_t p_chunk_no, std::size_t p_screen_no) const;
		bool has_screen_exit_up(std::size_t p_chunk_no, std::size_t p_screen_no) const;
		bool has_screen_exit_down(std::size_t p_chunk_no, std::size_t p_screen_no) const;

		std::size_t get_screen_exit_right(std::size_t p_chunk_no, std::size_t p_screen_no) const;
		std::size_t get_screen_exit_left(std::size_t p_chunk_no, std::size_t p_screen_no) const;
		std::size_t get_screen_exit_up(std::size_t p_chunk_no, std::size_t p_screen_no) const;
		std::size_t get_screen_exit_down(std::size_t p_chunk_no, std::size_t p_screen_no) const;

		std::size_t get_screen_sprite_count(std::size_t p_chunk_no, std::size_t p_screen_no) const;
		byte get_screen_sprite_id(std::size_t p_chunk_no, std::size_t p_screen_no, std::size_t p_sprite_no) const;
		byte get_screen_sprite_x(std::size_t p_chunk_no, std::size_t p_screen_no, std::size_t p_sprite_no) const;
		byte get_screen_sprite_y(std::size_t p_chunk_no, std::size_t p_screen_no, std::size_t p_sprite_no) const;
		byte get_screen_sprite_text(std::size_t p_chunk_no, std::size_t p_screen_no, std::size_t p_sprite_no) const;
		bool has_screen_sprite_text(std::size_t p_chunk_no, std::size_t p_screen_no, std::size_t p_sprite_no) const;
	};

}

#endif
