#include "MainWindow.h"
#include "gfx.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"

fe::MainWindow::MainWindow(void) :
	m_sel_chunk{ 0 }, m_sel_screen{ 0 }, m_sel_pal{ 0 }
{
}

void fe::MainWindow::generate_textures(SDL_Renderer* p_rnd, const fe::Game& p_game) {
	for (std::size_t i{ 0 }; i < p_game.get_tileset_count(); ++i)
		m_gfx.generate_textures(p_rnd, p_game.get_tileset(i), p_game.get_palettes().at(
			p_game.get_chunk_default_palette_no(i >= 8 ? 6 : i))
		);
}

std::size_t fe::MainWindow::get_tileset(int p_chunk_no, int p_screen_no) const {
	if (p_chunk_no == 6 && p_screen_no >= 3)
		return p_screen_no >= 8 ? 9 : 8;
	else
		return p_chunk_no;
}

void fe::MainWindow::draw_screen(SDL_Renderer* p_rnd, const fe::Game& p_game) const {
	const auto& l_tilemap{ p_game.get_screen_tilemap(m_sel_chunk, m_sel_screen) };
	const int X_OFFSET{ 300 };
	const int Y_OFFSET{ 20 };

	std::size_t l_tileset{ get_tileset(m_sel_chunk, m_sel_screen) };

	for (int y{ 0 }; y < 13; ++y)
		for (int x{ 0 }; x < 16; ++x) {

			byte mt_no = l_tilemap.at(y).at(x);

			// don't know if this should ever happen
			if (mt_no >= p_game.get_metatile_count(m_sel_chunk))
				mt_no = 0;

			const auto& l_metatile{ p_game.get_metatile(m_sel_chunk, mt_no) };
			const auto& l_mt_tilemap{ l_metatile.get_tilemap() };
			byte l_pal_no{ l_metatile.get_palette_attribute(x, y) };

			//l_pal_no = m_sel_pal;

			//if (l_pal_no != 0)
			//	throw std::exception("wtf");

			m_gfx.blit(p_rnd, m_gfx.get_texture(l_tileset, l_mt_tilemap.at(0).at(0), l_pal_no), X_OFFSET + 16 * x, Y_OFFSET + 16 * y);
			m_gfx.blit(p_rnd, m_gfx.get_texture(l_tileset, l_mt_tilemap.at(0).at(1), l_pal_no), X_OFFSET + 16 * x + 8, Y_OFFSET + 16 * y);
			m_gfx.blit(p_rnd, m_gfx.get_texture(l_tileset, l_mt_tilemap.at(1).at(0), l_pal_no), X_OFFSET + 16 * x, Y_OFFSET + 16 * y + 8);
			m_gfx.blit(p_rnd, m_gfx.get_texture(l_tileset, l_mt_tilemap.at(1).at(1), l_pal_no), X_OFFSET + 16 * x + 8, Y_OFFSET + 16 * y + 8);
		}
}

void fe::MainWindow::draw(SDL_Renderer* p_rnd, const fe::Game& p_game) {
	SDL_SetRenderDrawColor(p_rnd, 126, 126, 255, 0);
	SDL_RenderClear(p_rnd);

	draw_screen(p_rnd, p_game);

	std::size_t l_tileset{ get_tileset(m_sel_chunk, m_sel_screen) };

	for (std::size_t y{ 0 }; y < 16; ++y) {
		for (std::size_t x{ 0 }; x < 16; ++x) {
		//	m_gfx.blit(p_rnd, m_gfx.get_texture(l_tileset, 16 * y + x, 0), 8 * x, 8 * y);
		}
	}

	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Main");

	ImGui::SliderInt("Chunk", &m_sel_chunk, 0, 7);

	ImGui::Text("Chunk screen count %d", p_game.get_screen_count(m_sel_chunk));

	std::size_t l_sc_count{ p_game.get_screen_count(m_sel_chunk) };

	if (m_sel_screen >= l_sc_count)
		m_sel_screen = l_sc_count - 1;

	ImGui::SliderInt("Screen", &m_sel_screen, 0, l_sc_count - 1);

	ImGui::SliderInt("Palette", &m_sel_pal, 0, 3);

	ImGui::End();

	ImGui::Render();
	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), p_rnd);
}
