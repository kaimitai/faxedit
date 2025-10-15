#include "MainWindow.h"

#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include "Imgui_helper.h"
#include "./../fe/xml/Xml_helper.h"
#include "./../common/klib/Kfile.h"
#include "./../fe/ROM_manager.h"

void fe::MainWindow::draw_control_window(SDL_Renderer* p_rnd, fe::Game& p_game) {

	ui::imgui_screen("Main###gw");

	if (ImGui::Button("Save XML"))
		xml::save_xml("c:/temp/out.xml", p_game);

	if (ImGui::Button("Test duplicates")) {
		for (std::size_t c{ 0 }; c < p_game.m_chunks.size(); ++c) {
			std::vector<std::vector<byte>> l_cmpr;

			for (std::size_t s{ 0 }; s < p_game.m_chunks[c].m_screens.size(); ++s)
				l_cmpr.push_back(p_game.m_chunks[c].m_screens[s].get_tilemap_bytes());

			for (std::size_t i{ 0 }; i < l_cmpr.size() - 1; ++i) {
				for (std::size_t j{ i + 1 }; j < l_cmpr.size(); ++j)
					if (l_cmpr[i] == l_cmpr[j])
						add_message(std::format("Chunk {}: Screens {} and {} have identical tilemaps!", c, i, j));
			}
		}
	}

	if (ImGui::Button("Patch ROM")) {
		// p_game.m_chunks.at(0).m_screens.at(0).m_tilemap.at(3).at(3) = 0x30;

		std::vector<byte> l_rom{ p_game.m_rom_data };
		auto l_bank0_screen_data{ m_rom_manager.encode_bank_screen_data(0, p_game) };

		std::copy(begin(l_bank0_screen_data),
			end(l_bank0_screen_data),
			begin(l_rom) + 0x10);

		klib::file::write_bytes_to_file(l_rom, "c:/temp/fax-out.nes");

		if (p_game.m_rom_data != l_rom)
			add_message("ROM patching bad :(");
		else
			add_message("ROM patching good! :)");
	}

	if (ImGui::Button("Unused metatiles?")) {

		for (std::size_t i{ 0 }; i < 8; ++i) {
			std::map<byte, int> l_count;

			for (std::size_t s{ 0 }; s < p_game.m_chunks[i].m_screens.size(); ++s) {

				const auto& tm{ p_game.m_chunks[i].m_screens[s].m_tilemap };

				for (const auto& row : tm)
					for (byte b : row)
						++l_count[b];

			}

			int l_unused_cnt{ 0 };

			for (std::size_t b{ 0 }; b < p_game.m_chunks[i].m_metatiles.size(); ++b) {
				if (l_count.find(static_cast<byte>(b)) == end(l_count))
					++l_unused_cnt;
			}

			add_message(std::format("Chunk {} unused metatiles: {}", i, l_unused_cnt));
		}

	}

	ImGui::Separator();

	for (const auto& msg : m_messages)
		ImGui::Text(msg.c_str());

	ImGui::End();
}
