#ifndef FE_ATLASMOVIEEDITOR_H
#define FE_ATLASMOVIEEDITOR_H

#include "AtlasMovieBundle.h"
#include <array>
#include <cstddef>
#include <vector>

namespace fe::atlas_movie {

	AtlasMovieBundle make_starter_project(void);
	AtlasMovieTrack default_track(AtlasMovieTrackKind p_kind, byte p_frame = 0);
	byte remove_mask_bit(byte p_mask, std::size_t p_index);
	void ensure_unique_id(AtlasMovieBundle& p_bundle, AtlasMovie& p_movie);

	bool apply_painted_path(AtlasMovie& p_movie, AtlasMovieTrack& p_track,
		std::vector<std::array<byte, 2>> p_points, int p_speed, int p_snap);
	std::vector<std::array<byte, 2>> simulated_path(
		const AtlasMovieTrack& p_track);
	std::vector<AtlasMovieWaypoint> runtime_waypoints(
		const AtlasMovieTrack& p_track);
	std::vector<bool> invalid_waypoint_segments(
		const std::vector<AtlasMovieWaypoint>& p_points);
	void translate_actor(AtlasMovieTrack& p_track, int p_dx, int p_dy);
	std::vector<std::size_t> paste_actors(AtlasMovieBundle& p_bundle,
		std::size_t p_movie_index, const std::vector<AtlasMovieTrack>& p_clipboard,
		std::size_t p_phase_index);
	std::size_t estimated_phase_frames(const AtlasMoviePhase& p_phase);

}

#endif
