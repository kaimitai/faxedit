#include <vector>
#include <iostream>
#include "./common/klib/Kfile.h"
#include "./fe/Chunk.h"

using byte = unsigned char;

static std::string byte_to_hex(byte p_b) {
	std::string result; // { "0x" };

	if (p_b < 16)
		result += '0';
	else {
		byte l_val{ static_cast<byte>(p_b / 16) };

		if (l_val < 10)
			l_val += '0';
		else
			l_val += 'a' - 10;

		result += l_val;
	}

	byte l_val{ static_cast<byte>(p_b % 16) };

	if (l_val < 10)
		l_val += '0';
	else
		l_val += 'a' - 10;

	result += l_val;

	return result;
}

static std::size_t get_pointer_address(const std::vector<byte>& p_rom,
	std::size_t p_offset) {

	return
		0x10 + (p_offset / 0x4000) * 0x4000 +
		static_cast<std::size_t>(p_rom.at(p_offset + 1)) * 256 +
		static_cast<std::size_t>(p_rom.at(p_offset));
}

static std::vector<std::size_t> get_screen_pointers(const std::vector<byte>& p_rom,
	const std::vector<std::size_t>& p_offsets,
	std::size_t p_chunk_no) {
	std::vector<std::size_t> l_result;

	const std::vector<std::size_t> C_CHUNK_TO_BANK{ 0, 0, 0, 1, 1, 2, 2, 2 };

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

static void set_various(const std::vector<byte>& p_rom,
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

int main(int argc, char** argv) {
	// pointers to the screen pointers for each of the 8 chunks. treated as immutable
	const std::vector<std::size_t> SCREEN_PT_PT{
		0x10, 0x12, 0x14, 0x4010, 0x4012, 0x8010, 0x8012, 0x8014
	};

	// pointer to the pointer table for various chunk metadata
	// treated as immutable
	const std::size_t VARIOUS_PT{ 0xc010 };

	klib::Kfile l_file;
	auto l_rom{ l_file.read_file_as_bytes("c:/Temp/Faxanadu (USA) (Rev A).nes") };
	std::vector<fe::Chunk> l_chunks;

	for (std::size_t i{ 0 }; i < SCREEN_PT_PT.size(); ++i) {
		l_chunks.push_back(fe::Chunk());

		auto l_os{ get_screen_pointers(l_rom, SCREEN_PT_PT, i) };

		for (auto l_idx : l_os)
			l_chunks.back().decompress_and_add_screen(l_rom, l_idx);
	}

	for (std::size_t i{ 0 }; i < 8; ++i)
		set_various(l_rom, l_chunks.at(i), i, VARIOUS_PT);

}
