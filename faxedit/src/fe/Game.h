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

	struct Game {

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
		void set_interchunk_scrolling(const std::vector<byte>& p_rom, std::size_t p_chunk_no, std::size_t pt_to_interchunk);

		Game(const std::vector<byte>& p_rom_data);

	};

}

#endif
