#include "Game.h"
#include "Metatile.h"
#include "fe_constants.h"
#include "Chunk_door_connections.h"
#include <algorithm>

fe::Game::Game(const std::vector<byte>& p_rom_data) :
	m_rom_data{ p_rom_data },
	m_ptr_chunk_screen_data{ c::PTR_CHUNK_SCREEN_DATA },
	m_ptr_chunk_metadata{ c::PTR_CHUNK_METADATA },
	m_ptr_chunk_interchunk_transitions{ c::PTR_CHUNK_INTERCHUNK_TRANSITIONS },
	m_ptr_chunk_intrachunk_transitions{ c::PTR_CHUNK_INTRACHUNK_TRANSITIONS },
	m_ptr_chunk_sprite_data{ c::PTR_CHUNK_SPRITE_DATA },
	m_ptr_chunk_default_palette_idx{ c::PTR_CHUNK_DEFAULT_PALETTE_IDX },
	m_ptr_chunk_palettes{ c::PTR_CHUNK_PALETTES },
	m_map_chunk_idx{ c::MAP_CHUNK_IDX },
	m_map_chunk_levels{ c::MAP_CHUNK_LEVELS },
	m_ptr_chunk_door_to_chunk{ c::PTR_CHUNK_DOOR_TO_CHUNK },
	m_ptr_chunk_door_to_screen{ c::PTR_CHUNK_DOOR_TO_SCREEN },
	m_ptr_chunk_door_reqs{ c::PTR_CHUNK_DOOR_REQUIREMENTS },
	m_offsets_bg_gfx{ c::OFFSETS_BG_GFX }
{
	// extract screens for all chunks
	for (std::size_t i{ 0 }; i < m_ptr_chunk_screen_data.size(); ++i) {
		m_chunks.push_back(fe::Chunk());

		auto l_os{ get_screen_pointers(m_ptr_chunk_screen_data, i) };

		for (auto l_idx : l_os)
			m_chunks.back().decompress_and_add_screen(p_rom_data, l_idx);
	}

	// extract various
	for (std::size_t i{ 0 }; i < 8; ++i)
		set_various(i, m_ptr_chunk_metadata);
	// extract sprites
	for (std::size_t i{ 0 }; i < 8; ++i)
		set_sprites(i, m_ptr_chunk_sprite_data);
	// extract inter-chunk transitions
	for (std::size_t i{ 0 }; i < 8; ++i) {
		set_interchunk_scrolling(i, m_ptr_chunk_interchunk_transitions);
		set_intrachunk_scrolling(i, m_ptr_chunk_intrachunk_transitions);
	}

	// extract inter-chunk connections when using "next level" and "last level" doors
	// not all chunks have this, so consult the mapping table
	for (std::size_t i{ 0 }; i < 8; ++i) {
		const auto iter{ std::find(begin(m_map_chunk_levels), end(m_map_chunk_levels), i) };

		// found a match for this chunk id - extract the data
		if (iter != end(m_map_chunk_levels)) {
			std::size_t l_true_chunk{ static_cast<std::size_t>(iter - begin(m_map_chunk_levels)) };

			m_chunks.at(i).m_door_connections = fe::Chunk_door_connections(
				static_cast<byte>(m_map_chunk_levels.at(m_rom_data.at(m_ptr_chunk_door_to_chunk + 2 * l_true_chunk + 1))),
				m_rom_data.at(m_ptr_chunk_door_to_screen + 2 * l_true_chunk + 1),
				m_rom_data.at(m_ptr_chunk_door_reqs + 2 * l_true_chunk + 1),
				static_cast<byte>(m_map_chunk_levels.at(m_rom_data.at(m_ptr_chunk_door_to_chunk + 2 * l_true_chunk))),
				m_rom_data.at(m_ptr_chunk_door_to_screen + 2 * l_true_chunk),
				m_rom_data.at(m_ptr_chunk_door_reqs + 2 * l_true_chunk)
			);
		}
	}

	// extract gfx
	for (std::size_t c{ 0 }; c < m_offsets_bg_gfx.size(); ++c) {
		m_tilesets.push_back(std::vector<klib::NES_tile>());

		for (std::size_t i{ 0 }; i < 256; ++i) {
			m_tilesets[c].push_back(klib::NES_tile::NES_tile(p_rom_data,
				m_offsets_bg_gfx[c] + 16 * i));
		}
	}

	// set default palette indexes for each chunk
	for (std::size_t i{ 0 }; i < 8; ++i)
		m_chunks.at(m_map_chunk_idx.at(i)).set_default_palette_no(m_rom_data.at(m_ptr_chunk_default_palette_idx + i));

	for (std::size_t i{ 0 }; i < 31; ++i) {
		NES_Palette l_tmp_palette;
		for (std::size_t pidx{ m_ptr_chunk_palettes + 16 * i }; pidx < m_ptr_chunk_palettes + 16 * i + 16; ++pidx)
			l_tmp_palette.push_back(p_rom_data[pidx]);

		m_palettes.push_back(l_tmp_palette);
	}

}

std::size_t fe::Game::get_pointer_address(std::size_t p_offset, std::size_t p_relative_offset) const {

	std::size_t l_value{
	static_cast<std::size_t>(m_rom_data.at(p_offset + 1)) * 256 +
		static_cast<std::size_t>(m_rom_data.at(p_offset))
	};

	std::size_t l_total_offset{
		p_relative_offset == 0 ?
		0x10 + (p_offset / 0x4000) * 0x4000 :
		p_relative_offset
	};

	return l_total_offset + l_value;
}

std::vector<std::size_t> fe::Game::get_screen_pointers(const std::vector<std::size_t>& p_offsets, std::size_t p_chunk_no) const {

	std::vector<std::size_t> l_result;

	std::size_t l_ptr{ get_pointer_address(p_offsets[p_chunk_no]) };
	std::size_t l_start_dest_offset{ get_pointer_address(l_ptr) };
	std::size_t l_offset{ l_start_dest_offset };

	// we don't know a priori how many screens are defined
	// but the screen layout data begins immediately after the pointer table
	while (l_ptr < l_start_dest_offset) {
		l_result.push_back(l_offset);
		l_ptr += 2;
		l_offset = get_pointer_address(l_ptr);
	}

	return l_result;
}

void fe::Game::set_various(std::size_t p_chunk_no, std::size_t pt_to_various) {
	// get address from the 16-bit metatable relating to the chunk we want
	std::size_t l_table_offset{ get_pointer_address(pt_to_various) + 2 * p_chunk_no };
	// go to the metadata pointer table for our chunk
	std::size_t l_chunk_offset{ get_pointer_address(l_table_offset) };

	// metadata, 2 bytes: points 2 bytes forward - but why?
	std::size_t l_chunk_attributes{ get_pointer_address(l_chunk_offset) };
	// properties per metatile: #metatile bytes
	std::size_t l_block_properties{ get_pointer_address(l_chunk_offset + 2) };
	// 4 bytes per screen, defining which screen each edge scrolls to
	// order: left, right, up, down
	std::size_t l_chunk_scroll_data{ get_pointer_address(l_chunk_offset + 4) };
	// 4 bytes per door: screen id, yx-coords, byte in door dest data to use?, exit yx-coords
	std::size_t l_chunk_door_data{ get_pointer_address(l_chunk_offset + 6) };
	// door destination screen ids? referenced to from the previous data?
	std::size_t l_chunk_door_dest_data{ get_pointer_address(l_chunk_offset + 8) };
	// palettes, 1 byte per metatile defining its palette - 2 bits per quadrant
	std::size_t l_chunk_palette_attr{ get_pointer_address(l_chunk_offset + 10) };

	std::size_t l_tsa_top_left{ get_pointer_address(l_chunk_offset + 12) };
	std::size_t l_tsa_top_right{ get_pointer_address(l_chunk_offset + 14) };
	std::size_t l_tsa_bottom_left{ get_pointer_address(l_chunk_offset + 16) };
	std::size_t l_tsa_bottom_right{ get_pointer_address(l_chunk_offset + 18) };

	// the meta-tile definition counts seem to vary between chunks
	std::size_t l_metatile_count{ l_tsa_top_right - l_tsa_top_left };

	std::size_t l_true_chunk_no{ m_map_chunk_idx[p_chunk_no] };

	m_chunks[l_true_chunk_no].set_block_properties(m_rom_data, l_block_properties, l_metatile_count);
	m_chunks[l_true_chunk_no].set_screen_scroll_properties(m_rom_data, l_chunk_scroll_data);
	m_chunks[l_true_chunk_no].add_metatiles(m_rom_data, l_tsa_top_left, l_tsa_top_right, l_tsa_bottom_left, l_tsa_bottom_right, l_chunk_palette_attr, l_metatile_count);

	// the doors for the town chunk offsets the index by 0x20 (hard coded game logic)
	// probably to save space since all the doors there go to buildings
	m_chunks[l_true_chunk_no].set_screen_doors(m_rom_data, l_chunk_door_data, l_chunk_door_dest_data,
		l_true_chunk_no == 2 ? 0x20 : 0x00);
}

void fe::Game::set_sprites(std::size_t p_chunk_no, std::size_t pt_to_sprites) {

	std::size_t l_true_chunk = m_map_chunk_idx[p_chunk_no];

	std::size_t l_ptr_to_screens{ get_pointer_address(pt_to_sprites + 2 * p_chunk_no, 0x24010) };

	for (std::size_t i{ 0 }; i < m_chunks.at(l_true_chunk).m_screens.size(); ++i) {
		std::size_t l_ptr_to_screen{ get_pointer_address(l_ptr_to_screens + 2 * i, 0x24010) };

		std::size_t l_spr_count{ 0 };

		// firstly: extract sprite data
		while (m_rom_data.at(l_ptr_to_screen) != 0xff) {
			byte l_id{ m_rom_data.at(l_ptr_to_screen) };
			byte l_y{ static_cast<byte>(m_rom_data.at(l_ptr_to_screen + 1) / 16) };
			byte l_x{ static_cast<byte>(m_rom_data.at(l_ptr_to_screen + 1) % 16) };

			m_chunks.at(l_true_chunk).add_screen_sprite(i, l_id, l_x, l_y);
			++l_spr_count;
			l_ptr_to_screen += 2;
		}

		// secondly: extract sprite text data
		// hypothesis: 0xff at end of stream seems to be optional
		// when every sprite has a text byte associated with it
		std::size_t l_sprite_no{ 0 };
		while (m_rom_data.at(++l_ptr_to_screen) != 0xff && l_sprite_no < l_spr_count) {
			m_chunks.at(l_true_chunk).set_screen_sprite_text(i, l_sprite_no++,
				m_rom_data.at(l_ptr_to_screen));
		}

	}
}

void fe::Game::set_interchunk_scrolling(std::size_t p_chunk_no, std::size_t pt_to_interchunk) {

	std::size_t l_true_chunk_no{ m_map_chunk_idx[p_chunk_no] };
	std::size_t l_ptr_to_screens{ get_pointer_address(pt_to_interchunk + 2 * p_chunk_no, 0x30010) };

	for (std::size_t i{ 0 }; m_rom_data.at(l_ptr_to_screens + i) != 0xff; i += 5) {
		byte l_screen_id{ m_rom_data.at(l_ptr_to_screens + i) };

		m_chunks.at(l_true_chunk_no).m_screens.at(l_screen_id).m_interchunk_scroll =
			fe::InterChunkScroll(
				static_cast<byte>(m_map_chunk_idx[m_rom_data.at(l_ptr_to_screens + i + 1)]),
				m_rom_data.at(l_ptr_to_screens + i + 2),
				m_rom_data.at(l_ptr_to_screens + i + 3),
				m_rom_data.at(l_ptr_to_screens + i + 4)
			);
	}

}

void fe::Game::set_intrachunk_scrolling(std::size_t p_chunk_no, std::size_t pt_to_intrachunk) {
	std::size_t l_true_chunk_no{ m_map_chunk_idx[p_chunk_no] };
	std::size_t l_ptr_to_screens{ get_pointer_address(pt_to_intrachunk + 2 * p_chunk_no, 0x30010) };

	for (std::size_t i{ 0 }; m_rom_data.at(l_ptr_to_screens + i) != 0xff; i += 4) {
		byte l_screen_id{ m_rom_data.at(l_ptr_to_screens + i) };

		m_chunks.at(l_true_chunk_no).m_screens.at(l_screen_id).m_intrachunk_scroll =
			fe::IntraChunkScroll(
				m_rom_data.at(l_ptr_to_screens + i + 1),
				m_rom_data.at(l_ptr_to_screens + i + 2),
				m_rom_data.at(l_ptr_to_screens + i + 3)
			);
	}
}
