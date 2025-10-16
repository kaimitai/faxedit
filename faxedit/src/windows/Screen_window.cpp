#include "MainWindow.h"
#include "Imgui_helper.h"
#include "./../fe/fe_constants.h"
#include <format>
#include <string>

void fe::MainWindow::draw_screen_window(SDL_Renderer* p_rnd, fe::Game& p_game) {
	fe::ui::imgui_screen(std::format("{} - Screen {}###sw",
		fe::c::LABELS_CHUNKS.at(m_sel_chunk),
		m_sel_screen)
	);

	auto& l_screen{ p_game.m_chunks.at(m_sel_chunk).m_screens.at(m_sel_screen) };

	if (fe::ui::imgui_slider_with_arrows("##ss", std::format("Screen #{} of {}", m_sel_screen, p_game.m_chunks.at(m_sel_chunk).m_screens.size()),
		m_sel_screen, static_cast<std::size_t>(0), p_game.m_chunks.at(m_sel_chunk).m_screens.size() - 1)) {
		m_atlas_new_tileset_no = get_default_tileset_no(m_sel_chunk, m_sel_screen);
		m_atlas_new_palette_no = get_default_palette_no(p_game, m_sel_chunk, m_sel_screen);
	}

	ImGui::Separator();

	if (ui::collapsing_header("Scrolling###swscr")) {

	}

	ImGui::Separator();

	bool l_iwt{ l_screen.m_intrachunk_scroll.has_value() };

	if (ui::collapsing_header(
		std::format("Intra-world transitions ({})###swiwt",
			l_iwt ? "enabled" : "disabled"
		))) {

		if (l_iwt) {
			if (ImGui::Button("Delete###swiwtdel"))
				l_screen.m_intrachunk_scroll.reset();
		}
		else {
			if (ImGui::Button("Add###swiwtadd"))
				l_screen.m_intrachunk_scroll = fe::IntraChunkScroll(
					static_cast<byte>(0),
					static_cast<byte>(0),
					p_game.m_chunks.at(m_sel_chunk).m_default_palette_no
				);
		}

	}

	ImGui::Separator();

	bool l_ict{ l_screen.m_interchunk_scroll.has_value() };

	if (ui::collapsing_header(
		std::format("Inter-world transitions ({})###swict",
			l_ict ? "enabled" : "disabled"
		))) {

	}

	ImGui::Separator();

	if (ui::collapsing_header(std::format("Sprites: {}###sws", l_screen.m_sprites.size()))) {
		imgui_text(std::format("Doors: {}", l_screen.m_sprites.size()));
	}

	ImGui::Separator();

	if (ui::collapsing_header(std::format("Doors: {}###swd", l_screen.m_doors.size()))) {
		imgui_text(std::format("Doors: {}", l_screen.m_doors.size()));
	}

	ImGui::End();
}

void fe::MainWindow::add_message(const std::string& p_msg) {
	if (m_messages.size() > 50)
		m_messages.pop_back();
	m_messages.push_front(p_msg);
}
