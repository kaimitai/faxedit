#ifndef FE_CHUNK_DOOR_CONNECTIONS_H
#define FE_CHUNK_DOOR_CONNECTIONS_H

using byte = unsigned char;

namespace fe {

	struct Chunk_door_connections {

		byte m_next_chunk, m_next_screen, m_next_door_req,
			m_prev_chunk, m_prev_screen, m_prev_door_req;

		Chunk_door_connections(byte p_nc, byte p_ns, byte p_nr, byte p_pc, byte p_ps, byte p_pr);

	};

}

#endif
