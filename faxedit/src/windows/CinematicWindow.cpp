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
		c::WIN_TILEMAP_W / 2, c::WIN_TILEMAP_H + 50);

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

			ImGui::SeparatorText("Threshold Stage");

			ui::imgui_slider_with_arrows("###thresh",
				std::format("Threshold #{} of {}", ls_threshold_idx, thresh_data.size()),
				ls_threshold_idx, 0, thresh_data.size() - 1, "", false, true);

			show_cinematic_threshold(thresh_data.at(ls_threshold_idx));
		}
		else if (editmode == fe::CinematicEditMode::Settings) {
			ImGui::SeparatorText("Patching");
			ui::imgui_checkbox("Patch ROM", m_settings.patch_cinematic,
				"Whether cinematic data should be written when patching ROM");
			ui::imgui_checkbox("Allow overflow", m_settings.throw_on_cinematic_overflow,
				"Whether to allow patching if cinematic data could potentially overwrite script data (see documentation)");
		}
		else
			ImGui::Separator();

	}
	catch (const std::exception& ex) {
		add_message(ex.what(), 1);
	}

	ImGui::End();
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
