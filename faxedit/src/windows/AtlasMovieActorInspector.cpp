#include "MainWindow.h"
#include "AtlasMovieUi.h"
#include "Imgui_helper.h"
#include "fe/AtlasMovieEditor.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <vector>

namespace {

	using namespace fe;
	using namespace fe::atlas_movie;
	using namespace fe::atlas_movie::ui_detail;

}

void fe::MainWindow::draw_atlas_movie_actor_inspector(
	AtlasMovieBundle& bundle, AtlasMovie& movie, AtlasMovieTrack& track,
	bool advanced_mode) {
	auto guarded = [this](auto&& action) {
		try { action(); }
		catch (const std::exception& ex) { add_message(ex.what(), 1); }
	};
				ImGui::SeparatorText(advanced_mode ? "Track properties" : "Actor details");
				char actor_name[64]{};
				std::snprintf(actor_name, sizeof(actor_name), "%s", track.editor_name.c_str());
				if (ImGui::InputText("Actor name", actor_name, sizeof(actor_name))) {
					track.editor_name = actor_name; m_atlas_movie_dirty = true;
				}
				if (!advanced_mode)
					ImGui::TextDisabled("Use Poses to change its appearance, or Advanced for animation and runtime fields.");
				if (advanced_mode) {
				char actor_group[64]{};
				std::snprintf(actor_group, sizeof(actor_group), "%s", track.editor_group.c_str());
				if (ImGui::InputText("Group", actor_group, sizeof(actor_group))) {
					track.editor_group = actor_group; m_atlas_movie_dirty = true;
				}
				ImVec4 actor_color{ ImGui::ColorConvertU32ToFloat4(track.editor_color) };
				if (ImGui::ColorEdit4("Actor color", &actor_color.x,
					ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf)) {
					track.editor_color = ImGui::ColorConvertFloat4ToU32(actor_color); m_atlas_movie_dirty = true;
				}
				ImGui::SeparatorText("Semantic animation sets");
				if (ImGui::Checkbox("Automatic facing", &track.editor_animation.automatic_facing))
					m_atlas_movie_dirty = true;
				auto edit_animation_set = [&](const char* label, std::vector<byte>& frames) {
					ImGui::PushID(label); ImGui::Text("%s", label); ImGui::SameLine();
					for (std::size_t i{ 0 }; i < frames.size(); ++i) {
						if (i) ImGui::SameLine();
						ImGui::PushID(static_cast<int>(i));
						ImGui::SetNextItemWidth(62);
						m_atlas_movie_dirty |= frame_combo("##semantic-frame", frames[i], movie.metasprite_count);
						ImGui::PopID();
					}
					if (!frames.empty()) ImGui::SameLine();
					if (ui::imgui_button("+", 2, "Add the Frame Browser pose", frames.size() >= 16)) {
						frames.push_back(static_cast<byte>(m_atlas_movie_browser_frame)); m_atlas_movie_dirty = true;
					}
					ImGui::SameLine();
					if (ui::imgui_button("-", 1, "Remove last pose", frames.empty())) {
						frames.pop_back(); m_atlas_movie_dirty = true;
					}
					ImGui::PopID();
				};
				edit_animation_set("Idle", track.editor_animation.idle);
				edit_animation_set("Left", track.editor_animation.left);
				edit_animation_set("Right", track.editor_animation.right);
				edit_animation_set("Toward", track.editor_animation.toward);
				edit_animation_set("Away", track.editor_animation.away);
				edit_animation_set("Attack", track.editor_animation.attack);
				edit_animation_set("Hurt", track.editor_animation.hurt);
				if (track.editor_animation.automatic_facing && track.kind == AtlasMovieTrackKind::Path
					&& ui::imgui_button("Apply facing sets to path", 2,
						"Recompile editable waypoints so every movement leg uses its semantic direction set",
						track.editor_waypoints.size() < 2)) {
					std::vector<std::array<byte, 2>> points;
					for (const auto& point : track.editor_waypoints) points.push_back({ point.x, point.y });
					const AtlasMovieTrack backup{ track };
					guarded([&] { try {
						apply_painted_path(movie, track, points, m_atlas_movie_path_speed, 0);
						AtlasMovieBundleCodec::validate(bundle); m_atlas_movie_dirty = true;
					} catch (...) { track = backup; throw; } });
				}
				static constexpr std::array track_kinds{ "Path", "Cyclic", "Counter toggle" };
				AtlasMovieTrackKind new_kind{ track.kind };
				if (enum_combo("Type", new_kind, track_kinds, 1) && new_kind != track.kind) {
					const auto name{ track.editor_name }; const auto color{ track.editor_color };
					const auto group{ track.editor_group }; const auto animation{ track.editor_animation };
					track = default_track(new_kind, track.kind == AtlasMovieTrackKind::Path ? track.stage_frames.front().front() : 0);
					track.editor_name = name; track.editor_color = color;
					track.editor_group = group; track.editor_animation = animation;
					m_atlas_movie_dirty = true;
				}
				const byte old_x{ track.x }, old_y{ track.y };
				if (edit_byte("Initial X", track.x)) {
					const byte requested{ track.x }; track.x = old_x;
					translate_actor(track, static_cast<int>(requested) - old_x, 0); m_atlas_movie_dirty = true;
				}
				if (edit_byte("Initial Y", track.y)) {
					const byte requested{ track.y }; track.y = old_y;
					translate_actor(track, 0, static_cast<int>(requested) - old_y); m_atlas_movie_dirty = true;
				}
				if (track.kind != AtlasMovieTrackKind::CounterToggle) {
					m_atlas_movie_dirty |= edit_byte("X fraction", track.x_fraction);
					m_atlas_movie_dirty |= edit_byte("Y fraction", track.y_fraction);
					m_atlas_movie_dirty |= edit_i8("Velocity X", track.velocity_x);
					m_atlas_movie_dirty |= edit_i8("Velocity Y", track.velocity_y);
					m_atlas_movie_dirty |= edit_byte("Integrator shift", track.integrator_shift, 1, 8);
				}
				if (track.kind == AtlasMovieTrackKind::Path) {
					static constexpr std::array coordinates{ "X", "Y" }, comparisons{ "Less than", "Greater/equal" };
					m_atlas_movie_dirty |= enum_combo("Gate coordinate", track.coordinate, coordinates, 1);
					m_atlas_movie_dirty |= enum_combo("Gate comparison", track.comparison, comparisons, 1);
					m_atlas_movie_dirty |= edit_byte("Animation dwell", track.dwell_frames, 1, 128);
					ImGui::SeparatorText("Editable waypoints");
					if (track.editor_waypoints.empty()) {
						ImGui::TextDisabled("No editor waypoints stored for this legacy/runtime path.");
						if (ui::imgui_button("Recover from runtime path", 2)) {
							track.editor_waypoints = runtime_waypoints(track); m_atlas_movie_dirty = true;
						}
					}
					for (std::size_t i{ 0 }; i < track.editor_waypoints.size();) {
						ImGui::PushID(static_cast<int>(i)); auto& point{ track.editor_waypoints[i] };
						ImGui::Text("P%zu", i); ImGui::SameLine();
						int x{ point.x }, y{ point.y };
						ImGui::SetNextItemWidth(90);
						if (ImGui::DragInt("X", &x, 1, 0, 255)) { point.x = static_cast<byte>(x); m_atlas_movie_dirty = true; }
						ImGui::SameLine(); ImGui::SetNextItemWidth(90);
						if (ImGui::DragInt("Y", &y, 1, 0, 239)) { point.y = static_cast<byte>(y); m_atlas_movie_dirty = true; }
						ImGui::SameLine();
						if (ui::imgui_button("Delete", 1, "Keep at least two points", track.editor_waypoints.size() <= 2)) {
							track.editor_waypoints.erase(track.editor_waypoints.begin() + i);
							m_atlas_movie_dirty = true; ImGui::PopID(); continue;
						}
						ImGui::PopID(); ++i;
					}
					if (ui::imgui_button("Append waypoint", 2, "Maximum sixteen", track.editor_waypoints.size() >= 16)) {
						const auto last{ track.editor_waypoints.empty() ? AtlasMovieWaypoint{ track.x, track.y }
							: track.editor_waypoints.back() };
						track.editor_waypoints.push_back({ static_cast<byte>(std::min(255, last.x + 16)), last.y });
						m_atlas_movie_dirty = true;
					}
					ImGui::SeparatorText("Movement keyframes");
					for (std::size_t i{ 0 }; i < track.keyframes.size(); ++i) {
						ImGui::PushID(static_cast<int>(i)); auto& key{ track.keyframes[i] };
						ImGui::Text("Stage %zu -> %zu", i, i + 1); ImGui::SameLine();
						ImGui::SetNextItemWidth(100); m_atlas_movie_dirty |= edit_byte("Threshold", key.threshold);
						ImGui::SameLine(); ImGui::SetNextItemWidth(90); m_atlas_movie_dirty |= edit_i8("VX", key.velocity_x);
						ImGui::SameLine(); ImGui::SetNextItemWidth(90); m_atlas_movie_dirty |= edit_i8("VY", key.velocity_y);
						ImGui::PopID();
					}
					if (ui::imgui_button("Add keyframe", 2, "Maximum fifteen", track.keyframes.size() >= 15)) {
						track.keyframes.push_back({ 200, 0, 0 }); track.stage_frames.push_back(track.stage_frames.back()); m_atlas_movie_dirty = true;
					}
					ImGui::SameLine();
					if (ui::imgui_button("Remove keyframe", 1, "Keep at least one", track.keyframes.size() <= 1)) {
						track.keyframes.pop_back(); track.stage_frames.pop_back(); m_atlas_movie_dirty = true;
					}
					ImGui::SeparatorText("Animation frame IDs by stage");
					for (std::size_t stage{ 0 }; stage < track.stage_frames.size(); ++stage) {
						ImGui::PushID(static_cast<int>(stage)); ImGui::Text("Stage %zu", stage);
						for (std::size_t slot{ 0 }; slot < track.stage_frames[stage].size(); ++slot) {
							if (slot)
								ImGui::SameLine();
							ImGui::SetNextItemWidth(70);
							ImGui::PushID(static_cast<int>(slot));
							m_atlas_movie_dirty |= frame_combo("##frame", track.stage_frames[stage][slot], movie.metasprite_count);
							m_atlas_movie_dirty |= frame_drop_target(track.stage_frames[stage][slot], movie.metasprite_count);
							ImGui::PopID();
						}
						ImGui::PopID();
					}
					if (ui::imgui_button("Add animation slot", 2, "Adds one slot to every stage", track.stage_frames.front().size() >= 255)) {
						for (auto& row : track.stage_frames)
							row.push_back(row.back());
						m_atlas_movie_dirty = true;
					}
					ImGui::SameLine();
					if (ui::imgui_button("Remove animation slot", 1, "Keep at least one", track.stage_frames.front().size() <= 1)) {
						for (auto& row : track.stage_frames)
							row.pop_back();
						m_atlas_movie_dirty = true;
					}
				}
				else if (track.kind == AtlasMovieTrackKind::Cyclic) {
					m_atlas_movie_dirty |= edit_byte("Animation dwell", track.dwell_frames, 1, 255);
					m_atlas_movie_dirty |= edit_byte("Reset pose", track.reset_at_pose, 2, 255);
					for (std::size_t i{ 0 }; i < track.visible_frames.size(); ++i) {
						ImGui::PushID(static_cast<int>(i)); ImGui::SetNextItemWidth(100);
						m_atlas_movie_dirty |= frame_combo("Frame", track.visible_frames[i], movie.metasprite_count);
						m_atlas_movie_dirty |= frame_drop_target(track.visible_frames[i], movie.metasprite_count); ImGui::PopID();
					}
					if (ui::imgui_button("Add visible frame", 2)) { track.visible_frames.push_back(track.visible_frames.back()); track.reset_at_pose = std::max<byte>(track.reset_at_pose, track.visible_frames.size() + 1); m_atlas_movie_dirty = true; }
					ImGui::SameLine();
					if (ui::imgui_button("Remove visible frame", 1, "Keep one", track.visible_frames.size() <= 1)) { track.visible_frames.pop_back(); m_atlas_movie_dirty = true; }
				}
				else {
					m_atlas_movie_dirty |= edit_word("Counter address", track.counter_address);
					if (track.counter_address != 0x001a)
						ImGui::TextDisabled("Preview cannot read this game RAM address; it shows Frame A.");
					m_atlas_movie_dirty |= edit_byte("Counter mask", track.counter_mask, 1, 255);
					m_atlas_movie_dirty |= frame_combo("Frame A", track.toggle_frames[0], movie.metasprite_count);
					m_atlas_movie_dirty |= frame_drop_target(track.toggle_frames[0], movie.metasprite_count);
					m_atlas_movie_dirty |= frame_combo("Frame B", track.toggle_frames[1], movie.metasprite_count);
					m_atlas_movie_dirty |= frame_drop_target(track.toggle_frames[1], movie.metasprite_count);
				}
				}
}
