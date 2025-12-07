#include "MainWindow.h"
#include "gfx.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include "Imgui_helper.h"
#include "./../fe/fe_constants.h"
#include "./../fe/fe_app_constants.h"

using byte = unsigned char;

static std::vector<std::vector<byte>> flat_pal_to_2d_pal(const std::vector<byte>& pal) {
	std::vector<std::vector<byte>> result;

	for (std::size_t j{ 0 }; j < 4; ++j) {
		std::vector<byte> l_subpal;
		for (std::size_t i{ 0 }; i < 4; ++i)
			l_subpal.push_back(pal.at(4 * j + i));
		result.push_back(l_subpal);
	}

	return result;
}

void fe::MainWindow::draw_gfx_window(SDL_Renderer* p_rnd) {

	ui::imgui_screen("Graphics Editor",
		c::WIN_ISCRIPT_X, c::WIN_ISCRIPT_Y,
		c::WIN_ISCRIPT_W, c::WIN_ISCRIPT_H);

	ImGui::SeparatorText("Edit Mode");

	if (ImGui::RadioButton("Tilesets and Metatiles",
		m_gfx_emode == fe::GfxEditMode::WorldChr))
		m_gfx_emode = fe::GfxEditMode::WorldChr;
	ImGui::SameLine();
	if (ImGui::RadioButton("Palettes",
		m_gfx_emode == fe::GfxEditMode::Palettes))
		m_gfx_emode = fe::GfxEditMode::Palettes;

	ImGui::Separator();

	ui::imgui_slider_with_arrows("###tsw",
		std::format("World: {}", m_labels_worlds.at(m_sel_gfx_ts_world)), m_sel_gfx_ts_world, 0, 7);

	if (m_sel_gfx_ts_world == c::CHUNK_IDX_BUILDINGS)
		ui::imgui_slider_with_arrows("###tss",
			m_labels_buildings.at(m_sel_gfx_ts_screen), m_sel_gfx_ts_screen, 0, c::WORLD_BUILDINGS_SCREEN_COUNT - 1);

	std::size_t l_ts_no{ get_default_tileset_no(m_sel_gfx_ts_world, m_sel_gfx_ts_screen) };
	std::size_t l_palette_no{ get_default_palette_no(m_sel_gfx_ts_world, m_sel_gfx_ts_screen) };

	imgui_text(std::format("Tileset {}: {}", l_ts_no, m_labels_tilesets.at(l_ts_no)));
	imgui_text(std::format("Palette {}: {}", l_palette_no, m_labels_palettes.at(static_cast<byte>(l_palette_no))));

	ImGui::Separator();

	std::size_t l_pass_screen{ m_sel_gfx_ts_world == c::CHUNK_IDX_BUILDINGS ?
		m_sel_gfx_ts_screen : 0 };

	auto txt{ m_gfx.get_tileset_txt(m_sel_gfx_ts_world, l_pass_screen) };

	if (txt == nullptr)
		imgui_text("Graphics not yet extracted");
	else {
		ImGui::Image(txt, ImVec2(
			static_cast<float>(2 * txt->w),
			static_cast<float>(2 * txt->h)
		));
	}

	ImGui::Separator();

	if (ui::imgui_button("Extract", 4))
		m_gfx.gen_tileset_txt(p_rnd,
			m_sel_gfx_ts_world,
			l_pass_screen,
			flat_pal_to_2d_pal(m_game->m_palettes.at(l_palette_no)),
			m_game->m_chunks.at(m_sel_gfx_ts_world).m_metatiles,
			m_game->m_tilesets.at(l_ts_no));

	ImGui::SameLine();

	if (ui::imgui_button("Save bmp", 2, "", txt == nullptr)) try {
		m_gfx.save_tileset_bmp(m_sel_gfx_ts_world, l_pass_screen,
			flat_pal_to_2d_pal(m_game->m_palettes.at(l_palette_no)),
			m_game->m_chunks.at(m_sel_gfx_ts_world).m_metatiles,
			m_game->m_tilesets.at(l_ts_no),
			m_path.string() + "/bmp",
			std::format("tileset-{}-{}.bmp", m_sel_gfx_ts_world, l_pass_screen)
			);

		add_message("Tileset/metatile bmp saved", 2);
	}
	catch (const std::runtime_error& ex) {
		add_message(ex.what(), 1);
	}
	catch (const std::exception& ex) {
		add_message(ex.what(), 1);
	}

	if (ui::imgui_button("Load bmp", 4)) try {
		std::set<std::size_t> l_res_idx;
		std::size_t l_tileset_start{ m_tileset_start.at(l_ts_no) };
		std::size_t l_tileset_end{ l_tileset_start + m_tileset_size.at(l_ts_no) };

		for (std::size_t i{ 0 }; i < l_tileset_start; ++i)
			l_res_idx.insert(i);
		for (std::size_t i{ l_tileset_end }; i < 256; ++i)
			l_res_idx.insert(i);

		auto gfxres = m_gfx.import_tileset_bmp(m_game->m_tilesets.at(l_ts_no),
			l_res_idx,
			flat_pal_to_2d_pal(m_game->m_palettes.at(l_palette_no)),
			m_path.string() + "/bmp",
			std::format("tileset-{}-{}.bmp", m_sel_gfx_ts_world, l_pass_screen));

		m_game->m_tilesets.at(l_ts_no) = gfxres.m_tileset;

		auto& mts{ m_game->m_chunks.at(m_sel_gfx_ts_world).m_metatiles };

		// update metatile sub-palette indexes (for all quadrants)
		for (std::size_t i{ 0 }; i < mts.size() && i < gfxres.m_metatile_sub_palette.size();
			++i) {
			mts[i].m_attr_bl = static_cast<byte>(gfxres.m_metatile_sub_palette[i]);
			mts[i].m_attr_br = static_cast<byte>(gfxres.m_metatile_sub_palette[i]);
			mts[i].m_attr_tl = static_cast<byte>(gfxres.m_metatile_sub_palette[i]);
			mts[i].m_attr_tr = static_cast<byte>(gfxres.m_metatile_sub_palette[i]);
		}

		// update all metatile definitions
		for (const auto& kv : gfxres.m_usage) {
			byte l_chr_idx{ static_cast<byte>(gfxres.m_index_map.at(kv.first)) };

			for (std::size_t i{ 0 }; i < kv.second.size(); ++i) {

				std::size_t l_mt_idx{ kv.second[i].first };
				if (l_mt_idx < mts.size()) {
					std::size_t l_quadrant{ kv.second[i].second };

					// update the quadrants
					if (l_quadrant == 0)
						mts[l_mt_idx].m_tilemap.at(0).at(0) = l_chr_idx;
					else if (l_quadrant == 1)
						mts[l_mt_idx].m_tilemap.at(0).at(1) = l_chr_idx;
					else if (l_quadrant == 2)
						mts[l_mt_idx].m_tilemap.at(1).at(0) = l_chr_idx;
					else if (l_quadrant == 3)
						mts[l_mt_idx].m_tilemap.at(1).at(1) = l_chr_idx;

				}
			}
		}

		if (l_ts_no == get_default_tileset_no(m_sel_chunk, m_sel_screen))
			m_atlas_force_update = true;
	}
	catch (const std::runtime_error& ex) {
		add_message(ex.what(), 1);
	}
	catch (const std::exception& ex) {
		add_message(ex.what(), 1);
	}

	ImGui::End();
}
