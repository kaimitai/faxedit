#include "AtlasMovieCompatibility.h"
#include "AtlasMovieLayout.h"
#include <algorithm>
#include <array>

bool fe::atlas_movie::has_atlas_resident_scheduler(
	const std::vector<byte>& p_rom) {
	constexpr std::array<byte, 5> CURRENT_ATLAS_RESIDENT_SCHEDULER{
		0x20, 0xce, 0xfc, 0xea, 0xea };
	const auto offset{ layout::file_offset(15, 0xc9af) };
	return offset <= p_rom.size()
		&& CURRENT_ATLAS_RESIDENT_SCHEDULER.size() <= p_rom.size() - offset
		&& std::equal(CURRENT_ATLAS_RESIDENT_SCHEDULER.begin(),
			CURRENT_ATLAS_RESIDENT_SCHEDULER.end(), p_rom.begin() + offset);
}
