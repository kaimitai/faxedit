#include "MainWindow.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include "Imgui_helper.h"
#include "fe/xml/Xml_helper.h"
#include "common/klib/Kfile.h"
#include "fe/ROM_Manager.h"
#include "fe/fe_app_constants.h"
#include "fe/sprite/fe_sprite_constants.h"
#include "fe/WorldVisualizer.h"
#include "fe/game/GameManager.h"
#include "fh/HackManager.h"

namespace {
	// translate GUI editor options into rom patch options
	fe::game::RomPatchOptions get_rom_patch_options(const fe::EditorSettings& p_settings) {
		fe::game::RomPatchOptions options{
			.world_chr_data = p_settings.m_patch_world_chr_data,
			.palettes = p_settings.m_patch_palettes,
			.stages = p_settings.m_patch_stages,
			.mattock_animations = p_settings.m_patch_mattock_animations,
			.push_blocks = p_settings.m_patch_push_blocks,
			.jump_on_tiles = p_settings.m_patch_jump_on_tiles,
			.scenes = p_settings.m_patch_scenes,
			.fog = p_settings.m_patch_fog,
			.bg_gfx = p_settings.m_patch_bg_gfx,
			.cinematics = p_settings.m_patch_cinematics,
			.sprite_gfx = p_settings.m_patch_sprite_gfx,
			.bank15_data = p_settings.m_patch_bank15_data,
			.sprite_data = p_settings.m_patch_sprite_data,
			.metadata = p_settings.m_patch_metadata,
			.tilemaps = p_settings.m_patch_tilemaps,
			.apply_sw_pal2mus_hack = p_settings.m_apply_sw_pal2mus_hack,
			.throw_on_cinematic_overflow = p_settings.throw_on_cinematic_overflow
		};
		return options;
	}
}

void fe::MainWindow::save_xml(void) {
	try {
		fe::game::save_game_xml_to_file(m_config, m_game.value(), get_xml_path(), m_msg_callback);
	}
	catch (const std::runtime_error& p_ex) {
		add_message(p_ex.what(), fe::MsgType::Error);
	}
	catch (const std::exception& p_ex) {
		add_message(p_ex.what(), fe::MsgType::Error);
	}
}

void fe::MainWindow::patch_nes_rom(bool p_in_place) {
	try {
		std::string l_out_file{ p_in_place ? m_loaded_rom_path : get_nes_path() };
		fe::game::patch_rom_to_file(m_config, m_game.value(),
			l_out_file, get_rom_patch_options(m_settings), m_msg_callback);
	}
	catch (const std::exception& ex) {
		add_message(ex.what(), fe::MsgType::Error);
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

			try {
				fe::game::generate_ips_to_file(m_config, m_game.value(), get_ips_path(),
					get_rom_patch_options(m_settings), m_msg_callback);
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), fe::MsgType::Error);
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
		// load new game into a temporary
		fe::Game new_game{ fe::game::load_game_xml_from_file(m_config, get_xml_path(), m_game->m_rom_data, m_msg_callback) };

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
