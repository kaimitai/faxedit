#include "MainWindow.h"
#include "gfx.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include "Imgui_helper.h"
#include "./../fe/fe_constants.h"
#include "./../fe/fe_app_constants.h"
#include "./../common/klib/Kfile.h"
#include <format>
#include <SDL3/SDL.h>
#include "./../fe/Cinematic.h"

void fe::MainWindow::draw_cinematic_window(SDL_Renderer* p_rnd) {
	static CinematicEditMode editmode{ fe::CinematicEditMode::Player };
	static bool ls_intro{ true };
	static std::size_t ls_ripple_idx{ 0 }, ls_threshold_idx{ 0 };

	auto& cinema{ m_game->cinematic };

	ui::imgui_screen("Cinematic Editor",
		c::WIN_TILEMAP_X + 50, c::WIN_TILEMAP_Y + 50,
		c::WIN_TILEMAP_W - 50, c::WIN_TILEMAP_H + 50);

	try {

		ImGui::SeparatorText("Edit Mode");

		if (ImGui::RadioButton("Player",
			editmode == fe::CinematicEditMode::Player))
			editmode = fe::CinematicEditMode::Player;
		ImGui::SameLine();
		if (ImGui::RadioButton("Waterfall",
			editmode == fe::CinematicEditMode::Waterfall))
			editmode = fe::CinematicEditMode::Waterfall;
		ImGui::SameLine();
		if (ImGui::RadioButton("Ripples",
			editmode == fe::CinematicEditMode::Ripples))
			editmode = fe::CinematicEditMode::Ripples;
		ImGui::SameLine();
		if (ImGui::RadioButton("Sprite Palettes",
			editmode == fe::CinematicEditMode::Palette))
			editmode = fe::CinematicEditMode::Palette;
		ImGui::SameLine();
		if (ImGui::RadioButton("Animations",
			editmode == fe::CinematicEditMode::AnimationFrames))
			editmode = fe::CinematicEditMode::AnimationFrames;
		ImGui::SameLine();
		if (ImGui::RadioButton("Settings",
			editmode == fe::CinematicEditMode::Settings))
			editmode = fe::CinematicEditMode::Settings;

		if (editmode == fe::CinematicEditMode::Palette) {
			show_cinematic_edit_mode(ls_intro);

			show_palette_window(ls_intro ? c::CINEMATIC_NUM_ID_INTRO : c::CINEMATIC_NUM_ID_OUTRO,
				ls_intro ? cinema.sprite_palette_intro : cinema.sprite_palette_outro);
		}
		else if (editmode == fe::CinematicEditMode::Waterfall) {
			show_cinematic_position(cinema.waterfall_position);
		}
		else if (editmode == fe::CinematicEditMode::Ripples) {
			auto& ripples{ cinema.ripple_data };
			if (ls_ripple_idx >= ripples.size())
				ls_ripple_idx = ripples.size() - 1;

			ImGui::Separator();

			ui::imgui_slider_with_arrows("###ripple",
				std::format("Ripple #{} of {}", ls_ripple_idx, ripples.size()),
				ls_ripple_idx, 0, ripples.size() - 1, "", false, true);

			show_cinematic_position(ripples.at(ls_ripple_idx).initial_position);
			show_cinematic_velocity(ripples.at(ls_ripple_idx).velocity);
		}
		else if (editmode == fe::CinematicEditMode::Player) {
			show_cinematic_edit_mode(ls_intro);
			auto& player_data{ cinema.player_data.at(ls_intro ? 0 : 1) };
			show_cinematic_position(player_data.initial_position);

			auto& thresh_data{ player_data.depth_stages };

			ImGui::SeparatorText("Threshold Stages");

			ui::imgui_slider_with_arrows("###thresh",
				std::format("Threshold #{} of {}", ls_threshold_idx, thresh_data.size()),
				ls_threshold_idx, 0, thresh_data.size() - 1, "", false, true);

			show_cinematic_threshold(thresh_data.at(ls_threshold_idx));
		}
		else if (editmode == fe::CinematicEditMode::Settings) {
			ImGui::SeparatorText("Patching");
			ui::imgui_checkbox("Patch Cinematic Data", m_settings.patch_cinematic,
				"Whether cinematic data should be written to rom during patching");
			ui::imgui_checkbox("Disallow overflow", m_settings.throw_on_cinematic_overflow,
				"Whether to fail patching if cinematic data could potentially overwrite script data (see documentation)");
		}
		else if (editmode == fe::CinematicEditMode::AnimationFrames) {
			show_cinematic_frames(p_rnd);
		}
		else
			ImGui::Separator();

	}
	catch (const std::exception& ex) {
		add_message(ex.what(), 1);
	}

	ImGui::End();
}

void fe::MainWindow::show_cinematic_frames(SDL_Renderer* p_rnd) {
	static bool ls_redraw_bank{ true }, ls_redraw_frame{ true };
	static int ls_sel_frame_x{ 0 }, ls_sel_frame_y{ 0 },
		ls_sel_bank_chr_x{ 0 }, ls_sel_bank_chr_y{ 0 };
	static std::size_t ls_sel_frame{ 0 },
		ls_palette_cutoff{ m_config.constant(c::ID_CINEMATIC_PALETTE_CUTOFF) };

	auto& cinema{ m_game->cinematic };

	if (ls_sel_frame >= cinema.frames.size()) {
		ls_sel_frame = 0;
		ls_redraw_frame = true;
	}

	// force a redraw from the outside
	if (m_settings.m_redraw_cinema_gfx) {
		ls_redraw_frame = true;
		ls_redraw_bank = true;
		m_settings.m_redraw_cinema_gfx = false;
	}

	if (ls_redraw_bank) {
		m_gfx.gen_cinema_selected_chr_bank(p_rnd, cinema.tiles, cinema.sprite_palette_intro);
		ls_redraw_bank = false;
	}
	if (ls_redraw_frame) {
		m_gfx.gen_cinema_selected_texture(p_rnd, cinema.frames.at(ls_sel_frame),
			cinema.tiles, ls_sel_frame >= ls_palette_cutoff ? cinema.sprite_palette_outro : cinema.sprite_palette_intro);
		ls_redraw_frame = false;
	}

	ImGui::Separator();

	if (ui::imgui_slider_with_arrows("###cinself",
		std::format("Frame #{}/{} ({}x{})", ls_sel_frame, cinema.frames.size(),
			cinema.frames.at(ls_sel_frame).w(),
			cinema.frames.at(ls_sel_frame).h()),
		ls_sel_frame, 0, cinema.frames.size() - 1, "", false, true))
		ls_redraw_frame = true;

	auto& lr_frame{ cinema.frames[ls_sel_frame] };

	ImGui::SeparatorText("Frame Metadata");

	ui::imgui_slider_with_arrows("###sfofx", "x-offset", lr_frame.offset_x, -128, 127);
	ui::imgui_slider_with_arrows("###sfofy", "y-offset", lr_frame.offset_y, -128, 127);

	ImGui::SeparatorText("Dimensions");

	if (ui::imgui_button("+row", 2))
		if (lr_frame.add_row())
			ls_redraw_frame = true;
	ImGui::SameLine();
	if (ui::imgui_button("-row", 1))
		if (lr_frame.pop_row())
			ls_redraw_frame = true;

	if (ui::imgui_button("+col", 2))
		if (lr_frame.add_col())
			ls_redraw_frame = true;
	ImGui::SameLine();
	if (ui::imgui_button("-col", 1))
		if (lr_frame.pop_col())
			ls_redraw_frame = true;

	auto paint_tile{ imgui_select_tile_image(m_gfx.get_cinema_selected_texture(),
	m_settings.scale_frame, ls_sel_frame_x, ls_sel_frame_y) };

	if (paint_tile) {
		std::size_t bank_idx{ static_cast<std::size_t>(ls_sel_bank_chr_y * 16 + ls_sel_bank_chr_x) };

		if (paint_tile->first < lr_frame.w() &&
			paint_tile->second < lr_frame.h()) {
			auto& tile{ lr_frame.tilemap.at(paint_tile->second).at(paint_tile->first) };
			if (tile) {
				if (tile->index != static_cast<byte>(bank_idx)) {
					tile->index = static_cast<byte>(bank_idx);
					ls_redraw_frame = true;
				}
			}
			else {
				tile = fe::SpriteFrameTile(static_cast<byte>(bank_idx), 0, false, false);
				ls_redraw_frame = true;
			}
		}
	}

	ImGui::SeparatorText("Selected Tile");

	if (static_cast<std::size_t>(ls_sel_frame_x) < lr_frame.w() &&
		static_cast<std::size_t>(ls_sel_frame_y) < lr_frame.h()) {
		auto& lr_tile{ lr_frame.tilemap.at(static_cast<std::size_t>(ls_sel_frame_y)).at(static_cast<std::size_t>(ls_sel_frame_x)) };

		if (!lr_tile) {
			ImGui::Text("No tile");
			if (ui::imgui_button("Add tile", 2))
				lr_tile = fe::SpriteFrameTile(0, 0, false, false);
		}
		else {
			if (ui::imgui_slider_with_arrows("###stchrind",
				std::format("chr-tile {}", lr_tile->index),
				lr_tile->index, 0, cinema.tiles.empty() ? 0 :
				cinema.tiles.size() - 1))
				ls_redraw_frame = true;

			if (ui::imgui_slider_with_arrows("###stsp",
				std::format("sub-palette {}", lr_tile->sub_palette),
				lr_tile->sub_palette, 0, 3))
				ls_redraw_frame = true;

			if (ui::imgui_checkbox("v-flip", lr_tile->v_flip))
				ls_redraw_frame = true;
			ImGui::SameLine();
			if (ui::imgui_checkbox("h-flip", lr_tile->h_flip))
				ls_redraw_frame = true;;

			if (ui::imgui_button("Clear tile", 1)) {
				lr_tile = std::nullopt;
				ls_redraw_frame = true;
			}
		}
	}
	else {
		imgui_text("No tile selected");
	}

	ImGui::SeparatorText("chr-bank");

	imgui_select_tile_image(m_gfx.get_cinema_selected_chr_bank(),
		m_settings.scale_bank, ls_sel_bank_chr_x, ls_sel_bank_chr_y);

	ImGui::SeparatorText("File Operations");

	if (ui::imgui_button("Export bmps", 2)) {
		export_cinematic_frame_bmps();
	}

	ImGui::SameLine();

	if (ui::imgui_button("Import bmps", 4, "Makes a snapshot, and imports bmps for all cinematic frames")) {
		import_cinematic_frame_bmps();
		ls_redraw_bank = true;
		ls_redraw_frame = true;
	}

	if (ui::imgui_button("Export chr", 2)) {
		export_cinematic_chr_bank(cinema.tiles);
	}

	ImGui::SameLine();

	if (ui::imgui_button("Import chr", 4, "Makes a snapshot, and imports the cinematic chr-bank from file")) {
		import_cinematic_chr_bank(cinema.tiles);
		ls_redraw_bank = true;
		ls_redraw_frame = true;
	}

	ImGui::SeparatorText("Snapshots");

	if (ui::imgui_button("Store Snapshot", 2, "Store snapshot of related chr-banks and frames")) {
		m_sprite_snap_manager.add_flat_snapshot(c::CINEMATIC_NUM_ID_GFX_COLL,
			cinema.tiles, cinema.frames);
		add_message(std::format("Stored cinematic chr-bank and {} frames as snapshot",
			cinema.frames.size()), 2, true);
	}
	ImGui::SameLine();
	if (ui::imgui_button("Restore Snapshot", 4, "Restore snapshot of related chr-banks and frames",
		!m_sprite_snap_manager.has_snapshot(c::CINEMATIC_NUM_ID_GFX_COLL))
		) {
		const auto restoreres{
			m_sprite_snap_manager.restore_flat_snapshot(cinema.tiles, cinema.frames, c::CINEMATIC_NUM_ID_GFX_COLL)
		};

		ls_redraw_bank = true;
		ls_redraw_frame = true;
		add_message(std::format("Restored cinematic chr-bank and {} frame(s) from snapshot", restoreres.second), 2);
	}
	ImGui::SameLine();
	if (ui::imgui_button("Query Snapshot", 4, "Check count of banks and frames stored in the most recent snapshot",
		!m_sprite_snap_manager.has_snapshot(c::CINEMATIC_NUM_ID_GFX_COLL))
		) {
		const auto queryres{ m_sprite_snap_manager.query_snapshot(c::CINEMATIC_NUM_ID_GFX_COLL) };
		add_message(std::format("Most recent snapshot contains {} chr-bank(s) and {} frame(s)",
			queryres.first, queryres.second), 2);
	}
}

void fe::MainWindow::show_cinematic_position(fe::AnimPosition& p_pos) const {
	ImGui::SeparatorText("Initial Coordinates");
	ui::imgui_slider_with_arrows("###afx", "pixel-x", p_pos.x, 0, 255);
	ui::imgui_slider_with_arrows("###afy", "pixel-y", p_pos.y, 0, 255);
}

void fe::MainWindow::show_cinematic_threshold(fe::DepthState& p_threshold) const {
	ImGui::SeparatorText("Threshold");
	ui::imgui_slider_with_arrows("###dsyc", "y-cutoff", p_threshold.y_threshold, 0, 255);

	show_cinematic_velocity(p_threshold.velocity);
}

void fe::MainWindow::show_cinematic_velocity(fe::Velocity& p_velocity) const {
	ImGui::SeparatorText("Velocity");
	ui::imgui_slider_with_arrows("###dsvx", "delta-x", p_velocity.vel_x, -128, 127);
	ui::imgui_slider_with_arrows("###dsvy", "delta-y", p_velocity.vel_y, -128, 127);
}

void fe::MainWindow::show_cinematic_edit_mode(bool& p_mode) {
	ImGui::SeparatorText("Context");

	if (ImGui::RadioButton("Intro",
		p_mode))
		p_mode = true;
	ImGui::SameLine();
	if (ImGui::RadioButton("Outro",
		!p_mode))
		p_mode = false;
}

// file operations
void fe::MainWindow::import_cinematic_frame_bmps(void) {
	auto& cinema{ m_game->cinematic };
	const std::size_t max_tile_count{ m_config.constant(c::ID_CINEMATIC_SPRITE_CHR_COUNT) };

	const auto impres{ m_gfx.import_cinematic_frames_from_folder(get_bmp_path(),
		get_sprite_gfx_file_prefix(c::CINEMATIC_NUM_ID_GFX_COLL),
		cinema.sprite_palette_intro, cinema.sprite_palette_outro,
		max_tile_count, m_settings.transp_tolerance,
		m_config.constant(c::ID_CINEMATIC_PALETTE_CUTOFF),
		m_config.constant(c::ID_CINEMATIC_MIN_ANIM_FRAME_COUNT)) };

	if (impres.approximated_tile_count > 0)
		throw std::runtime_error(
			std::format("Imported bmps required creating {} chr-tiles, but maximum allowed is {}",
				max_tile_count + impres.approximated_tile_count, max_tile_count
			));

	// apply padding to make it visually obvious how much free space exists
	auto importedbank{ impres.tiles };
	while (importedbank.size() < max_tile_count)
		importedbank.push_back(klib::NES_tile());

	m_sprite_snap_manager.apply_flat_bmp_import(c::CINEMATIC_NUM_ID_GFX_COLL,
		importedbank, impres.frames, cinema.tiles, cinema.frames);

	add_message(std::format("Created snapshot, and imported {} bmp files", impres.frames.size()), 2);
}

void fe::MainWindow::export_cinematic_frame_bmps(void) {
	const auto& cinema{ m_game->cinematic };

	m_gfx.save_cinema_frames_bmp(cinema.frames, cinema.tiles, cinema.sprite_palette_intro,
		cinema.sprite_palette_outro, get_bmp_path(),
		get_sprite_gfx_file_prefix(c::CINEMATIC_NUM_ID_GFX_COLL),
		m_config.constant(c::ID_CINEMATIC_PALETTE_CUTOFF));

	add_message(std::format("Saved {} bmps as {}", cinema.frames.size(),
		m_gfx.get_sprite_frame_bmp_wc_filpath(get_bmp_path(),
			get_sprite_gfx_file_prefix(c::CINEMATIC_NUM_ID_GFX_COLL), 0)), 2);
}

void fe::MainWindow::export_cinematic_chr_bank(const std::vector<klib::NES_tile>& p_bank) {
	std::string bank_str_id{ std::format("{}-{:03}", get_sprite_gfx_file_prefix(c::CINEMATIC_NUM_ID_GFX_COLL), 0) };
	save_chr(p_bank, bank_str_id);
}

void fe::MainWindow::import_cinematic_chr_bank(std::vector<klib::NES_tile>& p_cinematic_bank) {
	std::string in_file{ get_chr_file_path(std::format("{}-{:03}",
	get_sprite_gfx_file_prefix(c::CINEMATIC_NUM_ID_GFX_COLL), 0)) };

	const auto tiles_flat{ klib::file::read_file_as_bytes(in_file) };

	if (tiles_flat.size() % 16 != 0)
		throw std::runtime_error(std::format("File size of '{}' not a multiple of 16", in_file));
	std::size_t tile_count{ tiles_flat.size() / 0x10 };

	std::size_t cinema_tile_count{ m_config.constant(c::ID_CINEMATIC_SPRITE_CHR_COUNT) };
	if (tile_count > cinema_tile_count)
		throw std::runtime_error(std::format("File '{}' contains {} tiles, but tile count must less than {}",
			in_file, tile_count, cinema_tile_count));

	std::vector<klib::NES_tile> imp_bank;
	for (std::size_t i{ 0 }; i < tiles_flat.size(); i += 16)
		imp_bank.push_back(klib::NES_tile(tiles_flat, i));

	while (imp_bank.size() < cinema_tile_count)
		imp_bank.push_back(klib::NES_tile());

	m_sprite_snap_manager.apply_flat_chr_import(c::CINEMATIC_NUM_ID_GFX_COLL, 0,
		imp_bank, p_cinematic_bank);

	add_message(std::format("Created snapshot, and imported a {}-tile chr-bank from file {}", imp_bank.size(), in_file), 2);
}
