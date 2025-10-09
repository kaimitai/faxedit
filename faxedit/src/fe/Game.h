#ifndef FE_GAME_H
#define FE_GAME_H

#include <vector>
#include "./../common/klib/NES_tile.h"
#include "Chunk.h"

using byte = unsigned char;
using Metatile = std::vector<std::vector<byte>>;
using Tilemap = std::vector<std::vector<byte>>;

namespace fe {

	class Game {

		std::vector<klib::NES_tile> m_nes_tiles;
		std::vector<fe::Chunk> m_chunks;

		std::size_t get_pointer_address(const std::vector<byte>& p_rom,
			std::size_t p_offset) const;
		std::vector<std::size_t> get_screen_pointers(const std::vector<byte>& p_rom,
			const std::vector<std::size_t>& p_offsets,
			std::size_t p_chunk_no) const;
		void set_various(const std::vector<byte>& p_rom,
			fe::Chunk& p_chunk, std::size_t p_chunk_no,
			std::size_t pt_to_various);

	public:
		Game(const std::vector<byte>& p_rom_data);

		std::size_t get_chunk_count(void) const;
		std::size_t get_screen_count(std::size_t p_chunk_no) const;
		const Metatile& get_metatile(std::size_t p_chunk_no, std::size_t p_metatile_no) const;
		const Tilemap& get_screen_tilemap(std::size_t p_chunk_no, std::size_t p_screen_no) const;

		const std::vector<klib::NES_tile>& get_nes_tiles(void) const;
	};

}

#endif
