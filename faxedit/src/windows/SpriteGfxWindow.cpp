#include "MainWindow.h"
#include "gfx.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include "Imgui_helper.h"
#include "./../fe/fe_constants.h"
#include "./../fe/fe_app_constants.h"
#include "./../common/klib/Kfile.h"

using byte = unsigned char;

void fe::MainWindow::draw_sprite_gfx_window(SDL_Renderer* p_rnd) {
	static SpriteGfxEditMode editmode{ fe::SpriteGfxEditMode::Portraits };

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

		ImGui::Separator();

		if (editmode == fe::SpriteGfxEditMode::Portraits)
			show_gfx_collection_editor(p_rnd, 1000, m_game->m_sprite_gfx_manager.portraits,
				30);
	}
	catch (const std::exception& ex) {
		add_message(ex.what(), 1);
	}

	ImGui::End();
}

void fe::MainWindow::show_gfx_collection_editor(SDL_Renderer* p_rnd,
	std::size_t p_gfx_key, fe::SpriteGfxCollection& coll, std::size_t p_palette_no) {
	static std::unordered_map<std::size_t, std::size_t> ls_selected_frame_map;
	static std::size_t ls_selected_frame{ 0 };
	const auto& lr_palette{ m_game->m_palettes.at(p_palette_no) };

	auto iter{ ls_selected_frame_map.find(p_gfx_key) };
	if (iter == end(ls_selected_frame_map))
		ls_selected_frame_map.insert(std::make_pair(p_gfx_key, ls_selected_frame));


	ui::imgui_slider_with_arrows("###fsel", std::format("Frame #{}/{}", ls_selected_frame, coll.frames.size()),
		ls_selected_frame, 0, coll.frames.size() - 1, "", false, true);

	auto& frame{ coll.frames.at(ls_selected_frame) };

	ImGui::SeparatorText("Frame Metadata");

	ui::imgui_slider_with_arrows("###frxo", "x-offset", frame.offset_x, -128, 127);
	ui::imgui_slider_with_arrows("###fryo", "y-offset", frame.offset_y, -128, 127);
	ui::imgui_slider_with_arrows("###frxp", "x-pivot", frame.pivot_x, -128, 127);

	ImGui::SeparatorText("Frame Gfx");

	auto frametxt{ m_gfx.get_gfx_coll_frame_txt(p_gfx_key, ls_selected_frame) };
	if (frametxt == nullptr)
		m_gfx.gen_gfx_collection_textures(p_rnd, p_gfx_key, coll, lr_palette);
	else
		ImGui::Image(frametxt, ImVec2(
			static_cast<float>(2 * frametxt->w),
			static_cast<float>(2 * frametxt->h)
		));

	ImGui::Separator();

	if (ui::imgui_button("Save bmps", 2, "Save all frames as bmp files")) {
		std::string file_prefix{ get_file_prefix(p_gfx_key) };
		std::string file_path{ get_bmp_path() };
		m_gfx.save_sprite_frames_bmp(coll, lr_palette, file_path, file_prefix);
		add_message(
			std::format("Saved {} bmps as {}/{}-*.bmp", coll.frames.size(), file_path, file_prefix),
			2);
	}

	ImGui::SameLine();

	if (ui::imgui_button("Load bmps", 4)) {
		std::string file_prefix{ get_file_prefix(p_gfx_key) };
		std::string file_path{ get_bmp_path() };

		auto importres{ m_gfx.import_sprite_frames_from_folder(file_path, file_prefix, lr_palette, 255, 3) };

		if (importres.collection.frames.size() == 0)
			throw std::runtime_error(std::format("No matching files ({}/{}-*.bmp) found", file_path, file_prefix));

		m_game->m_sprite_gfx_manager.merge_portrait_collection(importres.collection);
		m_gfx.gen_gfx_collection_textures(p_rnd, p_gfx_key, coll, lr_palette);

		add_message(std::format("Imported {}/{}-*.bmp to {} frames ({} tiles approximated)",
			file_path, file_prefix,
			importres.collection.frames.size(), importres.approximated_tile_count),
			importres.approximated_tile_count == 0 ? 2 : 1);
	}
}

std::string fe::MainWindow::get_file_prefix(std::size_t p_gfx_key) const {
	if (p_gfx_key == 1000)
		return "portraits";
	else
		throw std::runtime_error(
			std::format("Could not deduce file prefix from sprite collection key {}", p_gfx_key)
		);
}
