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

	struct Spawn_location {
		byte m_world, m_screen, m_x, m_y;
	};

	struct Game {

		std::vector<std::vector<klib::NES_tile>> m_tilesets;

		// stored as building chunk sprite data, but is globally referred to
		std::vector<std::vector<byte>> m_npc_bundles;
		std::vector<fe::Chunk> m_chunks;
		std::vector<byte> m_rom_data;
		std::vector<NES_Palette> m_palettes;
		std::vector<fe::Spawn_location> m_spawn_locations;

			Game(const std::vector<byte>& p_rom_data);

		std::size_t m_ptr_chunk_metadata, m_ptr_chunk_sprite_data, m_ptr_chunk_interchunk_transitions,
			m_ptr_chunk_intrachunk_transitions, m_ptr_chunk_default_palette_idx, m_ptr_chunk_palettes,
			m_ptr_chunk_door_to_chunk, m_ptr_chunk_door_to_screen, m_ptr_chunk_door_reqs;
		std::vector<std::size_t> m_ptr_chunk_screen_data, m_map_chunk_idx, m_map_chunk_levels, m_offsets_bg_gfx;

		void calculate_spawn_locations_by_guru(const std::vector<std::size_t>& p_chunk_remap);

	private:
		std::size_t get_pointer_address(std::size_t p_offset, std::size_t p_relative_offset = 0) const;
		std::vector<std::size_t> get_screen_pointers(const std::vector<std::size_t>& p_offsets, std::size_t p_chunk_no) const;
		void set_various(std::size_t p_chunk_no, std::size_t pt_to_various);
		void set_sprites(std::size_t p_chunk_no, std::size_t pt_to_sprites);
		void set_interchunk_scrolling(std::size_t p_chunk_no, std::size_t pt_to_interchunk);
		void set_intrachunk_scrolling(std::size_t p_chunk_no, std::size_t pt_to_intrachunk);
	};

}

#endif
