#ifndef FE_ATLASMOVIEEDITORSESSION_H
#define FE_ATLASMOVIEEDITORSESSION_H

#include "fe/AtlasMovieBundle.h"
#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace fe {

	// reset when the ROM or project changes
	struct AtlasMovieActorSession {
		int stage_snap{ 16 };
		int stage_scale{ 1 };
		bool waypoint_edit_mode{ false };
		int dragged_waypoint{ -1 };
		bool onion_skin{ false };
		int onion_distance{ 8 };
		bool box_select_mode{ false };
		bool box_select_dragging{ false };
		std::array<byte, 2> box_select_start{};
		std::array<byte, 2> box_select_end{};
		byte selection_mask{ 1 };
		std::vector<AtlasMovieTrack> clipboard;
		std::optional<std::array<std::size_t, 3>> pending_placement;
		std::array<char, 64> composition_group{};
		int dragged_track{ -1 };
		int preview_dragged_track{ -1 };
		std::array<byte, 2> drag_point{};
	};

}

#endif
