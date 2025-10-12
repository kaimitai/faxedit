#include "InterChunkScroll.h"

fe::InterChunkScroll::InterChunkScroll(byte p_dest_chunk, byte p_dest_screen,
	byte p_dest_coords, byte p_palette_id) :
	m_dest_chunk{ p_dest_chunk }, m_dest_screen{ p_dest_screen },
	m_dest_x{ static_cast<byte>(p_dest_coords % 16) },
	m_dest_y{ static_cast<byte>(p_dest_coords / 16) },
	m_palette_id{ p_palette_id }
{
}
