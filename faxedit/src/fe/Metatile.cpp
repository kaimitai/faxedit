#include "Metatile.h"

fe::Metatile::Metatile(byte l_tl, byte l_tr, byte l_bl, byte l_br,
	byte p_attributes, byte p_block_property) :
	m_attr_br{ static_cast<byte>((p_attributes & 0b11000000) >> 6) },
	m_attr_bl{ static_cast<byte>((p_attributes & 0b00110000) >> 4) },
	m_attr_tr{ static_cast<byte>((p_attributes & 0b00001100) >> 2) },
	m_attr_tl{ static_cast<byte>(p_attributes & 0b00000011) },
	m_tilemap{ { {l_tl, l_tr}, {l_bl, l_br} } },
	m_block_property{ p_block_property }
{
}

byte fe::Metatile::get_palette_attribute(std::size_t p_x, std::size_t p_y) const {
	if (p_x % 2)
		return (p_y % 2 ? m_attr_tl : m_attr_tr);
	else
		return (p_y % 2 ? m_attr_br : m_attr_bl);
}
