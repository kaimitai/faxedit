#include "MainWindow.h"

#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include "Imgui_helper.h"
#include "./../fe/xml/Xml_helper.h"
#include "./../common/klib/Kfile.h"
#include "./../fe/ROM_manager.h"

void fe::MainWindow::draw_control_window(SDL_Renderer* p_rnd) {

	ui::imgui_screen("Project Control###pcw");

	if (ImGui::Button("Save XML")) {
		try {
			xml::save_xml(get_xml_path(), m_game.value());
			add_message("xml file written to " + get_xml_path(), 2);
		}
		catch (const std::runtime_error& p_ex) {
			add_message(p_ex.what(), 1);
		}
		catch (const std::exception& p_ex) {
			add_message(p_ex.what(), 1);
		}
	}

	ImGui::SameLine();

	if (ui::imgui_button("Find mattock-breakables", 3)) {
		for (std::size_t c{ 0 }; c < m_game->m_chunks.size(); ++c)
			for (std::size_t s{ 0 }; s < m_game->m_chunks[c].m_screens.size(); ++s) {
				const auto& l_tm{ m_game->m_chunks[c].m_screens[s].m_tilemap };

				for (std::size_t j{ 0 }; j < l_tm.size(); ++j)
					for (std::size_t i{ 0 }; i < l_tm[j].size(); ++i)
						if (l_tm[j][i] == (c == 1 ? 0x63 : 0x00))
							add_message(std::format("World {}, Screen {}, Pos ({},{})",
								c, s, i, j));

			}
	}

	if (ImGui::Button("Load XML")) {

		try {
			add_message("Attempting to load xml " + get_xml_path(), 5);
			auto l_rom{ m_game->m_rom_data };
			m_game = xml::load_xml(get_xml_path());
			m_game->m_rom_data = l_rom;

			// extract gfx
			for (std::size_t c{ 0 }; c < m_game->m_offsets_bg_gfx.size(); ++c) {
				m_game->m_tilesets.push_back(std::vector<klib::NES_tile>());

				for (std::size_t i{ 0 }; i < 256; ++i) {
					m_game->m_tilesets[c].push_back(klib::NES_tile::NES_tile(m_game->m_rom_data,
						m_game->m_offsets_bg_gfx[c] + 16 * i));
				}
			}
			add_message("Loaded xml file " + get_xml_path(), 2);
		}
		catch (const std::runtime_error& p_ex) {
			add_message(p_ex.what(), 1);
		}
		catch (const std::exception& p_ex) {
			add_message(p_ex.what(), 1);
		}
	}

	if (ImGui::Button("Test duplicates")) {
		for (std::size_t c{ 0 }; c < m_game->m_chunks.size(); ++c) {
			std::vector<std::vector<byte>> l_cmpr;

			for (std::size_t s{ 0 }; s < m_game->m_chunks[c].m_screens.size(); ++s)
				l_cmpr.push_back(m_game->m_chunks[c].m_screens[s].get_tilemap_bytes());

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

			for (std::size_t s{ 0 }; s < m_game->m_chunks[i].m_screens.size(); ++s) {

				const auto& tm{ m_game->m_chunks[i].m_screens[s].m_tilemap };

				for (const auto& row : tm)
					for (byte b : row)
						++l_count[b];

			}

			int l_unused_cnt{ 0 };

			for (std::size_t b{ 0 }; b < m_game->m_chunks[i].m_metatiles.size(); ++b) {
				if (l_count.find(static_cast<byte>(b)) == end(l_count))
					++l_unused_cnt;
			}

			add_message(std::format("Chunk {} unused metatiles: {}", i, l_unused_cnt));
		}

	}

	if (ImGui::Button("Patch ROM")) {
		bool l_good{ true };
		std::size_t l_dyndata_bytes{ 0 };

		auto x_rom{ m_game->m_rom_data };

		m_rom_manager.encode_static_data(m_game.value(), x_rom);
		add_message("Patched static data", 2);

		auto l_bret{ m_rom_manager.encode_transitions(m_game.value(), x_rom) };
		l_good &= check_patched_size("Transition Data", l_bret.first, l_bret.second);
		l_dyndata_bytes += l_bret.first;

		l_bret = m_rom_manager.encode_sprite_data(m_game.value(), x_rom);
		l_good &= check_patched_size("Sprite Data", l_bret.first, l_bret.second);
		l_dyndata_bytes += l_bret.first;

		for (std::size_t i{ 0 }; i < 3; ++i) {
			l_bret = m_rom_manager.encode_bank_tilemaps(m_game.value(), x_rom, i);
			l_good &= check_patched_size(std::format("Batch {} Tilemaps", i + 1), l_bret.first, l_bret.second);
			l_dyndata_bytes += l_bret.first;
		}

		l_bret = m_rom_manager.encode_metadata(m_game.value(), x_rom);
		l_good &= check_patched_size("Worlds Metadata", l_bret.first, l_bret.second);
		l_dyndata_bytes += l_bret.first;

		auto l_out_nes_file{ get_filepath("nes") };

		if (l_good) {
			klib::file::write_bytes_to_file(x_rom,
				l_out_nes_file);

			add_message(std::format("Contents written to file {} ({} dynamic bytes)",
				l_out_nes_file, l_dyndata_bytes), 2);
		}
		else
			add_message("NES-file could not be written", 1);
	}

	if (ui::imgui_button("Delete Unreferenced Metatiles", 2)) {
		std::size_t l_del_cnt{ m_game->delete_unreferenced_metatiles(m_sel_chunk) };
		add_message(std::format("{} metatiles deleted from world {}",
			l_del_cnt, m_sel_chunk));
		if (l_del_cnt > 0)
			generate_metatile_textures(p_rnd);
	}

	ImGui::SameLine();

	if (ui::imgui_button("Delete Unreferenced Screens", 2)) {
		if (m_sel_chunk == c::CHUNK_IDX_BUILDINGS) {
			add_message("Can not delete screens from the Buildings word");
		}
		else {
			std::size_t l_del_cnt{ m_game->delete_unreferenced_screens(m_sel_chunk) };
			add_message(std::format("{} screens deleted from world {}",
				l_del_cnt, m_sel_chunk), 2);
			if (m_sel_screen >= m_game->m_chunks.at(m_sel_chunk).m_screens.size())
				m_sel_screen = m_game->m_chunks.at(m_sel_chunk).m_screens.size() - 1;
		}
	}

	if (ui::imgui_button("Find unused screens")) {
		for (std::size_t i{ 0 }; i < 8; ++i) {
			auto l_scr{ m_game->get_referenced_screens(i) };
			add_message(std::format("World {}: {} unreferenced screens",
				i, m_game->m_chunks.at(i).m_screens.size() - l_scr.size()
			));
		}
	}

	show_output_messages();

	ImGui::End();
}
