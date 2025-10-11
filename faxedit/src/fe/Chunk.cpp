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

byte fe::Chunk::get_metatile_property(std::size_t p_metatile_no) const {
	return m_block_properties.at(p_metatile_no);
}

const fe::Metatile& fe::Chunk::get_metatile(std::size_t p_metatile_no) const {
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

void fe::Chunk::add_metatiles(const std::vector<byte>& p_rom, std::size_t p_tl_offset,
	std::size_t p_tr_offset, std::size_t p_bl_offset, std::size_t p_br_offset,
	std::size_t p_attributes_offset, std::size_t p_metatile_count) {

	for (std::size_t i{ 0 }; i < p_metatile_count; ++i) {
		m_metatiles.push_back(fe::Metatile(p_rom.at(p_tl_offset + i),
			p_rom.at(p_tr_offset + i),
			p_rom.at(p_bl_offset + i),
			p_rom.at(p_br_offset + i),
			p_rom.at(p_attributes_offset + i)
		)
		);
	}
}

byte fe::Chunk::get_palette_attribute(std::size_t p_x, std::size_t p_y) const {
	return m_palette_attributes.at(p_y).at(p_x);
}

byte fe::Chunk::get_default_palette_no(void) const {
	return m_default_palette_no;
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

void fe::Chunk::set_default_palette_no(byte p_palette_no) {
	m_default_palette_no = p_palette_no;
}

bool fe::Chunk::has_screen_exit_right(std::size_t p_screen_no) const {
	return m_screens.at(p_screen_no).has_exit_right();
}

bool fe::Chunk::has_screen_exit_left(std::size_t p_screen_no) const {
	return m_screens.at(p_screen_no).has_exit_left();
}

bool fe::Chunk::has_screen_exit_up(std::size_t p_screen_no) const {
	return m_screens.at(p_screen_no).has_exit_up();
}

bool fe::Chunk::has_screen_exit_down(std::size_t p_screen_no) const {
	return m_screens.at(p_screen_no).has_exit_down();
}

std::size_t fe::Chunk::get_screen_exit_right(std::size_t p_screen_no) const {
	return  m_screens.at(p_screen_no).get_exit_right();
}

std::size_t fe::Chunk::get_screen_exit_left(std::size_t p_screen_no) const {
	return  m_screens.at(p_screen_no).get_exit_left();
}

std::size_t fe::Chunk::get_screen_exit_up(std::size_t p_screen_no) const {
	return  m_screens.at(p_screen_no).get_exit_up();
}

std::size_t fe::Chunk::get_screen_exit_down(std::size_t p_screen_no) const {
	return  m_screens.at(p_screen_no).get_exit_down();
}

std::size_t fe::Chunk::get_screen_sprite_count(std::size_t p_screen_no) const {
	return m_screens.at(p_screen_no).get_sprite_count();
}

byte fe::Chunk::get_screen_sprite_id(std::size_t p_screen_no, std::size_t p_sprite_no) const {
	return m_screens.at(p_screen_no).get_sprite_id(p_sprite_no);
}

byte fe::Chunk::get_screen_sprite_x(std::size_t p_screen_no, std::size_t p_sprite_no) const {
	return m_screens.at(p_screen_no).get_sprite_x(p_sprite_no);
}

byte fe::Chunk::get_screen_sprite_y(std::size_t p_screen_no, std::size_t p_sprite_no) const {
	return m_screens.at(p_screen_no).get_sprite_y(p_sprite_no);
}

byte fe::Chunk::get_screen_sprite_text(std::size_t p_screen_no, std::size_t p_sprite_no) const {
	return m_screens.at(p_screen_no).get_sprite_text(p_sprite_no);
}

bool fe::Chunk::has_screen_sprite_text(std::size_t p_screen_no, std::size_t p_sprite_no) const {
	return m_screens.at(p_screen_no).has_sprite_text(p_sprite_no);
}

void fe::Chunk::add_screen_sprite(std::size_t p_screen_no, byte p_id, byte p_x,
	byte p_y) {
	m_screens.at(p_screen_no).add_sprite(p_id, p_x, p_y);
}

void fe::Chunk::set_screen_sprite_text(std::size_t p_screen_no,
	std::size_t p_sprite_no, byte p_text_id) {
	m_screens.at(p_screen_no).set_sprite_text(p_sprite_no, p_text_id);
}
