#include "Metatile.h"

fe::Metatile::Metatile(byte p_tl, byte p_tr, byte p_bl, byte p_br, byte p_tl_attr, byte p_tr_attr, byte p_bl_attr, byte p_br_attr,
	byte p_block_property) :
	m_attr_br{ p_br_attr },
	m_attr_bl{ p_bl_attr },
	m_attr_tr{ p_tr_attr },
	m_attr_tl{ p_tl_attr },
	m_tilemap{ { {p_tl, p_tr}, {p_bl, p_br} } },
	m_block_property{ p_block_property }
{
}

fe::Metatile::Metatile(byte p_tl, byte p_tr, byte p_bl, byte p_br,
	byte p_attributes, byte p_block_property) :
	Metatile(p_tl, p_tr, p_bl, p_br,
		static_cast<byte>(p_attributes & 0b00000011),
		static_cast<byte>((p_attributes & 0b00001100) >> 2),
		static_cast<byte>((p_attributes & 0b00110000) >> 4),
		static_cast<byte>((p_attributes & 0b11000000) >> 6),
		p_block_property)
{
}

fe::Metatile::Metatile(void) :
	Metatile(0, 0, 0, 0, 0, 0)
{
}

byte fe::Metatile::get_palette_attribute(std::size_t p_x, std::size_t p_y) const {
	if (p_x % 2)
		return (p_y % 2 ? m_attr_br : m_attr_tr);
	else
		return (p_y % 2 ? m_attr_bl : m_attr_tl);
}
