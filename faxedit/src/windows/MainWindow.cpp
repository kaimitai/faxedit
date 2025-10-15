#include "MainWindow.h"
#include "gfx.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include <map>
#include "./../common/klib/Bitreader.h"
#include "Imgui_helper.h"
#include "./../fe/fe_constants.h"
#include "./../fe/xml/Xml_helper.h"

fe::MainWindow::MainWindow(SDL_Renderer* p_rnd) :
	m_sel_chunk{ 0 }, m_sel_screen{ 0 },
	m_gfx{ fe::gfx(p_rnd) },
	m_atlas_palette_no{ 1 },
	m_atlas_tileset_no{ 1 },
	m_atlas_new_tileset_no{ 0 },
	m_atlas_new_palette_no{ 0 }
{
}

void fe::MainWindow::generate_textures(SDL_Renderer* p_rnd, const fe::Game& p_game) {

	// ensure the atlas will be generated
	m_atlas_new_tileset_no = get_default_tileset_no(0, 0);
	m_atlas_new_palette_no = get_default_palette_no(p_game, 0, 0);

	// TODO: generate sprite textures
}

void fe::MainWindow::draw_tilemap_window(SDL_Renderer* p_rnd, const fe::Game& p_game,
	int& hoverMX, int& hoverMY, bool& clicked) const {

	const auto& l_chunk{ p_game.m_chunks.at(m_sel_chunk) };
	const auto& l_screen{ p_game.m_chunks.at(m_sel_chunk).m_screens.at(m_sel_screen) };
	const auto& l_tilemap{ l_screen.m_tilemap };
	std::size_t l_tileset{ get_default_tileset_no(m_sel_chunk, m_sel_screen) };

	// draw the screen
	for (int y{ 0 }; y < 13; ++y)
		for (int x{ 0 }; x < 16; ++x) {

			byte mt_no = l_tilemap.at(y).at(x);

			// don't know if this should ever happen
			if (mt_no >= p_game.m_chunks.at(m_sel_chunk).m_metatiles.size())
				mt_no = 0;

			const auto& l_metatile{ p_game.m_chunks.at(m_sel_chunk).m_metatiles.at(mt_no) };
			const auto& l_mt_tilemap{ l_metatile.m_tilemap };
			byte l_pal_no{ l_metatile.get_palette_attribute(x, y) };

			m_gfx.blit_to_screen(p_rnd, l_mt_tilemap.at(0).at(0), l_pal_no, 2 * x, 2 * y);
			m_gfx.blit_to_screen(p_rnd, l_mt_tilemap.at(0).at(1), l_pal_no, 2 * x + 1, 2 * y);
			m_gfx.blit_to_screen(p_rnd, l_mt_tilemap.at(1).at(0), l_pal_no, 2 * x, 2 * y + 1);
			m_gfx.blit_to_screen(p_rnd, l_mt_tilemap.at(1).at(1), l_pal_no, 2 * x + 1, 2 * y + 1);
		}

	// draw placeholder rectangles for sprites
	for (std::size_t s{ 0 }; s < l_screen.m_sprites.size(); ++s) {
		const auto& l_sprite{ l_screen.m_sprites[s] };

		m_gfx.draw_rect_on_screen(p_rnd,
			SDL_Color(l_sprite.m_text_id.has_value() ? 255 : 0,
				120, 120, 255),
			l_sprite.m_x, l_sprite.m_y,
			1, 1
		);

	}

	ImGui::Begin("Screen##tm");

	SDL_Texture* l_screen_txt{ m_gfx.get_screen_texture() };

	// Get how much space the ImGui window gives us
	ImVec2 avail = ImGui::GetContentRegionAvail();

	// Compute how to fit the texture inside while preserving aspect ratio
	float texAspect = float(l_screen_txt->w) / float(l_screen_txt->h);
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

	dl->AddImage((ImTextureID)l_screen_txt, pMin, pMax);

	hoverMX = hoverMY = -1;
	clicked = false;

	ImVec2 mousePos = ImGui::GetIO().MousePos;
	if (mousePos.x >= pMin.x && mousePos.y >= pMin.y &&
		mousePos.x < pMax.x && mousePos.y < pMax.y)
	{
		float relX = (mousePos.x - pMin.x) / (pMax.x - pMin.x);
		float relY = (mousePos.y - pMin.y) / (pMax.y - pMin.y);
		float pxX = relX * l_screen_txt->w;
		float pxY = relY * l_screen_txt->h;

		int mtX = int(pxX) / (8 * 2);
		int mtY = int(pxY) / (8 * 2);
		if (mtX >= 0 && mtX < 16 && mtY >= 0 && mtY < 16) {
			hoverMX = mtX;
			hoverMY = mtY;

			// highlight hovered metatile
			float hx0 = pMin.x + (float(mtX * 8 * 2) / l_screen_txt->w) * drawSize.x;
			float hy0 = pMin.y + (float(mtY * 8 * 2) / l_screen_txt->h) * drawSize.y;
			float hx1 = pMin.x + (float((mtX + 1) * 8 * 2) / l_screen_txt->w) * drawSize.x;
			float hy1 = pMin.y + (float((mtY + 1) * 8 * 2) / l_screen_txt->h) * drawSize.y;

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

void fe::MainWindow::draw(SDL_Renderer* p_rnd, fe::Game& p_game) {
	SDL_SetRenderDrawColor(p_rnd, 126, 126, 255, 0);
	SDL_RenderClear(p_rnd);

	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	ui::imgui_screen("Game###gw");

	if (ImGui::Button("Save XML"))
		xml::save_xml("c:/temp/out.xml", p_game);

	ImGui::End();

	int l_hover_x, l_hover_y;
	bool l_clicked;

	regenerate_atlas_if_needed(p_rnd, p_game);
	draw_tilemap_window(p_rnd, p_game, l_hover_x, l_hover_y, l_clicked);

	if (l_hover_x >= 0 && l_hover_y >= 0)
		draw_metatile_info(p_game, m_sel_chunk, m_sel_screen,
			static_cast<std::size_t>(l_hover_x), static_cast<std::size_t>(l_hover_y));

	draw_chunk_window(p_rnd, p_game);
	draw_screen_window(p_rnd, p_game);

	ImGui::Render();
	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), p_rnd);
}

void fe::MainWindow::imgui_text(const std::string& p_str) {
	ImGui::Text(p_str.c_str());
}

void fe::MainWindow::draw_metatile_info(const fe::Game& p_game,
	std::size_t p_sel_chunk, std::size_t p_sel_screen,
	std::size_t p_sel_x, std::size_t p_sel_y) {

	byte l_metatile_id{ p_game.m_chunks.at(m_sel_chunk).m_screens.at(m_sel_screen).m_tilemap.at(p_sel_y).at(p_sel_x) };

	ImGui::Begin("Metatile##mt");

	imgui_text("Metatile ID: " +
		klib::Bitreader::byte_to_hex(l_metatile_id));

	imgui_text("Position: " + std::to_string(p_sel_x) + "," + std::to_string(p_sel_y));

	imgui_text("Property: " + klib::Bitreader::byte_to_hex(p_game.m_chunks.at(m_sel_chunk).m_block_properties.at(l_metatile_id)));

	ImGui::End();

}

void fe::MainWindow::regenerate_atlas_if_needed(SDL_Renderer* p_rnd,
	const fe::Game& p_game) {

	if (m_atlas_new_tileset_no != m_atlas_tileset_no ||
		m_atlas_new_palette_no != m_atlas_palette_no) {
		m_gfx.generate_atlas(p_rnd, p_game.m_tilesets.at(m_atlas_new_tileset_no),
			p_game.m_palettes.at(m_atlas_new_palette_no));

		m_atlas_tileset_no = m_atlas_new_tileset_no;
		m_atlas_palette_no = m_atlas_new_palette_no;
	}

}

std::size_t fe::MainWindow::get_default_tileset_no(std::size_t p_chunk_no, std::size_t p_screen_no) const {
	if (p_chunk_no == 6 && p_screen_no >= 3)
		return p_screen_no >= 8 ? 9 : 8;
	else
		return p_chunk_no;
}

std::size_t fe::MainWindow::get_default_palette_no(const fe::Game& p_game,
	std::size_t p_chunk_no, std::size_t p_screen_no) const {

	if (p_chunk_no == 6)
		return p_screen_no + 17;
	else
		return p_game.m_chunks.at(p_chunk_no).m_default_palette_no;
}

std::string fe::MainWindow::get_description(byte p_index,
	const std::map<byte, std::string>& p_map) const {
	const auto& iter{ p_map.find(p_index) };

	if (iter == end(p_map))
		return std::format("Unknown ({})", klib::Bitreader::byte_to_hex(p_index));
	else
		return std::format("{} ({})", iter->second, klib::Bitreader::byte_to_hex(p_index));
}

std::string fe::MainWindow::get_description(byte p_index,
	const std::vector<std::string>& p_vec) const {
	if (p_index < p_vec.size())
		return std::format("{} ({})", p_vec[p_index], klib::Bitreader::byte_to_hex(p_index));
	else
		return std::format("Unknown ({})", klib::Bitreader::byte_to_hex(p_index));
}
