#include "IntraChunkScroll.h"

fe::IntraChunkScroll::IntraChunkScroll(byte p_dest_chunk, byte p_dest_screen, byte p_dest_coords, byte p_palette_id) :
	IntraChunkScroll(p_dest_chunk, p_dest_screen, p_dest_coords % 16, p_dest_coords / 16, p_palette_id)
{
}

fe::IntraChunkScroll::IntraChunkScroll(byte p_dest_chunk, byte p_dest_screen, byte p_x, byte p_y, byte p_palette_id) :
	m_dest_chunk{ p_dest_chunk },
	m_dest_screen{ p_dest_screen },
	m_dest_x{ p_x },
	m_dest_y{ p_y },
	m_palette_id{ p_palette_id }
{
}
