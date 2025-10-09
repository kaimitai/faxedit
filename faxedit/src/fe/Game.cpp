#include "Game.h"

fe::Game::Game(const std::vector<byte>& p_rom_data) {
	// pointers to the screen pointers for each of the 8 chunks. treated as immutable
	const std::vector<std::size_t> SCREEN_PT_PT{
		0x10, 0x12, 0x14, 0x4010, 0x4012, 0x8010, 0x8012, 0x8014
	};

	// pointer to the pointer table for various chunk metadata
	// treated as immutable
	const std::size_t VARIOUS_PT{ 0xc010 };
	// pointer to gfx data bank 1?
	// treated as immutable
	 const std::size_t GFX_OFFSET{ 0x10010 - 128*16 }; // chunk 0 ok for some reason
	 //const std::size_t GFX_OFFSET{ 0x12810 - (0xa0) * 16 };
	//const std::size_t GFX_OFFSET{ 0x12e10 - (0xa0) * 16 };

	// extract screens for all chunks
	for (std::size_t i{ 0 }; i < SCREEN_PT_PT.size(); ++i) {
		m_chunks.push_back(fe::Chunk());

		auto l_os{ get_screen_pointers(p_rom_data, SCREEN_PT_PT, i) };

		for (auto l_idx : l_os)
			m_chunks.back().decompress_and_add_screen(p_rom_data, l_idx);
	}

	// extract various
	for (std::size_t i{ 0 }; i < 8; ++i)
		set_various(p_rom_data, m_chunks.at(i), i, VARIOUS_PT);

	// extract gfx
	for (std::size_t i{ 0 }; i < 256; ++i) {
		m_nes_tiles.push_back(klib::NES_tile::NES_tile(p_rom_data,
			GFX_OFFSET + 16 * i));
	}
}

std::size_t fe::Game::get_pointer_address(const std::vector<byte>& p_rom,
	std::size_t p_offset) const {
	return
		0x10 + (p_offset / 0x4000) * 0x4000 +
		static_cast<std::size_t>(p_rom.at(p_offset + 1)) * 256 +
		static_cast<std::size_t>(p_rom.at(p_offset));
}

std::vector<std::size_t> fe::Game::get_screen_pointers(const std::vector<byte>& p_rom,
	const std::vector<std::size_t>& p_offsets,
	std::size_t p_chunk_no) const {

	std::vector<std::size_t> l_result;

	std::size_t l_ptr{ get_pointer_address(p_rom, p_offsets[p_chunk_no]) };
	std::size_t l_start_dest_offset{ get_pointer_address(p_rom, l_ptr) };
	std::size_t l_offset{ l_start_dest_offset };

	// we don't know a priori how many screens are defined
	// but the screen layout data begins immediately after the pointer table
	while (l_ptr < l_start_dest_offset) {
		l_result.push_back(l_offset);
		l_ptr += 2;
		l_offset = get_pointer_address(p_rom, l_ptr);
	}

	return l_result;
}

void fe::Game::set_various(const std::vector<byte>& p_rom,
	fe::Chunk& p_chunk, std::size_t p_chunk_no,
	std::size_t pt_to_various) {

	// get address from the 16-bit metatable relating to the chunk we want
	std::size_t l_table_offset{ get_pointer_address(p_rom, pt_to_various) + 2 * p_chunk_no };
	// go to the metadata pointer table for our chunk
	std::size_t l_chunk_offset{ get_pointer_address(p_rom, l_table_offset) };

	// metadata, 2 bytes: points 2 bytes forward - but why?
	std::size_t l_chunk_attributes{ get_pointer_address(p_rom, l_chunk_offset) };
	// properties per metatile: 128 bytes
	std::size_t l_block_properties{ get_pointer_address(p_rom, l_chunk_offset + 2) };
	// 4 bytes per screen, defining which screen each edge scrolls to
	// order: left, right, up, down
	std::size_t l_chunk_scroll_data{ get_pointer_address(p_rom, l_chunk_offset + 4) };
	// 4 bytes per door: screen id, yx-coords, byte in door dest data to use?, exit yx-coords
	std::size_t l_chunk_door_data{ get_pointer_address(p_rom, l_chunk_offset + 6) };
	// door destination screen ids? referenced to from the previous data?
	std::size_t l_chunk_door_dest_data{ get_pointer_address(p_rom, l_chunk_offset + 8) };
	// palettes, 128 bytes: 1 byte per 2x2 block area
	// if 128 bytes and not 64, why?
	std::size_t l_chunk_palette_attr{ get_pointer_address(p_rom, l_chunk_offset + 10) };

	std::size_t l_tsa_top_left{ get_pointer_address(p_rom, l_chunk_offset + 12) };
	std::size_t l_tsa_top_right{ get_pointer_address(p_rom, l_chunk_offset + 14) };
	std::size_t l_tsa_bottom_left{ get_pointer_address(p_rom, l_chunk_offset + 16) };
	std::size_t l_tsa_bottom_right{ get_pointer_address(p_rom, l_chunk_offset + 18) };

	p_chunk.set_block_properties(p_rom, l_block_properties);
	p_chunk.set_screen_scroll_properties(p_rom, l_chunk_scroll_data);
	p_chunk.set_tsa_data(p_rom, l_tsa_top_left, l_tsa_top_right, l_tsa_bottom_left, l_tsa_bottom_right);
	p_chunk.set_screen_doors(p_rom, l_chunk_door_data, l_chunk_door_dest_data);
}

// getters
std::size_t fe::Game::get_chunk_count(void) const {
	return m_chunks.size();
}

std::size_t fe::Game::get_screen_count(std::size_t p_chunk_no) const {
	return m_chunks.at(p_chunk_no).get_screen_count();
}

const std::vector<klib::NES_tile>&  fe::Game::get_nes_tiles(void) const {
	return m_nes_tiles;
}

const Metatile& fe::Game::get_metatile(std::size_t p_chunk_no, std::size_t p_metatile_no) const {
	return m_chunks.at(p_chunk_no).get_metatile(p_metatile_no);
}

const Tilemap& fe::Game::get_screen_tilemap(std::size_t p_chunk_no, std::size_t p_screen_no) const {
	return m_chunks.at(p_chunk_no).get_screen_tilemap(p_screen_no);
}
