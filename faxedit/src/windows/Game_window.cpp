#include "MainWindow.h"
#include "Imgui_helper.h"
#include "./../common/klib/Bitreader.h"
#include "./../fe/fe_constants.h"

void fe::MainWindow::draw_game_window(SDL_Renderer* p_rnd, fe::Game& p_game) {

	fe::ui::imgui_screen("Game Settings");

	if (fe::ui::imgui_slider_with_arrows("##ws", std::format("Selected World: {} (#{} of {})",
		c::LABELS_CHUNKS.at(m_sel_chunk),
		m_sel_chunk, p_game.m_chunks.size()),
		m_sel_chunk, static_cast<std::size_t>(0), p_game.m_chunks.size() - 1)) {
		m_sel_screen = 0;
		m_atlas_new_tileset_no = get_default_tileset_no(m_sel_chunk, m_sel_screen);
		m_atlas_new_palette_no = get_default_palette_no(p_game, m_sel_chunk, m_sel_screen);
	}

	ImGui::End();
}
