#include "MainWindow.h"
#include "AtlasMovieRenderer.h"
#include "AtlasMovieUi.h"
#include "Imgui_helper.h"
#include "fe/AtlasMovieEditor.h"
#include "fe/AtlasMoviePreview.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <optional>
#include <ranges>
#include <set>
#include <vector>

namespace {

	using namespace fe;
	using namespace fe::atlas_movie;
	using namespace fe::atlas_movie::ui_detail;

	constexpr auto& ACTOR_UI_COLORS{ ACTOR_COLORS };

}

void fe::MainWindow::draw_atlas_movie_actors_tab(SDL_Renderer* p_renderer,
	AtlasMovieBundle& bundle, AtlasMovie& movie,
	const std::vector<SpriteAnimationFrame>& decoded_frames,
	std::string& graphics_error, bool advanced_mode) {
	auto guarded = [this](auto&& action) {
		try { action(); }
		catch (const std::exception& ex) { add_message(ex.what(), 1); }
	};
		if (ImGui::BeginTabItem(advanced_mode ? "Actors / Stage" : "Scene")) {
			auto& actor_stage_snap{ m_atlas_movie_actor_session.stage_snap };
			auto& actor_stage_scale{ m_atlas_movie_actor_session.stage_scale };
			auto& waypoint_edit_mode{ m_atlas_movie_actor_session.waypoint_edit_mode };
			auto& dragged_waypoint{ m_atlas_movie_actor_session.dragged_waypoint };
			auto& onion_skin{ m_atlas_movie_actor_session.onion_skin };
			auto& onion_distance{ m_atlas_movie_actor_session.onion_distance };
			auto& box_select_mode{ m_atlas_movie_actor_session.box_select_mode };
			auto& box_select_dragging{ m_atlas_movie_actor_session.box_select_dragging };
			auto& box_select_start{ m_atlas_movie_actor_session.box_select_start };
			auto& box_select_end{ m_atlas_movie_actor_session.box_select_end };
			auto& actor_selection_mask{ m_atlas_movie_actor_session.selection_mask };
			auto& actor_clipboard{ m_atlas_movie_actor_session.clipboard };
			auto& pending_actor_placement{ m_atlas_movie_actor_session.pending_placement };
			auto& composition_group{ m_atlas_movie_actor_session.composition_group };
			auto& actor_stage_dragged_track{ m_atlas_movie_actor_session.dragged_track };
			auto& actor_stage_drag_point{ m_atlas_movie_actor_session.drag_point };
			if (!advanced_mode) {
				waypoint_edit_mode = false; dragged_waypoint = -1; onion_skin = false;
				box_select_mode = box_select_dragging = false;
			}
			bool actor_structure_changed{ false };
			if (!advanced_mode)
				ImGui::TextDisabled("Choose an actor, drag it on the stage, or draw where it should move.");
			if (pending_actor_placement) {
				const auto request{ *pending_actor_placement }; pending_actor_placement.reset();
				guarded([&] { place_atlas_movie_pose(bundle, movie, static_cast<byte>(m_atlas_movie_browser_frame),
					static_cast<byte>(request[0]), static_cast<byte>(request[1]), request[2]); });
			}
			if (ui::imgui_button(m_atlas_movie_preview_playing ? "Pause" : "Play",
				m_atlas_movie_preview_playing ? 4 : 2)) {
				m_atlas_movie_preview_playing = !m_atlas_movie_preview_playing;
				m_atlas_movie_preview_tick = SDL_GetTicks();
			}
			ImGui::SameLine();
			if (ui::imgui_button("Restart", 2)) {
				m_atlas_movie_preview_frame = 0; m_atlas_movie_preview_tick = SDL_GetTicks();
			}
			if (m_atlas_movie_preview_playing) {
				const auto now{ SDL_GetTicks() };
				while (now - m_atlas_movie_preview_tick >= 16) {
					++m_atlas_movie_preview_frame; m_atlas_movie_preview_tick += 16;
				}
			}
			int actor_stage_frame{ static_cast<int>(std::min<std::size_t>(m_atlas_movie_preview_frame, 3600)) };
			ImGui::SetNextItemWidth(360);
			if (ImGui::SliderInt("Frame##actor-stage", &actor_stage_frame, 0, 3600)) {
				m_atlas_movie_preview_frame = actor_stage_frame; m_atlas_movie_preview_playing = false;
			}
			const auto identity_state{ preview(movie, m_atlas_movie_preview_frame) };
			const byte actor_allowed_mask{ static_cast<byte>((1u << movie.tracks.size()) - 1u) };
			actor_selection_mask &= actor_allowed_mask;
			if (!(actor_selection_mask & (1u << m_atlas_movie_sel_track)))
				actor_selection_mask |= static_cast<byte>(1u << m_atlas_movie_sel_track);
			auto track_identity = [&](std::size_t index) {
				if (index >= identity_state.tracks.size()) return std::format("Track {}", index);
				const byte frame{ identity_state.tracks[index].frame };
				const auto& named{ movie.tracks[index].editor_name };
				return std::format("{} — T{} / F{:02} {}",
					named.empty() ? std::format("Actor {}", index + 1) : named,
					index, frame, frame_family(frame));
			};
			if (advanced_mode && ImGui::BeginCombo("Track", track_identity(m_atlas_movie_sel_track).c_str())) {
				for (std::size_t i{ 0 }; i < movie.tracks.size(); ++i)
					if (ImGui::Selectable(track_identity(i).c_str(), i == m_atlas_movie_sel_track)) {
						m_atlas_movie_sel_track = i;
						actor_selection_mask = static_cast<byte>(1u << i);
					}
				ImGui::EndCombo();
			}
			ImGui::SeparatorText("Actor roster");
			if (ImGui::BeginTable("actor-roster", advanced_mode ? 4 : 2,
				ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
				if (advanced_mode) ImGui::TableSetupColumn("Select", ImGuiTableColumnFlags_WidthFixed, 52);
				ImGui::TableSetupColumn("Pose", ImGuiTableColumnFlags_WidthFixed, 42);
				ImGui::TableSetupColumn("Actor", ImGuiTableColumnFlags_WidthStretch);
				if (advanced_mode) ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthFixed, 110);
				ImGui::TableHeadersRow();
				for (std::size_t i{ 0 }; i < movie.tracks.size(); ++i) {
					ImGui::PushID(static_cast<int>(i)); ImGui::TableNextRow();
					if (advanced_mode) {
						ImGui::TableNextColumn(); bool selected{ static_cast<bool>(actor_selection_mask & (1u << i)) };
						if (ImGui::Checkbox("##multi", &selected)) {
							if (selected) actor_selection_mask |= static_cast<byte>(1u << i);
							else actor_selection_mask &= static_cast<byte>(~(1u << i));
						}
					}
					ImGui::TableNextColumn();
					const byte pose{ identity_state.tracks[i].frame };
					if (pose < m_atlas_movie_frame_textures.size() && m_atlas_movie_frame_textures[pose])
						ImGui::Image(m_atlas_movie_frame_textures[pose], ImVec2(30, 30));
					ImGui::TableNextColumn();
					ImGui::ColorButton("##color", ImGui::ColorConvertU32ToFloat4(movie.tracks[i].editor_color),
						ImGuiColorEditFlags_NoTooltip, ImVec2(12, 12)); ImGui::SameLine();
					if (ImGui::Selectable(movie.tracks[i].editor_name.c_str(), i == m_atlas_movie_sel_track)) {
						m_atlas_movie_sel_track = i;
						if (!advanced_mode || !ImGui::GetIO().KeyCtrl) actor_selection_mask = static_cast<byte>(1u << i);
						else actor_selection_mask ^= static_cast<byte>(1u << i);
					}
					if (advanced_mode) {
						ImGui::TableNextColumn(); ImGui::TextDisabled("%s", movie.tracks[i].editor_group.c_str());
					}
					ImGui::PopID();
				}
				ImGui::EndTable();
			}
			if (advanced_mode && ui::imgui_button("Add path", 2, "Maximum eight tracks", movie.tracks.size() >= 8)) {
				auto actor{ default_track(AtlasMovieTrackKind::Path) };
				initialize_actor_editor(actor, movie.tracks.size()); movie.tracks.push_back(std::move(actor));
				m_atlas_movie_sel_track = movie.tracks.size() - 1; actor_selection_mask = static_cast<byte>(1u << m_atlas_movie_sel_track);
				m_atlas_movie_dirty = actor_structure_changed = true;
			}
			if (advanced_mode) ImGui::SameLine();
			if (advanced_mode && ui::imgui_button("Add cyclic", 2, "Maximum eight tracks", movie.tracks.size() >= 8)) {
				auto actor{ default_track(AtlasMovieTrackKind::Cyclic) };
				initialize_actor_editor(actor, movie.tracks.size()); movie.tracks.push_back(std::move(actor));
				m_atlas_movie_sel_track = movie.tracks.size() - 1; actor_selection_mask = static_cast<byte>(1u << m_atlas_movie_sel_track);
				m_atlas_movie_dirty = actor_structure_changed = true;
			}
			if (advanced_mode) ImGui::SameLine();
			if (advanced_mode && ui::imgui_button("Add toggle", 2, "Maximum eight tracks", movie.tracks.size() >= 8)) {
				auto actor{ default_track(AtlasMovieTrackKind::CounterToggle) };
				initialize_actor_editor(actor, movie.tracks.size()); movie.tracks.push_back(std::move(actor));
				m_atlas_movie_sel_track = movie.tracks.size() - 1; actor_selection_mask = static_cast<byte>(1u << m_atlas_movie_sel_track);
				m_atlas_movie_dirty = actor_structure_changed = true;
			}
			if (advanced_mode) ImGui::SameLine();
			if (ui::imgui_button(advanced_mode ? "Delete track" : "Delete selected actor", 1,
				"Removes this actor from every timeline phase", movie.tracks.size() <= 1)) {
				const auto removed{ m_atlas_movie_sel_track };
				movie.tracks.erase(movie.tracks.begin() + removed);
				for (auto& phase : movie.phases) {
					phase.update_mask = remove_mask_bit(phase.update_mask, removed);
					phase.draw_mask = remove_mask_bit(phase.draw_mask, removed);
					if (phase.effect_track == removed) { phase.effect = AtlasMovieEffect::None; phase.effect_track = 0xff; }
					else if (phase.effect_track != 0xff && phase.effect_track > removed) --phase.effect_track;
					if (phase.condition_track == removed) phase.condition_track = 0xff;
					else if (phase.condition_track != 0xff && phase.condition_track > removed) --phase.condition_track;
				}
				movie.sfx.erase(std::remove_if(movie.sfx.begin(), movie.sfx.end(), [&](const auto& event) { return event.track == removed; }), movie.sfx.end());
				for (auto& event : movie.sfx) if (event.track > removed) --event.track;
				actor_selection_mask = remove_mask_bit(actor_selection_mask, removed);
				m_atlas_movie_sel_track = std::min(m_atlas_movie_sel_track, movie.tracks.size() - 1);
				actor_selection_mask |= static_cast<byte>(1u << m_atlas_movie_sel_track);
				m_atlas_movie_dirty = actor_structure_changed = true;
			}

			if (advanced_mode && !actor_structure_changed) {
				ImGui::SeparatorText("Composition");
				auto selected_indices = [&]() {
					std::vector<std::size_t> result;
					for (std::size_t i{ 0 }; i < movie.tracks.size(); ++i)
						if (actor_selection_mask & (1u << i)) result.push_back(i);
					return result;
				};
				auto unique_actor_name = [&](std::string base) {
					if (base.empty()) base = "Actor";
					auto exists = [&](const std::string& name) {
						return std::ranges::any_of(movie.tracks,
							[&](const auto& actor) { return actor.editor_name == name; });
					};
					if (!exists(base)) return base;
					for (std::size_t suffix{ 2 }; ; ++suffix) {
						const auto candidate{ std::format("{} {}", base, suffix) };
						if (!exists(candidate)) return candidate;
					}
				};
				auto add_composed_actor = [&](AtlasMovieTrack actor, std::optional<std::size_t> source) {
					const std::size_t new_index{ movie.tracks.size() };
					actor.editor_name = unique_actor_name(actor.editor_name + " Copy");
					translate_actor(actor, 8, 8);
					movie.tracks.push_back(std::move(actor));
					for (auto& phase : movie.phases) {
						if (source) {
							if (phase.update_mask & (1u << *source)) phase.update_mask |= static_cast<byte>(1u << new_index);
							if (phase.draw_mask & (1u << *source)) phase.draw_mask |= static_cast<byte>(1u << new_index);
						}
					}
					if (!source) {
						const std::size_t phase{ std::min(m_atlas_movie_sel_phase, movie.phases.size() - 1) };
						movie.phases[phase].update_mask |= static_cast<byte>(1u << new_index);
						movie.phases[phase].draw_mask |= static_cast<byte>(1u << new_index);
					}
					if (source) {
						std::vector<AtlasMovieSfx> copied;
						for (const auto& event : movie.sfx)
							if (event.track == *source) { auto clone{ event }; clone.track = static_cast<byte>(new_index); copied.push_back(clone); }
						movie.sfx.insert(movie.sfx.end(), copied.begin(), copied.end());
					}
					return new_index;
				};
				const auto selected{ selected_indices() };
				if (ui::imgui_button("Duplicate selected", 2, "Maximum eight tracks",
					selected.empty() || movie.tracks.size() + selected.size() > 8)) {
					byte new_mask{};
					for (const auto source : selected) {
						const auto new_index{ add_composed_actor(movie.tracks[source], source) };
						new_mask |= static_cast<byte>(1u << new_index);
					}
					actor_selection_mask = new_mask; m_atlas_movie_sel_track = movie.tracks.size() - 1;
					m_atlas_movie_dirty = actor_structure_changed = true;
				}
				ImGui::SameLine();
				if (ui::imgui_button("Copy", 2, "Select at least one actor", selected.empty())) {
					actor_clipboard.clear();
					for (const auto index : selected) actor_clipboard.push_back(movie.tracks[index]);
				}
				ImGui::SameLine();
				if (ui::imgui_button("Paste", 2, "Clipboard empty or insufficient track slots",
					actor_clipboard.empty() || movie.tracks.size() + actor_clipboard.size() > 8)) {
					guarded([&] {
						const auto pasted{ paste_actors(bundle, m_atlas_movie_sel_movie,
							actor_clipboard, m_atlas_movie_sel_phase) };
						byte new_mask{};
						for (const auto index : pasted)
							new_mask |= static_cast<byte>(1u << index);
						actor_selection_mask = new_mask;
						m_atlas_movie_sel_track = pasted.back();
						m_atlas_movie_dirty = actor_structure_changed = true;
					});
				}
				if (!actor_structure_changed) {
					auto mirror_velocity = [](std::int8_t value) {
						return static_cast<std::int8_t>(value == -128 ? 127 : -value);
					};
					if (ui::imgui_button("Mirror X", 2, "Select at least one actor", selected.empty())) {
						for (const auto index : selected) {
							auto& actor{ movie.tracks[index] }; actor.x = static_cast<byte>(255 - actor.x);
							actor.velocity_x = mirror_velocity(actor.velocity_x);
							for (auto& key : actor.keyframes) key.velocity_x = mirror_velocity(key.velocity_x);
							if (actor.kind == AtlasMovieTrackKind::Path && actor.coordinate == AtlasMovieCoordinate::X) {
								for (auto& key : actor.keyframes) key.threshold = static_cast<byte>(255 - key.threshold);
								actor.comparison = actor.comparison == AtlasMovieComparison::LessThan
									? AtlasMovieComparison::GreaterEqual : AtlasMovieComparison::LessThan;
							}
							for (auto& point : actor.editor_waypoints) point.x = static_cast<byte>(255 - point.x);
							std::swap(actor.editor_animation.left, actor.editor_animation.right);
						}
						m_atlas_movie_dirty = true;
					}
					ImGui::SameLine();
					if (ui::imgui_button("Align X", 2, "Select two or more actors", selected.size() < 2)) {
						const byte x{ movie.tracks[m_atlas_movie_sel_track].x };
						for (const auto index : selected)
							translate_actor(movie.tracks[index], static_cast<int>(x) - movie.tracks[index].x, 0);
						m_atlas_movie_dirty = true;
					}
					ImGui::SameLine();
					if (ui::imgui_button("Align Y", 2, "Select two or more actors", selected.size() < 2)) {
						const byte y{ movie.tracks[m_atlas_movie_sel_track].y };
						for (const auto index : selected)
							translate_actor(movie.tracks[index], 0, static_cast<int>(y) - movie.tracks[index].y);
						m_atlas_movie_dirty = true;
					}
					ImGui::SameLine();
					if (ui::imgui_button("Space X", 2, "Select at least three actors", selected.size() < 3)) {
						auto ordered{ selected };
						std::ranges::sort(ordered, {}, [&](std::size_t i) { return movie.tracks[i].x; });
						const int first{ movie.tracks[ordered.front()].x }, last{ movie.tracks[ordered.back()].x };
						for (std::size_t i{ 1 }; i + 1 < ordered.size(); ++i) {
							const int target{ first + (last - first) * static_cast<int>(i) / static_cast<int>(ordered.size() - 1) };
							translate_actor(movie.tracks[ordered[i]], target - movie.tracks[ordered[i]].x, 0);
						}
						m_atlas_movie_dirty = true;
					}
					ImGui::SameLine();
					if (ui::imgui_button("Space Y", 2, "Select at least three actors", selected.size() < 3)) {
						auto ordered{ selected };
						std::ranges::sort(ordered, {}, [&](std::size_t i) { return movie.tracks[i].y; });
						const int first{ movie.tracks[ordered.front()].y }, last{ movie.tracks[ordered.back()].y };
						for (std::size_t i{ 1 }; i + 1 < ordered.size(); ++i) {
							const int target{ first + (last - first) * static_cast<int>(i) / static_cast<int>(ordered.size() - 1) };
							translate_actor(movie.tracks[ordered[i]], 0, target - movie.tracks[ordered[i]].y);
						}
						m_atlas_movie_dirty = true;
					}
					ImGui::SameLine();
					if (ui::imgui_button(box_select_mode ? "Cancel box select" : "Box select on stage",
						box_select_mode ? 4 : 2)) {
						box_select_mode = !box_select_mode; box_select_dragging = false;
						waypoint_edit_mode = false; m_atlas_movie_actor_place_mode = false;
						m_atlas_movie_path_draw_mode = m_atlas_movie_path_painting = false;
					}
					ImGui::SetNextItemWidth(180); ImGui::InputTextWithHint("##composition-group", "group name", composition_group.data(), composition_group.size());
					ImGui::SameLine();
					if (ui::imgui_button("Set group", 2, "Select at least one actor", selected.empty())) {
						for (const auto index : selected) movie.tracks[index].editor_group = composition_group.data();
						m_atlas_movie_dirty = true;
					}
				}
			}

			if (!actor_structure_changed && !movie.tracks.empty()) {
				auto& track{ movie.tracks[m_atlas_movie_sel_track] };
				const auto& actor{ identity_state.tracks[m_atlas_movie_sel_track] };
				ImGui::SeparatorText("Selected actor");
				ImGui::BeginGroup();
				if (actor.frame < m_atlas_movie_frame_textures.size()
					&& m_atlas_movie_frame_textures[actor.frame])
					ImGui::Image(m_atlas_movie_frame_textures[actor.frame], ImVec2(88, 88));
				else {
					ImGui::Dummy(ImVec2(88, 88));
					ImGui::SetItemTooltip("No decoded sprite is available for this frame");
				}
				ImGui::EndGroup();
				ImGui::SameLine();
				ImGui::BeginGroup();
				ImGui::Text("%s", track.editor_name.c_str());
				if (advanced_mode) {
					ImGui::TextDisabled("Track %zu", m_atlas_movie_sel_track);
					ImGui::Text("Frame F%02u", actor.frame);
					ImGui::TextDisabled("%s", frame_family(actor.frame));
				}
				ImGui::Text("Position %u, %u", actor.x, actor.y);
				if (advanced_mode) ImGui::Text("Runtime stage %u", actor.stage);
				const bool actor_drawn{ identity_state.phase < movie.phases.size()
					&& (movie.phases[identity_state.phase].draw_mask & (1u << m_atlas_movie_sel_track))
					&& actor.visible };
				ImGui::TextColored(actor_drawn ? ImVec4(0.45f, 1.0f, 0.55f, 1.0f)
					: ImVec4(1.0f, 0.65f, 0.3f, 1.0f), actor_drawn ? "Visible now" : "Hidden in current phase");
				ImGui::EndGroup();

				ImGui::SeparatorText("Editable stage");
				static constexpr const char* stage_size_labels[]{ "1x — 256x240", "2x — 512x480" };
				int stage_size_choice{ actor_stage_scale - 1 };
				ImGui::SetNextItemWidth(150);
				if (ImGui::Combo("Stage size", &stage_size_choice, stage_size_labels, 2)) {
					actor_stage_scale = stage_size_choice + 1;
					m_atlas_movie_path_painting = false; m_atlas_movie_painted_path.clear();
				}
				if (advanced_mode) { ImGui::SameLine(); ImGui::Checkbox("Onion skin", &onion_skin); }
				if (advanced_mode && onion_skin) {
					ImGui::SameLine(); ImGui::SetNextItemWidth(105);
					ImGui::SliderInt("Frames##onion", &onion_distance, 1, 60);
				}
				m_atlas_movie_browser_frame = std::min<std::size_t>(m_atlas_movie_browser_frame,
					movie.metasprite_count - 1);
				byte actor_place_frame{ static_cast<byte>(m_atlas_movie_browser_frame) };
				ImGui::SetNextItemWidth(190);
				if (frame_combo("Pose to place##actor-stage", actor_place_frame, movie.metasprite_count))
					m_atlas_movie_browser_frame = actor_place_frame;
				if (m_atlas_movie_browser_frame < m_atlas_movie_frame_textures.size()
					&& m_atlas_movie_frame_textures[m_atlas_movie_browser_frame]) {
					ImGui::Image(m_atlas_movie_frame_textures[m_atlas_movie_browser_frame], ImVec2(38, 38));
					ImGui::SameLine();
				}
				if (movie.tracks.size() >= 8)
					ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.25f, 1.0f),
						"8/8 tracks used — delete an actor before placing another");
				if (ui::imgui_button(m_atlas_movie_actor_place_mode
					? "Cancel placement" : advanced_mode
						? std::format("Place pose F{:02}", m_atlas_movie_browser_frame)
						: std::string("Place selected pose"),
					m_atlas_movie_actor_place_mode ? 4 : 2,
					"Uses the pose selected in Frame Browser; then click its stage position",
					movie.tracks.size() >= 8)) {
					m_atlas_movie_actor_place_mode = !m_atlas_movie_actor_place_mode;
					m_atlas_movie_path_draw_mode = m_atlas_movie_path_painting = false;
					m_atlas_movie_painted_path.clear(); m_atlas_movie_preview_playing = false;
				}
				if (m_atlas_movie_actor_place_mode) {
					ImGui::SameLine();
					ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f), "Click the stage to place it");
				}
				if (ui::imgui_button(m_atlas_movie_path_draw_mode ? "Cancel path"
					: advanced_mode ? "Draw selected actor path" : "Draw movement path",
					m_atlas_movie_path_draw_mode ? 4 : 2,
					"Drag directly on the stage to draw where the selected actor should move")) {
					m_atlas_movie_path_draw_mode = !m_atlas_movie_path_draw_mode;
					m_atlas_movie_actor_place_mode = false;
					waypoint_edit_mode = false; dragged_waypoint = -1;
					m_atlas_movie_path_painting = false; m_atlas_movie_painted_path.clear();
					m_atlas_movie_preview_playing = false;
				}
				if (advanced_mode) { ImGui::SameLine(); ImGui::SetNextItemWidth(125);
				ImGui::SliderInt("Speed##actor-stage", &m_atlas_movie_path_speed, 8, 127);
				ImGui::SameLine();
				static constexpr const char* actor_snap_labels[]{ "Off", "8 px", "16 px" };
				int actor_snap_choice{ actor_stage_snap == 0 ? 0 : actor_stage_snap == 8 ? 1 : 2 };
				ImGui::SetNextItemWidth(90);
				if (ImGui::Combo("Snap##actor-stage", &actor_snap_choice, actor_snap_labels, 3))
					actor_stage_snap = actor_snap_choice == 0 ? 0 : actor_snap_choice == 1 ? 8 : 16;
				ImGui::SameLine();
				if (ui::imgui_button(waypoint_edit_mode ? "Finish waypoint editing" : "Edit waypoints",
					waypoint_edit_mode ? 4 : 2,
					"Click to append; Shift-click to insert; drag to move; right-click a handle to delete",
					track.kind != AtlasMovieTrackKind::Path)) {
					if (track.editor_waypoints.empty()) track.editor_waypoints = runtime_waypoints(track);
					waypoint_edit_mode = !waypoint_edit_mode; dragged_waypoint = -1;
					m_atlas_movie_actor_place_mode = false;
					m_atlas_movie_path_draw_mode = m_atlas_movie_path_painting = false;
					m_atlas_movie_painted_path.clear(); m_atlas_movie_preview_playing = false;
				}
				const auto invalid_segments{ invalid_waypoint_segments(track.editor_waypoints) };
				const bool invalid_waypoints{ track.editor_waypoints.size() < 2
					|| std::ranges::any_of(invalid_segments, [](bool invalid) { return invalid; }) };
				ImGui::SameLine();
				if (ui::imgui_button("Apply waypoints", 2,
					"Needs 2..16 points monotonic on one dominant axis; one final reversal is allowed",
					track.kind != AtlasMovieTrackKind::Path || invalid_waypoints)) {
					std::vector<std::array<byte, 2>> points;
					for (const auto& point : track.editor_waypoints) points.push_back({ point.x, point.y });
					guarded([&] {
						const AtlasMovieTrack backup{ track };
						try {
							apply_painted_path(movie, track, points, m_atlas_movie_path_speed, 0);
							AtlasMovieBundleCodec::validate(bundle); m_atlas_movie_dirty = true;
							m_atlas_movie_preview_frame = 0;
							add_message("Waypoints compiled into the selected actor's runtime Path", 2);
						}
						catch (...) { track = backup; throw; }
					});
				}
				}

				try {
					auto* texture{ render_movie_texture(p_renderer, m_gfx.get_nes_palette(),
						m_game->m_rom_data, movie, identity_state, decoded_frames) };
					if (texture) {
						if (m_atlas_movie_preview_texture) SDL_DestroyTexture(m_atlas_movie_preview_texture);
						m_atlas_movie_preview_texture = texture;
					}
				}
				catch (const std::exception& ex) {
					graphics_error = ex.what();
					if (m_atlas_movie_preview_texture) SDL_DestroyTexture(m_atlas_movie_preview_texture);
					m_atlas_movie_preview_texture = nullptr;
				}
				if (m_atlas_movie_preview_texture) {
					const float stage_scale{ static_cast<float>(actor_stage_scale) };
					const ImVec2 scene_origin{ ImGui::GetCursorScreenPos() };
					ImGui::Image(m_atlas_movie_preview_texture,
						ImVec2(256.0f * stage_scale, 240.0f * stage_scale));
					const bool stage_hovered{ ImGui::IsItemHovered() };
					auto stage_mouse_point = [&]() {
						const auto mouse{ ImGui::GetMousePos() };
						return std::array<byte, 2>{
							static_cast<byte>(std::clamp((mouse.x - scene_origin.x) / stage_scale, 0.0f, 255.0f)),
							static_cast<byte>(std::clamp((mouse.y - scene_origin.y) / stage_scale, 0.0f, 239.0f)) };
					};
					if (box_select_mode && stage_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
						box_select_start = box_select_end = stage_mouse_point(); box_select_dragging = true;
					}
					if (box_select_dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left))
						box_select_end = stage_mouse_point();
					if (box_select_dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
						const byte min_x{ std::min(box_select_start[0], box_select_end[0]) };
						const byte max_x{ std::max(box_select_start[0], box_select_end[0]) };
						const byte min_y{ std::min(box_select_start[1], box_select_end[1]) };
						const byte max_y{ std::max(box_select_start[1], box_select_end[1]) };
						actor_selection_mask = 0;
						for (std::size_t i{ 0 }; i < identity_state.tracks.size(); ++i)
							if (identity_state.tracks[i].x >= min_x && identity_state.tracks[i].x <= max_x
								&& identity_state.tracks[i].y >= min_y && identity_state.tracks[i].y <= max_y)
								actor_selection_mask |= static_cast<byte>(1u << i);
						if (actor_selection_mask) {
							for (std::size_t i{ 0 }; i < movie.tracks.size(); ++i)
								if (actor_selection_mask & (1u << i)) { m_atlas_movie_sel_track = i; break; }
						}
						box_select_dragging = box_select_mode = false;
					}
					if (waypoint_edit_mode && stage_hovered
						&& ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
						const auto mouse_point{ stage_mouse_point() };
						float nearest{ 9.0f * 9.0f }; dragged_waypoint = -1;
						for (std::size_t i{ 0 }; i < track.editor_waypoints.size(); ++i) {
							const float dx{ static_cast<float>(mouse_point[0]) - track.editor_waypoints[i].x };
							const float dy{ static_cast<float>(mouse_point[1]) - track.editor_waypoints[i].y };
							const float distance{ dx * dx + dy * dy };
							if (distance < nearest) { nearest = distance; dragged_waypoint = static_cast<int>(i); }
						}
						if (dragged_waypoint < 0 && track.editor_waypoints.size() < 16) {
							auto point{ mouse_point };
							if (actor_stage_snap) {
								point[0] = static_cast<byte>(std::clamp(((point[0] + actor_stage_snap / 2)
									/ actor_stage_snap) * actor_stage_snap, 0, (255 / actor_stage_snap) * actor_stage_snap));
								point[1] = static_cast<byte>(std::clamp(((point[1] + actor_stage_snap / 2)
									/ actor_stage_snap) * actor_stage_snap, 0, (239 / actor_stage_snap) * actor_stage_snap));
							}
							std::size_t insert_at{ track.editor_waypoints.size() };
							if (ImGui::GetIO().KeyShift && track.editor_waypoints.size() >= 2) {
								float best{ std::numeric_limits<float>::max() };
								for (std::size_t i{ 1 }; i < track.editor_waypoints.size(); ++i) {
									const auto& a{ track.editor_waypoints[i - 1] }; const auto& b{ track.editor_waypoints[i] };
									const float vx{ static_cast<float>(b.x) - a.x }, vy{ static_cast<float>(b.y) - a.y };
									const float length2{ vx * vx + vy * vy };
									const float t{ length2 > 0 ? std::clamp(((point[0] - a.x) * vx + (point[1] - a.y) * vy) / length2, 0.0f, 1.0f) : 0.0f };
									const float dx{ point[0] - (a.x + t * vx) }, dy{ point[1] - (a.y + t * vy) };
									if (dx * dx + dy * dy < best) { best = dx * dx + dy * dy; insert_at = i; }
								}
							}
							track.editor_waypoints.insert(track.editor_waypoints.begin() + insert_at, { point[0], point[1] });
							dragged_waypoint = static_cast<int>(insert_at); m_atlas_movie_dirty = true;
						}
					}
					if (waypoint_edit_mode && stage_hovered
						&& ImGui::IsMouseClicked(ImGuiMouseButton_Right) && track.editor_waypoints.size() > 2) {
						const auto point{ stage_mouse_point() }; float nearest{ 9.0f * 9.0f }; std::size_t remove{ track.editor_waypoints.size() };
						for (std::size_t i{ 0 }; i < track.editor_waypoints.size(); ++i) {
							const float dx{ static_cast<float>(point[0]) - track.editor_waypoints[i].x };
							const float dy{ static_cast<float>(point[1]) - track.editor_waypoints[i].y };
							if (dx * dx + dy * dy < nearest) { nearest = dx * dx + dy * dy; remove = i; }
						}
						if (remove < track.editor_waypoints.size()) {
							track.editor_waypoints.erase(track.editor_waypoints.begin() + remove);
							dragged_waypoint = -1; m_atlas_movie_dirty = true;
						}
					}
					if (waypoint_edit_mode && dragged_waypoint >= 0
						&& ImGui::IsMouseDown(ImGuiMouseButton_Left)
						&& static_cast<std::size_t>(dragged_waypoint) < track.editor_waypoints.size()) {
						auto point{ stage_mouse_point() };
						if (actor_stage_snap) {
							point[0] = static_cast<byte>(std::clamp(((point[0] + actor_stage_snap / 2)
								/ actor_stage_snap) * actor_stage_snap, 0, (255 / actor_stage_snap) * actor_stage_snap));
							point[1] = static_cast<byte>(std::clamp(((point[1] + actor_stage_snap / 2)
								/ actor_stage_snap) * actor_stage_snap, 0, (239 / actor_stage_snap) * actor_stage_snap));
						}
						track.editor_waypoints[dragged_waypoint] = { point[0], point[1] };
						m_atlas_movie_dirty = true;
					}
					if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) dragged_waypoint = -1;
					if (!box_select_mode && !waypoint_edit_mode && m_atlas_movie_path_draw_mode && stage_hovered
						&& ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
						m_atlas_movie_path_painting = true;
						m_atlas_movie_painted_path = { stage_mouse_point() };
					}
					if (m_atlas_movie_path_painting && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
						const auto point{ stage_mouse_point() };
						if (m_atlas_movie_painted_path.empty() || point != m_atlas_movie_painted_path.back())
							m_atlas_movie_painted_path.push_back(point);
					}
					if (m_atlas_movie_path_painting && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
						m_atlas_movie_path_painting = false;
						guarded([&] {
							auto& selected{ movie.tracks.at(m_atlas_movie_sel_track) };
							const AtlasMovieTrack backup{ selected };
							try {
								apply_painted_path(movie, selected, m_atlas_movie_painted_path,
									m_atlas_movie_path_speed, actor_stage_snap);
								AtlasMovieBundleCodec::validate(bundle);
								m_atlas_movie_dirty = true; m_atlas_movie_preview_frame = 0;
								add_message("Selected actor path applied; cyan is the exact compiled route", 2);
							}
							catch (...) { selected = backup; throw; }
						});
						m_atlas_movie_path_draw_mode = false; m_atlas_movie_painted_path.clear();
					}
					if (m_atlas_movie_actor_place_mode && stage_hovered
						&& ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
						const auto point{ stage_mouse_point() };
						pending_actor_placement = std::array<std::size_t, 3>{ point[0], point[1],
							identity_state.phase < movie.phases.size() ? identity_state.phase : m_atlas_movie_sel_phase };
						m_atlas_movie_actor_place_mode = false;
					}
					if (!box_select_mode && !waypoint_edit_mode && !m_atlas_movie_actor_place_mode && !pending_actor_placement
						&& !m_atlas_movie_path_draw_mode && stage_hovered
						&& ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
						const auto mouse{ ImGui::GetMousePos() };
						float nearest{ 22.0f * stage_scale * 22.0f * stage_scale };
						actor_stage_dragged_track = -1;
						const byte phase_draw_mask{ static_cast<byte>(identity_state.phase < movie.phases.size()
							? movie.phases[identity_state.phase].draw_mask : 0) };
						for (std::size_t i{ 0 }; i < identity_state.tracks.size(); ++i) {
							if (!(phase_draw_mask & (1u << i)) || !identity_state.tracks[i].visible) continue;
							const float dx{ mouse.x - (scene_origin.x + identity_state.tracks[i].x * stage_scale) };
							const float dy{ mouse.y - (scene_origin.y + identity_state.tracks[i].y * stage_scale) };
							const float distance{ dx * dx + dy * dy };
							if (distance < nearest) { nearest = distance; actor_stage_dragged_track = static_cast<int>(i); }
						}
						if (actor_stage_dragged_track >= 0) {
							m_atlas_movie_sel_track = static_cast<std::size_t>(actor_stage_dragged_track);
							actor_stage_drag_point = stage_mouse_point();
							m_atlas_movie_preview_playing = false;
						}
					}
					if (actor_stage_dragged_track >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left)
						&& static_cast<std::size_t>(actor_stage_dragged_track) < movie.tracks.size()) {
						const auto mouse_point{ stage_mouse_point() };
						auto& dragged{ movie.tracks[actor_stage_dragged_track] };
						translate_actor(dragged, static_cast<int>(mouse_point[0]) - actor_stage_drag_point[0],
							static_cast<int>(mouse_point[1]) - actor_stage_drag_point[1]);
						actor_stage_drag_point = mouse_point;
						m_atlas_movie_dirty = true;
					}
					if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) actor_stage_dragged_track = -1;
					auto* draw{ ImGui::GetWindowDrawList() };
					const ImU32 selected_actor_color{ track.editor_color
						? track.editor_color : ACTOR_UI_COLORS[m_atlas_movie_sel_track % ACTOR_UI_COLORS.size()] };
					if (onion_skin) {
						const std::array<std::pair<std::size_t, ImU32>, 2> onion_frames{
							std::pair{ m_atlas_movie_preview_frame > static_cast<std::size_t>(onion_distance)
								? m_atlas_movie_preview_frame - onion_distance : 0u, IM_COL32(90, 170, 255, 105) },
							std::pair{ m_atlas_movie_preview_frame + onion_distance, IM_COL32(255, 110, 170, 105) }
						};
						for (const auto& [frame_number, tint] : onion_frames) {
							const auto ghost_state{ preview(movie, frame_number) };
							if (m_atlas_movie_sel_track >= ghost_state.tracks.size()) continue;
							const auto& ghost{ ghost_state.tracks[m_atlas_movie_sel_track] };
							if (ghost.frame >= decoded_frames.size() || ghost.frame >= m_atlas_movie_frame_textures.size()
								|| !m_atlas_movie_frame_textures[ghost.frame]) continue;
							const auto& ghost_frame{ decoded_frames[ghost.frame] };
							const ImVec2 p0{ scene_origin.x + (ghost.x + ghost_frame.offset_x) * stage_scale,
								scene_origin.y + (ghost.y + ghost_frame.offset_y) * stage_scale };
							const ImVec2 p1{ p0.x + ghost_frame.w() * 8.0f * stage_scale,
								p0.y + ghost_frame.h() * 8.0f * stage_scale };
							draw->AddImage(m_atlas_movie_frame_textures[ghost.frame], p0, p1,
								ImVec2(0, 0), ImVec2(1, 1), tint);
						}
					}
					if (actor_stage_snap) {
						for (int x{ 0 }; x <= 256; x += actor_stage_snap)
							draw->AddLine(ImVec2(scene_origin.x + x * stage_scale, scene_origin.y),
								ImVec2(scene_origin.x + x * stage_scale, scene_origin.y + 240 * stage_scale), IM_COL32(255,255,255,38));
						for (int y{ 0 }; y <= 240; y += actor_stage_snap)
							draw->AddLine(ImVec2(scene_origin.x, scene_origin.y + y * stage_scale),
								ImVec2(scene_origin.x + 256 * stage_scale, scene_origin.y + y * stage_scale), IM_COL32(255,255,255,38));
					}
					if (box_select_dragging) {
						const ImVec2 a{ scene_origin.x + box_select_start[0] * stage_scale,
							scene_origin.y + box_select_start[1] * stage_scale };
						const ImVec2 b{ scene_origin.x + box_select_end[0] * stage_scale,
							scene_origin.y + box_select_end[1] * stage_scale };
						draw->AddRectFilled(ImVec2(std::min(a.x,b.x), std::min(a.y,b.y)),
							ImVec2(std::max(a.x,b.x), std::max(a.y,b.y)), IM_COL32(80, 180, 255, 45));
						draw->AddRect(ImVec2(std::min(a.x,b.x), std::min(a.y,b.y)),
							ImVec2(std::max(a.x,b.x), std::max(a.y,b.y)), IM_COL32(90, 210, 255, 255), 0, 0, 2.0f);
					}
					if (waypoint_edit_mode && track.editor_waypoints.size() >= 1) {
						const auto invalid{ invalid_waypoint_segments(track.editor_waypoints) };
						for (std::size_t i{ 1 }; i < track.editor_waypoints.size(); ++i) {
							const auto& a{ track.editor_waypoints[i - 1] }; const auto& b{ track.editor_waypoints[i] };
							draw->AddLine(ImVec2(scene_origin.x + a.x * stage_scale, scene_origin.y + a.y * stage_scale),
								ImVec2(scene_origin.x + b.x * stage_scale, scene_origin.y + b.y * stage_scale),
								invalid[i - 1] ? IM_COL32(255, 70, 70, 255) : IM_COL32(255, 180, 45, 255),
								3.0f * stage_scale);
						}
						for (std::size_t i{ 0 }; i < track.editor_waypoints.size(); ++i) {
							const auto& point{ track.editor_waypoints[i] };
							const ImVec2 center{ scene_origin.x + point.x * stage_scale, scene_origin.y + point.y * stage_scale };
							draw->AddCircleFilled(center, 5.0f * stage_scale, IM_COL32(20, 20, 25, 240));
							draw->AddCircle(center, 5.0f * stage_scale, IM_COL32(255, 210, 70, 255), 0, 2.0f * stage_scale);
							draw->AddText(ImVec2(center.x + 7.0f, center.y - 7.0f), IM_COL32_WHITE,
								std::format("{}", i).c_str());
						}
					}
					const auto actor_route{ !m_atlas_movie_path_painting
						? simulated_path(movie.tracks[m_atlas_movie_sel_track])
						: std::vector<std::array<byte, 2>>{} };
					for (std::size_t i{ 1 }; i < actor_route.size(); ++i)
						draw->AddLine(ImVec2(scene_origin.x + actor_route[i - 1][0] * stage_scale, scene_origin.y + actor_route[i - 1][1] * stage_scale),
							ImVec2(scene_origin.x + actor_route[i][0] * stage_scale, scene_origin.y + actor_route[i][1] * stage_scale),
							IM_COL32(70, 235, 255, 230), 2.0f * stage_scale);
					if (!actor_route.empty()) {
						draw->AddCircleFilled(ImVec2(scene_origin.x + actor_route.front()[0] * stage_scale,
							scene_origin.y + actor_route.front()[1] * stage_scale), 3.0f * stage_scale, IM_COL32(80, 255, 150, 255));
						draw->AddCircleFilled(ImVec2(scene_origin.x + actor_route.back()[0] * stage_scale,
							scene_origin.y + actor_route.back()[1] * stage_scale), 3.0f * stage_scale, IM_COL32(255, 120, 100, 255));
					}
					for (std::size_t i{ 1 }; i < m_atlas_movie_painted_path.size(); ++i)
						draw->AddLine(ImVec2(scene_origin.x + m_atlas_movie_painted_path[i - 1][0] * stage_scale, scene_origin.y + m_atlas_movie_painted_path[i - 1][1] * stage_scale),
							ImVec2(scene_origin.x + m_atlas_movie_painted_path[i][0] * stage_scale, scene_origin.y + m_atlas_movie_painted_path[i][1] * stage_scale),
							IM_COL32(255, 80, 220, 255), 2.0f * stage_scale);
					for (const auto& point : m_atlas_movie_painted_path)
						draw->AddCircleFilled(ImVec2(scene_origin.x + point[0] * stage_scale, scene_origin.y + point[1] * stage_scale),
							2.0f * stage_scale, IM_COL32(255, 245, 120, 255));
					float actor_center_x{ static_cast<float>(actor.x) };
					float actor_center_y{ static_cast<float>(actor.y) };
					if (actor.frame < decoded_frames.size()) {
						const auto& frame{ decoded_frames[actor.frame] };
						actor_center_x += frame.offset_x + frame.w() * 4.0f;
						actor_center_y += frame.offset_y + frame.h() * 4.0f;
					}
					const ImVec2 marker{
						scene_origin.x + std::clamp(actor_center_x, 4.0f, 252.0f) * stage_scale,
						scene_origin.y + std::clamp(actor_center_y, 4.0f, 236.0f) * stage_scale };
					const ImVec2 arrow_start{ marker.x, std::max(scene_origin.y + 7.0f, marker.y - 34.0f) };
					const ImVec2 arrow_tip{ marker.x, std::max(scene_origin.y + 4.0f, marker.y - 10.0f) };
					draw->AddCircle(marker, 10.0f, selected_actor_color, 0, 3.0f);
					draw->AddLine(arrow_start, arrow_tip, selected_actor_color, 4.0f);
					draw->AddTriangleFilled(arrow_tip,
						ImVec2(arrow_tip.x - 6.0f, arrow_tip.y - 8.0f),
						ImVec2(arrow_tip.x + 6.0f, arrow_tip.y - 8.0f), selected_actor_color);
					const auto label{ std::format("T{}", m_atlas_movie_sel_track) };
					const ImVec2 label_pos{ std::clamp(marker.x + 13.0f, scene_origin.x + 3.0f,
						scene_origin.x + 256.0f * stage_scale - 25.0f),
						std::clamp(marker.y - 8.0f, scene_origin.y + 3.0f,
						scene_origin.y + 240.0f * stage_scale - 18.0f) };
					const ImVec2 label_size{ ImGui::CalcTextSize(label.c_str()) };
					draw->AddRectFilled(ImVec2(label_pos.x - 3, label_pos.y - 2),
						ImVec2(label_pos.x + label_size.x + 3, label_pos.y + label_size.y + 2),
						IM_COL32(0, 0, 0, 220), 3.0f);
					draw->AddText(label_pos, IM_COL32(255, 245, 145, 255), label.c_str());
				}
				else ImGui::TextDisabled("Scene thumbnail unavailable: %s", graphics_error.c_str());
				draw_atlas_movie_actor_inspector(
					bundle, movie, track, advanced_mode);
			}
			ImGui::EndTabItem();
		}
}
