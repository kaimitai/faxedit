#include "MainWindow.h"
#include "AtlasMovieRenderer.h"
#include "AtlasMovieUi.h"
#include "Imgui_helper.h"
#include "common/klib/Kfile.h"
#include "fe/AtlasMovieAssets.h"
#include "fe/AtlasMovieCompatibility.h"
#include "fe/AtlasMovieEditor.h"
#include "fe/AtlasMovieEngine.h"
#include "fe/AtlasMoviePreview.h"
#include "fe/AtlasMovieRuntime.h"
#include "fe/fe_app_constants.h"
#include "./../common/imguifiledialog/ImGuiFileDialog.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <format>
#include <limits>
#include <set>
#include <string_view>
#include <utility>

namespace {

	using namespace fe;
	using namespace fe::atlas_movie;
	using namespace fe::atlas_movie::ui_detail;

	constexpr auto& ATLAS_FRAME_PAYLOAD{ FRAME_PAYLOAD };
	constexpr auto& ATLAS_GAME_SPRITE_PAYLOAD{ GAME_SPRITE_PAYLOAD };
	constexpr auto& ACTOR_UI_COLORS{ ACTOR_COLORS };

}

void fe::MainWindow::draw_atlas_movie_window(SDL_Renderer* p_renderer) {
	ui::imgui_screen("Atlas Movie Creator",
		c::WIN_TILEMAP_X + 20, c::WIN_TILEMAP_Y + 20,
		c::WIN_TILEMAP_W + 280, c::WIN_TILEMAP_H + 180, 4);
	if (!m_game.has_value()) {
		imgui_text("Load a ROM before opening an Atlas movie project.");
		ImGui::End();
		return;
	}

	auto guarded = [this](auto&& action) {
		try { action(); return true; }
		catch (const std::exception& ex) { add_message(ex.what(), 1); return false; }
	};
	auto open_dialog = [this](const char* key, const char* title,
		const char* filter, bool save, const std::filesystem::path& current = {}) {
		IGFD::FileDialogConfig config;
		const auto parent{ current.empty() ? m_path : current.parent_path() };
		config.path = parent.string();
		if (!current.empty()) config.fileName = current.filename().string();
		config.flags = save ? ImGuiFileDialogFlags_Default
			: ImGuiFileDialogFlags_HideColumnDate;
		ImGuiFileDialog::Instance()->OpenDialog(key, title, filter, config);
	};
	auto perform_pending_action = [this, &guarded]() {
		const auto action{ m_atlas_movie_pending_action };
		const auto path{ m_atlas_movie_pending_path };
		const bool succeeded{ guarded([&] {
			switch (action) {
			case AtlasMoviePendingAction::NewProject:
				new_atlas_movie_project(); break;
			case AtlasMoviePendingAction::OpenProject:
				load_atlas_movie_project(path); break;
			case AtlasMoviePendingAction::LoadAme:
				load_atlas_movie_bundle_from_ame(path); break;
			case AtlasMoviePendingAction::LoadInstalled:
				load_atlas_movie_bundle_from_rom(); break;
			case AtlasMoviePendingAction::None: break;
			}
		}) };
		m_atlas_movie_pending_action = AtlasMoviePendingAction::None;
		m_atlas_movie_pending_path.clear();
		return succeeded;
	};
	auto request_action = [this, &perform_pending_action](
		AtlasMoviePendingAction action, const std::filesystem::path& path = {}) {
		m_atlas_movie_pending_action = action;
		m_atlas_movie_pending_path = path;
		if (!m_atlas_movie_dirty) perform_pending_action();
	};
	static bool advanced_mode{ false };
	const bool shared_installed{ AtlasMovieEngine::is_installed(m_game->m_rom_data) };
	const bool atlas_scheduler_detected{
		has_atlas_resident_scheduler(m_game->m_rom_data) };
	if (shared_installed)
		m_atlas_movie_runtime_mode = AtlasMovieRuntimeMode::Shared;

	// AMP keeps disabled movies, AME does not
	if (!m_atlas_movie_autoload_attempted && !m_atlas_movie_bundle) {
		m_atlas_movie_autoload_attempted = true;
		if (std::filesystem::exists(m_path / "atlas-movie-project.amp"))
			guarded([&] { load_atlas_movie_project(
				m_path / "atlas-movie-project.amp"); });
	}

	ImGui::TextDisabled("Runtime mode"); ImGui::SameLine();
	int runtime_mode{ m_atlas_movie_runtime_mode == AtlasMovieRuntimeMode::Standalone ? 0 : 1 };
	if (shared_installed) ImGui::BeginDisabled();
	if (ImGui::RadioButton("Standalone", runtime_mode == 0)) {
		runtime_mode = 0;
		m_atlas_movie_runtime_mode = AtlasMovieRuntimeMode::Standalone;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Shared — HIGHLY EXPERIMENTAL", runtime_mode == 1)) {
		runtime_mode = 1;
		m_atlas_movie_runtime_mode = AtlasMovieRuntimeMode::Shared;
	}
	if (shared_installed) ImGui::EndDisabled();
	ImGui::SameLine();
	if (m_atlas_movie_runtime_mode == AtlasMovieRuntimeMode::Standalone)
		ImGui::TextDisabled("pure generated opcode; %zu-byte private player; original intro/ending unchanged",
			AtlasMovieRuntime::STANDALONE_PLAYER_BYTES);
	else
		ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.2f, 1.0f),
			"6-byte opcode adapter; replaces Faxanadu's internal movie engine");
	if (atlas_scheduler_detected)
		ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.25f, 1.0f),
			"Atlas Resident Scheduler detected — active palette and sprite effects continue during movies");

	ImGui::TextDisabled("Workspace"); ImGui::SameLine();
	if (ui::imgui_button("Basic", advanced_mode ? 2 : 4,
		"Scene-first controls for creating, moving, animating, and previewing actors"))
		advanced_mode = false;
	ImGui::SameLine();
	if (ui::imgui_button("Advanced", advanced_mode ? 4 : 2,
		"Runtime fields, ROM assets, event predicates, detailed budgets, and export/install tools"))
		advanced_mode = true;
	ImGui::SameLine(); ImGui::TextDisabled(advanced_mode
		? "All authoring and engine controls" : "Recommended for normal movie creation");

	if (ui::imgui_button("New Project", 2,
		"Create a minimal editable USA Rev 0 project using ROM-owned graphics"))
		request_action(AtlasMoviePendingAction::NewProject);
	ImGui::SameLine();
	if (ui::imgui_button("Open Project", 2, "Open an editable AMP project"))
		open_dialog("OpenAtlasMovieProject", "Open Atlas movie project",
			".amp", false, m_atlas_movie_project_path);
	ImGui::SameLine();
	if (ui::imgui_button("Load AME", 2, "Inspect or import an external AME package"))
		open_dialog("OpenAtlasMovieAme", "Open Atlas Movie Engine package",
			".ame", false);
	ImGui::SameLine();
	if (ui::imgui_button("Load installed", 2,
		"Load the FMB from the in-memory ROM",
		!AtlasMovieEngine::is_installed(m_game->m_rom_data)))
		request_action(AtlasMoviePendingAction::LoadInstalled);
	ImGui::SameLine();
	if (ui::imgui_button("Save Project", 2,
		"Save to the current AMP path", !m_atlas_movie_bundle)) {
		if (m_atlas_movie_project_path.empty()) {
			m_atlas_movie_pending_save_as = false;
			open_dialog("SaveAtlasMovieProject", "Save Atlas movie project",
				".amp", true, m_path / "atlas-movie-project.amp");
		}
		else guarded([&] { save_atlas_movie_project(); });
	}
	ImGui::SameLine();
	if (ui::imgui_button("Save As", 2, "Choose a new AMP project path",
		!m_atlas_movie_bundle)) {
		m_atlas_movie_pending_save_as = false;
		open_dialog("SaveAtlasMovieProject", "Save Atlas movie project",
			".amp", true, m_atlas_movie_project_path.empty()
				? m_path / "atlas-movie-project.amp" : m_atlas_movie_project_path);
	}

	if (m_atlas_movie_bundle) {
		const auto project_label{ m_atlas_movie_project_path.empty()
			? std::string("Untitled") : m_atlas_movie_project_path.string() };
		ImGui::TextDisabled("Project: %s%s",
			project_label.c_str(),
			m_atlas_movie_dirty ? "  *" : "");
	}

	std::optional<AtlasMovieBundleReport> validation_report;
	std::string validation_error;
	bool has_imports{ false };
	if (m_atlas_movie_bundle) {
		has_imports = std::ranges::any_of(m_atlas_movie_bundle->movies,
			[](const auto& movie) { return !movie.imports.empty(); });
		try { validation_report = AtlasMovieBundleCodec::validate(*m_atlas_movie_bundle); }
		catch (const std::exception& ex) { validation_error = ex.what(); }
		if (validation_error.empty()
			&& m_atlas_movie_runtime_mode == AtlasMovieRuntimeMode::Standalone
			&& has_imports)
			validation_error = "Standalone does not support imported assets; remove imports or select Shared mode";
	}
	const bool mode_ready{ validation_report.has_value() && validation_error.empty() };
	if (advanced_mode) {
		ImGui::SameLine();
		if (ui::imgui_button("Save FMB", 2, "Write movie-bundle-created.fmb",
			!validation_report.has_value()))
			guarded([&] { save_atlas_movie_bundle(); });
		ImGui::SameLine();
		if (m_atlas_movie_runtime_mode == AtlasMovieRuntimeMode::Standalone) {
			if (ui::imgui_button("Export Standalone config", 4,
				mode_ready ? "Write a complete opcode/FMB override; use it while assembling scripts"
					: validation_error,
				!mode_ready))
				guarded([&] { save_atlas_movie_standalone_config(); });
		}
		else {
			if (ui::imgui_button("Export Shared AME", 2,
				"Write atlas-movie-engine-created.ame (HIGHLY EXPERIMENTAL)",
				!mode_ready))
				guarded([&] { save_atlas_movie_ame(); });
			ImGui::SameLine();
			if (ui::imgui_button("Install Shared mode — HIGHLY EXPERIMENTAL", 4,
				"Replaces the internal movie engine in memory; Patch nes ROM writes the output file",
				!mode_ready))
				guarded([&] { apply_atlas_movie_bundle(); });
		}
	}

	if (m_atlas_movie_pending_action != AtlasMoviePendingAction::None
		&& !m_atlas_movie_pending_save_as)
		ImGui::OpenPopup("Unsaved Atlas movie project");
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
		ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("Unsaved Atlas movie project", nullptr,
		ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextUnformatted("The current Atlas movie project has unsaved changes.");
		ImGui::TextUnformatted("Save it before replacing the project?");
		if (ui::imgui_button("Save", 2)) {
			if (m_atlas_movie_project_path.empty()) {
				m_atlas_movie_pending_save_as = true;
				open_dialog("SaveAtlasMovieProject", "Save Atlas movie project",
					".amp", true, m_path / "atlas-movie-project.amp");
				ImGui::CloseCurrentPopup();
			}
			else if (guarded([&] { save_atlas_movie_project(); })) {
				perform_pending_action();
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ui::imgui_button("Discard", 1)) {
			perform_pending_action();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ui::imgui_button("Cancel", 4)) {
			m_atlas_movie_pending_action = AtlasMoviePendingAction::None;
			m_atlas_movie_pending_path.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	const auto display_file_dialog = [](const char* p_key) {
		return ImGuiFileDialog::Instance()->Display(p_key,
			ImGuiWindowFlags_NoCollapse, ImVec2(640, 420), ImVec2(1200, 700));
	};
	if (display_file_dialog("OpenAtlasMovieProject")) {
		if (ImGuiFileDialog::Instance()->IsOk())
			request_action(AtlasMoviePendingAction::OpenProject,
				ImGuiFileDialog::Instance()->GetFilePathName());
		ImGuiFileDialog::Instance()->Close();
	}
	if (display_file_dialog("OpenAtlasMovieAme")) {
		if (ImGuiFileDialog::Instance()->IsOk())
			request_action(AtlasMoviePendingAction::LoadAme,
				ImGuiFileDialog::Instance()->GetFilePathName());
		ImGuiFileDialog::Instance()->Close();
	}
	if (display_file_dialog("SaveAtlasMovieProject")) {
		if (ImGuiFileDialog::Instance()->IsOk()) {
			const bool continue_replacement{ m_atlas_movie_pending_save_as };
			if (guarded([&] { save_atlas_movie_project_as(
				ImGuiFileDialog::Instance()->GetFilePathName()); })
				&& continue_replacement)
				perform_pending_action();
		}
		else if (m_atlas_movie_pending_save_as) {
			m_atlas_movie_pending_action = AtlasMoviePendingAction::None;
			m_atlas_movie_pending_path.clear();
		}
		m_atlas_movie_pending_save_as = false;
		ImGuiFileDialog::Instance()->Close();
	}

	if (!m_atlas_movie_bundle) {
		ImGui::Separator();
		imgui_text("Create a new project, open an AMP, inspect an AME, or load movies already installed in the ROM.");
		ImGui::End();
		return;
	}

	auto& bundle{ *m_atlas_movie_bundle };
	const bool shared_mode{ m_atlas_movie_runtime_mode == AtlasMovieRuntimeMode::Shared };
	m_atlas_movie_sel_movie = std::min(m_atlas_movie_sel_movie, bundle.movies.size() - 1);

	if (mode_ready) {
		const auto& report{ *validation_report };
		const std::size_t capacity{ 0xc000 - AtlasMovieBundleCodec::BUNDLE_CPU };
		const std::size_t used{ static_cast<std::size_t>(report.reserved_cpu_end)
			- AtlasMovieBundleCodec::BUNDLE_CPU };
		const std::size_t remaining{ capacity - used };
		ImGui::SameLine();
		const ImVec4 budget_color{ remaining > 1024 ? ImVec4(0.4f, 1.0f, 0.5f, 1.0f)
			: remaining > 512 ? ImVec4(1.0f, 0.78f, 0.2f, 1.0f)
			: ImVec4(1.0f, 0.35f, 0.3f, 1.0f) };
		if (m_atlas_movie_runtime_mode == AtlasMovieRuntimeMode::Standalone) {
			const auto total{ report.bytes + AtlasMovieRuntime::STANDALONE_PLAYER_BYTES };
			if (advanced_mode)
				ImGui::TextColored(budget_color,
					"VALID  %zu FMB + %zu player = %zu bytes%s",
					report.bytes, AtlasMovieRuntime::STANDALONE_PLAYER_BYTES,
					total, m_atlas_movie_dirty ? "  (unsaved)" : "");
			else {
				ImGui::TextColored(budget_color, "Ready — %zu-byte Standalone runtime%s",
					total, m_atlas_movie_dirty ? " — unsaved changes" : "");
				ImGui::TextDisabled("Final free space depends on the other generated opcodes selected during assembly.");
			}
		}
		else if (advanced_mode)
			ImGui::TextColored(budget_color,
				"VALID  %zu FMB bytes  $B264-$%04X  %zu free%s", report.bytes,
				report.reserved_cpu_end, remaining, m_atlas_movie_dirty ? "  (unsaved)" : "");
		else
			ImGui::TextColored(budget_color, "Ready — %zu ROM bytes free%s",
				remaining, m_atlas_movie_dirty ? " — unsaved changes" : "");
		if (m_atlas_movie_runtime_mode == AtlasMovieRuntimeMode::Shared) {
			ImGui::PushStyleColor(ImGuiCol_PlotHistogram, budget_color);
			const auto overlay{ advanced_mode
				? std::format("Shared bank 12: {} / {} bytes ({} FMB + {} dispatch) — {} free",
					used, capacity, report.bytes, AtlasMovieBundleCodec::DISPATCH_BYTES, remaining)
				: std::format("Shared movie space: {}% used — {} bytes free",
					static_cast<int>(100 * used / capacity), remaining) };
			ImGui::ProgressBar(static_cast<float>(used) / capacity, ImVec2(-1, 0), overlay.c_str());
			ImGui::PopStyleColor();
		}
	}
	else {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.3f, 1.0f),
			"NOT READY: %s", validation_error.c_str());
	}

	ImGui::Separator();
	ImGui::BeginChild("movie-list", ImVec2(175, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
	ImGui::SeparatorText(advanced_mode ? "Movies / IDs" : "Movies");
	ImGui::TextDisabled(advanced_mode ? "[x] Included in ROM" : "Checked movies will be included");
	std::vector<std::optional<std::size_t>> runtime_movie_ids(bundle.movies.size());
	std::size_t next_runtime_id{ 0 };
	for (const auto role : { AtlasMovieProjectRole::OfficialIntro, AtlasMovieProjectRole::OfficialEnding })
		for (std::size_t i{ 0 }; i < bundle.movies.size(); ++i)
			if (bundle.movies[i].enabled && bundle.movies[i].project_role == role)
				runtime_movie_ids[i] = next_runtime_id++;
	for (std::size_t i{ 0 }; i < bundle.movies.size(); ++i)
		if (bundle.movies[i].enabled && bundle.movies[i].project_role == AtlasMovieProjectRole::Normal)
			runtime_movie_ids[i] = next_runtime_id++;
	for (std::size_t i{ 0 }; i < bundle.movies.size(); ++i) {
		auto& listed_movie{ bundle.movies[i] };
		ImGui::PushID(static_cast<int>(i));
		const bool official{ listed_movie.project_role != AtlasMovieProjectRole::Normal };
		if (official) ImGui::BeginDisabled();
		if (ImGui::Checkbox("##included", &listed_movie.enabled)) m_atlas_movie_dirty = true;
		if (!official && ImGui::IsItemHovered())
			ImGui::SetTooltip("Include this movie in the compiled FMB/AME ROM payload");
		if (official) {
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("Transfer this movie's official role before disabling it");
		}
		ImGui::SameLine();
		const auto role_prefix{ listed_movie.project_role == AtlasMovieProjectRole::OfficialIntro ? "[INTRO] "
			: listed_movie.project_role == AtlasMovieProjectRole::OfficialEnding ? "[ENDING] " : "" };
		const auto label{ advanced_mode
			? listed_movie.enabled ? std::format("ID {}  {}{}", *runtime_movie_ids[i], role_prefix, listed_movie.id)
				: std::format("OFF  {}", listed_movie.id)
			: listed_movie.enabled ? std::format("{}{}", role_prefix, listed_movie.id)
				: std::format("Not included: {}", listed_movie.id) };
		if (!listed_movie.enabled) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		if (ImGui::Selectable(label.c_str(), i == m_atlas_movie_sel_movie)) {
			m_atlas_movie_sel_movie = i; m_atlas_movie_sel_track = m_atlas_movie_sel_phase = m_atlas_movie_sel_sfx = 0;
			m_atlas_movie_preview_frame = 0;
			m_atlas_movie_path_draw_mode = m_atlas_movie_path_painting = false;
			m_atlas_movie_actor_place_mode = false;
			m_atlas_movie_painted_path.clear();
		}
		if (!listed_movie.enabled) ImGui::PopStyleColor();
		ImGui::PopID();
	}
	if (ui::imgui_button("Duplicate", 2)) {
		auto copy{ bundle.movies[m_atlas_movie_sel_movie] };
		copy.project_role = AtlasMovieProjectRole::Normal;
		bundle.movies.push_back(std::move(copy)); ensure_unique_id(bundle, bundle.movies.back());
		m_atlas_movie_sel_movie = bundle.movies.size() - 1; m_atlas_movie_dirty = true;
	}
	if (ImGui::BeginCombo("Example", "Create from...")) {
		for (std::size_t i{ 0 }; i < bundle.movies.size(); ++i) {
			const auto label{ std::format("{} — editable copy", bundle.movies[i].id) };
			if (ImGui::Selectable(label.c_str())) {
				auto copy{ bundle.movies[i] };
				copy.project_role = AtlasMovieProjectRole::Normal;
				bundle.movies.push_back(std::move(copy));
				ensure_unique_id(bundle, bundle.movies.back());
				m_atlas_movie_sel_movie = bundle.movies.size() - 1;
				m_atlas_movie_sel_track = m_atlas_movie_sel_phase = m_atlas_movie_sel_sfx = 0;
				m_atlas_movie_preview_frame = 0; m_atlas_movie_dirty = true;
			}
		}
		ImGui::EndCombo();
	}
	if (ui::imgui_button("Delete", 1, "Transfer an official role before deleting its movie",
		bundle.movies.size() <= 2
			|| bundle.movies[m_atlas_movie_sel_movie].project_role != AtlasMovieProjectRole::Normal)) {
		bundle.movies.erase(bundle.movies.begin() + m_atlas_movie_sel_movie);
		m_atlas_movie_sel_movie = std::min(m_atlas_movie_sel_movie, bundle.movies.size() - 1);
		m_atlas_movie_dirty = true;
	}
	ImGui::EndChild();
	ImGui::SameLine();
	auto& movie{ bundle.movies[m_atlas_movie_sel_movie] };
	m_atlas_movie_sel_track = movie.tracks.empty() ? 0 : std::min(m_atlas_movie_sel_track, movie.tracks.size() - 1);
	m_atlas_movie_sel_phase = movie.phases.empty() ? 0 : std::min(m_atlas_movie_sel_phase, movie.phases.size() - 1);
	m_atlas_movie_sel_sfx = movie.sfx.empty() ? 0 : std::min(m_atlas_movie_sel_sfx, movie.sfx.size() - 1);
	std::vector<SpriteAnimationFrame> decoded_frames;
	std::string graphics_error;
	try {
		decoded_frames = decode_movie_frames(m_game->m_rom_data, movie);
		const auto* sprite_asset{ find_asset(movie, AtlasMovieAssetKind::SpriteChr) };
		const auto* palette_asset{ find_asset(movie, AtlasMovieAssetKind::Palette) };
		if (sprite_asset && palette_asset) {
			const auto sprite_tiles{ movie_sprite_tiles(m_game->m_rom_data, movie) };
			const auto palette{ resolved_asset_bytes(m_game->m_rom_data, movie, AtlasMovieAssetKind::Palette) };
			std::uint64_t signature{ 1469598103934665603ULL };
			auto mix = [&](std::uint64_t value) { signature ^= value; signature *= 1099511628211ULL; };
			mix(movie.metasprite_bank); mix(movie.metasprite_pointer_lo);
			mix(movie.metasprite_pointer_hi); mix(movie.metasprite_count);
			for (const auto& asset : movie.assets) {
				mix(static_cast<byte>(asset.kind)); mix(asset.bank); mix(asset.cpu);
				mix(asset.destination); mix(asset.bytes);
			}
			for (const auto& imported : movie.imports) {
				mix(static_cast<byte>(imported.kind)); mix(imported.destination); mix(imported.aux);
				for (const byte value : imported.data) mix(value);
			}
			for (const byte value : encode_chr(sprite_tiles)) mix(value);
			for (const byte value : palette) mix(value);
			for (const byte value : encode_metasprite_library(decoded_frames)) mix(value);
			static std::uint64_t last_signature{ std::numeric_limits<std::uint64_t>::max() };
			if (signature != last_signature || m_atlas_movie_frame_textures.size() != decoded_frames.size()) {
				for (auto* texture : m_atlas_movie_frame_textures) SDL_DestroyTexture(texture);
				m_atlas_movie_frame_textures.clear();
				if (palette.size() >= 32)
					for (const auto& frame : decoded_frames) {
						std::vector<byte> sprite_palette{ palette.begin() + 16, palette.begin() + 32 };
						m_atlas_movie_frame_textures.push_back(render_frame_texture(p_renderer,
							m_gfx.get_nes_palette(), frame, sprite_tiles, sprite_palette));
					}
				last_signature = signature;
			}
		}
	}
	catch (const std::exception& ex) { graphics_error = ex.what(); }

	// imports are copies, FaxEdit's source assets stay untouched
	if (!m_atlas_game_sprites) {
		m_atlas_game_sprites.emplace();
		m_atlas_game_sprites->load_sprites_for_gui(m_config,
			m_game->m_sprite_gfx_manager, m_game->m_rom_data);
		const auto palette_index{ std::min<std::size_t>(m_settings.coll_palettes.at(0), m_game->m_palettes.size() - 1) };
		const auto& sprite_palette{ m_game->m_palettes.at(palette_index) };
		for (std::size_t sprite{ 0 }; sprite < m_atlas_game_sprites->animations.size(); ++sprite) {
			std::vector<SDL_Texture*> textures;
			const auto bank{ m_atlas_game_sprites->npc_to_bank_idx.at(sprite) };
			for (const auto& frame : m_atlas_game_sprites->animations[sprite])
				textures.push_back(render_frame_texture(p_renderer, m_gfx.get_nes_palette(),
					frame, m_atlas_game_sprites->banks.at(bank), sprite_palette));
			m_atlas_game_sprite_textures.push_back(std::move(textures));
		}
	}

	ImGui::BeginChild("movie-editor", ImVec2(0, 0), ImGuiChildFlags_None);
	if (ImGui::BeginTabBar("atlas-movie-tabs")) {
		if (ImGui::BeginTabItem("Movie")) {
			if (!advanced_mode)
				ImGui::TextDisabled("Name the movie, choose its role, music, and what happens when it ends.");
			char id[256]{}; std::snprintf(id, sizeof(id), "%s", movie.id.c_str());
			if (ImGui::InputText("Movie ID", id, sizeof(id))) { movie.id = id; m_atlas_movie_dirty = true; }
			static constexpr std::array role_names{ "Normal movie", "Official intro", "Official ending" };
			const auto role_label{ role_names[static_cast<byte>(movie.project_role)] };
			if (ImGui::BeginCombo("Special attribute", role_label)) {
				for (std::size_t candidate{ 0 }; candidate < role_names.size(); ++candidate) {
					const auto new_role{ static_cast<AtlasMovieProjectRole>(candidate) };
					const bool cannot_clear{ new_role == AtlasMovieProjectRole::Normal
						&& movie.project_role != AtlasMovieProjectRole::Normal };
					if (cannot_clear) ImGui::BeginDisabled();
					if (ImGui::Selectable(role_names[candidate], movie.project_role == new_role) && !cannot_clear
						&& movie.project_role != new_role) {
						const auto old_role{ movie.project_role };
						auto prior{ std::ranges::find_if(bundle.movies, [&](const auto& other) {
							return &other != &movie && other.project_role == new_role;
						}) };
						movie.project_role = new_role; movie.enabled = true;
						if (prior != bundle.movies.end()) {
							prior->project_role = old_role;
							if (old_role != AtlasMovieProjectRole::Normal) prior->enabled = true;
						}
						m_atlas_movie_dirty = true;
					}
					if (cannot_clear) ImGui::EndDisabled();
				}
				ImGui::EndCombo();
			}
			if (movie.project_role != AtlasMovieProjectRole::Normal)
				ImGui::TextDisabled("Transfer this official role to another movie before disabling or deleting it.");
			static constexpr std::array exits{ "New game", "Title reset", "Reload current room" };
			m_atlas_movie_dirty |= enum_combo("Exit behavior", movie.exit_mode, exits, 1);
			int music_mode{ movie.entry_music == 0xff ? 0 : movie.entry_music == 0xfe ? 1 : 2 };
			static constexpr const char* music_modes[]{ "Leave unchanged", "Stop", "Play track" };
			if (ImGui::Combo("Entry music", &music_mode, music_modes, 3)) {
				movie.entry_music = music_mode == 0 ? 0xff : music_mode == 1 ? 0xfe : 0;
				m_atlas_movie_dirty = true;
			}
			if (music_mode == 2) {
				const auto selected{ std::format("${:02X} — {}", movie.entry_music,
					get_description(movie.entry_music, m_cache.m_labels_music)) };
				if (ImGui::BeginCombo("Music track", selected.c_str())) {
					static char music_search[48]{};
					ImGui::InputTextWithHint("##music-search", "search music...", music_search, sizeof(music_search));
					std::string needle{ music_search };
					std::ranges::transform(needle, needle.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
					for (std::size_t i{ 0 }; i < m_cache.m_music_count; ++i) {
						const auto label{ std::format("${:02X} — {}", i,
							get_description(static_cast<byte>(i), m_cache.m_labels_music)) };
						std::string searchable{ label };
						std::ranges::transform(searchable, searchable.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
						if (!needle.empty() && searchable.find(needle) == std::string::npos) continue;
						if (ImGui::Selectable(label.c_str(), i == movie.entry_music)) {
							movie.entry_music = static_cast<byte>(i); m_atlas_movie_dirty = true;
						}
					}
					ImGui::EndCombo();
				}
			}
			if (advanced_mode) {
				ImGui::SeparatorText("Metasprite library");
				m_atlas_movie_dirty |= edit_byte("Bank", movie.metasprite_bank, 0, 15);
				m_atlas_movie_dirty |= edit_word("Pointer table low", movie.metasprite_pointer_lo, 0x8000, 0xbfff);
				m_atlas_movie_dirty |= edit_word("Pointer table high", movie.metasprite_pointer_hi, 0x8000, 0xbfff);
				m_atlas_movie_dirty |= edit_byte("Frame count", movie.metasprite_count, 1, 255);
			}
			if (graphics_error.empty())
				ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f),
					"Graphics decoded: %zu real metasprite frames", decoded_frames.size());
			else
				ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f), "Graphics preview: %s", graphics_error.c_str());
			ImGui::EndTabItem();
		}

		if (advanced_mode && ImGui::BeginTabItem("ROM Assets")) {
			static constexpr std::array kinds{ "Sprite CHR", "Background CHR", "Nametable", "Palette" };
			static constexpr std::array spaces{ "PPU", "RAM" };
			for (std::size_t i{ 0 }; i < movie.assets.size(); ++i) {
				ImGui::PushID(static_cast<int>(i)); auto& asset{ movie.assets[i] };
				if (ImGui::CollapsingHeader(std::format("Asset {}: {}", i, kinds[static_cast<byte>(asset.kind) - 1]).c_str(),
					i == 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
					m_atlas_movie_dirty |= enum_combo("Kind", asset.kind, kinds, 1);
					m_atlas_movie_dirty |= edit_byte("Source bank", asset.bank, 0, 15);
					m_atlas_movie_dirty |= edit_word("Source CPU", asset.cpu, 0x8000, 0xbfff);
					m_atlas_movie_dirty |= enum_combo("Destination space", asset.destination_space, spaces);
					m_atlas_movie_dirty |= edit_word("Destination", asset.destination);
					m_atlas_movie_dirty |= edit_word("Bytes", asset.bytes, 1, 0xffff);
				}
				ImGui::PopID();
			}
			ImGui::EndTabItem();
		}

		if (advanced_mode && ImGui::BeginTabItem("ROM Budget")) {
			guarded([&] {
				const auto report{ AtlasMovieBundleCodec::validate(bundle) };
				const auto details{ AtlasMovieBundleCodec::detailed_budget(bundle) };
				const std::size_t capacity{ 0xc000 - AtlasMovieBundleCodec::BUNDLE_CPU };
				const std::size_t used{ report.bytes + AtlasMovieBundleCodec::DISPATCH_BYTES };
				ImGui::Text("Runtime payload: %zu FMB + %zu dispatch = %zu / %zu bank bytes",
					report.bytes, AtlasMovieBundleCodec::DISPATCH_BYTES, used, capacity);
				ImGui::ProgressBar(static_cast<float>(used) / capacity, ImVec2(-1, 0),
					std::format("{} bytes free", capacity - used).c_str());
				ImGui::TextDisabled("Bundle headers: %zu bytes; import trailer header: %zu bytes",
					details.bundle_header_bytes, details.import_header_bytes);
				auto budget_table = [&](const char* title, const std::vector<AtlasMovieBudgetLine>& lines) {
					if (!ImGui::CollapsingHeader(std::format("{} ({})", title, lines.size()).c_str(),
						ImGuiTreeNodeFlags_DefaultOpen)) return;
					if (ImGui::BeginTable(title, 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
						ImGui::TableSetupColumn("Object", ImGuiTableColumnFlags_WidthStretch);
						ImGui::TableSetupColumn("Runtime bytes", ImGuiTableColumnFlags_WidthFixed, 110);
						ImGui::TableHeadersRow();
						for (const auto& line : lines) {
							ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted(line.label.c_str());
							ImGui::TableNextColumn(); ImGui::Text("%zu", line.bytes);
						}
						ImGui::EndTable();
					}
				};
				budget_table("Movies", details.movies);
				budget_table("Actors / tracks", details.tracks);
				budget_table("Imported payloads", details.imports);
				budget_table("ROM asset descriptors", details.assets);
				budget_table("Phases", details.phases);
				budget_table("SFX events", details.events);
				ImGui::SeparatorText("Optimization hints");
				if (details.suggestions.empty()) ImGui::TextDisabled("No obvious duplicate imports detected.");
				for (const auto& suggestion : details.suggestions) ImGui::BulletText("%s", suggestion.c_str());
			});
			ImGui::EndTabItem();
		}

		draw_atlas_movie_actors_tab(p_renderer, bundle, movie,
			decoded_frames, graphics_error, advanced_mode);

		if (ImGui::BeginTabItem(advanced_mode ? "Phases" : "Timeline")) {
			ImGui::SeparatorText("Movie timeline");
			ImGui::TextDisabled(advanced_mode
				? "Width is exact for frame conditions; music/effect/track gates use preview estimates."
				: "Click a phase to edit it, then set its duration below the timeline.");
			const float timeline_label_width{ 135.0f };
			std::vector<float> phase_widths;
			for (const auto& row : movie.phases)
				phase_widths.push_back(std::clamp(estimated_phase_frames(row) * 0.45f, 80.0f, 220.0f));
			if (ImGui::BeginChild("phase-timeline", ImVec2(0, 240), ImGuiChildFlags_Borders)) {
				const auto table_flags{ ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuter
					| ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY
					| ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings };
				if (ImGui::BeginTable("phase-grid", static_cast<int>(movie.phases.size() + 1),
					table_flags, ImVec2(0, 190))) {
					ImGui::TableSetupColumn("Actor", ImGuiTableColumnFlags_WidthFixed, timeline_label_width);
					for (std::size_t phase_index{ 0 }; phase_index < movie.phases.size(); ++phase_index)
						ImGui::TableSetupColumn(std::format("Phase {}", phase_index).c_str(),
							ImGuiTableColumnFlags_WidthFixed, phase_widths[phase_index]);
					ImGui::TableSetupScrollFreeze(1, 1);
					ImGui::TableNextRow(ImGuiTableRowFlags_Headers, 42.0f);
					ImGui::TableSetColumnIndex(0);
					ImGui::TextDisabled("Actors / phases");
					ImGui::PushID("phase-headers");
					for (std::size_t phase_index{ 0 }; phase_index < movie.phases.size(); ++phase_index) {
						ImGui::TableSetColumnIndex(static_cast<int>(phase_index + 1));
						ImGui::PushID(static_cast<int>(phase_index));
						const auto title{ std::format("Phase {}  ~{}f{}", phase_index,
							estimated_phase_frames(movie.phases[phase_index]),
							movie.phases[phase_index].effect == AtlasMovieEffect::None ? "" : "\nFADE / FX") };
						const bool selected{ phase_index == m_atlas_movie_sel_phase };
						if (selected)
							ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.22f, 0.48f, 0.72f, 1.0f));
						if (ImGui::Selectable(title.c_str(), selected,
							0, ImVec2(ImGui::GetContentRegionAvail().x, 38.0f)))
							m_atlas_movie_sel_phase = phase_index;
						if (selected) ImGui::PopStyleColor();
						ImGui::PopID();
					}
					ImGui::PopID();
					for (std::size_t actor_index{ 0 }; actor_index < movie.tracks.size(); ++actor_index) {
						ImGui::TableNextRow(0, 28.0f);
						ImGui::TableSetColumnIndex(0);
						ImGui::PushID("actor-name");
						ImGui::PushID(static_cast<int>(actor_index));
						ImGui::PushStyleColor(ImGuiCol_Text,
							ImGui::ColorConvertU32ToFloat4(movie.tracks[actor_index].editor_color));
						if (ImGui::Selectable(movie.tracks[actor_index].editor_name.c_str(),
							actor_index == m_atlas_movie_sel_track, 0,
							ImVec2(ImGui::GetContentRegionAvail().x, 24.0f)))
							m_atlas_movie_sel_track = actor_index;
						ImGui::PopStyleColor();
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("%s", movie.tracks[actor_index].editor_name.c_str());
						ImGui::PopID();
						ImGui::PopID();
						for (std::size_t phase_index{ 0 }; phase_index < movie.phases.size(); ++phase_index) {
							ImGui::TableSetColumnIndex(static_cast<int>(phase_index + 1));
							const auto& row{ movie.phases[phase_index] };
							const bool update{ static_cast<bool>(row.update_mask & (1u << actor_index)) };
							const bool draw_actor{ static_cast<bool>(row.draw_mask & (1u << actor_index)) };
							const auto lane_label{ advanced_mode
								? draw_actor ? update ? "MOVE + DRAW" : "DRAW" : update ? "MOVE HIDDEN" : "HIDDEN"
								: draw_actor ? update ? "Moving + visible" : "Visible" : update ? "Moving, hidden" : "Hidden" };
							ImGui::PushID("phase-cell");
							ImGui::PushID(static_cast<int>(actor_index));
							ImGui::PushID(static_cast<int>(phase_index));
							if (ImGui::Button(lane_label, ImVec2(ImGui::GetContentRegionAvail().x, 24.0f))) {
								m_atlas_movie_sel_phase = phase_index; m_atlas_movie_sel_track = actor_index;
							}
							if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
								movie.phases[phase_index].draw_mask ^= static_cast<byte>(1u << actor_index);
								m_atlas_movie_dirty = true;
							}
							ImGui::PopID();
							ImGui::PopID();
							ImGui::PopID();
						}
					}
					ImGui::EndTable();
				}
				ImGui::Separator();
				if (advanced_mode)
					ImGui::TextColored(ImVec4(0.5f, 0.85f, 1.0f, 1.0f), "MUSIC $%02X", movie.entry_music);
				else ImGui::TextColored(ImVec4(0.5f, 0.85f, 1.0f, 1.0f), "Music");
				for (std::size_t i{ 0 }; i < movie.sfx.size(); ++i) {
					ImGui::SameLine();
					if (ImGui::SmallButton((advanced_mode
						? std::format("SFX{} T{}:${:02X}", i, movie.sfx[i].track, movie.sfx[i].sound)
						: std::format("Sound {}", i + 1)).c_str()))
						m_atlas_movie_sel_sfx = i;
				}
			}
			ImGui::EndChild();
			if (ImGui::BeginCombo("Phase", std::format("Phase {}", m_atlas_movie_sel_phase).c_str())) {
				for (std::size_t i{ 0 }; i < movie.phases.size(); ++i)
					if (ImGui::Selectable(std::format("Phase {}", i).c_str(), i == m_atlas_movie_sel_phase)) m_atlas_movie_sel_phase = i;
				ImGui::EndCombo();
			}
			if (ui::imgui_button("Duplicate phase", 2)) { movie.phases.insert(movie.phases.begin() + m_atlas_movie_sel_phase + 1, movie.phases[m_atlas_movie_sel_phase]); ++m_atlas_movie_sel_phase; m_atlas_movie_dirty = true; }
			ImGui::SameLine();
			if (ui::imgui_button("Delete phase", 1, "Keep at least one", movie.phases.size() <= 1)) { movie.phases.erase(movie.phases.begin() + m_atlas_movie_sel_phase); m_atlas_movie_sel_phase = std::min(m_atlas_movie_sel_phase, movie.phases.size() - 1); m_atlas_movie_dirty = true; }
			auto& phase{ movie.phases[m_atlas_movie_sel_phase] };
			ImGui::SeparatorText(advanced_mode ? "Track timeline" : "Actors in this phase");
			for (std::size_t i{ 0 }; i < movie.tracks.size(); ++i) {
				bool update{ static_cast<bool>(phase.update_mask & (1u << i)) };
				bool draw{ static_cast<bool>(phase.draw_mask & (1u << i)) };
				ImGui::PushID(static_cast<int>(i)); ImGui::Text("%s", advanced_mode
					? std::format("Track {}", i).c_str() : movie.tracks[i].editor_name.c_str()); ImGui::SameLine();
				if (ImGui::Checkbox(advanced_mode ? "Update" : "Moves", &update)) { phase.update_mask ^= static_cast<byte>(1u << i); m_atlas_movie_dirty = true; }
				ImGui::SameLine(); if (ImGui::Checkbox(advanced_mode ? "Draw" : "Visible", &draw)) { phase.draw_mask ^= static_cast<byte>(1u << i); m_atlas_movie_dirty = true; }
				ImGui::PopID();
			}
			if (!advanced_mode) {
				if (phase.condition == AtlasMovieCondition::Frames)
					m_atlas_movie_dirty |= edit_word("Duration (frames)", phase.condition_value, 1, 65535);
				else
					ImGui::TextDisabled("This phase uses an event-based ending. Switch to Advanced to edit it.");
			}
			if (advanced_mode) {
			static constexpr std::array enter_names{ "None", "Set frame counter" };
			m_atlas_movie_dirty |= enum_combo("On enter", phase.enter_action, enter_names);
			if (phase.enter_action != AtlasMovieEnterAction::None) m_atlas_movie_dirty |= edit_byte("Enter value", phase.enter_value);
			static constexpr std::array effect_names{ "None", "Palette fade" };
			m_atlas_movie_dirty |= enum_combo("Effect", phase.effect, effect_names);
			if (phase.effect == AtlasMovieEffect::PaletteFade) {
				m_atlas_movie_dirty |= edit_byte("Effect track", phase.effect_track, 0, movie.tracks.size() - 1);
				m_atlas_movie_dirty |= edit_byte("Activate at stage", phase.effect_stage);
				m_atlas_movie_dirty |= edit_byte("Period calls", phase.effect_period, 1, 255);
				m_atlas_movie_dirty |= edit_byte("Subtract", phase.effect_subtract);
				m_atlas_movie_dirty |= edit_byte("Floor", phase.effect_floor);
			}
			static constexpr std::array condition_names{ "Effect calls", "Track Y >=", "Music ended", "Frame counter zero", "Frames" };
			m_atlas_movie_dirty |= enum_combo("End condition", phase.condition, condition_names, 1);
			if (phase.condition == AtlasMovieCondition::TrackYGte)
				m_atlas_movie_dirty |= edit_byte("Condition track", phase.condition_track, 0, movie.tracks.size() - 1);
			if (phase.condition != AtlasMovieCondition::MusicZero)
				m_atlas_movie_dirty |= edit_word("Condition value", phase.condition_value);
			}
			ImGui::EndTabItem();
		}

		if (advanced_mode && ImGui::BeginTabItem("SFX")) {
			if (ui::imgui_button("Add SFX trigger", 2)) { movie.sfx.push_back({}); m_atlas_movie_sel_sfx = movie.sfx.size() - 1; m_atlas_movie_dirty = true; }
			if (!movie.sfx.empty()) {
				if (ImGui::BeginCombo("Trigger", std::format("SFX {}", m_atlas_movie_sel_sfx).c_str())) {
					for (std::size_t i{ 0 }; i < movie.sfx.size(); ++i)
						if (ImGui::Selectable(std::format("SFX {}", i).c_str(), i == m_atlas_movie_sel_sfx)) m_atlas_movie_sel_sfx = i;
					ImGui::EndCombo();
				}
				ImGui::SameLine();
				if (ui::imgui_button("Delete trigger", 1)) { movie.sfx.erase(movie.sfx.begin() + m_atlas_movie_sel_sfx); m_atlas_movie_sel_sfx = movie.sfx.empty() ? 0 : std::min(m_atlas_movie_sel_sfx, movie.sfx.size() - 1); m_atlas_movie_dirty = true; }
				if (!movie.sfx.empty()) {
					auto& event{ movie.sfx[m_atlas_movie_sel_sfx] };
					m_atlas_movie_dirty |= edit_byte("Path track", event.track, 0, movie.tracks.size() - 1);
					m_atlas_movie_dirty |= edit_byte("Sound ID", event.sound);
					m_atlas_movie_dirty |= edit_byte("Stage below", event.stage_lt);
					m_atlas_movie_dirty |= edit_byte("Tick mask", event.tick_mask);
					m_atlas_movie_dirty |= edit_byte("Tick value", event.tick_value);
					m_atlas_movie_dirty |= edit_byte("Animation-slot mask", event.slot_mask);
					m_atlas_movie_dirty |= edit_byte("Animation-slot value", event.slot_value);
				}
			}
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem(advanced_mode ? "Game Assets" : "Add Assets")) {
			ImGui::TextWrapped(advanced_mode
				? "Browse FaxEdit's decoded game data and import a compact, Atlas-owned copy into this movie. The source sprite, room, palette, and music editors are never modified."
				: "Add a character, room background, palette, or music from Faxanadu. Select what you want, then use its Import or Use button.");
			if (!shared_mode)
				ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.25f, 1.0f),
					"Standalone uses ROM-owned graphics. Select Shared mode to copy sprites, rooms, or palettes into FMB; music references remain available.");
			if (ImGui::BeginTabBar("atlas-game-assets")) {
				if (ImGui::BeginTabItem("Sprites")) {
					static char sprite_filter[64]{};
					static int category_filter{ 0 };
					ImGui::SetNextItemWidth(260); ImGui::InputTextWithHint("Search", "sprite name, ID, category...", sprite_filter, sizeof(sprite_filter));
					ImGui::SameLine();
					static constexpr std::array categories{ "All categories", "Enemy", "Dropped item", "NPC", "Special effect", "Game trigger", "Item", "Magic effect", "Boss", "Glitched" };
					ImGui::SetNextItemWidth(180); ImGui::Combo("Category", &category_filter, categories.data(), categories.size());
					std::string needle{ sprite_filter };
					std::ranges::transform(needle, needle.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
					ImGui::BeginChild("game-sprite-cards", ImVec2(0, 300), ImGuiChildFlags_Borders,
						ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar);
					constexpr float sprite_card_width{ 124.0f };
					const int sprite_columns{ std::max(1, static_cast<int>(
						(std::max(0.0f, ImGui::GetContentRegionAvail().x - 28.0f)) / sprite_card_width)) };
					if (ImGui::BeginTable("game-sprite-grid", sprite_columns,
						ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_NoSavedSettings)) {
					for (int column{ 0 }; column < sprite_columns; ++column)
						ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, sprite_card_width);
					for (std::size_t i{ 0 }; i < m_atlas_game_sprites->animations.size(); ++i) {
						const auto category{ m_atlas_game_sprites->sprite_cats.at(i) };
						if (category_filter > 0 && static_cast<int>(category) != category_filter - 1) continue;
						const std::string name{ i < m_cache.m_labels_sprites.size() ? m_cache.m_labels_sprites[i] : std::format("Sprite {}", i) };
						std::string searchable{ std::format("{} {} {}", i, name, SpriteGUILoader::SpriteCatToString(category)) };
						std::ranges::transform(searchable, searchable.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
						if (!needle.empty() && searchable.find(needle) == std::string::npos) continue;
						ImGui::TableNextColumn();
						ImGui::PushID(static_cast<int>(i)); ImGui::BeginGroup();
						const bool selected{ i == m_atlas_asset_sprite };
						if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.78f, 1));
						if (!m_atlas_game_sprite_textures[i].empty() && m_atlas_game_sprite_textures[i][0]) {
							if (ImGui::ImageButton("game-sprite", m_atlas_game_sprite_textures[i][0], ImVec2(96, 96))) {
								m_atlas_asset_sprite = i; m_atlas_asset_frame = 0;
							}
							if (shared_mode && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
								ImGui::SetDragDropPayload(ATLAS_GAME_SPRITE_PAYLOAD, &i, sizeof(i));
								ImGui::Text("Import %s", name.c_str()); ImGui::EndDragDropSource();
							}
						}
						if (selected) ImGui::PopStyleColor();
						ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + sprite_card_width - 10.0f);
						ImGui::Text("#%02zu %s", i, name.c_str());
						ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
						ImGui::TextWrapped("%s · %zu frames", SpriteGUILoader::SpriteCatToString(category).c_str(),
							m_atlas_game_sprites->animations[i].size());
						ImGui::PopStyleColor(); ImGui::PopTextWrapPos();
						ImGui::EndGroup(); ImGui::PopID();
					}
					ImGui::EndTable();
					}
					ImGui::EndChild();
					m_atlas_asset_sprite = std::min(m_atlas_asset_sprite, m_atlas_game_sprites->animations.size() - 1);
					const auto& selected_frames{ m_atlas_game_sprite_textures[m_atlas_asset_sprite] };
					ImGui::SeparatorText("Selected animation");
					const int frame_columns{ std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / 78.0f)) };
					if (ImGui::BeginTable("selected-game-animation", frame_columns,
						ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_NoSavedSettings)) {
					for (int column{ 0 }; column < frame_columns; ++column)
						ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, 72.0f);
					for (std::size_t frame_index{ 0 }; frame_index < selected_frames.size(); ++frame_index) {
						ImGui::TableNextColumn();
						ImGui::PushID(static_cast<int>(frame_index));
						if (selected_frames[frame_index]
							&& ImGui::ImageButton("game-frame", selected_frames[frame_index], ImVec2(64, 64)))
							m_atlas_asset_frame = frame_index;
						ImGui::PopID();
					}
					ImGui::EndTable();
					}
					if (ui::imgui_button("Import animation + actor", 2,
						"Copies only used CHR tiles and frames into this FMB, then creates an actor track",
						!shared_mode))
						guarded([&] { import_atlas_game_sprite(bundle, movie,
							decoded_frames, m_atlas_asset_sprite, 128, 120); });
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Rooms")) {
					show_world_screen_slider(m_atlas_asset_world, m_atlas_asset_screen);
					const auto palette_no{ m_game->get_default_palette_no(m_atlas_asset_world, m_atlas_asset_screen) };
					const auto tileset_no{ m_game->get_default_tileset_no(m_atlas_asset_world, m_atlas_asset_screen) };
					ImGui::Text("Real room graphics · tileset %zu · palette %zu", tileset_no, palette_no);
					const std::uint64_t signature{ (static_cast<std::uint64_t>(m_atlas_asset_world) << 32)
						| (static_cast<std::uint64_t>(m_atlas_asset_screen) << 16) | palette_no };
					if (signature != m_atlas_game_room_signature) {
						if (m_atlas_game_room_texture) SDL_DestroyTexture(m_atlas_game_room_texture);
						m_atlas_game_room_texture = nullptr;
						guarded([&] {
							auto palette{ resolved_asset_bytes(m_game->m_rom_data, movie, AtlasMovieAssetKind::Palette) };
							std::vector<byte> sprite_palette(16, 0x0f);
							if (palette.size() >= 32) std::copy(palette.begin() + 16, palette.begin() + 32, sprite_palette.begin());
							const auto room{ convert_game_room(*m_game, world_ppu_tilesets,
								m_atlas_asset_world, m_atlas_asset_screen, sprite_palette) };
							m_atlas_game_room_texture = render_room_texture(p_renderer, m_gfx.get_nes_palette(), room);
						});
						m_atlas_game_room_signature = signature;
					}
					if (m_atlas_game_room_texture) ImGui::Image(m_atlas_game_room_texture, ImVec2(512, 480));
					if (ui::imgui_button("Import room background", 2,
						"Compacts the room to its used CHR tiles and replaces this movie's background, nametable, and background palette",
						!shared_mode))
						guarded([&] { import_atlas_game_room(bundle, movie,
							m_atlas_asset_world, m_atlas_asset_screen); });
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Palettes")) {
					m_atlas_asset_palette = std::min(m_atlas_asset_palette, m_game->m_palettes.size() - 1);
					int palette_index{ static_cast<int>(m_atlas_asset_palette) };
					if (ImGui::SliderInt("Game palette", &palette_index, 0, static_cast<int>(m_game->m_palettes.size() - 1)))
						m_atlas_asset_palette = palette_index;
					const auto& colors{ m_game->m_palettes[m_atlas_asset_palette] };
					for (std::size_t i{ 0 }; i < colors.size(); ++i) {
						if (i) ImGui::SameLine();
						const auto c{ m_gfx.get_nes_palette()->colors[colors[i] & 0x3f] };
						ImGui::ColorButton(std::format("##pal{}", i).c_str(),
							ImVec4(c.r / 255.f, c.g / 255.f, c.b / 255.f, 1), 0, ImVec2(34, 34));
					}
					if (ui::imgui_button("Use as movie background palette", 2, "", !shared_mode)) guarded([&] {
						const AtlasMovie backup{ movie };
						try {
							auto palette{ resolved_asset_bytes(m_game->m_rom_data, movie, AtlasMovieAssetKind::Palette) };
							palette.resize(32, 0x0f); std::copy_n(colors.begin(), 16, palette.begin());
							const auto* asset{ find_asset(movie, AtlasMovieAssetKind::Palette) };
							replace_import(movie, { AtlasMovieImportKind::Palette,
								std::format("game palette {}", m_atlas_asset_palette), asset->destination, 0, palette });
							AtlasMovieBundleCodec::validate(bundle); m_atlas_movie_dirty = true;
						}
						catch (...) { movie = backup; throw; }
					});
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Music")) {
					ImGui::TextWrapped("Music remains a lightweight game reference; unlike graphics, its data does not need to be copied into the FMB.");
					for (std::size_t i{ 0 }; i < m_cache.m_music_count; ++i) {
						const auto label{ get_description(static_cast<byte>(i), m_cache.m_labels_music) };
						if (ImGui::Selectable(label.c_str(), movie.entry_music == i)) {
							movie.entry_music = static_cast<byte>(i); m_atlas_movie_dirty = true;
						}
					}
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Imported")) {
					if (movie.imports.empty()) ImGui::TextDisabled("This movie has no copied game assets yet.");
					for (const auto& imported : movie.imports)
						ImGui::BulletText("%s — %zu bytes", imported.label.c_str(), imported.data.size());
					ImGui::TextWrapped("Imports live entirely inside this movie's ATI1/FMB data. Import another room or palette to replace that category.");
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem(advanced_mode ? "Frame Browser" : "Poses")) {
			ImGui::TextWrapped(advanced_mode
				? "ROM metasprites — click a real frame, then add it to the selected track."
				: "Choose how the selected actor should look. To place a new actor, select a pose here, then return to Scene and click Place pose.");
			static char frame_filter[48]{};
			ImGui::SetNextItemWidth(280);
			ImGui::InputTextWithHint("Search", "hero, waterfall, ripple, frame 12...", frame_filter, sizeof(frame_filter));
			std::string needle{ frame_filter };
			std::ranges::transform(needle, needle.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			const int browser_columns{ std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / 108.0f)) };
			if (ImGui::BeginTable("movie-frame-grid", browser_columns,
				ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_NoSavedSettings)) {
			for (int column{ 0 }; column < browser_columns; ++column)
				ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, 102.0f);
			for (std::size_t i{ 0 }; i < m_atlas_movie_frame_textures.size(); ++i) {
				const auto label{ std::format("Frame {:02} {}", i, frame_family(i)) };
				std::string searchable{ label };
				std::ranges::transform(searchable, searchable.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				if (!needle.empty() && searchable.find(needle) == std::string::npos) continue;
				ImGui::TableNextColumn();
				ImGui::BeginGroup(); ImGui::PushID(static_cast<int>(i));
				const bool selected{ i == m_atlas_movie_browser_frame };
				if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.8f, 1.0f));
				if (m_atlas_movie_frame_textures[i]) {
					if (ImGui::ImageButton("frame", m_atlas_movie_frame_textures[i], ImVec2(88, 88)))
						m_atlas_movie_browser_frame = i;
					frame_drag_source(static_cast<byte>(i));
				}
				if (selected) ImGui::PopStyleColor();
				ImGui::Text("Frame %02zu", i);
				ImGui::TextDisabled("%s", frame_family(i));
				ImGui::PopID(); ImGui::EndGroup();
			}
			ImGui::EndTable();
			}
			if (!graphics_error.empty())
				ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f), "%s", graphics_error.c_str());
			ImGui::SeparatorText(std::format("Selected: Frame {:02} — {}",
				m_atlas_movie_browser_frame, frame_family(m_atlas_movie_browser_frame)).c_str());
			if (!movie.tracks.empty() && m_atlas_movie_browser_frame < movie.metasprite_count) {
				auto& track{ movie.tracks[m_atlas_movie_sel_track] };
				if (track.kind == AtlasMovieTrackKind::Path) {
					if (ui::imgui_button(advanced_mode ? "Add as animation pose to every stage"
						: "Add pose to selected actor's movement", 2)) {
						for (auto& stage : track.stage_frames) stage.push_back(static_cast<byte>(m_atlas_movie_browser_frame));
						m_atlas_movie_dirty = true;
					}
				}
				else if (track.kind == AtlasMovieTrackKind::Cyclic) {
					if (ui::imgui_button(advanced_mode ? "Add as visible pose" : "Add pose to selected actor's animation", 2)) {
						track.visible_frames.push_back(static_cast<byte>(m_atlas_movie_browser_frame));
						track.reset_at_pose = std::max<byte>(track.reset_at_pose, track.visible_frames.size() + 1);
						m_atlas_movie_dirty = true;
					}
				}
				else {
					if (!advanced_mode) {
						if (ui::imgui_button("Use this pose for selected actor", 2)) {
							track.toggle_frames = { static_cast<byte>(m_atlas_movie_browser_frame),
								static_cast<byte>(m_atlas_movie_browser_frame) }; m_atlas_movie_dirty = true;
						}
					}
					else {
						if (ui::imgui_button("Use as Frame A", 2)) { track.toggle_frames[0] = static_cast<byte>(m_atlas_movie_browser_frame); m_atlas_movie_dirty = true; }
						ImGui::SameLine();
						if (ui::imgui_button("Use as Frame B", 2)) { track.toggle_frames[1] = static_cast<byte>(m_atlas_movie_browser_frame); m_atlas_movie_dirty = true; }
					}
				}
			}
			ImGui::EndTabItem();
		}

		draw_atlas_movie_preview_tab(p_renderer, bundle, movie,
			decoded_frames, graphics_error, advanced_mode, shared_mode);
		ImGui::EndTabBar();
	}
	ImGui::EndChild();
	ImGui::End();
}
