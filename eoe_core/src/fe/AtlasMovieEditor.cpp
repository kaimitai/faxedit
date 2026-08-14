#include "AtlasMovieEditor.h"
#include "AtlasMoviePreview.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <ranges>
#include <set>
#include <stdexcept>
#include <utility>

namespace fe::atlas_movie {

	AtlasMovieTrack default_track(AtlasMovieTrackKind p_kind, byte p_frame) {
		AtlasMovieTrack result;
		result.kind = p_kind;
		result.x = 128; result.y = 120;
		result.integrator_shift = 7;
		result.dwell_frames = 16;
		if (p_kind == AtlasMovieTrackKind::Path) {
			result.keyframes = { { 200, 0, 0 } };
			result.stage_frames = { { p_frame }, { p_frame } };
		}
		else if (p_kind == AtlasMovieTrackKind::Cyclic) {
			result.visible_frames = { p_frame };
			result.reset_at_pose = 2;
		}
		else {
			result.counter_mask = 1;
			result.toggle_frames = { p_frame, p_frame };
		}
		return result;
	}

	AtlasMovieBundle make_starter_project(void) {
		auto make_movie = [](std::string id, AtlasMovieProjectRole role,
			AtlasMovieExit exit_mode, std::uint16_t nametable,
			std::uint16_t palette, byte music, byte x) {
			AtlasMovie movie;
			movie.id = std::move(id);
			movie.project_role = role;
			movie.exit_mode = exit_mode;
			movie.entry_music = music;
			movie.metasprite_bank = 12;
			movie.metasprite_pointer_lo = 0xab17;
			movie.metasprite_pointer_hi = 0xab37;
			movie.metasprite_count = 32;
			movie.assets = {
				{ AtlasMovieAssetKind::SpriteChr, 10, 0x9ba0,
					AtlasMovieDestination::Ppu, 0x0000, 0x0900 },
				{ AtlasMovieAssetKind::BackgroundChr, 10, 0xa4a0,
					AtlasMovieDestination::Ppu, 0x1800, 0x0800 },
				{ AtlasMovieAssetKind::Nametable, 10, nametable,
					AtlasMovieDestination::Ppu, 0x2000, 0x0400 },
				{ AtlasMovieAssetKind::Palette, 12, palette,
					AtlasMovieDestination::Ram, 0x0293, 0x0020 }
			};
			auto actor{ default_track(AtlasMovieTrackKind::Path, 0) };
			actor.x = x;
			actor.editor_name = "Actor";
			actor.editor_color = 0xff46beff;
			movie.tracks.push_back(std::move(actor));
			AtlasMoviePhase phase;
			phase.condition_value = 120;
			movie.phases.push_back(phase);
			return movie;
		};

		AtlasMovieBundle result;
		result.movies.push_back(make_movie("intro",
			AtlasMovieProjectRole::OfficialIntro, AtlasMovieExit::NewGame,
			0xaca0, 0xa6c8, 0xfe, 64));
		result.movies.push_back(make_movie("ending",
			AtlasMovieProjectRole::OfficialEnding, AtlasMovieExit::TitleReset,
			0xb0a0, 0xa6e8, 12, 160));
		result.movies.push_back(make_movie("scene-1",
			AtlasMovieProjectRole::Normal, AtlasMovieExit::ReloadScreen,
			0xb0a0, 0xa6e8, 0xff, 96));
		AtlasMovieBundleCodec::validate(result);
		return result;
	}

	byte remove_mask_bit(byte p_mask, std::size_t p_index) {
		const unsigned low_mask{ p_index == 0 ? 0u : (1u << p_index) - 1u };
		const unsigned low{ p_mask & low_mask };
		const unsigned high{ static_cast<unsigned>(
			p_mask >> (p_index + 1)) << p_index };
		return static_cast<byte>(low | high);
	}

	void ensure_unique_id(AtlasMovieBundle& p_bundle, AtlasMovie& p_movie) {
		const std::string base{ p_movie.id.empty() ? "movie" : p_movie.id };
		std::set<std::string> ids;
		for (const auto& movie : p_bundle.movies)
			if (&movie != &p_movie) ids.insert(movie.id);
		if (!ids.contains(p_movie.id)) return;
		for (std::size_t suffix{ 2 }; ; ++suffix) {
			const auto candidate{ std::format("{}-{}", base, suffix) };
			if (!ids.contains(candidate)) {
				p_movie.id = candidate; return;
			}
		}
	}

	namespace {

		std::array<std::int8_t, 2> painted_velocity(
			const std::array<byte, 2>& p_from,
			const std::array<byte, 2>& p_to, int p_speed) {
			const int dx{ static_cast<int>(p_to[0]) - p_from[0] };
			const int dy{ static_cast<int>(p_to[1]) - p_from[1] };
			const int scale{ std::max(std::abs(dx), std::abs(dy)) };
			if (!scale) return { 0, 0 };
			return {
				static_cast<std::int8_t>(std::clamp(static_cast<int>(std::lround(
					static_cast<double>(dx) * p_speed / scale)), -127, 127)),
				static_cast<std::int8_t>(std::clamp(static_cast<int>(std::lround(
					static_cast<double>(dy) * p_speed / scale)), -127, 127))
			};
		}

		std::vector<byte> painted_facing_frames(const AtlasMovie& p_movie,
			std::int8_t p_vx, std::int8_t p_vy,
			const std::vector<byte>& p_fallback,
			const AtlasMovieAnimationSet* p_animation = nullptr) {
			if (!p_vx && !p_vy)
				return p_animation && !p_animation->idle.empty()
					? p_animation->idle : p_fallback.empty()
						? std::vector<byte>{ 0 } : p_fallback;
			if (p_animation && p_animation->automatic_facing) {
				const std::vector<byte>* semantic{};
				if (std::abs(p_vx) >= std::abs(p_vy))
					semantic = p_vx >= 0 ? &p_animation->right : &p_animation->left;
				else semantic = p_vy >= 0
					? &p_animation->toward : &p_animation->away;
				if (semantic && !semantic->empty()) return *semantic;
			}
			const bool stock_cinematic{ p_movie.imports.empty()
				&& p_movie.metasprite_count >= 24 };
			if (stock_cinematic)
				return static_cast<int>(p_vx) - p_vy >= 0
					? std::vector<byte>{ 0, 1, 2, 1 }
					: std::vector<byte>{ 21, 22, 23, 22 };
			const bool bidirectional_import{ p_movie.metasprite_count >= 6
				&& std::ranges::any_of(p_movie.imports, [](const auto& imported) {
					return imported.kind == AtlasMovieImportKind::MetaspriteLibrary
						&& imported.label.find("bidirectional") != std::string::npos;
				}) };
			if (bidirectional_import)
				return p_vx >= 0 ? std::vector<byte>{ 0, 1, 2, 1 }
					: std::vector<byte>{ 3, 4, 5, 4 };
			return p_fallback.empty() ? std::vector<byte>{ 0 } : p_fallback;
		}

		double point_segment_distance_squared(
			const std::array<byte, 2>& p_point,
			const std::array<byte, 2>& p_start,
			const std::array<byte, 2>& p_end) {
			const double dx{ static_cast<double>(p_end[0]) - p_start[0] };
			const double dy{ static_cast<double>(p_end[1]) - p_start[1] };
			if (!dx && !dy) {
				const double px{ static_cast<double>(p_point[0]) - p_start[0] };
				const double py{ static_cast<double>(p_point[1]) - p_start[1] };
				return px * px + py * py;
			}
			const double t{ std::clamp(
				((static_cast<double>(p_point[0]) - p_start[0]) * dx
					+ (static_cast<double>(p_point[1]) - p_start[1]) * dy)
				/ (dx * dx + dy * dy), 0.0, 1.0) };
			const double px{ p_start[0] + t * dx - p_point[0] };
			const double py{ p_start[1] + t * dy - p_point[1] };
			return px * px + py * py;
		}

		void mark_simplified_points(
			const std::vector<std::array<byte, 2>>& p_points,
			std::size_t p_first, std::size_t p_last,
			double p_tolerance_squared, std::vector<bool>& p_keep) {
			double farthest_distance{};
			std::size_t farthest_index{};
			for (std::size_t i{ p_first + 1 }; i < p_last; ++i) {
				const double distance{ point_segment_distance_squared(
					p_points[i], p_points[p_first], p_points[p_last]) };
				if (distance > farthest_distance) {
					farthest_distance = distance; farthest_index = i;
				}
			}
			if (farthest_distance <= p_tolerance_squared) return;
			p_keep[farthest_index] = true;
			mark_simplified_points(p_points, p_first, farthest_index,
				p_tolerance_squared, p_keep);
			mark_simplified_points(p_points, farthest_index, p_last,
				p_tolerance_squared, p_keep);
		}

		std::vector<std::array<byte, 2>> simplify_painted_points(
			const std::vector<std::array<byte, 2>>& p_points, int p_snap) {
			if (p_points.size() < 3) return p_points;
			const double tolerance{ p_snap
				? std::max(3.0, p_snap * 0.4) : 3.0 };
			std::vector<bool> keep(p_points.size(), false);
			keep.front() = keep.back() = true;
			mark_simplified_points(p_points, 0, p_points.size() - 1,
				tolerance * tolerance, keep);
			std::vector<std::array<byte, 2>> result;
			for (std::size_t i{ 0 }; i < p_points.size(); ++i) {
				if (!keep[i]) continue;
				auto point{ p_points[i] };
				if (p_snap) {
					point[0] = static_cast<byte>(std::clamp(
						((point[0] + p_snap / 2) / p_snap) * p_snap,
						0, (255 / p_snap) * p_snap));
					point[1] = static_cast<byte>(std::clamp(
						((point[1] + p_snap / 2) / p_snap) * p_snap,
						0, (239 / p_snap) * p_snap));
				}
				if (result.empty() || point != result.back()) result.push_back(point);
			}
			return result;
		}

	}

	bool apply_painted_path(AtlasMovie& p_movie, AtlasMovieTrack& p_track,
		std::vector<std::array<byte, 2>> p_points, int p_speed, int p_snap) {
		if (p_points.size() < 2)
			throw std::runtime_error("Draw a longer actor path");
		std::vector<std::array<byte, 2>> points{
			simplify_painted_points(p_points, p_snap) };
		while (points.size() > 16) {
			std::vector<std::array<byte, 2>> reduced{ points.front() };
			for (std::size_t i{ 2 }; i < points.size(); i += 2)
				reduced.push_back(points[i]);
			if (reduced.back() != points.back()) reduced.push_back(points.back());
			points = std::move(reduced);
		}
		if (points.size() < 2)
			throw std::runtime_error("Draw a longer actor path");
		const auto x_range{ std::ranges::minmax(
			points, {}, [](const auto& point) { return point[0]; }) };
		const auto y_range{ std::ranges::minmax(
			points, {}, [](const auto& point) { return point[1]; }) };
		const bool gate_x{ static_cast<int>(x_range.max[0]) - x_range.min[0]
			>= static_cast<int>(y_range.max[1]) - y_range.min[1] };
		const auto coordinate = [&](const auto& point) {
			return point[gate_x ? 0 : 1];
		};
		int direction{};
		for (std::size_t i{ 1 }; i < points.size() && !direction; ++i) {
			const int delta{ static_cast<int>(coordinate(points[i]))
				- coordinate(points[i - 1]) };
			if (std::abs(delta) >= 2) direction = delta > 0 ? 1 : -1;
		}
		if (!direction)
			throw std::runtime_error(
				"Painted path needs movement along its dominant axis");

		const std::vector<byte> fallback{
			p_track.kind == AtlasMovieTrackKind::Path
				&& !p_track.stage_frames.empty()
				? p_track.stage_frames.front() : std::vector<byte>{ 0 } };
		const auto editor_name{ p_track.editor_name };
		const auto editor_color{ p_track.editor_color };
		const auto editor_group{ p_track.editor_group };
		const auto editor_animation{ p_track.editor_animation };
		p_track = {};
		p_track.editor_name = editor_name; p_track.editor_color = editor_color;
		p_track.editor_group = editor_group;
		p_track.editor_animation = editor_animation;
		for (const auto& point : points)
			p_track.editor_waypoints.push_back({ point[0], point[1] });
		p_track.kind = AtlasMovieTrackKind::Path;
		p_track.x = points.front()[0]; p_track.y = points.front()[1];
		p_track.integrator_shift = 7;
		p_track.coordinate = gate_x
			? AtlasMovieCoordinate::X : AtlasMovieCoordinate::Y;
		p_track.comparison = direction > 0
			? AtlasMovieComparison::GreaterEqual : AtlasMovieComparison::LessThan;
		p_track.dwell_frames = 8;
		const auto initial{ painted_velocity(points[0], points[1], p_speed) };
		p_track.velocity_x = initial[0]; p_track.velocity_y = initial[1];
		p_track.stage_frames.push_back(painted_facing_frames(p_movie,
			initial[0], initial[1], fallback, &p_track.editor_animation));
		bool turned{ false };
		for (std::size_t segment{ 0 }; segment + 1 < points.size(); ++segment) {
			const auto& endpoint{ points[segment + 1] };
			std::array<std::int8_t, 2> next_velocity{ 0, 0 };
			if (segment + 2 < points.size())
				next_velocity = painted_velocity(
					endpoint, points[segment + 2], p_speed);
			const int raw_next_axis{ segment + 2 < points.size()
				? static_cast<int>(coordinate(points[segment + 2]))
					- coordinate(endpoint) : 0 };
			const bool reversal{ std::abs(raw_next_axis) >= 3
				&& ((raw_next_axis > 0) != (direction > 0)) };
			p_track.keyframes.push_back({ coordinate(endpoint),
				next_velocity[0], next_velocity[1] });
			p_track.stage_frames.push_back(painted_facing_frames(p_movie,
				next_velocity[0], next_velocity[1], p_track.stage_frames.back(),
				&p_track.editor_animation));
			if (reversal) { turned = true; break; }
		}
		return turned;
	}

	std::vector<std::array<byte, 2>> simulated_path(
		const AtlasMovieTrack& p_track) {
		if (p_track.kind != AtlasMovieTrackKind::Path
			|| p_track.stage_frames.empty()) return {};
		PreviewTrack state;
		state.x = p_track.x; state.xf = p_track.x_fraction;
		state.y = p_track.y; state.yf = p_track.y_fraction;
		state.vx = static_cast<byte>(p_track.velocity_x);
		state.vy = static_cast<byte>(p_track.velocity_y);
		state.frame = p_track.stage_frames.front().front();
		std::vector<std::array<byte, 2>> result{ { state.x, state.y } };
		std::size_t settled_frames{};
		for (std::size_t frame{ 0 }; frame < 4096; ++frame) {
			const byte old_x{ state.x }, old_y{ state.y };
			advance_track(p_track, state, static_cast<byte>(frame + 1));
			const int dx{ static_cast<int>(state.x) - old_x };
			const int dy{ static_cast<int>(state.y) - old_y };
			if (std::abs(dx) > 32 || std::abs(dy) > 32) break;
			const std::array<byte, 2> point{ state.x, state.y };
			if (point != result.back()
				&& (frame % 2 == 0 || state.stage >= p_track.keyframes.size()))
				result.push_back(point);
			if (state.stage >= p_track.keyframes.size()
				&& !state.vx && !state.vy) {
				if (++settled_frames >= 2) break;
			}
			else settled_frames = 0;
		}
		return result;
	}

	std::vector<AtlasMovieWaypoint> runtime_waypoints(
		const AtlasMovieTrack& p_track) {
		if (p_track.kind != AtlasMovieTrackKind::Path
			|| p_track.stage_frames.empty()) return {};
		PreviewTrack state;
		state.x = p_track.x; state.xf = p_track.x_fraction;
		state.y = p_track.y; state.yf = p_track.y_fraction;
		state.vx = static_cast<byte>(p_track.velocity_x);
		state.vy = static_cast<byte>(p_track.velocity_y);
		std::vector<AtlasMovieWaypoint> result{ { state.x, state.y } };
		byte prior_stage{};
		for (std::size_t frame{ 0 };
			frame < 4096 && state.stage < p_track.keyframes.size(); ++frame) {
			const byte old_x{ state.x }, old_y{ state.y };
			advance_track(p_track, state, static_cast<byte>(frame + 1));
			if (std::abs(static_cast<int>(state.x) - old_x) > 32
				|| std::abs(static_cast<int>(state.y) - old_y) > 32) break;
			if (state.stage != prior_stage) {
				result.push_back({ state.x, state.y }); prior_stage = state.stage;
			}
		}
		return result;
	}

	std::vector<bool> invalid_waypoint_segments(
		const std::vector<AtlasMovieWaypoint>& p_points) {
		std::vector<bool> invalid(
			p_points.size() > 1 ? p_points.size() - 1 : 0, false);
		if (p_points.size() < 2) return invalid;
		const auto x_range{ std::ranges::minmax(
			p_points, {}, [](const auto& point) { return point.x; }) };
		const auto y_range{ std::ranges::minmax(
			p_points, {}, [](const auto& point) { return point.y; }) };
		const bool gate_x{ static_cast<int>(x_range.max.x) - x_range.min.x
			>= static_cast<int>(y_range.max.y) - y_range.min.y };
		auto axis = [&](const auto& point) { return gate_x ? point.x : point.y; };
		int direction{}; bool reversed{};
		for (std::size_t i{ 0 }; i + 1 < p_points.size(); ++i) {
			const int delta{ static_cast<int>(axis(p_points[i + 1]))
				- axis(p_points[i]) };
			if (!delta) { invalid[i] = true; continue; }
			const int segment_direction{ delta > 0 ? 1 : -1 };
			if (!direction) direction = segment_direction;
			else if (segment_direction != direction) {
				if (reversed || i + 2 < p_points.size()) invalid[i] = true;
				else reversed = true;
			}
		}
		return invalid;
	}

	void translate_actor(AtlasMovieTrack& p_track, int p_dx, int p_dy) {
		int min_x{ p_track.x }, max_x{ p_track.x };
		int min_y{ p_track.y }, max_y{ p_track.y };
		for (const auto& point : p_track.editor_waypoints) {
			min_x = std::min(min_x, static_cast<int>(point.x));
			max_x = std::max(max_x, static_cast<int>(point.x));
			min_y = std::min(min_y, static_cast<int>(point.y));
			max_y = std::max(max_y, static_cast<int>(point.y));
		}
		int min_dx{ -min_x }, max_dx{ 255 - max_x };
		int min_dy{ -min_y }, max_dy{ 239 - max_y };
		if (p_track.kind == AtlasMovieTrackKind::Path) {
			for (const auto& keyframe : p_track.keyframes) {
				if (p_track.coordinate == AtlasMovieCoordinate::X) {
					min_dx = std::max(min_dx, -static_cast<int>(keyframe.threshold));
					max_dx = std::min(max_dx, 255 - static_cast<int>(keyframe.threshold));
				}
				else {
					min_dy = std::max(min_dy, -static_cast<int>(keyframe.threshold));
					max_dy = std::min(max_dy, 255 - static_cast<int>(keyframe.threshold));
				}
			}
		}
		p_dx = min_dx <= max_dx ? std::clamp(p_dx, min_dx, max_dx) : 0;
		p_dy = min_dy <= max_dy ? std::clamp(p_dy, min_dy, max_dy) : 0;
		p_track.x = static_cast<byte>(static_cast<int>(p_track.x) + p_dx);
		p_track.y = static_cast<byte>(static_cast<int>(p_track.y) + p_dy);
		for (auto& point : p_track.editor_waypoints) {
			point.x = static_cast<byte>(static_cast<int>(point.x) + p_dx);
			point.y = static_cast<byte>(static_cast<int>(point.y) + p_dy);
		}
		if (p_track.kind == AtlasMovieTrackKind::Path) {
			const int delta{ p_track.coordinate == AtlasMovieCoordinate::X
				? p_dx : p_dy };
			for (auto& keyframe : p_track.keyframes)
				keyframe.threshold = static_cast<byte>(
					static_cast<int>(keyframe.threshold) + delta);
		}
	}

	std::vector<std::size_t> paste_actors(AtlasMovieBundle& p_bundle,
		std::size_t p_movie_index, const std::vector<AtlasMovieTrack>& p_clipboard,
		std::size_t p_phase_index) {
		if (p_movie_index >= p_bundle.movies.size())
			throw std::runtime_error("movie clipboard destination is missing");
		if (p_clipboard.empty())
			throw std::runtime_error("movie actor clipboard is empty");
		auto candidate{ p_bundle };
		auto& movie{ candidate.movies[p_movie_index] };
		if (movie.tracks.size() + p_clipboard.size() > 8)
			throw std::runtime_error("pasted actors exceed the eight-track limit");
		if (movie.phases.empty())
			throw std::runtime_error("movie clipboard destination has no phases");
		p_phase_index = std::min(p_phase_index, movie.phases.size() - 1);
		auto unique_name = [&](std::string base) {
			if (base.empty()) base = "Actor";
			auto exists = [&](const std::string& name) {
				return std::ranges::any_of(movie.tracks,
					[&](const auto& actor) { return actor.editor_name == name; });
			};
			if (!exists(base)) return base;
			for (std::size_t suffix{ 2 }; ; ++suffix) {
				const auto candidate_name{ std::format("{} {}", base, suffix) };
				if (!exists(candidate_name)) return candidate_name;
			}
		};
		std::vector<std::size_t> pasted;
		for (auto actor : p_clipboard) {
			const auto index{ movie.tracks.size() };
			actor.editor_name = unique_name(actor.editor_name + " Copy");
			translate_actor(actor, 8, 8);
			movie.tracks.push_back(std::move(actor));
			movie.phases[p_phase_index].update_mask |= static_cast<byte>(1u << index);
			movie.phases[p_phase_index].draw_mask |= static_cast<byte>(1u << index);
			pasted.push_back(index);
		}
		AtlasMovieBundleCodec::validate(candidate);
		p_bundle.movies[p_movie_index] = std::move(candidate.movies[p_movie_index]);
		return pasted;
	}

	std::size_t estimated_phase_frames(const AtlasMoviePhase& p_phase) {
		switch (p_phase.condition) {
		case AtlasMovieCondition::Frames:
			return std::max<std::size_t>(1, p_phase.condition_value);
		case AtlasMovieCondition::FrameCounterZero:
			return p_phase.enter_action == AtlasMovieEnterAction::SetFrameCounter
				? std::max<std::size_t>(1, 256 - p_phase.enter_value) : 256;
		case AtlasMovieCondition::MusicZero: return 240;
		case AtlasMovieCondition::EffectCalls:
			return std::max<std::size_t>(1, p_phase.condition_value)
				* std::max<std::size_t>(1, p_phase.effect_period);
		case AtlasMovieCondition::TrackYGte: return 120;
		}
		return 120;
	}

}
