#include "MainWindow.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include "Imgui_helper.h"
#include "./../fe/fe_constants.h"
#include "./../fe/fe_app_constants.h"
#include <unordered_map>
#include <SDL3/SDL.h>
#include "./../common/klib/Asm6502.h"

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

			ui::imgui_checkbox("Show 2-step diagonal adjacent screens",
				m_settings.m_show_diagonal_adjacent);
			ui::imgui_checkbox("Show ambiguous diagonal adjacent screens",
				m_settings.m_show_ambiguous_diagonals);

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
			ui::imgui_checkbox("Enable Debug Features", m_settings.m_enable_config_dump);

			ImGui::SeparatorText("Data Integrity");
			ui::imgui_checkbox("Warn on (0, 0)-door destinations", m_settings.m_warn_00_doors);
			ui::imgui_checkbox("Warn on world tilemap >= 95% bank size", m_settings.m_warn_tilemap_95_pct);

			ImGui::SeparatorText("Miscellaneous");
			ui::imgui_checkbox("Enable IPS patching", m_settings.m_enable_ips_button);
			ui::imgui_checkbox("Show Door Padding Byte", m_settings.m_door_pad_byte,
				"Expose the unused padding byte in the sameworld and building door data");

			ImGui::SeparatorText("Default Advanced Settings");
			if (ui::imgui_button("Reset to Defaults###advanced", 4))
				m_settings.set_advanced_defaults();

			ImGui::SeparatorText("Apply Stage Door Hack");

			imgui_text("Turns same-world doors into flexible stage doors.");
			imgui_text("Allows doors to connect to any stage in the game.");
			imgui_text("Hold Shift while clicking to apply.");
			imgui_text("Warning: This permanently modifies the loaded ROM.");
			imgui_text("See the documentation for details.");

			if (ui::imgui_button("Enable Stage Doors", 4, "",
				!ImGui::IsKeyDown(ImGuiMod_Shift) ||
				m_game->m_sw_door_type != fe::SameWorldDoorType::Normal)) try {
				patch_randumizer_doors(*m_game, true);
				add_message("Sameworld-door to Stage-door hack applied!", 2);
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), 1);
			}

			ImGui::EndTabItem();
		}

		ImGui::PopStyleColor(3);

		ImGui::EndTabBar();
	}

	ImGui::End();
}

void fe::MainWindow::patch_randumizer_doors(fe::Game& p_game, bool migrate_door_data) {
	// check if the patch can actually be applied

	// copy the world -> stages lookup map so we can use [] to populate missing entries
	auto world2stages{ p_game.m_stages.m_world_to_stage };

	if (migrate_door_data) {
		for (std::size_t w{ 0 }; w < p_game.m_chunks.size(); ++w) {
			const auto& stages = world2stages[w];

			for (const auto& scr : p_game.m_chunks[w].m_screens)
				for (const auto& door : scr.m_doors)
					if (door.m_door_type == fe::DoorType::SameWorld &&
						stages.size() != 1)
						throw std::runtime_error(
							std::format("World {} is referenced by {} stage(s). Expected exactly one.",
								w, stages.size()));
		}
	}

	auto newrom{ p_game.m_rom_data };

	// game routines
	const word Game_SetupAndLoadOutsideArea{ 0xdadc };
	const word Player_CheckHandleEnterDoor_enterScreen{ 0xe565 };
	const word Screen_LoadSpritePalette{ 0xd062 };
	const word Game_LoadCurrentArea_LDX_Stage{ 0xdf22 };
	const word Screen_Load{ 0xdd46 };
	const word Game_LoadCurrentArea_LoadPalette{ 0xdf1d };
	const word Area_SetStateFromDoorDestination_STA_DoorReq{ 0xe84c };
	const word Player_EnterDoorToOutside_JMP_SetupArea{ 0xe5d7 };

	// RAM
	const word CurrentDoor_KeyRequirement{ 0x042b };
	const word RAM_CurrentStage{ 0x0435 };
	const word RAM_StageChangePending{ 0x1fff };
	const word RAM_PendingStage{ 0x1ffe };

	// new routine addresses

	// randumizer addresses for reference
	// const word Hack_ClearPendingStageAndLoadWorld{ 0xfda0 };
	// const word Hack_SetPendingStage{ 0xfe00 };
	// const word Hack_HandlePalette{ 0xfe20 };
	// const word Hack_ExtractStageAndRequirement{ 0xfe40 };
	// (we do not add a separate function for palette to music like the
	// randomizer as we already handle the map dynamically in the frontend)

	// new routine addresses to avoid claiming free space
	// use the sound effect priority table from index 1
	const word Hack_HandlePalette{ 0xf389 };
	// and use the normally unreachable debug code
	const word Hack_SetPendingStage{ 0xdf99 };
	const word Hack_ExtractStageAndRequirement{ 0xdfa8 };
	const word Hack_ClearPendingStageAndLoadWorld{ 0xdfb7 };

	klib::Asm6502 code{};

	// new routine for setting pending stage: Hack_SetPendingStage
	code.lda_imm(0x01);
	code.sta_abs(RAM_StageChangePending);
	code.lda_abs(RAM_PendingStage);
	code.sta_abs(RAM_CurrentStage);
	code.jsr(Game_SetupAndLoadOutsideArea);
	code.rts();
	code.apply_hack_and_clear(newrom, 15, Hack_SetPendingStage);

	// update the sameworld-door logic to jump into our new routine instead of vanilla
	code.jmp(Hack_SetPendingStage);
	code.apply_hack_and_clear(newrom, 15, Player_CheckHandleEnterDoor_enterScreen);

	// new routine for handling hack door palette: Hack_HandlePalette
	code.lda_imm(0x00);
	code.jsr(Screen_LoadSpritePalette);
	code.lda_abs(RAM_StageChangePending);
	code.cmp_imm(0x01);
	code.beq("@stage_pending");
	// hack stage change not pending, use vanilla palette logic
	code.jmp(Game_LoadCurrentArea_LDX_Stage);
	// hack stage change pending - clear the flag and load screen
	code.label("@stage_pending");
	code.lda_imm(0x00);
	code.sta_abs(RAM_StageChangePending);
	code.jmp(Screen_Load);
	code.apply_hack_and_clear(newrom, 15, Hack_HandlePalette);

	// update the stage palette logic to jump into our palette handler
	code.jmp(Hack_HandlePalette);
	code.nop();
	code.nop();
	code.apply_hack_and_clear(newrom, 15, Game_LoadCurrentArea_LoadPalette);

	// extract stage and actual door requirement from hack-door data: Hack_ExtractStageAndRequirement
	code.tay();
	// get stage from the requirement byte (hi nibble)
	code.lsr_a();
	code.lsr_a();
	code.lsr_a();
	code.lsr_a();
	code.sta_abs(RAM_PendingStage);
	code.tya();
	// get actual requirement (lo nibble)
	code.and_imm(0x0f);
	code.sta_abs(CurrentDoor_KeyRequirement);
	code.rts();
	code.apply_hack_and_clear(newrom, 15, Hack_ExtractStageAndRequirement);

	// instead of storing A in door requirement ram directly, jump to the new routine
	code.jsr(Hack_ExtractStageAndRequirement);
	code.apply_hack_and_clear(newrom, 15, Area_SetStateFromDoorDestination_STA_DoorReq);

	// clear pending hack stage change flag and load world
	code.lda_imm(0x00);
	code.sta_abs(RAM_StageChangePending);
	code.jmp(Game_SetupAndLoadOutsideArea);
	code.apply_hack_and_clear(newrom, 15, Hack_ClearPendingStageAndLoadWorld);

	// hook vanilla code into our new routine
	code.jmp(Hack_ClearPendingStageAndLoadWorld);
	code.apply_hack_and_clear(newrom, 15, Player_EnterDoorToOutside_JMP_SetupArea);

	// apply door data changes
	if (migrate_door_data) {
		for (std::size_t w{ 0 }; w < p_game.m_chunks.size(); ++w)
			for (auto& scr : p_game.m_chunks[w].m_screens)
				for (auto& door : scr.m_doors)
					if (door.m_door_type == fe::DoorType::SameWorld) {
						byte dest_stage{ static_cast<byte>(world2stages[w][0]) };
						door.m_requirement = static_cast<byte>((dest_stage << 4) | (door.m_requirement & 0x0f));
					}
	}

	// apply rom patch
	p_game.m_rom_data = newrom;
	// set door type
	p_game.m_sw_door_type = fe::SameWorldDoorType::Randumizer_0_30;
}
