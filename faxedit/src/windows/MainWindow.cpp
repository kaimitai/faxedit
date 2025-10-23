#include "MainWindow.h"
#include "gfx.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include <algorithm>
#include <map>
#include "./../common/klib/Bitreader.h"
#include "Imgui_helper.h"
#include "./../fe/fe_constants.h"
#include "./../fe/xml/Xml_helper.h"
#include "./../common/klib/Kfile.h"

fe::MainWindow::MainWindow(SDL_Renderer* p_rnd) :
	m_sel_chunk{ 0 }, m_sel_screen{ 0 }, m_sel_door{ 0 },
	m_sel_sprite{ 0 }, m_sel_tile_x{ 0 }, m_sel_tile_y{ 0 },
	m_sel_tile_x2{ 16 }, m_sel_tile_y2{ 0 },
	m_gfx{ fe::gfx(p_rnd) },
	m_atlas_palette_no{ 1 },
	m_atlas_tileset_no{ 1 },
	m_atlas_new_tileset_no{ 0 },
	m_atlas_new_palette_no{ 0 },
	m_sel_metatile{ 0 },
	m_sel_tilemap_sub_palette{ 0 },
	m_sel_nes_tile{ 0x80 },
	m_emode{ fe::EditMode::Tilemap }
{

	add_message(std::format("Build date: {} {} CET",
		__DATE__, __TIME__));
	add_message("https://github.com/faxedit", 1);
	add_message("Welcome to Echoes of Eolis by Kai E. Froeland", 1);
}

void fe::MainWindow::generate_textures(SDL_Renderer* p_rnd, const fe::Game& p_game) {

	// ensure the atlas will be generated
	m_atlas_new_tileset_no = get_default_tileset_no(0, 0);
	m_atlas_new_palette_no = get_default_palette_no(p_game, 0, 0);

	// TODO: generate sprite textures
}

void fe::MainWindow::draw(SDL_Renderer* p_rnd, fe::Game& p_game) {
	// input handling, move to separate function later

	if (m_emode == fe::EditMode::Tilemap) {
		if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_C)) {
			clipboard_copy(p_game);
		}
		else if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_V)) {
			clipboard_paste(p_game);
		}
	}

	// input handling end

	regenerate_atlas_if_needed(p_rnd, p_game);

	SDL_SetRenderDrawColor(p_rnd, 126, 126, 255, 0);
	SDL_RenderClear(p_rnd);

	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	const auto& l_chunk{ p_game.m_chunks.at(m_sel_chunk) };
	const auto& l_screen{ p_game.m_chunks.at(m_sel_chunk).m_screens.at(m_sel_screen) };
	const auto& l_tilemap{ l_screen.m_tilemap };
	std::size_t l_tileset{ get_default_tileset_no(m_sel_chunk, m_sel_screen) };

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

	// draw selected rectangle

	if (m_emode == fe::EditMode::Tilemap) {
		if (m_sel_tile_x2 < 16) {
			const auto l_rect{ get_selection_dims() };

			m_gfx.draw_rect_on_screen(p_rnd, SDL_Color(255, 120, 0, 255),
				static_cast<int>(l_rect.x), static_cast<int>(l_rect.y),
				static_cast<int>(l_rect.w), static_cast<int>(l_rect.h));
		}
		else {
			m_gfx.draw_rect_on_screen(p_rnd, SDL_Color(255, 255, 0, 255),
				static_cast<int>(m_sel_tile_x),
				static_cast<int>(m_sel_tile_y), 1, 1);
		}
	}
	else if (m_emode == fe::EditMode::Sprites) {
		// draw placeholder rectangles for sprites
		for (std::size_t s{ 0 }; s < l_screen.m_sprites.size(); ++s) {
			const auto& l_sprite{ l_screen.m_sprites[s] };

			m_gfx.draw_rect_on_screen(p_rnd,
				SDL_Color(m_sel_sprite == s ? 255 : 0,
					255, 0, 255),
				l_sprite.m_x, l_sprite.m_y,
				1, 1
			);

		}
	}
	else if (m_emode == fe::EditMode::Doors) {
		// draw placeholder rectangles for doors
		for (std::size_t d{ 0 }; d < l_screen.m_doors.size(); ++d) {
			const auto& l_door{ l_screen.m_doors[d] };

			m_gfx.draw_rect_on_screen(p_rnd,
				SDL_Color(d == m_sel_door ? 255 : 0, 255, 0, 255),
				l_door.m_coords.first, l_door.m_coords.second,
				1, 1
			);

		}
	}

	draw_screen_tilemap_window(p_rnd, p_game);
	draw_control_window(p_rnd, p_game);
	draw_metadata_window(p_rnd, p_game);

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

	imgui_text("Property: " + klib::Bitreader::byte_to_hex(p_game.m_chunks.at(m_sel_chunk).m_metatiles.at(l_metatile_id).m_block_property));

	ImGui::End();

}

void fe::MainWindow::regenerate_atlas_if_needed(SDL_Renderer* p_rnd,
	const fe::Game& p_game) {

	if (m_atlas_new_tileset_no != m_atlas_tileset_no ||
		m_atlas_new_palette_no != m_atlas_palette_no) {

		m_gfx.generate_atlas(p_rnd, p_game.m_tilesets.at(m_atlas_new_tileset_no),
			p_game.m_palettes.at(m_atlas_new_palette_no));
		generate_metatile_textures(p_rnd, p_game);

		m_atlas_tileset_no = m_atlas_new_tileset_no;
		m_atlas_palette_no = m_atlas_new_palette_no;
	}

}

// can be regenerated independently of the atlas
void fe::MainWindow::generate_metatile_textures(SDL_Renderer* p_rnd, const fe::Game& p_game) {
	for (std::size_t i{ 0 }; i < p_game.m_chunks.at(m_sel_chunk).m_metatiles.size(); ++i)
		m_gfx.generate_mt_texture(p_rnd,
			p_game.m_chunks.at(m_sel_chunk).m_metatiles.at(i).m_tilemap,
			i, m_sel_tilemap_sub_palette);
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

void fe::MainWindow::add_message(const std::string& p_msg, int p_status) {
	if (m_messages.size() > 50)
		m_messages.pop_back();
	m_messages.push_front(fe::Message(p_msg, p_status));
}

// remember to push and pop IDs before calling this function
std::optional<std::pair<byte, byte>> fe::MainWindow::show_position_slider(byte p_x, byte p_y) {

	byte l_x{ p_x }, l_y{ p_y };
	bool l_updated{ false };

	imgui_text("x");
	ImGui::SameLine();
	if (ui::imgui_slider_with_arrows("pslx", "", l_x, 0, 15))
		l_updated = true;

	imgui_text("y");
	ImGui::SameLine();
	if (ui::imgui_slider_with_arrows("psly", "", l_y, 0, 12))
		l_updated = true;

	if (l_updated)
		return std::make_pair(l_x, l_y);
	else
		return std::nullopt;
}

std::string fe::MainWindow::get_editmode_as_string(void) const {
	if (m_emode == fe::EditMode::Tilemap)
		return "Tilemap";
	else if (m_emode == fe::EditMode::Sprites)
		return "Sprites";
	else if (m_emode == fe::EditMode::Doors)
		return "Doors";
	else if (m_emode == fe::EditMode::Transitions)
		return "Transitions";
	else if (m_emode == fe::EditMode::Scrolling)
		return "Scrolling";
	else
		return "Other";
}

fe::Size4 fe::MainWindow::get_selection_dims(void) const {
	if (m_sel_tile_x2 == 16)
		return Size4(m_sel_tile_x, m_sel_tile_y, 1, 1);
	else
		return Size4(
			std::min(m_sel_tile_x, m_sel_tile_x2),
			std::min(m_sel_tile_y, m_sel_tile_y2),
			std::max(m_sel_tile_x, m_sel_tile_x2) - std::min(m_sel_tile_x, m_sel_tile_x2) + 1,
			std::max(m_sel_tile_y, m_sel_tile_y2) - std::min(m_sel_tile_y, m_sel_tile_y2) + 1
		);
}

void fe::MainWindow::clipboard_copy(const fe::Game& p_game) {
	const auto l_rect{ get_selection_dims() };
	const auto& l_tm{ p_game.m_chunks.at(m_sel_chunk).m_screens.at(m_sel_screen).m_tilemap };

	std::vector<std::vector<byte>> l_clip;

	for (std::size_t j{ l_rect.y }; j < l_rect.y + l_rect.h; ++j) {
		std::vector<byte> l_tmp;

		for (std::size_t i{ l_rect.x }; i < l_rect.x + l_rect.w; ++i)
			l_tmp.push_back(l_tm.at(j).at(i));

		l_clip.push_back(l_tmp);
	}

	m_clipboard[m_sel_chunk] = l_clip;

	add_message(std::format("Copied {}x{} rectangle to world {} clipboard",
		l_clip.at(0).size(), l_clip.size(), m_sel_chunk));
}

void fe::MainWindow::clipboard_paste(fe::Game& p_game) {
	const auto& l_clip{ m_clipboard[m_sel_chunk] };

	if (l_clip.empty())
		add_message(std::format("Clipboard for world {} is empty", m_sel_chunk));
	else if (m_sel_tile_y + l_clip.size() > 13 ||
		m_sel_tile_x + l_clip[0].size() > 16)
		add_message("Clipboard data does not fit.");
	// all good, paste
	else {
		for (std::size_t j{ 0 }; j < l_clip.size(); ++j)
			for (std::size_t i{ 0 }; i < l_clip.at(j).size(); ++i)
				p_game.m_chunks.at(m_sel_chunk).
				m_screens.at(m_sel_screen).
				m_tilemap.at(m_sel_tile_y + j).at(m_sel_tile_x + i) =
				l_clip[j][i];

		add_message("Clipboard data pasted");
	}
}
