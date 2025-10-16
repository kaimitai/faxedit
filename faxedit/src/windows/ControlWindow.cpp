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

		for (std::size_t i{ 0 }; i < 3; ++i) {
			auto l_bank_screen_data{ m_rom_manager.encode_bank_screen_data(i, p_game) };
			klib::file::write_bytes_to_file(l_bank_screen_data, "c:/temp/bank" + std::to_string(i) + ".bin");
			std::copy(begin(l_bank_screen_data), end(l_bank_screen_data), begin(l_rom) + c::PTR_TILEMAPS_BANK_ROM_OFFSET.at(i));
		}

		if (l_rom != p_game.m_rom_data)
			add_message("patch bad to the bone");
		else
			add_message("patching is roses and sunshine! and rainbows!");

	}

	if (ImGui::Button("Chunk sprite data to file")) {
		std::vector<byte> l_sprite_data;

		for (std::size_t s{ 0 }; s < p_game.m_chunks.at(m_sel_chunk).m_screens.size(); ++s) {
			auto l_sc_sprite_data{ p_game.m_chunks.at(m_sel_chunk).m_screens.at(s).get_sprite_bytes() };
			l_sprite_data.insert(end(l_sprite_data), begin(l_sc_sprite_data), end(l_sc_sprite_data));
		}

		klib::file::write_bytes_to_file(l_sprite_data, "c:/temp/spr-" + std::to_string(m_sel_chunk) + ".bin");
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
