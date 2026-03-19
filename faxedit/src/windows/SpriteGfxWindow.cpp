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

using byte = unsigned char;

void fe::MainWindow::draw_sprite_gfx_window(SDL_Renderer* p_rnd) {
	static SpriteGfxEditMode editmode{ fe::SpriteGfxEditMode::Portraits };
	static std::size_t ls_sel_npc{ 0 };

	ui::imgui_screen("Sprite Graphics Editor",
		c::WIN_TILEMAP_X + 50, c::WIN_TILEMAP_Y + 50,
		c::WIN_TILEMAP_W, c::WIN_TILEMAP_H);

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

		ImGui::Separator();

		if (editmode == fe::SpriteGfxEditMode::Settings) {
			static std::size_t ls_sel_sprite;
			auto& lr_spr_first_frames{ m_game->m_sprite_gfx_manager.npc_start_frames };

			ui::imgui_slider_with_arrows("###sprssf",
				get_description(static_cast<byte>(ls_sel_sprite), m_labels_sprites),
				ls_sel_sprite, 0, m_sprite_count - 1, "", false, true);

			ui::imgui_slider_with_arrows("###sprsf", "", lr_spr_first_frames.at(ls_sel_sprite),
				0, m_game->m_sprite_gfx_manager.c_npcs.frames.size() - 1);
		}
		else if (editmode == fe::SpriteGfxEditMode::Portraits)
			show_sprite_gfx_editor(p_rnd, 2, m_game->m_sprite_gfx_manager.c_portraits);
		else if (editmode == fe::SpriteGfxEditMode::Player)
			show_sprite_gfx_editor(p_rnd, 1, m_game->m_sprite_gfx_manager.c_player);
		else if (editmode == fe::SpriteGfxEditMode::NPC) {
			show_sprite_gfx_editor(p_rnd, 0, m_game->m_sprite_gfx_manager.c_npcs);
		}

	}
	catch (const std::exception& ex) {
		add_message(ex.what(), 1);
	}

	ImGui::End();
}

void fe::MainWindow::show_sprite_gfx_editor(SDL_Renderer* p_rnd,
	std::size_t p_coll, fe::SpriteFrameCollection& p_collection) {
	static std::size_t ls_frame{ 0 }, ls_bank{ 0 }, ls_resolved_bank{ 0 };
	static int ls_sel_frame_x{ 0 }, ls_sel_frame_y{ 0 }, ls_sel_bank_x{ 0 }, ls_sel_bank_y{ 0 };
	constexpr float FRAME_GFX_SCALE{ 3.0f };
	constexpr float FRAME_TILE_SIZE{ 8.0f * FRAME_GFX_SCALE };
	constexpr float BANK_GFX_SCALE{ 2.0f };
	constexpr float BANK_TILE_SIZE{ 8.0f * BANK_GFX_SCALE };

	// draw the frame itself
	if (!p_collection.frames.empty()) {

		if (ls_frame >= p_collection.frames.size())
			ls_frame = p_collection.frames.size() - 1;

		ui::imgui_slider_with_arrows("###selframe",
			std::format("Frame #{}/{} ({}x{})", ls_frame, p_collection.frames.size() - 1,
				p_collection.frames.at(ls_frame).frame.w(),
				p_collection.frames.at(ls_frame).frame.h()),
			ls_frame, 0, p_collection.frames.size() - 1, "", false, true);

		// get the chr bank ID - all frames must be associated with at least one bank
		if (ls_bank >= p_collection.frames[ls_frame].chrbanks.size())
			ls_bank = p_collection.frames[ls_frame].chrbanks.size() - 1;
		ls_resolved_bank = p_collection.frames.at(ls_frame).chrbanks.at(ls_bank);

		auto& lr_frame{ p_collection.frames[ls_frame].frame };

		ImGui::SeparatorText("Frame Metadata");

		ui::imgui_slider_with_arrows("###sfofx", "x-offset", lr_frame.offset_x, -128, 127);
		ui::imgui_slider_with_arrows("###sfofy", "y-offset", lr_frame.offset_y, -128, 127);
		ui::imgui_slider_with_arrows("###sfpivx", "x-pivot", lr_frame.pivot_x, -128, 127);

		ImGui::SeparatorText("Rendered Frame");

		// draw the frame texture - we have a valid animation frame and bank index
		auto frametxt{ m_gfx.get_sprite_selected_texture(p_coll, ls_frame, ls_bank) };
		if (frametxt == nullptr) {
			m_gfx.gen_sprite_selected_texture(p_rnd, p_coll, ls_frame, ls_bank,
				p_collection.frames[ls_frame].frame,
				p_collection.banks[ls_resolved_bank],
				m_game->m_palettes.at(p_coll < 2 ? 28 : 30));
			frametxt = m_gfx.get_sprite_selected_texture(p_coll, ls_frame, ls_bank);
		}

		ImGui::Image(frametxt, ImVec2(
			FRAME_GFX_SCALE * static_cast<float>(frametxt->w),
			FRAME_GFX_SCALE * static_cast<float>(frametxt->h)
		));

		// image rect in screen coordinates
		const ImVec2 img_min{ ImGui::GetItemRectMin() };
		const ImVec2 img_max{ ImGui::GetItemRectMax() };

		// handle clicking on the frame
		if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			ImVec2 mouse = ImGui::GetIO().MousePos;

			int tile_x = static_cast<int>((mouse.x - img_min.x) / FRAME_TILE_SIZE);
			int tile_y = static_cast<int>((mouse.y - img_min.y) / FRAME_TILE_SIZE);

			const int tiles_w = frametxt->w / 8;
			const int tiles_h = frametxt->h / 8;

			if (tile_x >= 0 && tile_x < tiles_w && tile_y >= 0 && tile_y < tiles_h) {
				ls_sel_frame_x = tile_x;
				ls_sel_frame_y = tile_y;
			}
		}

		// draw selection rectangle
		{
			const int tiles_w = frametxt->w / 8;
			const int tiles_h = frametxt->h / 8;

			if (ls_sel_frame_x >= 0 && ls_sel_frame_x < tiles_w &&
				ls_sel_frame_y >= 0 && ls_sel_frame_y < tiles_h) {

				ImVec2 rect_min{
					img_min.x + ls_sel_frame_x * FRAME_TILE_SIZE,
					img_min.y + ls_sel_frame_y * FRAME_TILE_SIZE
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
				ui::imgui_slider_with_arrows("###stchrind",
					std::format("chr-tile {}", lr_tile->index),
					lr_tile->index, 0, p_collection.banks.at(ls_resolved_bank).size());

				ui::imgui_slider_with_arrows("###stsp",
					std::format("sub-palette {}", lr_tile->sub_palette),
					lr_tile->sub_palette, 0, 3);

				ui::imgui_checkbox("v-flip", lr_tile->v_flip);
				ImGui::SameLine();
				ui::imgui_checkbox("h-flip", lr_tile->h_flip);

				if (ui::imgui_button("Clear tile", 1))
					lr_tile = std::nullopt;
			}

		}
		else {
			imgui_text("No tile selected");
		}
	}
	else {
		imgui_text("Collection has no animation frames");
		return;
	}

	ImGui::Separator();

	if (ui::imgui_slider_with_arrows("###selbank",
		std::format("chr-bank #{}/{} ({}: {} tiles)", ls_bank, p_collection.frames[ls_frame].chrbanks.size() - 1,
			ls_resolved_bank, p_collection.banks[ls_resolved_bank].size()),
		ls_bank, 0, p_collection.frames[ls_frame].chrbanks.size() - 1, "", false, true))
		ls_resolved_bank = p_collection.frames.at(ls_frame).chrbanks.at(ls_bank);

	auto banktxt{ m_gfx.get_sprite_selected_chr_bank(p_coll, ls_bank) };
	if (banktxt == nullptr) {
		m_gfx.gen_sprite_selected_texture(p_rnd, p_coll, ls_frame, ls_bank,
			p_collection.frames[ls_frame].frame,
			p_collection.banks[ls_resolved_bank],
			m_game->m_palettes.at(p_coll < 2 ? 28 : 30));
		banktxt = m_gfx.get_sprite_selected_texture(p_coll, ls_frame, ls_bank);
	}

	ImGui::Image(banktxt, ImVec2(
		BANK_GFX_SCALE * static_cast<float>(banktxt->w),
		BANK_GFX_SCALE * static_cast<float>(banktxt->h)
	));

	if (ui::imgui_button("Canonicalize", 2)) {
		//m_game->m_sprite_gfx_manager.canonsort_everything();
		m_game->m_sprite_gfx_manager.canonsort_gfx_collection_chr_bank(p_collection, p_collection.frames[ls_frame].chrbanks[ls_bank]);
		m_gfx.clear_sprite_selected_texture();
	}

	if (ui::imgui_button("Export bmps", 2)) {

		const auto impact{ m_game->m_sprite_gfx_manager.analyze_bank_impact(p_collection, ls_resolved_bank) };

		m_gfx.save_sprite_frames_bmp(p_collection,
			ls_resolved_bank,
			impact.frame_indexes,
			m_game->m_palettes.at(p_coll < 2 ? 28 : 30),
			get_bmp_path(),
			get_file_prefix(p_coll));

		add_message(std::format("Saved bmps as {}", m_gfx.get_sprite_frame_bmp_wc_filpath(get_bmp_path(),
			get_file_prefix(p_coll), ls_resolved_bank)), 2);
	}

	ImGui::SameLine();

	if (ui::imgui_button("Import bmps", 2)) {
		const auto impact{ m_game->m_sprite_gfx_manager.analyze_bank_impact(p_collection, ls_resolved_bank) };

		if (impact.banks_identical) {
			const auto impres{ m_gfx.import_sprite_frames_from_folder(get_bmp_path(), get_file_prefix(p_coll),
				ls_resolved_bank, impact.frame_indexes, m_game->m_palettes.at(p_coll < 2 ? 28 : 30),
				255, 3) };

			if (impres.approximated_tile_count > 0)
				throw std::runtime_error("Imported bmps required creating too many chr-tiles");

			p_collection.banks.at(ls_resolved_bank) = impres.tiles;
			for (std::size_t i{ 0 }; i < impres.frames.size(); ++i)
				p_collection.frames.at(impact.frame_indexes.at(i)).frame.tilemap = impres.frames[i].tilemap;

			m_gfx.clear_sprite_selected_texture();
		}
		else {
			throw std::runtime_error("bmp import not possible; frames referenced by more than one unique chr-bank");
		}
	}
}

std::string fe::MainWindow::get_file_prefix(std::size_t p_coll_key) const {
	if (p_coll_key == 0)
		return "npc";
	else if (p_coll_key == 1)
		return "player";
	else if (p_coll_key == 2)
		return "portraits";
	else
		return "unknown";
}
