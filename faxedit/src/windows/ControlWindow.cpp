#include "MainWindow.h"

#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include "Imgui_helper.h"
#include "./../fe/xml/Xml_helper.h"
#include "./../common/klib/Kfile.h"
#include "./../fe/ROM_manager.h"

void fe::MainWindow::draw_control_window(SDL_Renderer* p_rnd, fe::Game& p_game) {

	ui::imgui_screen("Project Control###pcw");

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

	if (ImGui::Button("Patch ROM")) {
		auto l_rom{ p_game.m_rom_data };

		// encode metadata
		auto l_metadata{ m_rom_manager.encode_game_metadata_all(p_game) };
		std::copy(begin(l_metadata), end(l_metadata), begin(l_rom) + 0xc012);
		add_message("Patched metadata (" + std::to_string(l_metadata.size()) + " bytes)");

		// encode all screens
		for (std::size_t i{ 0 }; i < 3; ++i) {
			auto l_bank_screen_data{ m_rom_manager.encode_bank_screen_data(p_game, i) };
			std::copy(begin(l_bank_screen_data), end(l_bank_screen_data), begin(l_rom) + c::PTR_TILEMAPS_BANK_ROM_OFFSET.at(i));

			add_message("Patched screen in bank " +
				std::to_string(i) + " (" + std::to_string(l_bank_screen_data.size()) + " bytes)");
		}

		// encode sprites
		std::vector<byte> l_sprite_data{ m_rom_manager.encode_game_sprite_data_new(p_game) };
		std::copy(begin(l_sprite_data), end(l_sprite_data),
			begin(l_rom) + p_game.m_ptr_chunk_sprite_data);
		add_message("Patched sprite data (" + std::to_string(l_sprite_data.size()) + " bytes)");

		m_rom_manager.encode_chunk_door_data(p_game, l_rom);
		add_message("Patched world door connection data");

		auto l_ow_trans{ m_rom_manager.encode_game_otherworld_trans(p_game) };
		std::copy(begin(l_ow_trans), end(l_ow_trans),
			begin(l_rom) + p_game.m_ptr_chunk_intrachunk_transitions);
		add_message("Patched other-world transtion data ("
		+ std::to_string(l_ow_trans.size()) + " bytes)");

		auto l_sw_trans{ m_rom_manager.encode_game_sameworld_trans(p_game) };
		std::copy(begin(l_sw_trans), end(l_sw_trans),
			begin(l_rom) + p_game.m_ptr_chunk_interchunk_transitions);
		add_message("Patched same-world transtion data ("
			+ std::to_string(l_sw_trans.size()) + " bytes)");

		klib::file::write_bytes_to_file(l_rom,
			"c:/temp/faxanadu-out.nes");

		add_message("File written");
	}

	ImGui::Separator();

	for (const auto& msg : m_messages)
		ImGui::Text(msg.c_str());

	ImGui::End();
}
