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

void fe::Chunk::set_screen_scroll_properties(const std::vector<byte>& p_rom,
	std::size_t p_offset) {
	for (std::size_t i{ 0 }; i < m_screens.size(); ++i)
		m_screens[i].set_scroll_properties(p_rom, p_offset + 4 * i);
}

void fe::Chunk::add_metatiles(const std::vector<byte>& p_rom, std::size_t p_metatile_count,
	std::size_t p_tl_offset, std::size_t p_tr_offset, std::size_t p_bl_offset, std::size_t p_br_offset,
	std::size_t p_attributes_offset, std::size_t p_properties_offset) {

	for (std::size_t i{ 0 }; i < p_metatile_count; ++i) {
		m_metatiles.push_back(fe::Metatile(p_rom.at(p_tl_offset + i),
			p_rom.at(p_tr_offset + i),
			p_rom.at(p_bl_offset + i),
			p_rom.at(p_br_offset + i),
			p_rom.at(p_attributes_offset + i),
			p_rom.at(p_properties_offset + i)
		)
		);
	}
}

void fe::Chunk::set_screen_doors(const std::vector<byte>& p_rom,
	std::size_t p_offset, std::size_t p_door_param_offset,
	byte p_param_offset) {

	for (std::size_t i{ p_offset }; i < p_door_param_offset && p_rom.at(i) != 0xff; i += 4) {

		std::size_t l_screen_id{ p_rom.at(i) };

		m_screens.at(l_screen_id).m_doors.push_back(
			fe::Door(
				p_rom.at(i + 1),
				p_rom.at(i + 2),
				p_rom.at(i + 3),
				p_rom,
				p_door_param_offset,
				p_param_offset
			)
		);
	}
}

void fe::Chunk::set_default_palette_no(byte p_palette_no) {
	m_default_palette_no = p_palette_no;
}

void fe::Chunk::add_screen_sprite(std::size_t p_screen_no, byte p_id, byte p_x,
	byte p_y) {
	m_screens.at(p_screen_no).add_sprite(p_id, p_x, p_y);
}

void fe::Chunk::set_screen_sprite_text(std::size_t p_screen_no,
	std::size_t p_sprite_no, byte p_text_id) {
	m_screens.at(p_screen_no).set_sprite_text(p_sprite_no, p_text_id);
}

std::vector<byte> fe::Chunk::get_block_property_bytes(void) const {
	std::vector<byte> l_result;

	for (const auto& mt : m_metatiles)
		l_result.push_back(mt.m_block_property);

	return l_result;
}

std::vector<byte> fe::Chunk::get_screen_scroll_bytes(void) const {
	std::vector<byte> l_result;

	for (const auto& scr : m_screens) {
		l_result.push_back(scr.m_scroll_left.has_value() ? scr.m_scroll_left.value() : 0xff);
		l_result.push_back(scr.m_scroll_right.has_value() ? scr.m_scroll_right.value() : 0xff);
		l_result.push_back(scr.m_scroll_up.has_value() ? scr.m_scroll_up.value() : 0xff);
		l_result.push_back(scr.m_scroll_down.has_value() ? scr.m_scroll_down.value() : 0xff);
	}

	return l_result;
}
