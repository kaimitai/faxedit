#include "MainWindow.h"

#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include "Imgui_helper.h"
#include "./../fe/xml/Xml_helper.h"
#include "./../common/klib/Kfile.h"
#include "./../common/klib/IPS_Patch.h"
#include "./../fe/ROM_manager.h"
#include "./../fe/fe_app_constants.h"

void fe::MainWindow::draw_control_window(SDL_Renderer* p_rnd) {

	ui::imgui_screen("Project Control###pcw", c::WIN_CONTROLS_X, c::WIN_CONTROLS_Y,
		c::WIN_CONTROLS_W, c::WIN_CONTROLS_H, 4);

	if (ui::imgui_button("Save xml", 2)) {
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

	if (ui::imgui_button("Patch nes ROM", 2, "Patch loaded ROM file")) {
		auto l_patched_rom{ patch_rom() };

		if (l_patched_rom.has_value()) {
			try {
				klib::file::write_bytes_to_file(l_patched_rom.value(), get_nes_path());
				add_message("ROM file written to " + get_nes_path(), 2);
			}
			catch (const std::runtime_error& ex) {
				add_message(ex.what(), 1);
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), 1);
			}
		}
	}

	ImGui::SameLine();

	if (ui::imgui_button("Save ips", 2, "Generate ips patch file")) {
		auto l_patched_rom{ patch_rom() };

		if (l_patched_rom.has_value()) {
			try {
				auto l_ips{ klib::ips::generate_patch(m_game->m_rom_data,
					l_patched_rom.value()) };

				klib::file::write_bytes_to_file(l_ips, get_ips_path());

				add_message(std::format(
					"ips patch written to {} ({} bytes)",
					get_ips_path(), l_ips.size()), 2);
			}
			catch (const std::runtime_error& ex) {
				add_message(ex.what(), 1);
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), 1);
			}
		}
	}

	ImGui::SameLine();

	if (ui::imgui_button("Data Integrity Analysis", 4)) {
		add_message("Starting integrity analysis", 4);

		for (std::size_t c{ 0 }; c < m_game->m_chunks.size(); ++c) {
			const auto l_ref_scr{ m_game->get_referenced_screens(c) };
			for (std::size_t s{ 0 }; s < m_game->m_chunks[c].m_screens.size(); ++s) {
				const auto& scr{ m_game->m_chunks[c].m_screens[s] };

				// check if screen is referenced
				if (c != c::CHUNK_IDX_BUILDINGS && l_ref_scr.find(static_cast<byte>(s)) == end(l_ref_scr))
					add_message(std::format("World {}, Screen {} has no references", c, s), 1);

				// check that defined other-world transitions can be used
				if (scr.m_intrachunk_scroll.has_value()) {
					bool l_ow_block{ false };
					for (const auto& row : scr.m_tilemap)
						for (byte b : row)
							if (m_game->m_chunks[c].m_metatiles.at(b).m_block_property == 0x0c ||
								m_game->m_chunks[c].m_metatiles.at(b).m_block_property == 0x0d) {
								l_ow_block = true;
								break;
							}

					if (!l_ow_block)
						add_message(std::format("World {}, Screen {}: Other-world transition is defined, but no metatiles with ow transition is used",
							c, s), 1);
				}

				// check that defined same-world transitions can be used
				if (scr.m_interchunk_scroll.has_value()) {
					bool l_sw_block{ false };
					for (const auto& row : scr.m_tilemap)
						for (byte b : row)
							if (m_game->m_chunks[c].m_metatiles.at(b).m_block_property == 0x0a) {
								l_sw_block = true;
								break;
							}

					if (!l_sw_block)
						add_message(std::format("World {}, Screen {}: Same-world transition is defined, but no metatiles with transition ladder is used",
							c, s), 1);
				}

				// check that doors are correctly placed
				std::set<std::pair<byte, byte>> unique_door_pos;
				std::size_t doorcnt{ scr.m_doors.size() };

				for (std::size_t d{ 0 }; d < doorcnt; ++d) {
					if (m_game->m_chunks[c].m_metatiles.at(
						scr.get_mt_at_pos(scr.m_doors[d].m_coords.first,
							scr.m_doors[d].m_coords.second)).m_block_property
						!= 0x03) {
						add_message(std::format("World {}, Screen {}, Door {}: Not placed on door-type metatile",
							c, s, d), 1);
					}
					unique_door_pos.insert(scr.m_doors[d].m_coords);
				}

				if (unique_door_pos.size() != doorcnt)
					add_message(std::format("World {}, Screen {}: Several doors defined at the same position", c, s), 1);
			}
		}

		add_message("Integrity analysis completed", 4);
	}

	/*
	ImGui::SameLine();


	if (ui::imgui_button("Count Block Properties")) {
		std::map<byte, int> l_bc_counts;

		for (std::size_t c{ 0 }; c < m_game->m_chunks.size(); ++c) {
			for (std::size_t s{ 0 }; s < m_game->m_chunks[c].m_screens.size(); ++s) {
				const auto& scr{ m_game->m_chunks[c].m_screens[s] };

				for (const auto& row : scr.m_tilemap)
					for (byte b : row)
						++l_bc_counts[m_game->m_chunks[c].m_metatiles.at(b).m_block_property];
			}
		}

		for (const auto& kv : l_bc_counts) {
			add_message(std::format("Blocks with property {}: {}",
				get_description(kv.first, c::LABELS_BLOCK_PROPERTIES),
				kv.second), 5);
		}
	}
	*/

	if (ui::imgui_button("Load xml", 2, "", !ImGui::IsKeyDown(ImGuiMod_Shift))) {

		try {
			add_message("Attempting to load xml " + get_xml_path(), 5);
			auto l_rom{ m_game->m_rom_data };
			m_game = xml::load_xml(get_xml_path());
			m_game->m_rom_data = l_rom;

			// extract gfx
			m_game->generate_tilesets(m_config);
			add_message("Loaded xml file " + get_xml_path(), 2);
		}
		catch (const std::runtime_error& p_ex) {
			add_message(p_ex.what(), 1);
		}
		catch (const std::exception& p_ex) {
			add_message(p_ex.what(), 1);
		}
	}

	// ImGui::Checkbox("Animate", &m_animate);

	/*
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
	*/

	show_output_messages();

	ImGui::End();
}

std::optional<std::vector<byte>> fe::MainWindow::patch_rom(void) {
	bool l_good{ true };
	std::size_t l_dyndata_bytes{ 0 };

	auto x_rom{ m_game->m_rom_data };

	m_rom_manager.encode_static_data(m_game.value(), x_rom);
	add_message("Patched static data", 2);

	auto l_bret{ m_rom_manager.encode_transitions(m_game.value(), x_rom) };
	l_good &= check_patched_size("Transition Data", l_bret.first, l_bret.second);
	l_dyndata_bytes += l_bret.first;

	l_bret = m_rom_manager.encode_sprite_data(m_config, m_game.value(), x_rom);
	l_good &= check_patched_size("Sprite Data", l_bret.first, l_bret.second);
	l_dyndata_bytes += l_bret.first;

	for (std::size_t i{ 0 }; i < 3; ++i) {
		l_bret = m_rom_manager.encode_bank_tilemaps(m_game.value(), x_rom, i);
		l_good &= check_patched_size(std::format("Batch {} Tilemaps", i + 1), l_bret.first, l_bret.second);
		l_dyndata_bytes += l_bret.first;
	}

	l_bret = m_rom_manager.encode_metadata(m_config, m_game.value(), x_rom);
	l_good &= check_patched_size("Worlds Metadata", l_bret.first, l_bret.second);
	l_dyndata_bytes += l_bret.first;


	if (l_good) {
		add_message(std::format("ROM data patched ({} dynamic bytes)",
			l_dyndata_bytes), 2);
		return x_rom;
	}
	else {
		add_message("Could not patch ROM data", 1);
		return std::nullopt;
	}
}
