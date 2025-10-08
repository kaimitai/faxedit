#include "MainWindow.h"
#include "gfx.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"

fe::MainWindow::MainWindow(void) :
	m_sel_chunk{ 0 }, m_sel_screen{ 0 }
{
}

void fe::MainWindow::generate_textures(SDL_Renderer* p_rnd, const fe::Game& p_game) {
	m_gfx.generate_textures(p_rnd, p_game.get_nes_tiles());
}

void fe::MainWindow::draw(SDL_Renderer* p_rnd, const fe::Game& p_game) {
	SDL_SetRenderDrawColor(p_rnd, 126, 126, 255, 0);
	SDL_RenderClear(p_rnd);

	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Main");

	ImGui::SliderInt("Chunk", &m_sel_chunk, 0, 7);

	ImGui::Text("Chunk screen count %d", p_game.get_screen_count(m_sel_chunk));

	ImGui::End();

	ImGui::Render();
	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), p_rnd);
}
