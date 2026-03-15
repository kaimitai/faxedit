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

		if (ImGui::RadioButton("Settings",
			editmode == fe::SpriteGfxEditMode::Settings))
			editmode = fe::SpriteGfxEditMode::Settings;
		ImGui::SameLine();
		if (ImGui::RadioButton("Portraits",
			editmode == fe::SpriteGfxEditMode::Portraits))
			editmode = fe::SpriteGfxEditMode::Portraits;
		ImGui::SameLine();
		if (ImGui::RadioButton("Player",
			editmode == fe::SpriteGfxEditMode::Player))
			editmode = fe::SpriteGfxEditMode::Player;
		ImGui::SameLine();
		if (ImGui::RadioButton("NPCs++",
			editmode == fe::SpriteGfxEditMode::NPC))
			editmode = fe::SpriteGfxEditMode::NPC;

		ImGui::Separator();

		if (editmode == fe::SpriteGfxEditMode::Portraits)
			/*
			show_gfx_collection_editor(p_rnd, 1000, m_game->m_sprite_gfx_manager.portraits,
				30);
				*/
			show_sprite_gfx_editor(p_rnd, 2, m_game->m_sprite_gfx_manager.c_portraits);
		else if (editmode == fe::SpriteGfxEditMode::Player)
			/*
			show_gfx_collection_editor(p_rnd, 2000, m_game->m_sprite_gfx_manager.player,
				28);
				*/
			show_sprite_gfx_editor(p_rnd, 1, m_game->m_sprite_gfx_manager.c_player);
		else if (editmode == fe::SpriteGfxEditMode::NPC) {
			/*
			ui::imgui_slider_with_arrows("###selnpc",
				get_description(static_cast<byte>(ls_sel_npc), m_labels_sprites),
				ls_sel_npc,
				0, m_sprite_count - 1, "",
				false, true);

			show_gfx_collection_editor(p_rnd, ls_sel_npc, m_game->m_sprite_gfx_manager.npcs.at(ls_sel_npc),
				28);
				*/
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
	static std::size_t ls_frame{ 0 }, ls_bank{ 0 };

	if (ls_frame >= p_collection.frames.size())
		ls_frame = p_collection.frames.size() - 1;

	ui::imgui_slider_with_arrows("###selframe", "Frame", ls_frame, 0, p_collection.frames.size() - 1);

	if (p_collection.frames[ls_frame].chrbanks.empty()) {
		imgui_text("Frame not associated with any chr-bank");
		return;
	}
	else if (ls_bank >= p_collection.frames[ls_frame].chrbanks.size())
		ls_bank = p_collection.frames[ls_frame].chrbanks.size() - 1;

	ui::imgui_slider_with_arrows("###selbank", std::format("chr-bank {}", p_collection.frames[ls_frame].chrbanks[ls_bank]),
		ls_bank, 0, p_collection.frames[ls_frame].chrbanks.size() - 1);

	auto frametxt{ m_gfx.get_sprite_selected_texture(p_coll, ls_frame, ls_bank) };
	if (frametxt == nullptr) {
		m_gfx.gen_sprite_selected_texture(p_rnd, p_coll, ls_frame, ls_bank,
			p_collection.frames[ls_frame].frame,
			p_collection.banks[p_collection.frames[ls_frame].chrbanks[ls_bank]],
			m_game->m_palettes.at(p_coll < 2 ? 28 : 30));
		frametxt = m_gfx.get_sprite_selected_texture(p_coll, ls_frame, ls_bank);
	}

	ImGui::Image(frametxt, ImVec2(
		static_cast<float>(4 * frametxt->w),
		static_cast<float>(4 * frametxt->h)
	));

	ImGui::SeparatorText(std::format("chr-bank {} ({} tiles)",
		p_collection.frames[ls_frame].chrbanks[ls_bank],
		p_collection.banks[p_collection.frames[ls_frame].chrbanks[ls_bank]].size()
	).c_str());

	auto banktxt{ m_gfx.get_sprite_selected_chr_bank(p_coll, ls_bank) };
	if (banktxt == nullptr) {
		m_gfx.gen_sprite_selected_texture(p_rnd, p_coll, ls_frame, ls_bank,
			p_collection.frames[ls_frame].frame,
			p_collection.banks[p_collection.frames[ls_frame].chrbanks[ls_bank]],
			m_game->m_palettes.at(p_coll < 2 ? 28 : 30));
		banktxt = m_gfx.get_sprite_selected_texture(p_coll, ls_frame, ls_bank);
	}

	ImGui::Image(banktxt, ImVec2(
		static_cast<float>(4 * banktxt->w),
		static_cast<float>(4 * banktxt->h)
	));

	if (ui::imgui_button("Canonicalize", 2)) {
		//m_game->m_sprite_gfx_manager.canonsort_everything();
		m_game->m_sprite_gfx_manager.canonsort_gfx_collection_chr_bank(p_collection, p_collection.frames[ls_frame].chrbanks[ls_bank]);
		m_gfx.clear_sprite_selected_texture();
	}

	if (ui::imgui_button("Export bmps", 2)) {
		std::size_t resolved_bank_idx{ p_collection.frames.at(ls_frame).chrbanks.at(ls_bank) };
		const auto impact{ m_game->m_sprite_gfx_manager.analyze_bank_impact(p_collection, resolved_bank_idx) };

		m_gfx.save_sprite_frames_bmp(p_collection,
			resolved_bank_idx,
			impact.frame_indexes,
			m_game->m_palettes.at(p_coll < 2 ? 28 : 30),
			get_bmp_path(),
			get_file_prefix(p_coll));

		add_message(std::format("Saved bmps as {}", m_gfx.get_sprite_frame_bmp_wc_filpath(get_bmp_path(),
			get_file_prefix(p_coll), resolved_bank_idx)), 2);
	}

	ImGui::SameLine();

	if (ui::imgui_button("Import bmps", 2)) {
		std::size_t resolved_bank_idx{ p_collection.frames.at(ls_frame).chrbanks.at(ls_bank) };
		const auto impact{ m_game->m_sprite_gfx_manager.analyze_bank_impact(p_collection, resolved_bank_idx) };

		if (impact.banks_identical) {
			const auto impres{ m_gfx.import_sprite_frames_from_folder(get_bmp_path(), get_file_prefix(p_coll),
				resolved_bank_idx, impact.frame_indexes, m_game->m_palettes.at(p_coll < 2 ? 28 : 30),
				255, 3) };

			if (impres.approximated_tile_count > 0)
				throw std::runtime_error("Imported bmps required creating too many chr-tiles");

			p_collection.banks.at(resolved_bank_idx) = impres.tiles;
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
