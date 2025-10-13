#ifndef FE_INTRACHUNKSCROLL_H
#define FE_INTRACHUNKSCROLL_H

using byte = unsigned char;

namespace fe {

	struct IntraChunkScroll {

		byte m_dest_screen, m_palette_id, m_dest_x, m_dest_y;

		IntraChunkScroll(byte p_dest_screen, byte p_dest_coords, byte p_palette_id);

	};

}

#endif
