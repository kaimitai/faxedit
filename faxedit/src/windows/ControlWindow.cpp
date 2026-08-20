#include "MainWindow.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include "Imgui_helper.h"
#include "fe/xml/Xml_helper.h"
#include "common/klib/Kfile.h"
#include "common/klib/IPS_Patch.h"
#include "fe/ROM_Manager.h"
#include "fe/fe_app_constants.h"
#include "fe/sprite/fe_sprite_constants.h"
#include "fe/WorldVisualizer.h"
#include "fe/game/GameManager.h"

void fe::MainWindow::save_xml(void) {
	try {
		m_game->sync_palettes(m_cache.m_shared_palettes);
		xml::save_xml(get_xml_path(), m_game.value());
		add_message("xml file written to " + get_xml_path(), fe::MsgType::Success, true);
	}
	catch (const std::runtime_error& p_ex) {
		add_message(p_ex.what(), fe::MsgType::Error);
	}
	catch (const std::exception& p_ex) {
		add_message(p_ex.what(), fe::MsgType::Error);
	}
}

void fe::MainWindow::patch_nes_rom(bool p_in_place) {
	auto l_patched_rom{ patch_rom() };

	if (l_patched_rom.has_value()) {
		std::string l_out_file{ p_in_place ? m_loaded_rom_path : get_nes_path() };

		try {
			klib::file::write_bytes_to_file(l_patched_rom.value(), l_out_file);
			add_message("ROM file written to " + l_out_file, fe::MsgType::Success);
		}
		catch (const std::runtime_error& ex) {
			add_message(ex.what(), fe::MsgType::Error);
		}
		catch (const std::exception& ex) {
			add_message(ex.what(), fe::MsgType::Error);
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

	if (ui::imgui_button("Patch nes ROM",
		l_shift ? 4 : 2,
		"Patch loaded ROM file"))
		patch_nes_rom(l_shift);

	if (m_settings.m_enable_ips_button) {

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
						get_ips_path(), l_ips.size()), fe::MsgType::Success);
				}
				catch (const std::runtime_error& ex) {
					add_message(ex.what(), fe::MsgType::Error);
				}
				catch (const std::exception& ex) {
					add_message(ex.what(), fe::MsgType::Error);
				}
			}
		}

	}

	ImGui::SameLine();

	if (ui::imgui_button("Data Integrity Analysis", 4)) try {
		fe::game::analyze_game_data(m_game.value(), m_config,
			m_settings.m_warn_tilemap_95_pct, m_settings.m_warn_00_doors, m_msg_callback);
	}
	catch (const std::exception& ex) {
		add_message(std::format("Data Integrity Analysis failed: {}", ex.what()), fe::MsgType::Error);
	}

	ImGui::SameLine();

	if (ui::imgui_button("BG gfx editor",
		m_gfx_window ? 4 : 2))
		m_gfx_window = !m_gfx_window;

	ImGui::SameLine();

	if (ui::imgui_button("Sprite gfx editor",
		m_sprite_gfx_window ? 4 : 2))
		m_sprite_gfx_window = !m_sprite_gfx_window;

	ImGui::SameLine();

	if (ui::imgui_button("Cinematic editor",
		m_cinematic_window ? 4 : 2))
		m_cinematic_window = !m_cinematic_window;

	ImGui::SameLine();

	if (ui::imgui_button("World Visualizer",
		m_visualization_window ? 4 : 2))
		m_visualization_window = !m_visualization_window;

	ImGui::SameLine();

	if (ui::imgui_button("Scripting",
		m_scripting_window ? 4 : 2))
		m_scripting_window = !m_scripting_window;

	if (m_settings.m_enable_config_dump) {

		ImGui::SameLine();

		if (ui::imgui_button("DEBUG", 4, "Hold Shift to dump all debug data")) try {
			dump_debug_data(l_shift);
		}
		catch (const std::exception& ex) {
			add_message(ex.what(), fe::MsgType::Error);
		}
	}

	if (ui::imgui_button("Load xml", 2, "", !ImGui::IsKeyDown(ImGuiMod_Shift)))
		load_xml(p_rnd);

	ImGui::SameLine();

	if (ui::imgui_button("Apply External ROM Changes", 4,
		"Re-read the ROM file from disk and apply external changes. Does not rebuild or reset the editor state.")) try {
		int byte_diffs{ load_external_rom_data(klib::file::read_file_as_bytes(m_loaded_rom_path)) };
		add_message(std::format("Applied external changes from {} ({} bytes different)", m_loaded_rom_path, byte_diffs), fe::MsgType::Success);
	}
	catch (const std::exception& ex) {
		add_message(ex.what(), fe::MsgType::Error);
	}

	ImGui::SameLine();

	if (ui::imgui_button("Settings",
		m_settings_window ? 4 : 2))
		m_settings_window = !m_settings_window;

	show_output_messages();

	ImGui::End();
}

void fe::MainWindow::load_xml(SDL_Renderer* p_rnd) {
	try {
		add_message("Attempting to load xml " + get_xml_path(), fe::MsgType::Info);

		// keep old game around until we know everything succeeded
		auto old_game{ m_game.value() };

		// load new game into a temporary
		fe::Game new_game{ xml::load_xml(get_xml_path()) };

		// restore rom data from old game
		auto l_rom{ old_game.m_rom_data };
		new_game.m_rom_data = l_rom;

		// validate and extract everything on the temporary
		fe::game::validate_and_repair_game(new_game, m_msg_callback);
		new_game.extract_scenes_if_empty(m_config);
		new_game.extract_palette_to_music(m_config);
		new_game.extract_hud_attributes(m_config);
		new_game.generate_tilesets(m_config);
		new_game.m_gfx_manager.initialize(m_config, new_game.m_rom_data);
		new_game.m_sprite_gfx_manager.load_rom(m_config, new_game.m_rom_data, m_rom_manager);
		new_game.cinematic.parse_rom(m_config, new_game.m_rom_data);

		// report on sameworld door hack
		if (new_game.m_sw_door_type == fe::SameWorldDoorType::Randumizer_0_30)
			add_message("Loaded XML uses the sameworld-door to stage-door hack.", fe::MsgType::Success);

		// everything succeeded, so commit at this point
		*m_game = std::move(new_game);
		m_undo->clear_history();
		m_sprite_snap_manager.reset();

		// clear staging area for gfx, as well as loaded tilemap/tileset textures
		m_gfx.clear_all_tilemap_import_results();
		m_gfx.clear_tileset_textures();
		m_gfx.clear_bank_chr_textures();

		// update cached gfx
		generate_door_req_gfx(p_rnd);
		generate_editor_sprite_gfx(p_rnd);
		// update gui cache for world tilesets
		generate_world_tilesets();
		m_atlas_force_update = true;

		if (m_sel_chunk >= m_game->m_chunks.size())
			m_sel_chunk = 0;
		if (m_sel_screen >= m_game->m_chunks[m_sel_chunk].m_screens.size())
			m_sel_screen = 0;
		m_atlas_new_palette_no = m_game->get_default_palette_no(m_sel_chunk, m_sel_screen);

		add_message("Loaded xml file " + get_xml_path(), fe::MsgType::Success);
	}
	catch (const std::runtime_error& p_ex) {
		add_message(p_ex.what(), fe::MsgType::Error);
	}
	catch (const std::exception& p_ex) {
		add_message(p_ex.what(), fe::MsgType::Error);
	}
}

std::optional<std::vector<byte>> fe::MainWindow::patch_rom(void) try {
	bool l_good{ true };
	std::size_t l_dyndata_bytes{ 0 };

	m_game->sync_palettes(m_cache.m_shared_palettes);

	auto x_rom{ m_game->m_rom_data };

	// ensure the door hack is applied if it is supposed to be
	// skip it for randomizer ROMs as they keep the hack in different locations
	if (m_game->m_sw_door_type == fe::SameWorldDoorType::Randumizer_0_30 &&
		!m_cache.m_disable_pal2_mus) {
		patch_randumizer_doors(x_rom);
	}

	// world tileset chr
	if (m_settings.m_patch_world_chr_data) {
		m_rom_manager.encode_chr_data(m_config, m_game.value(), x_rom);
		add_message("Patched world tileset chr data", fe::MsgType::Success);
	}

	// world palettes
	if (m_settings.m_patch_palettes) {
		m_rom_manager.encode_palette_data(m_config, *m_game, x_rom);
		add_message("Patched world palettes", fe::MsgType::Success);
	}

	// stage definitions
	if (m_settings.m_patch_stages) {
		m_rom_manager.encode_stage_data(m_config, *m_game, x_rom);
		add_message("Patched stage definitions", fe::MsgType::Success);
	}

	// mattock animations
	if (m_settings.m_patch_mattock_animations) {
		m_rom_manager.encode_mattock_animations(m_config, *m_game, x_rom);
		add_message("Patched mattock animations", fe::MsgType::Success);
	}

	// push-block definition
	if (m_settings.m_patch_push_blocks) {
		m_rom_manager.encode_push_block(m_config, *m_game, x_rom);
		add_message("Patched push-block definition", fe::MsgType::Success);
	}

	// jump-on tiles
	if (m_settings.m_patch_jump_on_tiles) {
		m_rom_manager.encode_jump_on_tiles(m_config, *m_game, x_rom);
		add_message("Patched jump-on tiles", fe::MsgType::Success);
	}

	// world scene data
	if (m_settings.m_patch_scenes) {
		m_rom_manager.encode_scene_data(m_config, *m_game, x_rom);
		add_message("Patched world scenes", fe::MsgType::Success);
	}

	// fog definition
	if (m_settings.m_patch_fog) {
		m_rom_manager.encode_fog_data(m_config, *m_game, x_rom);
		add_message("Patched fog definition", fe::MsgType::Success);
	}

	// background graphics
	if (m_settings.m_patch_bg_gfx) {
		m_game->m_gfx_manager.patch_rom(x_rom);
		add_message("Patched background gfx", fe::MsgType::Success);
	}

	std::pair<std::size_t, std::size_t> l_bret(0, 0);

	// cinematics
	if (m_settings.m_patch_cinematics) {
		auto cinema_res{ m_game->cinematic.patch_rom(m_config, x_rom) };
		std::size_t used_space{ cinema_res.data_section_end - cinema_res.data_section_start };
		std::size_t free_space_end{ m_settings.throw_on_cinematic_overflow ?
			m_config.constant(c::ID_ISCRIPT_DATA_RG2_START) :
		m_config.constant(c::ID_ISCRIPT_DATA_RG2_END) };

		bool cinematic_patched_ok{ check_patched_size("Cinematic Data", used_space,
			free_space_end - cinema_res.data_section_start) };

		l_good &= cinematic_patched_ok;

		if (!cinematic_patched_ok && m_settings.throw_on_cinematic_overflow)
			add_message(
				std::format(
					"Cinematic data overflow: constant '{}' must be set to at least 0x{:05x} (see the documentation)",
					c::ID_ISCRIPT_DATA_RG2_START,
					cinema_res.data_section_end),
				fe::MsgType::Error);
	}

	// sprite gfx
	if (m_settings.m_patch_sprite_gfx) {
		auto spritegfxres{ m_game->m_sprite_gfx_manager.patch_rom(m_config, x_rom, m_rom_manager) };
		l_dyndata_bytes += spritegfxres.bank6_used.value_or(0);
		l_dyndata_bytes += spritegfxres.bank7_used.value_or(0);
		l_dyndata_bytes += spritegfxres.bank8_used.value_or(0);
		l_good &= spritegfxres.success;
		report_sprite_gfx_patch(spritegfxres);
		if (spritegfxres.success)
			add_message("Sprite Gfx data patched!", fe::MsgType::Success);
		else
			add_message("Could not patch Sprite Gfx data", fe::MsgType::Error);
	}

	// bank 15 - coupled dynamic data
	if (m_settings.m_patch_bank15_data) {
		l_bret = m_rom_manager.encode_bank_15_data(m_config, m_game.value(), x_rom,
			!m_cache.m_disable_pal2_mus);
		l_good &= check_patched_size("Bank 15 Data (transitions, palette-to-music, spawns, building scenes)", l_bret.first, l_bret.second);
		l_dyndata_bytes += l_bret.first;
	}

	// sprite metadata
	if (m_settings.m_patch_sprite_data) {
		l_bret = m_rom_manager.encode_sprite_data(m_config, m_game.value(), x_rom);
		l_good &= check_patched_size("Sprite Data", l_bret.first, l_bret.second);
		l_dyndata_bytes += l_bret.first;
	}

	// world metadata
	if (m_settings.m_patch_metadata) {
		l_bret = m_rom_manager.encode_metadata(m_config, m_game.value(), x_rom);
		l_good &= check_patched_size("Worlds Metadata", l_bret.first, l_bret.second);
		l_dyndata_bytes += l_bret.first;
	}

	// screen tilemaps
	if (m_settings.m_patch_tilemaps) {
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
						m_cache.m_labels_worlds[w], l_byte_size);
				}

				add_message(std::format("Bank {}: {}- total bytes: {}/{} ({:.2f}%)",
					kv.first, l_bank_output, l_bank_byte_size, l_max_tm_byte_size,
					100.0f * static_cast<float>(l_bank_byte_size) / static_cast<float>(l_max_tm_byte_size)),
					fe::MsgType::Info);
			}

			add_message("Tilemaps patched!", fe::MsgType::Success);
		}
		else {
			add_message(std::format("Could not pack all world tilemaps across the banks, each of byte size {}",
				l_max_tm_byte_size), fe::MsgType::Error);
			for (std::size_t i{ 0 }; i < 8; ++i) {
				add_message(std::format("Byte size for {}: {}",
					m_cache.m_labels_worlds[i],
					l_tm_result.m_sizes[i]), fe::MsgType::Info);
			}
		}
	}

	if (m_settings.m_apply_sw_pal2mus_hack) {
		patch_sw_transition_pal2mus(x_rom);
		add_message("Enabled palette to music functionality for sameworld-transitions", fe::MsgType::Info);
	}

	// bank duplication - region-specific config and not a setting
	// must be done after all other patching has completed
	if (m_config.boolean_or(c::ID_DUPLICATE_STATIC_BANK, false)) {
		m_rom_manager.duplicate_static_bank(x_rom);
		add_message("Duplicated bank 15 into bank 31", fe::MsgType::Info);
	}

	if (l_good) {
		add_message(std::format("ROM data patched ({} dynamic bytes)",
			l_dyndata_bytes), fe::MsgType::Success);
		return x_rom;
	}
	else {
		add_message("Could not patch ROM data", fe::MsgType::Error);
		return std::nullopt;
	}
}
catch (const std::exception& ex) {
	add_message(ex.what(), fe::MsgType::Error);
	return std::nullopt;
}

void fe::MainWindow::report_sprite_gfx_patch(const fe::SpriteGfxPatchResult& result) {

	const auto bank_header = [](int p_bank_no, std::optional<std::size_t> p_bank_used) -> std::string {
		if (!p_bank_used) {
			return std::format("Bank {} fail: ", p_bank_no);
		}
		else {
			std::size_t bank_used{ p_bank_used.value() };

			return std::format("Bank {}: {}/{} bytes ({:.2f}%): ", p_bank_no, bank_used, 0x4000,
				(100.0f * bank_used) / 0x4000);
		}
		};

	const auto bank_item = [](const std::string& p_item_name, std::optional<std::size_t> p_value,
		bool p_add_comma = true) -> std::string {
			std::string l_result{ p_item_name };

			if (p_value)
				l_result += std::format("={}", p_value.value());
			else
				l_result += "=null";

			if (p_add_comma)
				l_result += ", ";

			return l_result;
		};

	std::string bank6res{ bank_header(6, result.bank6_used) };
	std::string bank7res{ bank_header(7, result.bank7_used) };
	std::string bank8res{ bank_header(8, result.bank8_used) };

	bank6res += bank_item("npc_chr", result.bank6_used);
	bank6res += bank_item("sprite_cutoff", result.bank6_sprite_cutoff, false);

	bank7res += bank_item("npc_chr", result.bank7_sprite_chr_used);
	bank7res += bank_item("npc_frame", result.bank7_npc_anim_frame_used);
	bank7res += bank_item("player_frame", result.bank7_player_anim_frame_used);
	bank7res += bank_item("portrait_frame", result.bank7_portrait_anim_frame_used, false);

	bank8res += bank_item("player_list", result.bank8_player_load_lists);
	bank8res += bank_item("wep_list", result.bank8_weapons_load_lists);
	bank8res += bank_item("player_chr", result.bank8_player_chr);
	bank8res += bank_item("wep_chr", result.bank8_weapons_chr);
	bank8res += bank_item("common_chr", result.bank8_common_chr);
	bank8res += bank_item("shield_chr", result.bank8_shield_chr);
	bank8res += bank_item("shield_list", result.bank8_shield_load_lists);
	bank8res += bank_item("portrait_list", result.bank8_portrait_load_lists);
	bank8res += bank_item("portrait_chr", result.bank8_portrait_chr, false);

	add_message(bank6res, result.bank6_used ? fe::MsgType::Info : fe::MsgType::Error);
	add_message(bank7res, result.bank7_used ? fe::MsgType::Info : fe::MsgType::Error);
	add_message(bank8res, result.bank8_used ? fe::MsgType::Info : fe::MsgType::Error);
}

// dump debug data to disk
// if p_complete is set - dump all, otherwise dump config constants only
void fe::MainWindow::dump_debug_data(bool p_complete) {

	// resolved config constants
	{
		std::filesystem::path outputPath{ m_path / (m_filename + "-config_dump.txt") };
		std::string config_dump_out_file{ outputPath.string() };

		klib::file::write_string_to_file(m_config.to_string(), config_dump_out_file);
		add_message(std::format("Resolved configuration dumped to file {}", config_dump_out_file), fe::MsgType::Success);
	}

	if (p_complete) {
		// dump screen references
		std::string out_contents;
		auto screenrefs{ m_game->collect_screen_refs() };
		std::sort(begin(screenrefs), end(screenrefs));
		for (const auto& screenref : screenrefs)
			out_contents += screenref.to_string(true) + "\n";

		std::filesystem::path screenref_outputPath{ m_path / (m_filename + "-screen_refs.txt") };
		std::string screenref_dump_out_file{ screenref_outputPath.string() };

		klib::file::write_string_to_file(out_contents, screenref_dump_out_file);
		add_message(std::format("Screen References dumped to file {}", screenref_dump_out_file), fe::MsgType::Success);

		// validate bank 15==bank 31 for SUROM files
		if (m_config.boolean_or(c::ID_DUPLICATE_STATIC_BANK, false)) {
			int l_diffs{ m_rom_manager.get_bank_byte_diffs(m_game->m_rom_data, 15, 31) };
			if (l_diffs != 0)
				add_message(
					std::format("Banks 0f and 1f are expected to be identical, but have {} different bytes", l_diffs),
					fe::MsgType::Error);
		}
	}
}

void fe::MainWindow::request_exit_app(void) {
	m_exit_app_requested = true;
}

bool fe::MainWindow::is_exit_app_granted(void) const {
	return m_exit_app_granted;
}
