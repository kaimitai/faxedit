#include "MainWindow.h"
#include "gfx.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include "Imgui_helper.h"
#include "./../fe/fe_constants.h"
#include "./../fe/fe_app_constants.h"
#include "./../common/klib/Kfile.h"
#include "./../fe/sprite/fe_sprite_constants.h"
#include <format>
#include <SDL3/SDL.h>
#include "./../fe/sprite/SpriteGUILoader.h"
#include <cmath>

using byte = unsigned char;

void fe::MainWindow::draw_sprite_gfx_window(SDL_Renderer* p_rnd) {
	static SpriteGfxEditMode editmode{ fe::SpriteGfxEditMode::NPC };
	static std::size_t ls_sel_npc{ 0 };

	ui::imgui_screen("Sprite Graphics Editor",
		c::WIN_TILEMAP_X + 50, c::WIN_TILEMAP_Y + 50,
		c::WIN_TILEMAP_W / 2, c::WIN_TILEMAP_H + 50);

	try {
		ImGui::SeparatorText("Edit Mode");

		if (ImGui::RadioButton("NPCs++",
			editmode == fe::SpriteGfxEditMode::NPC))
			editmode = fe::SpriteGfxEditMode::NPC;
		ImGui::SameLine();
		if (ImGui::RadioButton("Player++",
			editmode == fe::SpriteGfxEditMode::Player))
			editmode = fe::SpriteGfxEditMode::Player;
		ImGui::SameLine();
		if (ImGui::RadioButton("Portraits",
			editmode == fe::SpriteGfxEditMode::Portraits))
			editmode = fe::SpriteGfxEditMode::Portraits;
		ImGui::SameLine();
		if (ImGui::RadioButton("Settings",
			editmode == fe::SpriteGfxEditMode::Settings))
			editmode = fe::SpriteGfxEditMode::Settings;

		if (editmode == fe::SpriteGfxEditMode::Settings) {
			ImGui::SeparatorText("Palettes (for GUI rendering and bmp-import/export)");
			ui::imgui_slider_with_arrows("###npcpal", std::format("NPCs: {}",
				get_description(static_cast<byte>(m_settings.coll_palettes[0]), m_cache.m_labels_palettes)),
				m_settings.coll_palettes[0], 0, m_game->m_palettes.size() - 1, "", false, true);
			ui::imgui_slider_with_arrows("###plapal", std::format("Player: {}",
				get_description(static_cast<byte>(m_settings.coll_palettes[1]), m_cache.m_labels_palettes)),
				m_settings.coll_palettes[1], 0, m_game->m_palettes.size() - 1, "", false, true);
			ui::imgui_slider_with_arrows("###porpal", std::format("Portraits: {}",
				get_description(static_cast<byte>(m_settings.coll_palettes[2]), m_cache.m_labels_palettes)),
				m_settings.coll_palettes[2], 0, m_game->m_palettes.size() - 1, "", false, true);

			ImGui::SeparatorText("Default Sprite Palettes");

			if (ui::imgui_button("Reset palette settings", 4, "Reset palettes to defaults")) {
				m_settings.set_sprite_palettes_defaults();
			}
			ImGui::SeparatorText("GUI");
			if (ui::imgui_button("Regenerate GUI sprites", 4, "Regenerate the sprite graphics seen in the editor UI")) {
				generate_editor_sprite_gfx(p_rnd);
				add_message("GUI sprite textures regenerated", 2);
			}

			// DEVELOPER-CODE - BEGIN
			if (m_settings.m_enable_config_dump) {
				auto& lr_spr_first_frames{ m_game->m_sprite_gfx_manager.npc_start_frames };

				ImGui::SeparatorText("Developer Feature: Collection bmp export");

				if (ui::imgui_button("Export all NPC frames as bmps", 2)) {
					const auto& npccoll{ m_game->m_sprite_gfx_manager.c_npcs };
					for (std::size_t i{ 0 }; i <= m_cache.m_sprite_count; ++i)
						export_sprite_frame_bmps(npccoll, c::KEY_COLL_NPCS, i);
				}
				if (ui::imgui_button("Export all player frames as bmps", 2)) {
					const auto& playercoll{ m_game->m_sprite_gfx_manager.c_player };
					export_sprite_frame_bmps(playercoll, c::KEY_COLL_PLAYER, c::KEY_BANK_ARMOR);
					export_sprite_frame_bmps(playercoll, c::KEY_COLL_PLAYER, c::KEY_BANK_WEAPONS);
				}
				if (ui::imgui_button("Export all portrait frames as bmps", 2)) {
					const auto& portraitcoll{ m_game->m_sprite_gfx_manager.c_portraits };
					export_sprite_frame_bmps(portraitcoll, c::KEY_COLL_PORTRAITS, c::KEY_BANK_PORTRAITS);
				}

				ImGui::SeparatorText("Developer Feature: Collection bmp import");

				if (ui::imgui_button("Import all NPC frame bmps", 4)) {
					auto& npccoll{ m_game->m_sprite_gfx_manager.c_npcs };

					for (std::size_t i{ 0 }; i <= m_cache.m_sprite_count; ++i) {
						try {
							import_sprite_frame_bmps(npccoll, c::KEY_COLL_NPCS, i);
						}
						catch (const std::exception& ex) {
							add_message(ex.what(), 1);
						}

					}
				}

				if (ui::imgui_button("Import all player frame bmps", 4)) {
					auto& playercoll{ m_game->m_sprite_gfx_manager.c_player };
					import_sprite_frame_bmps(playercoll, c::KEY_COLL_PLAYER, c::KEY_BANK_ARMOR);
					import_sprite_frame_bmps(playercoll, c::KEY_COLL_PLAYER, c::KEY_BANK_WEAPONS);
				}
				if (ui::imgui_button("Import all portrait frame bmps", 4)) {
					auto& portraitcoll{ m_game->m_sprite_gfx_manager.c_portraits };
					import_sprite_frame_bmps(portraitcoll, c::KEY_COLL_PORTRAITS, 0);
				}
			}
			// DEVELOPER-CODE - END
		}
		else if (editmode == fe::SpriteGfxEditMode::Portraits) {
			ImGui::Separator();
			show_sprite_gfx_editor(p_rnd, c::KEY_COLL_PORTRAITS, m_game->m_sprite_gfx_manager.c_portraits);
		}
		else if (editmode == fe::SpriteGfxEditMode::Player) {
			ImGui::Separator();
			show_sprite_gfx_editor(p_rnd, c::KEY_COLL_PLAYER, m_game->m_sprite_gfx_manager.c_player);
		}
		else if (editmode == fe::SpriteGfxEditMode::NPC) {
			ImGui::Separator();
			show_sprite_gfx_editor(p_rnd, c::KEY_COLL_NPCS, m_game->m_sprite_gfx_manager.c_npcs);
		}

	}
	catch (const std::exception& ex) {
		add_message(ex.what(), 1);
	}

	ImGui::End();
}

void fe::MainWindow::show_sprite_gfx_editor(SDL_Renderer* p_rnd,
	std::size_t p_coll, fe::SpriteFrameCollection& p_collection) {
	static std::optional<std::size_t> ls_coll;
	static std::size_t ls_frame{ 0 }, ls_bank{ 0 }, ls_resolved_bank{ 0 };
	static int ls_sel_frame_x{ 0 }, ls_sel_frame_y{ 0 }, ls_sel_bank_x{ 0 }, ls_sel_bank_y{ 0 },
		ls_sel_bank_chr_x{ 0 }, ls_sel_bank_chr_y{ 0 };
	static bool ls_redraw_bank{ true }, ls_redraw_frame{ true };

	// force a redraw from the outside
	if (m_settings.m_redraw_sprite_gfx) {
		ls_redraw_frame = true;
		ls_redraw_bank = true;
		m_settings.m_redraw_sprite_gfx = false;
	}

	if (!ls_coll || *ls_coll != p_coll) {
		ls_redraw_frame = true;
		ls_redraw_bank = true;
		ls_coll = p_coll;
	}

	if (ls_frame >= p_collection.frames.size()) {
		ls_frame = p_collection.frames.size() - 1;
		ls_redraw_frame = true;
	}

	// get the chr bank ID - all frames must be associated with at least one bank
	if (ls_bank >= p_collection.frames[ls_frame].chrbanks.size()) {
		ls_bank = p_collection.frames[ls_frame].chrbanks.size() - 1;
		ls_resolved_bank = p_collection.frames.at(ls_frame).chrbanks.at(ls_bank);
		ls_redraw_bank = true;
	}
	else {
		std::size_t resolved_bank = p_collection.frames.at(ls_frame).chrbanks.at(ls_bank);
		if (resolved_bank != ls_resolved_bank) {
			ls_resolved_bank = resolved_bank;
			ls_redraw_bank = true;
		}
	}

	if (ls_redraw_bank) {
		m_gfx.gen_sprite_selected_chr_bank(p_rnd, p_collection.banks[ls_resolved_bank],
			m_game->m_palettes.at(m_settings.coll_palettes[p_coll]));
		ls_redraw_bank = false;
		ls_redraw_frame = true;
	}

	if (ls_redraw_frame) {
		m_gfx.gen_sprite_selected_texture(p_rnd, p_collection.frames[ls_frame].frame,
			p_collection.banks[ls_resolved_bank], m_game->m_palettes.at(m_settings.coll_palettes[p_coll]));
		ls_redraw_frame = false;
	}

	// draw the frame itself
	bool is_shield_frame{ p_coll == c::KEY_COLL_PLAYER && ls_resolved_bank == c::KEY_BANK_SHIELDS };

	if (!p_collection.frames.empty()) {
		if (ls_frame >= p_collection.frames.size())
			ls_frame = p_collection.frames.size() - 1;

		if (ui::imgui_slider_with_arrows("###selframe",
			std::format("Frame #{}/{} ({}x{})", ls_frame, p_collection.frames.size(),
				p_collection.frames.at(ls_frame).frame.w(),
				p_collection.frames.at(ls_frame).frame.h()),
			ls_frame, 0, p_collection.frames.size() - 1, "", false, true))
			ls_redraw_frame = true;

		// get the chr bank ID - all frames must be associated with at least one bank
		if (ls_bank >= p_collection.frames[ls_frame].chrbanks.size()) {
			ls_bank = p_collection.frames[ls_frame].chrbanks.size() - 1;

			std::size_t resolved_bank{ p_collection.frames.at(ls_frame).chrbanks.at(ls_bank) };
			if (resolved_bank != ls_resolved_bank) {
				ls_resolved_bank = resolved_bank;
				ls_redraw_bank = true;
			}
		}

		auto& lr_frame{ p_collection.frames[ls_frame].frame };

		if (!is_shield_frame) {

			ImGui::SeparatorText("Frame Metadata");

			ui::imgui_slider_with_arrows("###sfofx", "x-offset", lr_frame.offset_x, -128, 127);
			ui::imgui_slider_with_arrows("###sfofy", "y-offset", lr_frame.offset_y, -128, 127);
			ui::imgui_slider_with_arrows("###sfpivx", "x-pivot", lr_frame.pivot_x, -128, 127);

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

			ImGui::SeparatorText("Rendered Frame");

			// draw the frame texture - we have a valid animation frame and bank index
			auto paint_tile{ imgui_select_tile_image(m_gfx.get_sprite_selected_texture(),
				m_settings.scale_frame, ls_sel_frame_x, ls_sel_frame_y) };

			if (paint_tile) {
				std::size_t bank_idx{ static_cast<std::size_t>(ls_sel_bank_chr_y * 16 + ls_sel_bank_chr_x) };
				if (ls_resolved_bank < p_collection.banks.size() &&
					bank_idx < p_collection.banks[ls_resolved_bank].size() &&
					paint_tile->first < lr_frame.w() &&
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
						lr_tile->index, 0, p_collection.banks.at(ls_resolved_bank).empty() ? 0 :
						p_collection.banks.at(ls_resolved_bank).size() - 1))
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
		}
		else {
			ImGui::Text("Shield frames are not supported outside of chr-import/export. See the documentation for details.");
		}

	}
	else {
		imgui_text("Collection has no animation frames");
		return;
	}

	ImGui::Separator();

	const auto& framebanks{ p_collection.frames[ls_frame] };
	if (ui::imgui_slider_with_arrows("###selbank",
		get_sprite_gfx_bank_name(p_coll, ls_bank, ls_resolved_bank, framebanks.chrbanks.size(),
			p_collection.banks.at(ls_resolved_bank).size()),
		ls_bank, 0, p_collection.frames[ls_frame].chrbanks.size() - 1, "", false, true))
		ls_redraw_bank = true;

	imgui_select_tile_image(m_gfx.get_sprite_selected_chr_bank(),
		m_settings.scale_bank, ls_sel_bank_chr_x, ls_sel_bank_chr_y);

	/*
	TODO: Decide if users should be allowed to canonicalize. bmp import will do this up to sorting anyway
	if (ui::imgui_button("Canonicalize", 2)) {
		m_game->m_sprite_gfx_manager.canonsort_gfx_collection_chr_bank(p_collection, p_collection.frames[ls_frame].chrbanks[ls_bank]);
		ls_redraw_bank = true;
	}
	*/

	ImGui::SeparatorText("File Operations");

	if (ui::imgui_button("Export bmps", 2, "", is_shield_frame)) {
		export_sprite_frame_bmps(p_collection, p_coll, ls_resolved_bank);
	}

	ImGui::SameLine();

	if (ui::imgui_button("Import bmps", 4, "Makes a snapshot, and imports bmps for all frames using the selected chr-bank", is_shield_frame)) {
		import_sprite_frame_bmps(p_collection, p_coll, ls_resolved_bank);
		ls_redraw_bank = true;
	}

	if (ui::imgui_button("Export chr", 2))
		export_sprite_chr_bank(p_collection, p_coll, ls_resolved_bank);

	ImGui::SameLine();

	if (ui::imgui_button("Import chr", 4, "Makes a snapshot, and imports the selected chr-bank from file")) {
		import_sprite_chr_bank(p_collection, p_coll, ls_resolved_bank);
		ls_redraw_bank = true;
	}

	ImGui::SeparatorText("Snapshots");
	if (ui::imgui_button("Store Snapshot", 2, "Store snapshot of related chr-banks and frames")) {
		const auto l_impact{ m_game->m_sprite_gfx_manager.analyze_bank_impact(p_collection, ls_resolved_bank) };
		m_sprite_snap_manager.add_snapshot(p_collection, p_coll,
			l_impact);

		add_message(std::format("Stored {} chr-banks and {} frames as snapshot",
			l_impact.chr_bank_indexes.size(), l_impact.frame_indexes.size()), 2);
	}
	ImGui::SameLine();
	if (ui::imgui_button("Restore Snapshot", 4, "Restore snapshot of related chr-banks and frames",
		!m_sprite_snap_manager.has_snapshot(p_coll))
		) {
		auto restore_result{ m_sprite_snap_manager.restore_snapshot(p_collection, p_coll) };
		ls_redraw_bank = true;
		ls_redraw_frame = true;
		add_message(std::format("Restored {} chr-bank(s) and {} frame(s) from snapshot", restore_result.first, restore_result.second), 2);
	}
	ImGui::SameLine();
	if (ui::imgui_button("Query Snapshot", 4, "Check count of banks and frames stored in the most recent snapshot",
		!m_sprite_snap_manager.has_snapshot(p_coll))
		) {
		const auto queryres{ m_sprite_snap_manager.query_snapshot(p_coll) };
		add_message(std::format("Most recent snapshot contains {} chr-bank(s) and {} frame(s)",
			queryres.first, queryres.second), 2);
	}
}

std::optional<std::pair<int, int>> fe::MainWindow::imgui_select_tile_image(SDL_Texture* tex, float scale, int& sel_x, int& sel_y) const {
	if (!tex)
		return std::nullopt;
	std::optional<std::pair<int, int>> result;
	const float FRAME_TILE_SIZE{ 8.0f * scale };

	ImVec2 size = ImVec2(
		scale * static_cast<float>(tex->w),
		scale * static_cast<float>(tex->h)
	);

	ImVec2 pos = ImGui::GetCursorScreenPos();

	// time
	float t = static_cast<float>(ImGui::GetTime());
	float pulse = (std::sin(t * 2.0f) + 1.0f) * 0.5f;

	// base colors
	ImVec4 c1 = ImVec4(0.1f, 0.2f, 0.8f, 1.0f);
	ImVec4 c2 = ImVec4(0.3f, 0.6f, 1.0f, 1.0f);

	ImVec4 c = ImVec4(
		c1.x + (c2.x - c1.x) * pulse,
		c1.y + (c2.y - c1.y) * pulse,
		c1.z + (c2.z - c1.z) * pulse,
		1.0f
	);

	ImU32 col = ImGui::ColorConvertFloat4ToU32(c);

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->AddRectFilled(
		pos,
		ImVec2(pos.x + size.x, pos.y + size.y),
		ImGui::ColorConvertFloat4ToU32(c)
	);

	ImGui::Image(tex, size);

	// image rect in screen coordinates
	const ImVec2 img_min{ ImGui::GetItemRectMin() };
	const ImVec2 img_max{ ImGui::GetItemRectMax() };

	// handle clicking on the frame
	if (ImGui::IsItemHovered() &&
		(ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
			ImGui::IsMouseDown(ImGuiMouseButton_Right))) {
		ImVec2 mouse = ImGui::GetIO().MousePos;

		int tile_x = static_cast<int>((mouse.x - img_min.x) / FRAME_TILE_SIZE);
		int tile_y = static_cast<int>((mouse.y - img_min.y) / FRAME_TILE_SIZE);

		const int tiles_w = tex->w / 8;
		const int tiles_h = tex->h / 8;

		if (tile_x >= 0 && tile_x < tiles_w && tile_y >= 0 && tile_y < tiles_h) {
			if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				sel_x = tile_x;
				sel_y = tile_y;
			}
			else
				result = std::make_pair(tile_x, tile_y);
		}
	}

	// draw selection rectangle
	{
		const int tiles_w = tex->w / 8;
		const int tiles_h = tex->h / 8;

		if (sel_x >= 0 && sel_x < tiles_w &&
			sel_y >= 0 && sel_y < tiles_h) {

			ImVec2 rect_min{
				img_min.x + sel_x * FRAME_TILE_SIZE,
				img_min.y + sel_y * FRAME_TILE_SIZE
			};
			ImVec2 rect_max{
				rect_min.x + FRAME_TILE_SIZE,
				rect_min.y + FRAME_TILE_SIZE
			};

			ImDrawList* draw = ImGui::GetWindowDrawList();
			draw->AddRect(rect_min, rect_max, IM_COL32(0, 0, 0, 255), 0.0f, 0, 3.0f);
			draw->AddRect(rect_min, rect_max, IM_COL32(255, 255, 0, 255), 0.0f, 0, 1.5f);
		}
	}

	return result;
}

void fe::MainWindow::export_sprite_chr_bank(const fe::SpriteFrameCollection& p_coll, std::size_t p_coll_id,
	std::size_t p_bank_id) {
	std::string bank_str_id{ std::format("{}-{:03}", get_sprite_gfx_file_prefix(p_coll_id), p_bank_id) };
	const auto tiles{ p_coll.get_chr_bank(p_bank_id, p_coll_id == c::KEY_COLL_NPCS) };
	save_chr(tiles, bank_str_id);
}

void fe::MainWindow::import_sprite_chr_bank(fe::SpriteFrameCollection& p_coll, std::size_t p_coll_id,
	std::size_t p_bank_id) {
	std::string in_file{ get_chr_file_path(std::format("{}-{:03}",
		get_sprite_gfx_file_prefix(p_coll_id), p_bank_id)) };

	const auto tiles_flat{ klib::file::read_file_as_bytes(in_file) };

	if (tiles_flat.size() % 16 != 0)
		throw std::runtime_error(std::format("File size of '{}' not a multiple of 16", in_file));
	std::size_t tile_count{ tiles_flat.size() / 0x10 };
	const auto tilerange{ m_game->m_sprite_gfx_manager.get_chr_tile_count_range(p_coll_id, p_bank_id) };

	if (tile_count < tilerange.first || tile_count > tilerange.second)
		throw std::runtime_error(std::format("File '{}' contains {} tiles, but tile count must be in range [{}-{}]",
			in_file, tile_count, tilerange.first, tilerange.second));

	std::vector<klib::NES_tile> imp_bank;
	for (std::size_t i{ 0 }; i < tiles_flat.size(); i += 16)
		imp_bank.push_back(klib::NES_tile(tiles_flat, i));

	if (p_coll_id == c::KEY_COLL_NPCS && p_bank_id == p_coll.banks.size() - 1) {
		fe::SpriteFrameCollection::expand_bank(imp_bank);
		if (imp_bank.at(c::SPRITE_0_PPU_IDX) != m_game->m_sprite_gfx_manager.get_sprite_0_hit_tile()) {
			add_message(std::format("Warning: chr-tile {} is not equal to the original 0-hit sprite. See the documentation.", c::SPRITE_0_PPU_IDX), 1);
		}
	}

	// store bank after making snapshot
	m_sprite_snap_manager.apply_chr_import(p_coll, p_coll_id, p_bank_id, imp_bank);

	add_message(std::format("Created snapshot, and imported a {}-tile chr-bank from file {}", imp_bank.size(), in_file), 2);
}

void fe::MainWindow::import_sprite_frame_bmps(fe::SpriteFrameCollection& p_coll, std::size_t p_coll_id,
	std::size_t p_bank_id) {

	const auto impact{ m_game->m_sprite_gfx_manager.analyze_bank_impact(p_coll, p_bank_id) };

	if (impact.frame_indexes.empty())
		add_message(std::format("No frames using chr bank {}", p_bank_id), 6);
	else if (impact.banks_identical) {
		auto impres{ m_gfx.import_sprite_frames_from_folder(get_bmp_path(), get_sprite_gfx_file_prefix(p_coll_id),
			p_bank_id, impact.frame_indexes, m_game->m_palettes.at(m_settings.coll_palettes[p_coll_id]),
			256, m_settings.transp_tolerance) };

		// check overflow
		if (impres.approximated_tile_count > 0)
			throw std::runtime_error("Imported bmps required creating more than 256 chr-tiles");

		for (std::size_t l_bank_id : impact.chr_bank_indexes) {
			const auto tilerange{ m_game->m_sprite_gfx_manager.get_chr_tile_count_range(p_coll_id, l_bank_id) };
			std::size_t tilecount{ impres.tiles.size() };

			if (tilecount < tilerange.first || tilecount > tilerange.second)
				throw std::runtime_error(std::format("bmp import generated {} tiles, but tile count must be in range [{}-{}]",
					tilecount, tilerange.first, tilerange.second));
		}

		if (p_coll_id == c::KEY_COLL_NPCS && p_bank_id == m_game->m_sprite_gfx_manager.c_npcs.banks.size() - 1)
			update_sgfx_result_for_common_bank(impres);

		// run bmp import through snapshot manager after validation
		m_sprite_snap_manager.apply_bmp_import(p_coll, p_coll_id, impres, impact);

		add_message(std::format("Created snapshot, and imported {} bmp files", impact.frame_indexes.size()), 2);
	}
	else {
		throw std::runtime_error(
			std::format("bmp import for bank {} not possible; frames referenced by more than one unique chr-bank",
				p_bank_id));
	}
}

void fe::MainWindow::export_sprite_frame_bmps(const fe::SpriteFrameCollection& p_coll,
	std::size_t p_coll_id, std::size_t p_bank_id) {

	const auto impact{ m_game->m_sprite_gfx_manager.analyze_bank_impact(p_coll, p_bank_id) };

	if (impact.frame_indexes.empty()) {
		add_message(std::format("No frames using chr bank {}", p_bank_id), 6);
	}
	else {
		m_gfx.save_sprite_frames_bmp(p_coll,
			p_bank_id,
			impact.frame_indexes,
			m_game->m_palettes.at(m_settings.coll_palettes[p_coll_id]),
			get_bmp_path(),
			get_sprite_gfx_file_prefix(p_coll_id));

		add_message(std::format("Saved {} bmps as {}", impact.frame_indexes.size(),
			m_gfx.get_sprite_frame_bmp_wc_filpath(get_bmp_path(),
				get_sprite_gfx_file_prefix(p_coll_id), p_bank_id)), 2);
	}
}

// when importing common chr bank frames and bank, everything will be relative to 0
// we need to update all frame indexes and the chr bank itself to start at $40
void fe::MainWindow::update_sgfx_result_for_common_bank(fe::SpriteImportResult& import) {
	std::vector<klib::NES_tile> l_bank(256, klib::NES_tile());
	std::vector<fe::SpriteAnimationFrame> l_frames;

	if (import.tiles.size() > c::PPU_COMMON_TILE_COUNT - 1)
		throw std::runtime_error(std::format(
			"Common chr-bank import generated {} tiles, but at most {} are allowed (one slot is reserved for the sprite-0 tile)",
import.tiles.size(), c::PPU_COMMON_TILE_COUNT - 1));

		std::size_t forbidden_chr_idx{ c::SPRITE_0_PPU_IDX_REL_ZERO };
	std::size_t new_7f_index{ import.tiles.size() };
	if (import.tiles.size() > c::SPRITE_0_PPU_IDX_REL_ZERO) {
		// move the generated chr tile to the end
		l_bank[c::PPU_COMMON_TILE_START + new_7f_index] = import.tiles[forbidden_chr_idx];
	}

	for (std::size_t i{ 0 }; i < import.tiles.size(); ++i)
		l_bank.at(c::PPU_COMMON_TILE_START + i) = import.tiles[i];

	// set the zero-hit tile to its necessary location
	l_bank[c::PPU_COMMON_TILE_START + c::SPRITE_0_PPU_IDX_REL_ZERO] =
		m_game->m_sprite_gfx_manager.get_sprite_0_hit_tile();

	for (const auto& frame : import.frames)
		l_frames.push_back(fe::SpriteAnimationFrame(frame, c::PPU_COMMON_TILE_START,
			static_cast<byte>(forbidden_chr_idx), static_cast<byte>(new_7f_index)));
import.tiles = l_bank;
import.frames = l_frames;
}

std::string fe::MainWindow::get_sprite_gfx_file_prefix(std::size_t p_coll_key) const {
	if (p_coll_key == 0)
		return "npc";
	else if (p_coll_key == 1)
		return "player";
	else if (p_coll_key == 2)
		return "portraits";
	else if (p_coll_key == c::CINEMATIC_NUM_ID_GFX_COLL)
		return "cinematic";
	else
		return "unknown";
}

std::string fe::MainWindow::get_sprite_gfx_bank_name(std::size_t p_coll_id, std::size_t p_sel_bank_id,
	std::size_t p_bank_id, std::size_t p_bank_count, std::size_t p_tile_count) const {
	if (p_coll_id == c::KEY_COLL_PORTRAITS) {
		return std::format("Bank #{}/{} - {}: 'Portraits' ({} tiles)", p_sel_bank_id, p_bank_count, p_bank_id, p_tile_count);
	}
	else if (p_coll_id == c::KEY_COLL_PLAYER) {
		std::string bank_name;
		if (p_bank_id == c::KEY_BANK_ARMOR)
			bank_name = "Armor";
		else if (p_bank_id == c::KEY_BANK_WEAPONS)
			bank_name = "Weapons";
		else
			bank_name = "Shields";
		return std::format("Bank #{}/{} - {}: '{}' ({} tiles)", p_sel_bank_id, p_bank_count, p_bank_id, bank_name, p_tile_count);
	}
	else {
		std::string bank_name;
		if (p_bank_id == m_cache.m_sprite_count)
			bank_name = "Common";
		else
			bank_name = get_description(static_cast<byte>(p_bank_id), m_cache.m_labels_sprites);
		return std::format("Bank #{}/{} - {}: '{}' ({} tiles)", p_sel_bank_id, p_bank_count, p_bank_id, bank_name, p_tile_count);
	}
}

// only call this once the sprite data has been populated
void fe::MainWindow::generate_editor_sprite_gfx(SDL_Renderer* p_rnd) {
	fe::SpriteGUILoader gui_sprites;
	gui_sprites.load_sprites_for_gui(m_config, m_game->m_sprite_gfx_manager, m_game->m_rom_data);

	m_cache.m_sprite_dims = gui_sprites.get_animation_dimension_data();

	// pass the resolved frames on to the gfx handler to make sprite textures
	m_gfx.gen_sprites(p_rnd, gui_sprites, m_game->m_palettes.at(m_settings.coll_palettes[0]));
	// force a redraw in the sprite gfx window
	m_settings.m_redraw_sprite_gfx = true;
	// force a redraw in the cinema gfx window
	m_settings.m_redraw_cinema_gfx = true;
}
