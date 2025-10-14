#include "Chunk_door_connections.h"

fe::Chunk_door_connections::Chunk_door_connections(byte p_nc, byte p_ns, byte p_nr, byte p_pc, byte p_ps, byte p_pr) :
	m_next_chunk{ p_nc },
	m_next_screen{ p_ns },
	m_next_door_req{ p_nr },
	m_prev_chunk{ p_pc },
	m_prev_screen{ p_ps },
	m_prev_door_req{ p_pr }
{
}
