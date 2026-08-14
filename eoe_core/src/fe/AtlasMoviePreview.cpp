#include "AtlasMoviePreview.h"
#include <algorithm>

namespace fe::atlas_movie {

	namespace {

		void integrate(byte& p_whole, byte& p_fraction,
			byte p_velocity, byte p_shift) {
			byte hi{ p_velocity }, lo{ 0 };
			for (byte i{ 0 }; i < p_shift; ++i) {
				const byte sign{ static_cast<byte>((hi >> 7) & 1) };
				const byte shifted{ static_cast<byte>(hi & 1) };
				hi = static_cast<byte>((hi >> 1) | (sign << 7));
				lo = static_cast<byte>((lo >> 1) | (shifted << 7));
			}
			const unsigned total{ static_cast<unsigned>(p_fraction) + lo };
			p_fraction = static_cast<byte>(total);
			p_whole = static_cast<byte>(p_whole + hi + (total >> 8));
		}

		void reset_cyclic(const AtlasMovieTrack& p_track, PreviewTrack& p_state) {
			p_state.x = p_track.x; p_state.xf = p_track.x_fraction;
			p_state.y = p_track.y; p_state.yf = p_track.y_fraction;
			p_state.vx = static_cast<byte>(p_track.velocity_x);
			p_state.vy = static_cast<byte>(p_track.velocity_y);
			p_state.tick = 0; p_state.pose = 0;
		}

	}

	void advance_track(const AtlasMovieTrack& p_track, PreviewTrack& p_state,
		byte p_frame_counter) {
		if (p_track.kind == AtlasMovieTrackKind::CounterToggle) {
			p_state.x = p_track.x; p_state.y = p_track.y;
			const auto index{ static_cast<std::size_t>(
				(p_frame_counter & p_track.counter_mask) ? 1 : 0) };
			p_state.frame = p_track.toggle_frames.at(
				std::min(index, p_track.toggle_frames.size() - 1));
			return;
		}
		integrate(p_state.x, p_state.xf, p_state.vx, p_track.integrator_shift);
		integrate(p_state.y, p_state.yf, p_state.vy, p_track.integrator_shift);
		if (p_track.kind == AtlasMovieTrackKind::Path) {
			if (p_state.stage < p_track.keyframes.size()) {
				const auto& keyframe{ p_track.keyframes[p_state.stage] };
				const byte coordinate{ p_track.coordinate == AtlasMovieCoordinate::X
					? p_state.x : p_state.y };
				const bool crossed{ p_track.comparison == AtlasMovieComparison::LessThan
					? coordinate < keyframe.threshold
					: coordinate >= keyframe.threshold };
				if (crossed) {
					++p_state.stage;
					p_state.vx = static_cast<byte>(keyframe.velocity_x);
					p_state.vy = static_cast<byte>(keyframe.velocity_y);
				}
			}
			++p_state.tick;
			const auto& slots{ p_track.stage_frames.at(
				std::min<std::size_t>(p_state.stage,
					p_track.stage_frames.size() - 1)) };
			p_state.frame = slots[(p_state.tick / p_track.dwell_frames) % slots.size()];
			p_state.visible = true;
		}
		else {
			++p_state.tick;
			if (p_state.tick == p_track.dwell_frames) {
				++p_state.pose; p_state.tick = 0;
				if (p_state.pose == p_track.reset_at_pose)
					reset_cyclic(p_track, p_state);
			}
			p_state.visible = p_state.pose < p_track.visible_frames.size();
			if (p_state.visible)
				p_state.frame = p_track.visible_frames[p_state.pose];
		}
	}

	PreviewState preview(const AtlasMovie& p_movie, std::size_t p_target_frame,
		std::size_t p_music_hold, const PreviewCounterReader& p_counter_reader) {
		PreviewState state;
		std::size_t elapsed_frames{};
		for (const auto& track : p_movie.tracks) {
			PreviewTrack row;
			row.x = track.x; row.xf = track.x_fraction;
			row.y = track.y; row.yf = track.y_fraction;
			row.vx = static_cast<byte>(track.velocity_x);
			row.vy = static_cast<byte>(track.velocity_y);
			if (track.kind == AtlasMovieTrackKind::Path
				&& !track.stage_frames.empty())
				row.frame = track.stage_frames.front().front();
			else if (track.kind == AtlasMovieTrackKind::Cyclic
				&& !track.visible_frames.empty())
				row.frame = track.visible_frames.front();
			else if (!track.toggle_frames.empty())
				row.frame = track.toggle_frames.front();
			state.tracks.push_back(row);
		}
		for (std::size_t frame{ 0 };
			frame < p_target_frame && !state.finished; ++frame) {
			++elapsed_frames;
			if (state.phase >= p_movie.phases.size()) {
				state.finished = true; break;
			}
			const auto& phase{ p_movie.phases[state.phase] };
			if (state.phase_frame == 0
				&& phase.enter_action == AtlasMovieEnterAction::SetFrameCounter)
				state.frame_counter = phase.enter_value;
			++state.frame_counter; ++state.phase_frame;
			for (std::size_t i{ 0 }; i < p_movie.tracks.size(); ++i)
				if (phase.update_mask & (1u << i))
					advance_track(p_movie.tracks[i], state.tracks[i],
						state.frame_counter);
			if (phase.effect == AtlasMovieEffect::PaletteFade
				&& phase.effect_track < state.tracks.size()
				&& state.tracks[phase.effect_track].stage >= phase.effect_stage)
				++state.effect_calls;
			bool done{ false };
			switch (phase.condition) {
			case AtlasMovieCondition::EffectCalls:
				done = state.effect_calls >= phase.condition_value; break;
			case AtlasMovieCondition::TrackYGte:
				done = phase.condition_track < state.tracks.size()
					&& state.tracks[phase.condition_track].y
						>= phase.condition_value;
				break;
			case AtlasMovieCondition::MusicZero:
				done = state.phase_frame >= p_music_hold; break;
			case AtlasMovieCondition::FrameCounterZero:
				done = state.frame_counter == 0; break;
			case AtlasMovieCondition::Frames:
				done = state.phase_frame >= phase.condition_value; break;
			}
			if (done) {
				++state.phase; state.phase_frame = 0; state.effect_calls = 0;
				if (state.phase >= p_movie.phases.size()) state.finished = true;
			}
		}
		// only $001A exists in the offline preview, other counters need a reader
		for (std::size_t i{}; i < p_movie.tracks.size(); ++i) {
			const auto& track{ p_movie.tracks[i] };
			if (track.kind != AtlasMovieTrackKind::CounterToggle) continue;
			std::optional<byte> counter;
			if (track.counter_address == 0x001a)
				counter = state.frame_counter;
			else if (p_counter_reader)
				counter = p_counter_reader(track.counter_address, elapsed_frames);
			state.tracks[i].counter_resolved = counter.has_value();
			advance_track(track, state.tracks[i], counter.value_or(0));
		}
		return state;
	}

}
