#ifndef FE_ATLASMOVIECOMPATIBILITY_H
#define FE_ATLASMOVIECOMPATIBILITY_H

#include <vector>

using byte = unsigned char;

namespace fe::atlas_movie {

	// Active scheduler roles continue while a movie is playing.
	bool has_atlas_resident_scheduler(const std::vector<byte>& p_rom);

}

#endif
