#include "MainWindow.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include "Imgui_helper.h"
#include "fe/fe_constants.h"
#include "fe/fe_app_constants.h"
#include "fe/game/GameManager.h"
#include <format>
#include <unordered_map>
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

			ui::imgui_checkbox("Show 2-step diagonal adjacent screens",
				m_settings.m_show_diagonal_adjacent);
			ui::imgui_checkbox("Show ambiguous diagonal adjacent screens",
				m_settings.m_show_ambiguous_diagonals);

			ui::imgui_slider_with_arrows("###balpha", "Adjacent Screen Alpha",
				m_settings.m_border_alpha, 0, 255, "How much to darken adjacent screens");

			ImGui::SeparatorText("GUI");

			ui::imgui_float_slider("###fontscale", "Font Scale", m_settings.m_font_scale, 0.75f, 3.0f);
			if (ImGui::IsItemDeactivatedAfterEdit())
				apply_font_scale(m_settings.m_font_scale);

			ImGui::SeparatorText("Default Rendering Settings");
			if (ui::imgui_button("Reset to Defaults###render", 4)) {
				m_settings.set_rendering_defaults();
				camera.set_zoom_factor(m_settings.m_cam_zoom_factor);
				apply_font_scale(m_settings.m_font_scale);
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
			const static bool ls_secondary_chr_bank{ m_config.has_constant(c::ID_TILESET_SECONDARY_BANK) };

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
				fe::game::migrate_stage_door_hack_data(m_game.value());
				add_message("Sameworld-door to Stage-door hack applied!", fe::MsgType::Success);
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), fe::MsgType::Error);
			}

			if (ls_secondary_chr_bank) {
				ImGui::SeparatorText("Apply Double Tileset Hack");

				if (ui::imgui_button("Enable Double Tileset", 4, "", !ImGui::IsKeyDown(ImGuiMod_Shift) ||
					m_game->m_tileset_type != fe::TilesetType::Normal)) try {
					fe::game::migrate_double_tileset_data(m_game.value());
					generate_world_tilesets();
					add_message("Double Tileset enabled!", fe::MsgType::Success);
				}
				catch (const std::exception& ex) {
					add_message(ex.what(), fe::MsgType::Error);
				}
			}

			ImGui::SeparatorText("Persistent Door Helper");
			static bool ls_def_returns{ true };

			if (ui::imgui_button("Generate asm", 2,
				"Generates a shared @select_door_flag subroutine that maps every key-locked door to an extended flag")) try {
				generate_extended_flag_to_door_map_asm(*m_game, ls_def_returns);
				add_message("Generated assembly copied to clipboard!", fe::MsgType::Success);
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), fe::MsgType::Error);
			}

			ui::imgui_checkbox("Defensive Return-Statements", ls_def_returns,
				"Adds defensive Return instructions to make the generated lookup safe against future door layout changes (slightly increases script size)");

			ImGui::EndTabItem();
		}

		ImGui::PopStyleColor(3);

		ImGui::EndTabBar();
	}

	ImGui::End();
}

void fe::MainWindow::generate_extended_flag_to_door_map_asm(const fe::Game& p_game, bool incl_defensive_returns) const {
	constexpr byte DOOR_KEY_REQ_MIN{ 0x01 };
	constexpr byte DOOR_KEY_REQ_MAX{ 0x05 };

	struct DoorFlag {
		byte yx;
		byte flag;
		byte req;
	};

	struct ScreenFlags {
		byte screen;
		std::vector<DoorFlag> doors;
	};

	struct WorldFlags {
		byte world;
		std::vector<ScreenFlags> screens;
	};

	std::vector<WorldFlags> lookup;
	byte curflag{ 0 };

	for (std::size_t world{ 0 }; world < p_game.m_chunks.size(); ++world) {
		WorldFlags worldflags{
			.world = static_cast<byte>(world)
		};

		for (std::size_t screen{ 0 }; screen < p_game.m_chunks[world].m_screens.size(); ++screen) {
			ScreenFlags screenflags{
				.screen = static_cast<byte>(screen)
			};

			for (std::size_t door{ 0 }; door < p_game.m_chunks[world].m_screens[screen].m_doors.size(); ++door) {
				const auto& l_door{ p_game.m_chunks[world].m_screens[screen].m_doors[door] };

				// stage doors
				if (l_door.m_door_type == fe::DoorType::NextWorld ||
					l_door.m_door_type == fe::DoorType::PrevWorld) {
					auto stage = p_game.m_stages.get_stage_from_world(world);

					if (!stage)
						continue; // world belongs to multiple stages -> ambiguous, skip

					byte l_requirement =
						(l_door.m_door_type == fe::DoorType::NextWorld)
						? (*stage)->m_next_requirement
						: (*stage)->m_prev_requirement;

					if (l_requirement >= DOOR_KEY_REQ_MIN && l_requirement <= DOOR_KEY_REQ_MAX) {
						screenflags.doors.emplace_back(
							DoorFlag{
							.yx = static_cast<byte>((l_door.m_coords.second << 4) | l_door.m_coords.first),
							.flag = static_cast<byte>(247 - curflag++),
							.req = l_requirement
							}
						);

						if (curflag >= 248)
							throw std::runtime_error("Too many key-locked doors");
					}

				}
				// other doors
				else {
					byte l_true_req{ static_cast<byte>(l_door.m_requirement & 0x0f) };

					if (l_true_req >= DOOR_KEY_REQ_MIN && l_true_req <= DOOR_KEY_REQ_MAX) {
						screenflags.doors.emplace_back(
							DoorFlag{
							.yx = static_cast<byte>((l_door.m_coords.second << 4) | l_door.m_coords.first),
							.flag = static_cast<byte>(247 - curflag++),
							.req = l_true_req
							}
						);

						if (curflag >= 248)
							throw std::runtime_error("Too many key-locked doors");
					}
				}
			}

			if (!screenflags.doors.empty())
				worldflags.screens.push_back(screenflags);
		}

		if (!worldflags.screens.empty())
			lookup.push_back(worldflags);
	}

	// emit the asm itself
	constexpr std::size_t BANNER_STAR_COUNT{ 45 };

	const std::string banner{ " ;" + std::string(BANNER_STAR_COUNT, '*') + "\n" };

	std::string result{ banner };
	result += " ; key-locked door to extended flag asm-code\n ; generated by Echoes of Eolis\n ;\n";

	if (curflag > 0) {
		result += std::format(" ; Uses extended flags {}-{} ({} flags)\n",
			248 - curflag, 247, curflag);
	}
	else
		result += " ; No key-locked doors found.\n";

	result += banner;

	result += "@select_door_flag:\n  SelectFlag $ff ; initialize with no flag\n\n";

	result += " ; world lookup\n";

	for (const auto& w : lookup)
		result += std::format("  IfWorld {} @world_{}\n", w.world, w.world);

	if (incl_defensive_returns)
		result += "  Return\n";

	result += "\n";

	result += " ; screen lookup\n";

	for (const auto& w : lookup) {
		result += std::format("@world_{}:\n", w.world);

		for (const auto& s : w.screens) {
			result += std::format("  IfScreen {} @world_{}_screen_{}\n", s.screen, w.world, s.screen);
		}

		if (incl_defensive_returns)
			result += "  Return\n";
	}

	result += "\n";

	result += " ; door lookup\n";

	for (const auto& w : lookup)
		for (const auto& s : w.screens) {

			result += std::format("@world_{}_screen_{}:\n", w.world, s.screen);

			if (s.doors.size() == 1) {
				result += std::format("  SelectFlag {} ; {}\n", s.doors[0].flag,
					get_description(s.doors[0].req, m_cache.m_labels_door_reqs));
				result += "  Return\n\n";
			}
			else {
				for (const auto& d : s.doors) {
					result += std::format("  IfDoorYX ${:02x} @world_{}_screen_{}_door_{:02x}\n",
						d.yx, w.world, s.screen, d.yx);
				}
				if (incl_defensive_returns)
					result += "  Return\n";

				result += "\n";

				for (const auto& d : s.doors) {
					result += std::format("@world_{}_screen_{}_door_{:02x}:\n",
						w.world, s.screen, d.yx);

					result += std::format("  SelectFlag {} ; {}\n", d.flag,
						get_description(d.req, m_cache.m_labels_door_reqs));
					result += "  Return\n\n";
				}
			}
		}

	result += banner;

	SDL_SetClipboardText(result.c_str());
}
