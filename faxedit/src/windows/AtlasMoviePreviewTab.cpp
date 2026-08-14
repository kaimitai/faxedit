#include "MainWindow.h"
#include "AtlasMovieRenderer.h"
#include "AtlasMovieUi.h"
#include "Imgui_helper.h"
#include "fe/AtlasMovieEditor.h"
#include "fe/AtlasMoviePreview.h"
#include <algorithm>
#include <array>
#include <format>
#include <ranges>
#include <vector>

namespace {

	using namespace fe;
	using namespace fe::atlas_movie;
	using namespace fe::atlas_movie::ui_detail;

	constexpr auto& ATLAS_FRAME_PAYLOAD{ FRAME_PAYLOAD };
	constexpr auto& ATLAS_GAME_SPRITE_PAYLOAD{ GAME_SPRITE_PAYLOAD };
	constexpr auto& ACTOR_UI_COLORS{ ACTOR_COLORS };

}

void fe::MainWindow::draw_atlas_movie_preview_tab(SDL_Renderer* p_renderer,
	AtlasMovieBundle& bundle, AtlasMovie& movie,
	const std::vector<SpriteAnimationFrame>& decoded_frames,
	std::string& graphics_error, bool advanced_mode, bool shared_mode) {
	auto guarded = [this](auto&& action) {
		try { action(); }
		catch (const std::exception& ex) { add_message(ex.what(), 1); }
	};
		if (ImGui::BeginTabItem(advanced_mode ? "Movie Preview" : "Preview")) {
			static bool show_grid{ false }, show_labels{ true }, editing_overlays{ false };
			bool placed_actor_this_frame{ false };
			m_atlas_movie_path_draw_mode = m_atlas_movie_path_painting = false;
			m_atlas_movie_painted_path.clear();
			if (ui::imgui_button(m_atlas_movie_preview_playing ? "Pause" : "Play", m_atlas_movie_preview_playing ? 4 : 2)) {
				m_atlas_movie_preview_playing = !m_atlas_movie_preview_playing; m_atlas_movie_preview_tick = SDL_GetTicks();
			}
			ImGui::SameLine();
			if (ui::imgui_button("Restart", 2)) { m_atlas_movie_preview_frame = 0; m_atlas_movie_preview_tick = SDL_GetTicks(); }
			if (advanced_mode) { ImGui::SameLine(); ImGui::Checkbox("Editing overlays", &editing_overlays); }
			else editing_overlays = false;
			if (m_atlas_movie_preview_playing) {
				const auto now{ SDL_GetTicks() };
				while (now - m_atlas_movie_preview_tick >= 16) { ++m_atlas_movie_preview_frame; m_atlas_movie_preview_tick += 16; }
			}
			int frame{ static_cast<int>(std::min<std::size_t>(m_atlas_movie_preview_frame, 36000)) };
			if (ImGui::SliderInt("Frame", &frame, 0, 3600)) { m_atlas_movie_preview_frame = frame; m_atlas_movie_preview_playing = false; }
			const auto state{ preview(movie, m_atlas_movie_preview_frame) };
			if (std::ranges::any_of(state.tracks,
				[](const auto& track) { return !track.counter_resolved; }))
				ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.2f, 1.0f),
					"Preview cannot observe arbitrary game RAM counters; unresolved toggles show Frame A.");
			if (advanced_mode)
				ImGui::Text("Phase %zu  phase frame %zu  %s", state.phase, state.phase_frame, state.finished ? "finished" : "running");
			else ImGui::Text("Phase %zu — %s", state.phase + 1, state.finished ? "finished" : "playing");

			const ImVec2 canvas_size{ 512, 480 }, origin{ ImGui::GetCursorScreenPos() };
			try {
				auto* texture{ render_movie_texture(p_renderer, m_gfx.get_nes_palette(),
					m_game->m_rom_data, movie, state, decoded_frames) };
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
			if (m_atlas_movie_preview_texture)
				ImGui::Image(m_atlas_movie_preview_texture, canvas_size);
			else
				ImGui::InvisibleButton("movie-canvas", canvas_size, ImGuiButtonFlags_MouseButtonLeft);
			const bool canvas_hovered{ ImGui::IsItemHovered() };
			auto mouse_nes_point = [&]() {
				const auto mouse{ ImGui::GetMousePos() };
				return std::array<byte, 2>{
					static_cast<byte>(std::clamp((mouse.x - origin.x) / 2.0f, 0.0f, 255.0f)),
					static_cast<byte>(std::clamp((mouse.y - origin.y) / 2.0f, 0.0f, 239.0f))
				};
			};
			if (editing_overlays && m_atlas_movie_actor_place_mode && canvas_hovered
				&& ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				const auto point{ mouse_nes_point() };
					guarded([&] { place_atlas_movie_pose(bundle, movie,
						static_cast<byte>(m_atlas_movie_browser_frame),
					point[0], point[1], state.phase < movie.phases.size() ? state.phase : m_atlas_movie_sel_phase); });
				m_atlas_movie_actor_place_mode = false;
				placed_actor_this_frame = true;
			}
			if (ImGui::BeginDragDropTarget()) {
				if (const auto* payload{ ImGui::AcceptDragDropPayload(ATLAS_FRAME_PAYLOAD) }) {
					if (movie.tracks.size() < 8) {
						const byte dropped_frame{ *static_cast<const byte*>(payload->Data) };
						const auto mouse{ ImGui::GetMousePos() };
						auto actor{ default_track(AtlasMovieTrackKind::CounterToggle, dropped_frame) };
						actor.x = static_cast<byte>(std::clamp((mouse.x - origin.x) / 2.0f, 0.0f, 255.0f));
						actor.y = static_cast<byte>(std::clamp((mouse.y - origin.y) / 2.0f, 0.0f, 239.0f));
						initialize_actor_editor(actor, movie.tracks.size(), std::format("Pose F{:02}", dropped_frame));
						movie.tracks.push_back(std::move(actor));
						m_atlas_movie_sel_track = movie.tracks.size() - 1;
						const std::size_t phase_index{ state.phase < movie.phases.size()
							? state.phase : m_atlas_movie_sel_phase };
						movie.phases[phase_index].update_mask |= static_cast<byte>(1u << m_atlas_movie_sel_track);
						movie.phases[phase_index].draw_mask |= static_cast<byte>(1u << m_atlas_movie_sel_track);
						m_atlas_movie_dirty = true;
					}
					else add_message("Atlas movies support at most eight tracks", 1);
				}
				if (shared_mode) if (const auto* payload{ ImGui::AcceptDragDropPayload(ATLAS_GAME_SPRITE_PAYLOAD) }) {
					const auto sprite_id{ *static_cast<const std::size_t*>(payload->Data) };
					const auto mouse{ ImGui::GetMousePos() };
					const byte x{ static_cast<byte>(std::clamp((mouse.x - origin.x) / 2.0f, 0.0f, 255.0f)) };
					const byte y{ static_cast<byte>(std::clamp((mouse.y - origin.y) / 2.0f, 0.0f, 239.0f)) };
					guarded([&] { import_atlas_game_sprite(bundle, movie,
						decoded_frames, sprite_id, x, y); });
				}
				ImGui::EndDragDropTarget();
			}
			auto* draw{ ImGui::GetWindowDrawList() };
			if (!m_atlas_movie_preview_texture)
				draw->AddRectFilled(origin, ImVec2(origin.x + canvas_size.x, origin.y + canvas_size.y), IM_COL32(10, 14, 24, 255));
			if (advanced_mode) {
			ImGui::SameLine();
			ImGui::BeginChild("stage-toolbox", ImVec2(125, 480), ImGuiChildFlags_Borders);
			ImGui::SeparatorText(editing_overlays ? "Preview overlays" : "Clean preview");
			if (!editing_overlays) ImGui::BeginDisabled();
			ImGui::Checkbox("Grid", &show_grid);
			ImGui::Checkbox("Actor labels", &show_labels);
			ImGui::Text("NES 256x240");
			ImGui::Text("2x nearest-neighbor");
			ImGui::SeparatorText("Place actor");
			m_atlas_movie_browser_frame = std::min<std::size_t>(m_atlas_movie_browser_frame,
				movie.metasprite_count - 1);
			byte preview_place_frame{ static_cast<byte>(m_atlas_movie_browser_frame) };
			ImGui::SetNextItemWidth(-1);
			if (frame_combo("Pose##preview-place", preview_place_frame, movie.metasprite_count))
				m_atlas_movie_browser_frame = preview_place_frame;
			if (m_atlas_movie_browser_frame < m_atlas_movie_frame_textures.size()
				&& m_atlas_movie_frame_textures[m_atlas_movie_browser_frame])
				ImGui::Image(m_atlas_movie_frame_textures[m_atlas_movie_browser_frame], ImVec2(54, 54));
			if (movie.tracks.size() >= 8)
				ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.25f, 1.0f), "8/8 tracks\ndelete one first");
			if (ui::imgui_button(m_atlas_movie_actor_place_mode ? "Cancel placement"
				: std::format("Place F{:02}", m_atlas_movie_browser_frame),
				m_atlas_movie_actor_place_mode ? 4 : 2,
				"Uses the pose selected in Frame Browser; then click the stage",
				movie.tracks.size() >= 8)) {
				m_atlas_movie_actor_place_mode = !m_atlas_movie_actor_place_mode;
				m_atlas_movie_path_draw_mode = m_atlas_movie_path_painting = false;
				m_atlas_movie_painted_path.clear(); m_atlas_movie_preview_playing = false;
			}
			if (m_atlas_movie_actor_place_mode)
				ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f), "Click stage");
			ImGui::SeparatorText("Drag actor");
			ImGui::TextWrapped("Drop a pose onto the stage to create a static actor.");
			if (m_atlas_asset_sprite < m_atlas_game_sprite_textures.size()
				&& !m_atlas_game_sprite_textures[m_atlas_asset_sprite].empty()) {
				ImGui::SeparatorText("Game asset");
				auto* texture{ m_atlas_game_sprite_textures[m_atlas_asset_sprite][0] };
				if (texture) {
					ImGui::Image(texture, ImVec2(64, 64));
					if (shared_mode && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
						const auto sprite_id{ m_atlas_asset_sprite };
						ImGui::SetDragDropPayload(ATLAS_GAME_SPRITE_PAYLOAD, &sprite_id, sizeof(sprite_id));
						ImGui::Text("Import gameplay sprite #%zu", sprite_id); ImGui::EndDragDropSource();
					}
					ImGui::TextWrapped(shared_mode ? "Drag to import animation" : "Shared mode only");
				}
			}
			ImGui::SeparatorText("Movie poses");
			for (std::size_t i{ 0 }; i < m_atlas_movie_frame_textures.size(); ++i) {
				ImGui::PushID(static_cast<int>(i));
				if (m_atlas_movie_frame_textures[i]) {
					if (ImGui::ImageButton("pose", m_atlas_movie_frame_textures[i], ImVec2(44, 44)))
						m_atlas_movie_browser_frame = i;
					frame_drag_source(static_cast<byte>(i));
				}
				ImGui::SameLine(); ImGui::Text("F%02zu", i);
				ImGui::PopID();
			}
			if (!editing_overlays) ImGui::EndDisabled();
			ImGui::EndChild();
			}
			if (editing_overlays && show_grid) {
				const int grid_step{ 16 };
				for (int x{ 0 }; x <= 256; x += grid_step) draw->AddLine(ImVec2(origin.x + x * 2, origin.y), ImVec2(origin.x + x * 2, origin.y + 480), IM_COL32(255,255,255,45));
				for (int y{ 0 }; y <= 240; y += grid_step) draw->AddLine(ImVec2(origin.x, origin.y + y * 2), ImVec2(origin.x + 512, origin.y + y * 2), IM_COL32(255,255,255,45));
			}
			const byte draw_mask{ static_cast<byte>(state.phase < movie.phases.size()
				? movie.phases[state.phase].draw_mask : 0) };
			for (std::size_t i{ 0 }; i < state.tracks.size(); ++i) {
				if (!(draw_mask & (1u << i)) || !state.tracks[i].visible) continue;
				const auto& row{ state.tracks[i] };
				const ImU32 actor_color{ movie.tracks[i].editor_color
					? movie.tracks[i].editor_color : ACTOR_UI_COLORS[i % ACTOR_UI_COLORS.size()] };
				const ImVec2 marker{ origin.x + row.x * 2.0f, origin.y + row.y * 2.0f };
				if (!m_atlas_movie_preview_texture) {
					const ImVec2 p0{ marker.x - 12, marker.y - 20 }, p1{ marker.x + 20, marker.y + 28 };
					draw->AddRectFilled(p0, p1, actor_color, 3.0f); draw->AddRect(p0, p1, IM_COL32_WHITE, 3.0f);
				}
				if (editing_overlays && show_labels) {
					const bool selected{ i == m_atlas_movie_sel_track };
					draw->AddCircleFilled(marker, selected ? 7.0f : 6.0f, IM_COL32(0, 0, 0, 230));
					draw->AddCircleFilled(marker, selected ? 5.0f : 4.0f, actor_color);
					const auto label{ std::format("{}\nT{} F{}", movie.tracks[i].editor_name, i, row.frame) };
					const ImVec2 text_size{ ImGui::CalcTextSize(label.c_str()) };
					ImVec2 label_pos{ marker.x + 8.0f, marker.y - text_size.y * 0.5f };
					label_pos.x = std::clamp(label_pos.x, origin.x + 2.0f,
						origin.x + canvas_size.x - text_size.x - 6.0f);
					label_pos.y = std::clamp(label_pos.y, origin.y + 2.0f,
						origin.y + canvas_size.y - text_size.y - 6.0f);
					const ImVec2 box_min{ label_pos.x - 3.0f, label_pos.y - 2.0f };
					const ImVec2 box_max{ label_pos.x + text_size.x + 3.0f,
						label_pos.y + text_size.y + 2.0f };
					draw->AddRectFilled(box_min, box_max, IM_COL32(0, 0, 0, 205), 3.0f);
					draw->AddRect(box_min, box_max,
						selected ? actor_color : IM_COL32(255, 255, 255, 120), 3.0f);
					draw->AddText(ImVec2(label_pos.x + 1.0f, label_pos.y + 1.0f),
						IM_COL32(0, 0, 0, 255), label.c_str());
					draw->AddText(label_pos,
						selected ? IM_COL32(255, 245, 140, 255) : IM_COL32_WHITE, label.c_str());
				}
			}
			auto& dragged_track{ m_atlas_movie_actor_session.preview_dragged_track };
			if (editing_overlays && !placed_actor_this_frame && !m_atlas_movie_actor_place_mode
				&& !m_atlas_movie_path_draw_mode && canvas_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
				&& ImGui::GetDragDropPayload() == nullptr) {
				const auto mouse{ ImGui::GetMousePos() };
				float nearest{ 32.0f * 32.0f }; dragged_track = -1;
				for (std::size_t i{ 0 }; i < state.tracks.size(); ++i) {
					if (!(draw_mask & (1u << i)) || !state.tracks[i].visible) continue;
					const float dx{ mouse.x - (origin.x + state.tracks[i].x * 2.0f) };
					const float dy{ mouse.y - (origin.y + state.tracks[i].y * 2.0f) };
					const float distance{ dx * dx + dy * dy };
					if (distance < nearest) { nearest = distance; dragged_track = static_cast<int>(i); }
				}
				if (dragged_track >= 0) {
					m_atlas_movie_sel_track = static_cast<std::size_t>(dragged_track);
					m_atlas_movie_preview_playing = false;
				}
			}
			if (dragged_track >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left)
				&& static_cast<std::size_t>(dragged_track) < movie.tracks.size()
				&& static_cast<std::size_t>(dragged_track) < state.tracks.size()) {
				const auto mouse{ ImGui::GetMousePos() };
				const auto& actor{ state.tracks[dragged_track] };
				auto& track{ movie.tracks[dragged_track] };
				const int desired_x{ static_cast<int>(std::clamp((mouse.x - origin.x) / 2.0f, 0.0f, 255.0f)) };
				const int desired_y{ static_cast<int>(std::clamp((mouse.y - origin.y) / 2.0f, 0.0f, 239.0f)) };
				translate_actor(track, desired_x - actor.x, desired_y - actor.y);
				m_atlas_movie_dirty = true;
			}
			if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) dragged_track = -1;
			if (graphics_error.empty())
				imgui_text("Real ROM nametable, CHR, palette, and metasprites. Movement uses the engine's fixed-point integrator. Music-ended phases use a 240-frame preview hold.");
			else
				ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f), "Graphics fallback: %s", graphics_error.c_str());
			ImGui::EndTabItem();
		}
}
