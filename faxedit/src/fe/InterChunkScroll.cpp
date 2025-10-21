#include "InterChunkScroll.h"

fe::InterChunkScroll::InterChunkScroll(byte p_dest_screen,
	byte p_dest_coords, byte p_palette_id) :
	InterChunkScroll(p_dest_screen, p_dest_coords % 16, p_dest_coords / 16, p_palette_id)
{
}

fe::InterChunkScroll::InterChunkScroll(byte p_dest_screen,
	byte p_dest_x, byte p_dest_y, byte p_palette_id) :
	m_dest_screen{ p_dest_screen },
	m_dest_x{ p_dest_x },
	m_dest_y{ p_dest_y },
	m_palette_id{ p_palette_id }

{
}
