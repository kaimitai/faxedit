#include "PaletteMusicMap.h"

std::optional<byte> fe::PaletteMusicMap::get_music(byte p_palette_no) const {
	for (const auto& slot : m_slots)
		if (slot.m_palette == p_palette_no)
			return slot.m_music;

	return std::nullopt;
}

void fe::PaletteMusicMap::set_slot_palette(std::size_t p_index, byte p_palette_no) {
	m_slots.at(p_index).m_palette = p_palette_no;
}

void fe::PaletteMusicMap::set_slot_music(std::size_t p_index, byte p_music_no) {
	m_slots.at(p_index).m_music = p_music_no;
}

void fe::PaletteMusicMap::add_slot(void) {
	m_slots.push_back(fe::PaletteMusicSlot(0xff, 0xff));
}

void fe::PaletteMusicMap::delete_slot(std::size_t p_slot_no) {
	m_slots.erase(begin(m_slots) + p_slot_no);
}

std::vector<byte> fe::PaletteMusicMap::get_palette_bytes(void) const {
	std::vector<byte> result;
	for (const auto& slot : m_slots)
		result.push_back(slot.m_palette);
	return result;
}

std::vector<byte> fe::PaletteMusicMap::get_music_bytes(void) const {
	std::vector<byte> result;
	for (const auto& slot : m_slots)
		result.push_back(slot.m_music);
	return result;
}

std::size_t fe::PaletteMusicMap::get_slot_count(void) const {
	return m_slots.size();
}

fe::PaletteMusicSlot::PaletteMusicSlot(byte p_palette, byte p_music) :
	m_palette{ p_palette },
	m_music{ p_music }
{
}
