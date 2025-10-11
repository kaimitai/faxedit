#include "MainWindow.h"
#include "gfx.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include <map>
#include "./../common/klib/Bitreader.h"

fe::MainWindow::MainWindow(SDL_Renderer* p_rnd) :
	m_sel_chunk{ 0 }, m_sel_screen{ 0 },
	m_screen_txt{
		SDL_CreateTexture(p_rnd, SDL_PIXELFORMAT_ABGR8888,
		SDL_TEXTUREACCESS_TARGET, 16 * 16, 13 * 16) }
{

	SDL_SetTextureScaleMode(m_screen_txt, SDL_SCALEMODE_NEAREST);

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

void fe::MainWindow::draw_screen_window(SDL_Renderer* p_rnd, const fe::Game& p_game,
	int& hoverMX, int& hoverMY, bool& clicked) const {

	// draw the screen to texture

	SDL_SetRenderTarget(p_rnd, m_screen_txt);

	const auto& l_tilemap{ p_game.get_screen_tilemap(m_sel_chunk, m_sel_screen) };
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

			m_gfx.blit(p_rnd, m_gfx.get_texture(l_tileset, l_mt_tilemap.at(0).at(0), l_pal_no), 16 * x, 16 * y);
			m_gfx.blit(p_rnd, m_gfx.get_texture(l_tileset, l_mt_tilemap.at(0).at(1), l_pal_no), 16 * x + 8, 16 * y);
			m_gfx.blit(p_rnd, m_gfx.get_texture(l_tileset, l_mt_tilemap.at(1).at(0), l_pal_no), 16 * x, 16 * y + 8);
			m_gfx.blit(p_rnd, m_gfx.get_texture(l_tileset, l_mt_tilemap.at(1).at(1), l_pal_no), 16 * x + 8, 16 * y + 8);
		}

	// draw placeholder rectangles for sprites
	for (std::size_t s{ 0 }; s < p_game.get_screen_sprite_count(m_sel_chunk, m_sel_screen); ++s) {
		SDL_SetRenderDrawColor(p_rnd,
			p_game.has_screen_sprite_text(m_sel_chunk, m_sel_screen, s) ? 255 : 0,
			120, 120, 255);

		auto l_x{ p_game.get_screen_sprite_x(m_sel_chunk, m_sel_screen, s) };
		auto l_y{ p_game.get_screen_sprite_y(m_sel_chunk, m_sel_screen, s) };

		SDL_FRect l_rect(16 * l_x, 16 * l_y, 16, 16);

		SDL_RenderRect(p_rnd, &l_rect);
	}

	SDL_SetRenderTarget(p_rnd, nullptr);

	ImGui::Begin("Screen");

	// Get how much space the ImGui window gives us
	ImVec2 avail = ImGui::GetContentRegionAvail();

	// Compute how to fit the texture inside while preserving aspect ratio
	float texAspect = float(m_screen_txt->w) / float(m_screen_txt->h);
	float winAspect = avail.x / avail.y;

	ImVec2 drawSize;
	if (winAspect > texAspect) {
		// Window is wider than texture: fit height
		drawSize.y = avail.y;
		drawSize.x = avail.y * texAspect;
	}
	else {
		// Window is taller than texture: fit width
		drawSize.x = avail.x;
		drawSize.y = avail.x / texAspect;
	}

	ImVec2 cursorPos = ImGui::GetCursorScreenPos();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 pMin = cursorPos;
	ImVec2 pMax = ImVec2(pMin.x + drawSize.x, pMin.y + drawSize.y);

	dl->AddImage((ImTextureID)m_screen_txt, pMin, pMax);

	hoverMX = hoverMY = -1;
	clicked = false;

	ImVec2 mousePos = ImGui::GetIO().MousePos;
	if (mousePos.x >= pMin.x && mousePos.y >= pMin.y &&
		mousePos.x < pMax.x && mousePos.y < pMax.y)
	{
		float relX = (mousePos.x - pMin.x) / (pMax.x - pMin.x);
		float relY = (mousePos.y - pMin.y) / (pMax.y - pMin.y);
		float pxX = relX * m_screen_txt->w;
		float pxY = relY * m_screen_txt->h;

		int mtX = int(pxX) / (8 * 2);
		int mtY = int(pxY) / (8 * 2);
		if (mtX >= 0 && mtX < 16 && mtY >= 0 && mtY < 16) {
			hoverMX = mtX;
			hoverMY = mtY;

			// highlight hovered metatile
			float hx0 = pMin.x + (float(mtX * 8 * 2) / m_screen_txt->w) * drawSize.x;
			float hy0 = pMin.y + (float(mtY * 8 * 2) / m_screen_txt->h) * drawSize.y;
			float hx1 = pMin.x + (float((mtX + 1) * 8 * 2) / m_screen_txt->w) * drawSize.x;
			float hy1 = pMin.y + (float((mtY + 1) * 8 * 2) / m_screen_txt->h) * drawSize.y;

			dl->AddRect(ImVec2(hx0, hy0), ImVec2(hx1, hy1),
				IM_COL32(255, 255, 0, 180), 0.0f, 0, 2.0f);

			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				clicked = true;
		}
	}

	// Keep aspect ratio centered if smaller than window

	float padX = (avail.x - drawSize.x) * 0.5f;
	float padY = (avail.y - drawSize.y) * 0.5f;
	ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + padX, cursorPos.y + padY));
	ImGui::Dummy(drawSize);  // reserve layout space

	ImGui::End();
}

void fe::MainWindow::draw(SDL_Renderer* p_rnd, const fe::Game& p_game) {
	SDL_SetRenderDrawColor(p_rnd, 126, 126, 255, 0);
	SDL_RenderClear(p_rnd);

	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	int l_hover_x, l_hover_y;
	bool l_clicked;

	draw_screen_window(p_rnd, p_game, l_hover_x, l_hover_y, l_clicked);

	if (l_hover_x >= 0 && l_hover_y >= 0)
		draw_metatile_info(p_game, m_sel_chunk, m_sel_screen,
			static_cast<std::size_t>(l_hover_x), static_cast<std::size_t>(l_hover_y));

	ImGui::Begin("Main");

	ImGui::Image(m_screen_txt, { 16 * 16, 13 * 16 });

	ImGui::SliderInt("Chunk", &m_sel_chunk, 0, 7);

	ImGui::Text("Chunk screen count %d", p_game.get_screen_count(m_sel_chunk));

	std::size_t l_sc_count{ p_game.get_screen_count(m_sel_chunk) };

	if (m_sel_screen >= l_sc_count)
		m_sel_screen = l_sc_count - 1;

	ImGui::SliderInt("Screen", &m_sel_screen, 0, l_sc_count - 1);

	if (ImGui::Button("Check")) {
		std::string l_output;
		std::map<byte, int> l_counts;

		for (std::size_t i{ 0 }; i < p_game.get_chunk_count(); ++i) {

			for (std::size_t j{ 0 }; j < p_game.get_screen_count(i); ++j) {
				for (std::size_t s{ 0 }; s < p_game.get_screen_sprite_count(i, j); ++s) {
					byte l_id{ p_game.get_screen_sprite_id(i, j, s) };

					if (p_game.has_screen_sprite_text(i, j, s)) {
						byte l_text_id{ p_game.get_screen_sprite_text(i, j, s) };
						if (l_text_id == 0x25)
							++l_counts[l_text_id];
						else
							++l_counts[l_text_id];
					}
				}
			}

		}



		for (const auto& kv : l_counts) {
			l_output += klib::Bitreader::byte_to_hex(kv.first) + ": "
				+ std::to_string(kv.second) + "\n";
		}

	}

	ImGui::Separator();

	if (ImGui::Button("Left") && p_game.has_screen_exit_left(m_sel_chunk, m_sel_screen))
		m_sel_screen = p_game.get_screen_exit_left(m_sel_chunk, m_sel_screen);
	if (ImGui::Button("Right") && p_game.has_screen_exit_right(m_sel_chunk, m_sel_screen))
		m_sel_screen = p_game.get_screen_exit_right(m_sel_chunk, m_sel_screen);
	if (ImGui::Button("Up") && p_game.has_screen_exit_up(m_sel_chunk, m_sel_screen))
		m_sel_screen = p_game.get_screen_exit_up(m_sel_chunk, m_sel_screen);
	if (ImGui::Button("Down") && p_game.has_screen_exit_down(m_sel_chunk, m_sel_screen))
		m_sel_screen = p_game.get_screen_exit_down(m_sel_chunk, m_sel_screen);


	std::size_t l_sprite_count{ p_game.get_screen_sprite_count(m_sel_chunk, m_sel_screen) };

	imgui_text("Screen sprite count: " + std::to_string(l_sprite_count));

	for (std::size_t i{ 0 }; i < l_sprite_count; ++i) {
		byte l_id{ p_game.get_screen_sprite_id(m_sel_chunk, m_sel_screen, i) };
		byte l_x{ p_game.get_screen_sprite_x(m_sel_chunk, m_sel_screen, i) };
		byte l_y{ p_game.get_screen_sprite_y(m_sel_chunk, m_sel_screen, i) };
		std::string l_text{ p_game.has_screen_sprite_text(m_sel_chunk, m_sel_screen, i) ?
			std::to_string(p_game.get_screen_sprite_text(m_sel_chunk, m_sel_screen, i)) :
			"None"
		};

		imgui_text("Sprite #" + std::to_string(i) +
			" [ID=" + klib::Bitreader::byte_to_hex(l_id) +
			"] - coords=("
			+ std::to_string(l_x) + "," + std::to_string(l_y) + "), Text: " + l_text);
	}

	ImGui::End();

	ImGui::Render();
	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), p_rnd);
}

void fe::MainWindow::imgui_text(const std::string& p_str) {
	ImGui::Text(p_str.c_str());
}

void fe::MainWindow::draw_metatile_info(const fe::Game& p_game,
	std::size_t p_sel_chunk, std::size_t p_sel_screen,
	std::size_t p_sel_x, std::size_t p_sel_y) {

	byte l_metatile_id{ p_game.get_screen_tilemap(p_sel_chunk, p_sel_screen).at(p_sel_y).at(p_sel_x) };

	ImGui::Begin("Metatile");

	imgui_text("Metatile ID: " +
		klib::Bitreader::byte_to_hex(l_metatile_id));

	imgui_text("Position: " + std::to_string(p_sel_x) + "," + std::to_string(p_sel_y));

	imgui_text("Property: " + klib::Bitreader::byte_to_hex(p_game.get_metatile_property(p_sel_chunk, l_metatile_id)));

	ImGui::End();

}
