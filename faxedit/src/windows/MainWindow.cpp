#include "MainWindow.h"
#include "gfx.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include <map>
#include "./../common/klib/Bitreader.h"

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

void fe::MainWindow::draw_screen_window(SDL_Renderer* p_rnd, const fe::Game& p_game,
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

	ImGui::Begin("Screen");

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

void fe::MainWindow::draw(SDL_Renderer* p_rnd, const fe::Game& p_game) {
	SDL_SetRenderDrawColor(p_rnd, 126, 126, 255, 0);
	SDL_RenderClear(p_rnd);

	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	int l_hover_x, l_hover_y;
	bool l_clicked;

	regenerate_atlas_if_needed(p_rnd, p_game);
	draw_screen_window(p_rnd, p_game, l_hover_x, l_hover_y, l_clicked);

	if (l_hover_x >= 0 && l_hover_y >= 0)
		draw_metatile_info(p_game, m_sel_chunk, m_sel_screen,
			static_cast<std::size_t>(l_hover_x), static_cast<std::size_t>(l_hover_y));

	ImGui::Begin("Main");

	ImGui::Image(m_gfx.get_screen_texture(), { 16 * 16, 13 * 16 });

	if (ImGui::SliderInt("Chunk", &m_sel_chunk, 0, 7)) {
		m_sel_screen = 0;
		m_atlas_new_tileset_no = get_default_tileset_no(m_sel_chunk, m_sel_screen);
		m_atlas_new_palette_no = get_default_palette_no(p_game, m_sel_chunk, m_sel_screen);
	}

	std::size_t l_sc_count{ p_game.m_chunks.at(m_sel_chunk).m_screens.size() };

	ImGui::Text("Chunk screen count %d", l_sc_count);

	if (m_sel_screen >= l_sc_count)
		m_sel_screen = l_sc_count - 1;

	if (ImGui::SliderInt("Screen", &m_sel_screen, 0, l_sc_count - 1)) {
		m_atlas_new_tileset_no = get_default_tileset_no(m_sel_chunk, m_sel_screen);
		m_atlas_new_palette_no = get_default_palette_no(p_game, m_sel_chunk, m_sel_screen);
	}

	if (ImGui::Button("Check")) {
		std::string l_output;

		for (std::size_t i{ 0 }; i < p_game.m_chunks.size(); ++i)
			for (std::size_t j{ 0 }; j < p_game.m_chunks[i].m_screens.size(); ++j)
				if (p_game.m_chunks[i].m_screens[j].m_intrachunk_scroll.has_value()) {
					const auto& l_inter{ p_game.m_chunks[i].m_screens[j].m_intrachunk_scroll.value() };

					l_output += "Chunk=" + std::to_string(i) +
						",Screen=" + std::to_string(j) +
						",Dest screen=" + std::to_string(l_inter.m_dest_screen) +
						",Palette ID=" + std::to_string(l_inter.m_palette_id)
						+ "\n";
				}

	}

	ImGui::Separator();

	const auto& l_screen{ p_game.m_chunks.at(m_sel_chunk).m_screens.at(m_sel_screen) };

	if (ImGui::Button("Left") && l_screen.m_scroll_left.has_value())
		m_sel_screen = l_screen.m_scroll_left.value();
	if (ImGui::Button("Right") && l_screen.m_scroll_right.has_value())
		m_sel_screen = l_screen.m_scroll_right.value();
	if (ImGui::Button("Up") && l_screen.m_scroll_up.has_value())
		m_sel_screen = l_screen.m_scroll_up.value();
	if (ImGui::Button("Down") && l_screen.m_scroll_down.has_value())
		m_sel_screen = l_screen.m_scroll_down.value();

	std::size_t l_sprite_count{ l_screen.m_sprites.size() };

	imgui_text("Screen sprite count: " + std::to_string(l_sprite_count));

	for (std::size_t i{ 0 }; i < l_sprite_count; ++i) {
		const auto& l_sprite{ l_screen.m_sprites[i] };

		byte l_id{ l_sprite.m_id };
		byte l_x{ l_sprite.m_x };
		byte l_y{ l_sprite.m_y };
		std::string l_text{ l_sprite.m_text_id.has_value() ?
			klib::Bitreader::byte_to_hex(l_sprite.m_text_id.value()) :
			"None"
		};

		imgui_text("Sprite #" + std::to_string(i) +
			" [ID=" + klib::Bitreader::byte_to_hex(l_id) +
			"] - coords=("
			+ std::to_string(l_x) + "," + std::to_string(l_y) + "), Text: " + l_text);
	}

	ImGui::Separator();

	if (l_screen.m_interchunk_scroll.has_value()) {
		const auto& l_is{ l_screen.m_interchunk_scroll };

		ImGui::Text("Inter-world scroll transition");
		imgui_text("Destination world=" + std::to_string(l_is.value().m_dest_chunk)
			+ ", screen=" + std::to_string(l_is.value().m_dest_screen)
			+ ", pos=(" + std::to_string(l_is.value().m_dest_x)
			+ "," + std::to_string(l_is.value().m_dest_y) + "), palette="
			+ klib::Bitreader::byte_to_hex(l_is.value().m_palette_id)
		);
	}

	ImGui::Separator();

	if (l_screen.m_intrachunk_scroll.has_value()) {
		const auto& l_is{ l_screen.m_intrachunk_scroll };

		ImGui::Text("Intra-world scroll transition");
		imgui_text("Destination screen=" + std::to_string(l_is.value().m_dest_screen)
			+ ", pos=(" + std::to_string(l_is.value().m_dest_x)
			+ "," + std::to_string(l_is.value().m_dest_y) + "), palette="
			+ klib::Bitreader::byte_to_hex(l_is.value().m_palette_id)
		);
	}

	ImGui::Separator();

	const auto& l_cconn{ p_game.m_chunks.at(m_sel_chunk).m_door_connections };

	if (l_cconn.has_value()) {
		imgui_text("Next chunk and screen: " + std::to_string(l_cconn.value().m_next_chunk) + ", " + std::to_string(l_cconn.value().m_next_screen) +
			" - req: " + klib::Bitreader::byte_to_hex(l_cconn.value().m_next_door_req));
		imgui_text("Prev chunk and screen: " + std::to_string(l_cconn.value().m_prev_chunk) + ", " + std::to_string(l_cconn.value().m_prev_screen)
			+ " - req: " + klib::Bitreader::byte_to_hex(l_cconn.value().m_prev_door_req));
	}
	else
		imgui_text("This world has no concept of next and previous world");

	ImGui::Separator();

	imgui_text("Door count: " + std::to_string(l_screen.m_doors.size()));

	for (std::size_t i{ 0 }; i < l_screen.m_doors.size(); ++i) {
		const auto& l_door{ l_screen.m_doors[i] };

		std::string l_doortxt{ "Door #" + std::to_string(i) + " at ("
		+ std::to_string(l_door.m_coords.first) + ","
			+ std::to_string(l_door.m_coords.second) + "), dest=("

		+ std::to_string(l_door.m_dest_coords.first) + ","
			+ std::to_string(l_door.m_dest_coords.second) + "), unknown="
			+ klib::Bitreader::byte_to_hex(l_door.m_unknown)
			+ ",type="
		};

		if (l_door.m_door_type == fe::DoorType::NextWorld)
			l_doortxt += "Next World";
		else if (l_door.m_door_type == fe::DoorType::PrevWorld)
			l_doortxt += "Previous World";
		else if (l_door.m_door_type == fe::DoorType::Building)
			l_doortxt += "Building, req=" + klib::Bitreader::byte_to_hex(l_door.m_requirement)
			+ "\nDest screen=" + std::to_string(l_door.m_dest_screen_id)
			+ ", NPC bundle=" + klib::Bitreader::byte_to_hex(l_door.m_npc_bundle);
		else
			l_doortxt += "IntraChunk, req=" + klib::Bitreader::byte_to_hex(l_door.m_requirement)
			+ "\nDest screen=" + std::to_string(l_door.m_dest_screen_id)
			+ ", Palette=" + klib::Bitreader::byte_to_hex(l_door.m_dest_palette_id);


		imgui_text(l_doortxt);
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

	byte l_metatile_id{ p_game.m_chunks.at(m_sel_chunk).m_screens.at(m_sel_screen).m_tilemap.at(p_sel_y).at(p_sel_x) };

	ImGui::Begin("Metatile");

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
