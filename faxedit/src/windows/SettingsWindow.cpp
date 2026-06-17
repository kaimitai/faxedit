#include "MainWindow.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include "Imgui_helper.h"
#include "./../fe/fe_constants.h"
#include "./../fe/fe_app_constants.h"
#include <SDL3/SDL.h>

void fe::MainWindow::draw_settings_window(SDL_Renderer* p_rnd) {

	ui::imgui_screen("Editor Settings",
		c::WIN_TILEMAP_X + 70, c::WIN_TILEMAP_Y + 70,
		c::WIN_TILEMAP_W - 400, c::WIN_TILEMAP_H + 50);

	if (ImGui::BeginTabBar("settings-tabs")) {

		ImGui::PushStyleColor(ImGuiCol_Tab, ui::g_uiStyles[2].normal);
		ImGui::PushStyleColor(ImGuiCol_TabActive, ui::g_uiStyles[2].active);
		ImGui::PushStyleColor(ImGuiCol_TabHovered, ui::g_uiStyles[2].hovered);

		if (ImGui::BeginTabItem("Patching")) {

			ImGui::SeparatorText("World Definitions");
			ui::imgui_checkbox("Screen Tilemaps", m_settings.m_patch_tilemaps);
			ui::imgui_checkbox("Sprites", m_settings.m_patch_sprite_data, "Sprite placements on screens");
			ui::imgui_checkbox("Metadata", m_settings.m_patch_metadata, "Doors, metatile definitions and screen scroll connections");
			ui::imgui_checkbox("Bank 15 Data (transitions and more)", m_settings.m_patch_bank15_data, "Building Scenes, Spawn Points, Pal2Mus, OW- and SW-transitions");
			ui::imgui_checkbox("Stage Definitions", m_settings.m_patch_stages);
			ui::imgui_checkbox("World Scenes", m_settings.m_patch_scenes, "Worlds' default tilesets, music tracks and palettes");

			ImGui::SeparatorText("Graphics");
			ui::imgui_checkbox("World Tileset CHR", m_settings.m_patch_world_chr_data);
			ui::imgui_checkbox("World Palettes", m_settings.m_patch_palettes);
			ui::imgui_checkbox("Background Gfx", m_settings.m_patch_bg_gfx, "Title Screen, Intro/Outro, Item gfx and related palettes");
			ui::imgui_checkbox("Sprite Gfx", m_settings.m_patch_sprite_gfx, "Sprite Animation Frames and related metadata");
			ui::imgui_checkbox("Cinematics", m_settings.m_patch_cinematics);

			ImGui::SeparatorText("Static Data");
			ui::imgui_checkbox("Mattock Animations", m_settings.m_patch_mattock_animations);
			ui::imgui_checkbox("Push-Block", m_settings.m_patch_push_blocks);
			ui::imgui_checkbox("Jump-On", m_settings.m_patch_jump_on_tiles);
			ui::imgui_checkbox("Fog", m_settings.m_patch_fog);

			ImGui::SeparatorText("Other");

			ui::imgui_checkbox("Disallow cinematic data overflow", m_settings.throw_on_cinematic_overflow,
				"Whether to fail patching if cinematic data could potentially overwrite script data (see documentation)");

			ImGui::SeparatorText("Default Patching Settings");
			if (ui::imgui_button("Reset to Defaults###patch", 4))
				m_settings.set_patching_defaults();

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Rendering")) {
			ImGui::SeparatorText("Camera");
			ui::imgui_checkbox("Invert Zoom", m_settings.m_invert_zoom);
			if (ui::imgui_float_slider("###setzf", "Camera Zoom Speed",
				m_settings.m_cam_zoom_factor, 1.1f, 4.0f))
				camera.set_zoom_factor(m_settings.m_cam_zoom_factor);

			ImGui::SeparatorText("Adjacent Screen Rendering");

			ui::imgui_slider_with_arrows("###balpha", "Adjacent Screen Alpha",
				m_settings.m_border_alpha, 0, 255, "How much to darken adjacent screens");

			ImGui::SeparatorText("Default Rendering Settings");
			if (ui::imgui_button("Reset to Defaults###render", 4)) {
				m_settings.set_rendering_defaults();
				camera.set_zoom_factor(m_settings.m_cam_zoom_factor);
			}

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Sprite Gfx")) {
			ImGui::SeparatorText("Animation Frame Rendering Scales");

			ui::imgui_float_slider("###setafs", "Animation Frames", m_settings.scale_frame,
				1.0f, 5.0f);
			ui::imgui_float_slider("###setbanks", "CHR-banks", m_settings.scale_bank,
				1.0f, 5.0f);

			ImGui::SeparatorText("bmp-import");

			ui::imgui_slider_with_arrows("###tratol", "Transparency Tolerance",
				m_settings.transp_tolerance, 0, 10,
				"How far a pixel color can deviate from hot pink and still be considered transparent");

			ImGui::SeparatorText("Default Sprite Gfx Settings");
			if (ui::imgui_button("Reset to Defaults###spritegfx", 4))
				m_settings.set_sprite_gfx_defaults();

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Advanced")) {

			ImGui::SeparatorText("Debug");
			ui::imgui_checkbox("Enable Debug Button", m_settings.m_enable_config_dump);

			ImGui::SeparatorText("Data Integrity");
			ui::imgui_checkbox("Warn on (0, 0)-door destinations", m_settings.m_warn_00_doors);
			ui::imgui_checkbox("Warn on world tilemap >= 95% bank size", m_settings.m_warn_tilemap_95_pct);

			ImGui::SeparatorText("Miscellaneous");
			ui::imgui_checkbox("Show Door Padding Byte", m_settings.m_door_pad_byte,
				"Expose the unused padding byte in the sameworld and building door data");

			ImGui::SeparatorText("Default Advanced Settings");
			if (ui::imgui_button("Reset to Defaults###advanced", 4))
				m_settings.set_advanced_defaults();

			ImGui::EndTabItem();
		}

		ImGui::PopStyleColor(3);

		ImGui::EndTabBar();
	}

	ImGui::End();
}
