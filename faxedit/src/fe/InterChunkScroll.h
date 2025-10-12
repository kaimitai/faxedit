#ifndef FE_INTERCHUNKSCROLL_H
#define FE_INTERCHUNKSCROLL_H

using byte = unsigned char;

namespace fe {

	struct InterChunkScroll {

		byte m_dest_chunk, m_dest_screen, m_dest_x, m_dest_y, m_palette_id;

		InterChunkScroll(byte p_dest_chunk, byte p_dest_screen,
			byte p_dest_coords, byte p_palette_id);

	};

}

#endif
