#ifndef FE_ATLASMOVIEPREVIEW_H
#define FE_ATLASMOVIEPREVIEW_H

#include "AtlasMovieBundle.h"
#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

namespace fe::atlas_movie {

	struct PreviewTrack {
		byte x{ 0 }, xf{ 0 }, y{ 0 }, yf{ 0 }, vx{ 0 }, vy{ 0 };
		byte tick{ 0 }, stage{ 0 }, pose{ 0 }, frame{ 0 };
		bool visible{ true };
		bool counter_resolved{ true };
	};

	struct PreviewState {
		std::vector<PreviewTrack> tracks;
		std::size_t phase{ 0 }, phase_frame{ 0 }, effect_calls{ 0 };
		byte frame_counter{ 0 };
		bool finished{ false };
	};

	using PreviewCounterReader = std::function<std::optional<byte>(
		std::uint16_t, std::size_t)>;

	void advance_track(const AtlasMovieTrack& p_track, PreviewTrack& p_state,
		byte p_frame_counter);
	PreviewState preview(const AtlasMovie& p_movie, std::size_t p_target_frame,
		std::size_t p_music_hold = 240,
		const PreviewCounterReader& p_counter_reader = {});

}

#endif
