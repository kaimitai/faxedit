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

void fe::MainWindow::save_xml(void) {
	try {
		m_game->sync_palettes(m_shared_palettes);
		xml::save_xml(get_xml_path(), m_game.value());
		add_message("xml file written to " + get_xml_path(), 2, true);
	}
	catch (const std::runtime_error& p_ex) {
		add_message(p_ex.what(), 1);
	}
	catch (const std::exception& p_ex) {
		add_message(p_ex.what(), 1);
	}
}

void fe::MainWindow::patch_nes_rom(bool p_in_place, bool p_exclude_dynamic) {
	auto l_patched_rom{ patch_rom(p_exclude_dynamic) };

	if (l_patched_rom.has_value()) {
		std::string l_out_file{ p_in_place ? get_filepath("nes") : get_nes_path() };

		try {
			klib::file::write_bytes_to_file(l_patched_rom.value(), l_out_file);
			add_message("ROM file written to " + l_out_file, 2);
		}
		catch (const std::runtime_error& ex) {
			add_message(ex.what(), 1);
		}
		catch (const std::exception& ex) {
			add_message(ex.what(), 1);
		}
	}
}

void fe::MainWindow::draw_control_window(SDL_Renderer* p_rnd) {

	ui::imgui_screen("Project Control###pcw", c::WIN_CONTROLS_X, c::WIN_CONTROLS_Y,
		c::WIN_CONTROLS_W, c::WIN_CONTROLS_H, 4);

	if (ui::imgui_button("Save xml", 2))
		save_xml();

	ImGui::SameLine();

	bool l_shift{ ImGui::IsKeyDown(ImGuiMod_Shift) };
	bool l_alt{ ImGui::IsKeyDown(ImGuiMod_Alt) };

	if (ui::imgui_button("Patch nes ROM",
		l_shift ? 4 : 2,
		"Patch loaded ROM file"))
		patch_nes_rom(l_shift, l_alt);

	ImGui::SameLine();

	if (ui::imgui_button("Save ips", 2, "Generate ips patch file")) {
		auto l_patched_rom{ patch_rom(l_alt) };

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

	ImGui::SameLine();

	if (ui::imgui_button(std::format("{} gfx editor",
		m_gfx_window ? "Hide" : "Show"),
		m_gfx_window ? 4 : 2))
		m_gfx_window = !m_gfx_window;

	if (ui::imgui_button("Load xml", 2, "", !ImGui::IsKeyDown(ImGuiMod_Shift)))
		load_xml();

	show_output_messages();

	ImGui::End();
}

void fe::MainWindow::load_xml(void) {
	try {
		add_message("Attempting to load xml " + get_xml_path(), 5);
		auto l_rom{ m_game->m_rom_data };
		m_game = xml::load_xml(get_xml_path());
		validate_game_data(m_game.value());
		m_game->m_rom_data = l_rom;
		m_undo->clear_history();

		// extract values not present in previous xml versions
		// none of this should do anything if we got values from the xml
		m_game->extract_scenes_if_empty(m_config);
		m_game->extract_palette_to_music(m_config);
		m_game->extract_hud_attributes(m_config);

		// extract gfx
		m_game->generate_tilesets(m_config);
		m_game->m_gfx_manager.initialize(m_config, m_game->m_rom_data);

		// clear staging area for gfx, as well as loaded tilemap/tileset textures
		m_gfx.clear_all_tilemap_import_results();
		m_gfx.clear_tileset_textures();

		// update gui cache for world tilesets
		generate_world_tilesets();
		m_atlas_force_update = true;

		if (m_sel_chunk >= m_game->m_chunks.size())
			m_sel_chunk = 0;
		if (m_sel_screen >= m_game->m_chunks[m_sel_chunk].m_screens.size())
			m_sel_screen = 0;
		m_atlas_new_palette_no = m_game->get_default_palette_no(m_sel_chunk, m_sel_screen);

		add_message("Loaded xml file " + get_xml_path(), 2);
	}
	catch (const std::runtime_error& p_ex) {
		add_message(p_ex.what(), 1);
	}
	catch (const std::exception& p_ex) {
		add_message(p_ex.what(), 1);
	}
}

std::optional<std::vector<byte>> fe::MainWindow::patch_rom(bool p_exclude_dynamic) {
	if (p_exclude_dynamic)
		add_message("*** using semi-static patching mode ***", 4);

	bool l_good{ true };
	std::size_t l_dyndata_bytes{ 0 };

	m_game->sync_palettes(m_shared_palettes);
	auto x_rom{ m_game->m_rom_data };

	m_rom_manager.encode_chr_data(m_config, m_game.value(), x_rom);
	add_message("Patched chr data", 2);

	m_rom_manager.encode_static_data(m_config, m_game.value(), x_rom);
	add_message("Patched static data", 2);

	std::pair<std::size_t, std::size_t> l_bret(0, 0);

	if (!p_exclude_dynamic) {
		if (m_config.has_constant(c::ID_SW_TRANS_DATA_END)) {
			l_bret = m_rom_manager.encode_sw_transitions(m_config, m_game.value(), x_rom);
			l_good &= check_patched_size("Same-World Transition Data", l_bret.first, l_bret.second);
			l_dyndata_bytes += l_bret.first;

			l_bret = m_rom_manager.encode_ow_transitions(m_config, m_game.value(), x_rom);
			l_good &= check_patched_size("Other-World Transition Data", l_bret.first, l_bret.second);
			l_dyndata_bytes += l_bret.first;
		}
		else {
			l_bret = m_rom_manager.encode_transitions(m_config, m_game.value(), x_rom);
			l_good &= check_patched_size("Transition Data", l_bret.first, l_bret.second);
			l_dyndata_bytes += l_bret.first;
		}

		l_bret = m_rom_manager.encode_sprite_data(m_config, m_game.value(), x_rom);
		l_good &= check_patched_size("Sprite Data", l_bret.first, l_bret.second);
		l_dyndata_bytes += l_bret.first;

		l_bret = m_rom_manager.encode_metadata(m_config, m_game.value(), x_rom);
		l_good &= check_patched_size("Worlds Metadata", l_bret.first, l_bret.second);
		l_dyndata_bytes += l_bret.first;
	}

	auto l_tm_result{ m_rom_manager.encode_game_tilemaps(m_config, x_rom,
		m_game.value()) };
	l_good &= l_tm_result.m_result;

	std::size_t l_max_tm_byte_size{ m_config.constant(c::ID_WORLD_TILEMAP_MAX_SIZE) };

	if (l_tm_result.m_result) {

		for (const auto& kv : l_tm_result.m_assignments) {
			std::size_t l_bank_byte_size{ 0 };
			std::string l_bank_output;

			for (std::size_t w : kv.second) {
				std::size_t l_byte_size{ l_tm_result.m_sizes[w] };
				l_bank_byte_size += l_byte_size;
				l_dyndata_bytes += l_byte_size;
				l_bank_output += std::format("({} {} bytes) ",
					m_labels_worlds[w], l_byte_size);
			}

			add_message(std::format("Bank {}: {}- total bytes: {}/{} ({:.2f}%)",
				kv.first, l_bank_output, l_bank_byte_size, l_max_tm_byte_size,
				100.0f * static_cast<float>(l_bank_byte_size) / static_cast<float>(l_max_tm_byte_size)),
				6);
		}

		add_message("Tilemaps patched!", 2);
	}
	else {
		add_message(std::format("Could not pack all world tilemaps across the banks, each of byte size {}",
			l_max_tm_byte_size), 1);
		for (std::size_t i{ 0 }; i < 8; ++i) {
			add_message(std::format("Byte size for {}: {}",
				m_labels_worlds[i],
				l_tm_result.m_sizes[i]), 6);
		}
	}

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

void fe::MainWindow::validate_game_data(fe::Game& p_game) {

	const auto validate_screen_connection = [this](std::optional<byte>& conn, std::size_t world,
		std::size_t screen, std::size_t screen_count) -> void {
			if (conn && (static_cast<std::size_t>(conn.value()) >= screen_count)) {
				conn.reset();
				add_message(
					std::format("Invalid connection reference on World {}, Screen {}: connection disabled", world, screen), 1
				);
			}
		};

	// check worlds
	for (std::size_t w{ 0 }; w < p_game.m_chunks.size(); ++w) {
		auto& world{ p_game.m_chunks[w] };

		// check all screen data for world
		std::size_t screen_count{ world.m_screens.size() };
		for (std::size_t s{ 0 }; s < screen_count; ++s) {
			auto& screen{ world.m_screens[s] };

			// validate connections
			validate_screen_connection(screen.m_scroll_left, w, s, screen_count);
			validate_screen_connection(screen.m_scroll_right, w, s, screen_count);
			validate_screen_connection(screen.m_scroll_up, w, s, screen_count);
			validate_screen_connection(screen.m_scroll_down, w, s, screen_count);

			// validate tilemap
			for (std::size_t y{ 0 }; y < screen.m_tilemap.size(); ++y)
				for (std::size_t x{ 0 }; x < screen.m_tilemap[y].size(); ++x)
					if (static_cast<std::size_t>(screen.get_mt_at_pos(x, y)) >= world.m_metatiles.size()) {
						add_message(
							std::format("Invalid metatile reference on World {}, Screen {}, x {}, y {}: {} was set to 0", w, s, x, y, screen.m_tilemap[y][x]), 1
						);
						screen.m_tilemap[y][x] = 0;
					}
		}

		// check scene for world
		if (world.m_scene.m_palette >= p_game.m_palettes.size()) {
			add_message(std::format("Invalid palette reference on World {}: {} was set to 0", w, world.m_scene.m_palette), 1);
			world.m_scene.m_palette = 0;
		}
	}

	// check building scenes
	for (std::size_t bscene{ 0 }; bscene < p_game.m_building_scenes.size(); ++bscene) {
		auto& bScene{ p_game.m_building_scenes[bscene] };

		if (bScene.m_palette >= p_game.m_palettes.size()) {
			add_message(std::format("Invalid palette reference on scene for building screen {}: {} was set to 0", bscene, bScene.m_palette), 1);
			bScene.m_palette = 0;
		}
	}
}

void fe::MainWindow::request_exit_app(void) {
	m_exit_app_requested = true;
}

bool fe::MainWindow::is_exit_app_granted(void) const {
	return m_exit_app_granted;
}
