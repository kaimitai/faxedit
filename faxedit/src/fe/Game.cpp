#include "Game.h"
#include "Metatile.h"

fe::Game::Game(const std::vector<byte>& p_rom_data) :
	m_rom_data{ p_rom_data }
{
	// pointers to the screen pointers for each of the 8 chunks. treated as immutable
	const std::vector<std::size_t> SCREEN_PT_PT{
		0x10, 0x12, 0x14, 0x4010, 0x4012, 0x8010, 0x8012, 0x8014
	};

	// pointer to the pointer table for various chunk metadata - treated as immutable
	const std::size_t VARIOUS_PT{ 0xc010 };

	// pointer to sprite pointer table - treates as immutable
	const std::size_t SPRITES_PT{ 0x2C220 };

	// pointer to gfx start location per chunk (with the exception of the shop/guru-chunk) - treated as immutable
	const std::vector<std::size_t> GFX_OFFSETS{
		0xf810,   // verified: eolis
		0x10810,  // verified: mist
		0x11810,  // verified: town
		0x10010,  // verified: road to apolune, towers, springs and screen outside forepaw and apolune
		0x11010,  // verified: branches
		0x13010,  // verified: dartmoor + evil fortress
		0x11e10,  // verified, guru and king screens in chunk 6
		0x13010,  // verified: dartmoor + evil fortress
		0x12410,  // verified: extra set used for the shop and building interior screens in chunk 6
		0x12a10   // verified: extra set used for the training shops in chunk 6
	};

	// pointer to palettes
	const std::size_t PALETTE_PT{ 0x2c010 };

	// extract screens for all chunks
	for (std::size_t i{ 0 }; i < SCREEN_PT_PT.size(); ++i) {
		m_chunks.push_back(fe::Chunk());

		auto l_os{ get_screen_pointers(p_rom_data, SCREEN_PT_PT, i) };

		for (auto l_idx : l_os)
			m_chunks.back().decompress_and_add_screen(p_rom_data, l_idx);
	}

	// extract various
	for (std::size_t i{ 0 }; i < 8; ++i)
		set_various(p_rom_data, i, VARIOUS_PT);

	for (std::size_t i{ 0 }; i < 8; ++i)
		set_sprites(p_rom_data, i, SPRITES_PT);

	// extract gfx
	for (std::size_t c{ 0 }; c < GFX_OFFSETS.size(); ++c) {
		m_tilesets.push_back(std::vector<klib::NES_tile>());

		for (std::size_t i{ 0 }; i < 256; ++i) {
			m_tilesets[c].push_back(klib::NES_tile::NES_tile(p_rom_data,
				GFX_OFFSETS[c] + 16 * i));
		}
	}

	// extract palettes
	// metadata: {0x00, 0x06, 0x0A, 0x1B, 0x1B, 0x08, 0x0C, 0x0F}
	// {chunk 0, 1 (3) }
	// rearrange to match my chunk ordering
	std::vector<std::size_t> l_pal_offsets{
		  0x00,
		  0x0A,
		  0x1B,
		  0x06,
		  0x08,
		  0x0C,
		  0x1B,
		  0x0F
	};

	for (std::size_t i{ 0 }; i < 31; ++i) {
		NES_Palette l_tmp_palette;
		for (std::size_t pidx{ PALETTE_PT + 16 * i }; pidx < PALETTE_PT + 16 * i + 16; ++pidx)
			l_tmp_palette.push_back(p_rom_data[pidx]);

		m_palettes.push_back(l_tmp_palette);
	}

	for (std::size_t i{ 0 }; i < 8; ++i) {
		m_chunks.at(i).set_default_palette_no(l_pal_offsets[i]);
	}
}

const std::vector<byte>& fe::Game::get_rom_data(void) const {
	return m_rom_data;
}

std::size_t fe::Game::get_pointer_address(const std::vector<byte>& p_rom,
	std::size_t p_offset, std::size_t p_relative_offset) const {

	std::size_t l_value{
	static_cast<std::size_t>(p_rom.at(p_offset + 1)) * 256 +
		static_cast<std::size_t>(p_rom.at(p_offset))
	};

	std::size_t l_total_offset{
		p_relative_offset == 0 ?
		0x10 + (p_offset / 0x4000) * 0x4000 :
		p_relative_offset
	};

	return l_total_offset + l_value;
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

void fe::Game::set_various(const std::vector<byte>& p_rom, std::size_t p_chunk_no, std::size_t pt_to_various) {
	// mapping from chunk id to index id in this metatable

	const std::vector<std::size_t> METADATA_CHUNK_REMAPS{
		0,  // verified
		3,  // verified
		1,  // verified
		2,  // verified
		6,  // verified
		4,  // verified
		5,  // verified
		7 }; // verified

	// get address from the 16-bit metatable relating to the chunk we want
	std::size_t l_table_offset{ get_pointer_address(p_rom, pt_to_various) + 2 * p_chunk_no };
	// go to the metadata pointer table for our chunk
	std::size_t l_chunk_offset{ get_pointer_address(p_rom, l_table_offset) };

	// metadata, 2 bytes: points 2 bytes forward - but why?
	std::size_t l_chunk_attributes{ get_pointer_address(p_rom, l_chunk_offset) };
	// properties per metatile: #metatile bytes
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

	// the meta-tile definition counts seem to vary between chunks
	std::size_t l_metatile_count{ l_tsa_top_right - l_tsa_top_left };

	std::size_t l_remapped_chunk_no{ METADATA_CHUNK_REMAPS[p_chunk_no] };

	m_chunks[l_remapped_chunk_no].set_block_properties(p_rom, l_block_properties, l_metatile_count);
	m_chunks[l_remapped_chunk_no].set_screen_scroll_properties(p_rom, l_chunk_scroll_data);
	m_chunks[l_remapped_chunk_no].add_metatiles(p_rom, l_tsa_top_left, l_tsa_top_right, l_tsa_bottom_left, l_tsa_bottom_right, l_chunk_palette_attr, l_metatile_count);
	m_chunks[l_remapped_chunk_no].set_screen_doors(p_rom, l_chunk_door_data, l_chunk_door_dest_data);
}

void fe::Game::set_sprites(const std::vector<byte>& p_rom, std::size_t p_chunk_no,
	std::size_t pt_to_sprites) {

	const std::vector<std::size_t> SPRITE_CHUNK_REMAP//{ 0, 3, 1, 2, 4, 5, 6, 7 };
	{
		0,  // verified
		3,  // verified
		1,  
		2,  
		6,  
		4,  
		5,  
		7
}; 

	std::size_t l_true_chunk = SPRITE_CHUNK_REMAP[p_chunk_no];

	std::size_t l_ptr_to_screens{ get_pointer_address(p_rom, pt_to_sprites + 2*p_chunk_no,
		0x24010) };

	for (std::size_t i{ 0 }; i < m_chunks.at(l_true_chunk).get_screen_count(); ++i) {
		std::size_t l_ptr_to_screen{ get_pointer_address(p_rom, l_ptr_to_screens + 2 * i, 0x24010) };

		std::size_t l_spr_count{ 0 };

		// firstly: extract sprite data
		while (p_rom.at(l_ptr_to_screen) != 0xff) {
			byte l_id{ p_rom.at(l_ptr_to_screen) };
			byte l_y{ static_cast<byte>(p_rom.at(l_ptr_to_screen + 1) / 16) };
			byte l_x{ static_cast<byte>(p_rom.at(l_ptr_to_screen + 1) % 16) };

			m_chunks.at(l_true_chunk).add_screen_sprite(i, l_id, l_x, l_y);
			++l_spr_count;
			l_ptr_to_screen += 2;
		}

		// secondly: extract sprite text data
		// hypothesis: 0xff at end of stream seems to be optional
		// when every sprite has a text byte associated with it
		std::size_t l_sprite_no{ 0 };
		while (p_rom.at(++l_ptr_to_screen) != 0xff && l_sprite_no < l_spr_count) {
			m_chunks.at(l_true_chunk).set_screen_sprite_text(i, l_sprite_no++,
				p_rom.at(l_ptr_to_screen));
		}

	}
}

// getters
std::size_t fe::Game::get_chunk_count(void) const {
	return m_chunks.size();
}

byte fe::Game::get_chunk_default_palette_no(std::size_t p_chunk_no) const {
	return m_chunks.at(p_chunk_no).get_default_palette_no();
}

std::size_t fe::Game::get_screen_count(std::size_t p_chunk_no) const {
	return m_chunks.at(p_chunk_no).get_screen_count();
}

const std::size_t fe::Game::get_tileset_count(void) const {
	return m_tilesets.size();
}

const std::vector<klib::NES_tile>& fe::Game::get_tileset(std::size_t p_tileset_no) const {
	return m_tilesets.at(p_tileset_no);
}

std::size_t fe::Game::get_metatile_count(std::size_t p_chunk_no) const {
	return m_chunks.at(p_chunk_no).get_metatile_count();
}

byte fe::Game::get_metatile_property(std::size_t p_chunk_no, std::size_t p_metatile_no) const {
	return m_chunks.at(p_chunk_no).get_metatile_property(p_metatile_no);
}

const std::vector<NES_Palette>& fe::Game::get_palettes(void) const {
	return m_palettes;
}

const fe::Metatile& fe::Game::get_metatile(std::size_t p_chunk_no, std::size_t p_metatile_no) const {
	return m_chunks.at(p_chunk_no).get_metatile(p_metatile_no);
}

const Tilemap& fe::Game::get_screen_tilemap(std::size_t p_chunk_no, std::size_t p_screen_no) const {
	return m_chunks.at(p_chunk_no).get_screen_tilemap(p_screen_no);
}

bool fe::Game::has_screen_exit_right(std::size_t p_chunk_no, std::size_t p_screen_no) const {
	return m_chunks.at(p_chunk_no).has_screen_exit_right(p_screen_no);
}

bool fe::Game::has_screen_exit_left(std::size_t p_chunk_no, std::size_t p_screen_no) const {
	return m_chunks.at(p_chunk_no).has_screen_exit_left(p_screen_no);
}

bool fe::Game::has_screen_exit_up(std::size_t p_chunk_no, std::size_t p_screen_no) const {
	return m_chunks.at(p_chunk_no).has_screen_exit_up(p_screen_no);
}

bool fe::Game::has_screen_exit_down(std::size_t p_chunk_no, std::size_t p_screen_no) const {
	return m_chunks.at(p_chunk_no).has_screen_exit_down(p_screen_no);
}

std::size_t fe::Game::get_screen_exit_right(std::size_t p_chunk_no, std::size_t p_screen_no) const {
	return m_chunks.at(p_chunk_no).get_screen_exit_right(p_screen_no);
}

std::size_t fe::Game::get_screen_exit_left(std::size_t p_chunk_no, std::size_t p_screen_no) const {
	return m_chunks.at(p_chunk_no).get_screen_exit_left(p_screen_no);
}

std::size_t fe::Game::get_screen_exit_up(std::size_t p_chunk_no, std::size_t p_screen_no) const {
	return m_chunks.at(p_chunk_no).get_screen_exit_up(p_screen_no);
}

std::size_t fe::Game::get_screen_exit_down(std::size_t p_chunk_no, std::size_t p_screen_no) const {
	return m_chunks.at(p_chunk_no).get_screen_exit_down(p_screen_no);
}

std::size_t fe::Game::get_screen_sprite_count(std::size_t p_chunk_no,
	std::size_t p_screen_no) const {
	return m_chunks.at(p_chunk_no).get_screen_sprite_count(p_screen_no);
}

byte fe::Game::get_screen_sprite_id(std::size_t p_chunk_no, std::size_t p_screen_no,
	std::size_t p_sprite_no) const {
	return m_chunks.at(p_chunk_no).get_screen_sprite_id(p_screen_no, p_sprite_no);
}

byte fe::Game::get_screen_sprite_x(std::size_t p_chunk_no, std::size_t p_screen_no,
	std::size_t p_sprite_no) const {
	return m_chunks.at(p_chunk_no).get_screen_sprite_x(p_screen_no, p_sprite_no);
}

byte fe::Game::get_screen_sprite_y(std::size_t p_chunk_no, std::size_t p_screen_no,
	std::size_t p_sprite_no) const {
	return m_chunks.at(p_chunk_no).get_screen_sprite_y(p_screen_no, p_sprite_no);
}

byte fe::Game::get_screen_sprite_text(std::size_t p_chunk_no, std::size_t p_screen_no,
	std::size_t p_sprite_no) const {
	return m_chunks.at(p_chunk_no).get_screen_sprite_text(p_screen_no, p_sprite_no);
}

bool fe::Game::has_screen_sprite_text(std::size_t p_chunk_no, std::size_t p_screen_no,
	std::size_t p_sprite_no) const {
	return m_chunks.at(p_chunk_no).has_screen_sprite_text(p_screen_no, p_sprite_no);
}
