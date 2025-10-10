#include "Chunk.h"
#include "./../common/klib/Bitreader.h"
#include "./../common/klib/Kutil.h"

#include <iostream>

void fe::Chunk::decompress_and_add_screen(const std::vector<byte>& p_rom,
	std::size_t p_offset) {
	m_screens.push_back(fe::Screen(p_rom, p_offset));
}

std::vector<byte> fe::Chunk::extract_bytes(const std::vector<byte>& p_rom,
	std::size_t p_offset, std::size_t p_length) const {
	std::vector<byte> l_result;

	l_result.insert(end(l_result), begin(p_rom) + p_offset,
		begin(p_rom) + p_offset + p_length);

	return l_result;
}

void fe::Chunk::set_block_properties(const std::vector<byte>& p_rom,
	std::size_t p_offset, std::size_t p_metatile_count) {
	m_block_properties = extract_bytes(p_rom, p_offset, p_metatile_count);
}

std::size_t fe::Chunk::get_screen_count(void) const {
	return m_screens.size();
}

std::size_t fe::Chunk::get_metatile_count(void) const {
	return m_metatiles.size();
}

const Metatile& fe::Chunk::get_metatile(std::size_t p_metatile_no) const {
	return m_metatiles.at(p_metatile_no);
}

const Tilemap& fe::Chunk::get_screen_tilemap(std::size_t p_screen_no) const {
	return m_screens.at(p_screen_no).get_tilemap();
}

void fe::Chunk::set_screen_scroll_properties(const std::vector<byte>& p_rom,
	std::size_t p_offset) {
	for (std::size_t i{ 0 }; i < m_screens.size(); ++i)
		m_screens[i].set_scroll_properties(p_rom, p_offset + 4 * i);
}

void fe::Chunk::set_tsa_data(const std::vector<byte>& p_rom, std::size_t p_tl_offset,
	std::size_t p_tr_offset, std::size_t p_bl_offset, std::size_t p_br_offset,
	std::size_t p_metatile_count) {
	for (std::size_t i{ 0 }; i < p_metatile_count; ++i) {
		m_metatiles.push_back({
			{p_rom.at(p_tl_offset + i), p_rom.at(p_tr_offset + i)},
			{p_rom.at(p_bl_offset + i), p_rom.at(p_br_offset + i)}
			});
	}
}

void fe::Chunk::set_screen_doors(const std::vector<byte>& p_rom,
	std::size_t p_offset, std::size_t p_end_offset) {

	for (std::size_t i{ p_offset }; i < p_end_offset; i += 4) {

		std::size_t l_screen_id{ static_cast<std::size_t>(p_rom.at(i)) };
		std::size_t l_coords{ static_cast<std::size_t>(p_rom.at(i + 1)) };
		std::size_t l_dest{ static_cast<std::size_t>(p_rom.at(i + 2)) };
		std::size_t l_dest_coords{ static_cast<std::size_t>(p_rom.at(i + 3)) };

		// TODO: This data interpretation is obviously wrong
		if (l_screen_id < m_screens.size())
			m_screens.at(l_screen_id).add_door(
				l_coords,
				l_dest,
				l_dest_coords
			);

	}

}
