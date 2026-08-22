#include "MainWindow.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include "Imgui_helper.h"
#include "common/klib/Kfile.h"
#include "fe/fe_app_constants.h"
#include "fe/script/ScriptManager.h"

void fe::MainWindow::draw_scripting_window(void) {
	static bool ls_shop_data_as_comments{ true };
	static bool ls_emit_mscript_notes{ true };
	static bool ls_include_all_sprites{ false };
	static bool ls_lilypond_percussion{ false };

	const auto get_script_dir = [this](void) -> std::filesystem::path {
		const std::filesystem::path dir{ m_path / std::format("{}-scripts", m_filename) };
		klib::file::create_directories(dir);
		return dir;
		};

	const auto get_script_path = [this, &get_script_dir](const std::string& p_name,
		const std::string& p_extension) -> std::string {
			return (get_script_dir() /
				(this->m_filename + "-" + p_name + "." + p_extension)).string();
		};

	const auto get_mml_path = [this, &get_script_dir]() -> std::string {
		return (get_script_dir() / (this->m_filename + ".mml")).string();
		};

	const auto get_script_file_prefix = [&get_script_dir](const std::string& p_name) -> std::string {
		return (get_script_dir() / p_name).string();
		};

	const bool l_shift{ ImGui::IsKeyDown(ImGuiMod_Shift) };

	ui::imgui_screen("Scripting",
		c::WIN_TILEMAP_X + 70, c::WIN_TILEMAP_Y + 70,
		c::WIN_TILEMAP_W - 400, c::WIN_TILEMAP_H / 2);

	if (ImGui::BeginTabBar("scripting-tabs")) {

		ImGui::PushStyleColor(ImGuiCol_Tab, ui::g_uiStyles[2].normal);
		ImGui::PushStyleColor(ImGuiCol_TabActive, ui::g_uiStyles[2].active);
		ImGui::PushStyleColor(ImGuiCol_TabHovered, ui::g_uiStyles[2].hovered);

		if (ui::imgui_tab("iScripts",
			"Interaction scripts for dialogue, shops, quest logic, and other story-related events")) {

			ImGui::SeparatorText("iScript Assembly");

			if (ui::imgui_button("Assemble", 2, "Assemble iScripts from asm-file and patch ROM")) try {
				const auto tmp_config{ hot_reload_config() };
				const auto l_opcode_info{ fe::script::get_iscript_opcode_info(tmp_config) };

				auto xrom{
					fe::script::asm_iscripts(tmp_config,
						m_game->m_rom_data,
						klib::file::read_file_as_strings(get_script_path("iscript", "asm")),
						l_opcode_info,
						false,
						m_msg_callback)
				};

				m_game->m_rom_data = std::move(xrom);
				m_cache.iscript_opcode_info = l_opcode_info;

				refresh_screen_event_handler_cache(m_game->m_rom_data);

				// cannot reasonably fail as assembly already verified the script layer from all entrypoints
				if (!refresh_iscript_cache(m_game->m_rom_data))
					throw std::runtime_error("iScript assembly succeeded, but GUI cache refresh failed");

				add_message("iScript assembly succeeded", fe::MsgType::Success);
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), fe::MsgType::Error);
			}

			ImGui::SeparatorText("iScript Disassembly");

			if (ui::imgui_button("Disassemble", 4, "Disassemble interaction scripts from ROM and write to file (hold shift to overwrite existing file)")) try {
				fe::script::disasm_iscripts_to_file(m_config, m_game->m_rom_data,
					m_cache.iscript_opcode_info.opcodes,
					get_script_path("iscript", "asm"), ls_shop_data_as_comments, l_shift, m_msg_callback);
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), fe::MsgType::Error);
			}

			ui::imgui_checkbox("Shop contents as comments", ls_shop_data_as_comments,
				"Append shop contents as comments to shop-related code in the output asm-file");

			ImGui::EndTabItem();
		}

		if (ui::imgui_tab("bScripts",
			"Sprite behavior scripts that define how enemies, NPCs, items, and other sprites move and behave")) {

			ImGui::SeparatorText("bScript Assembly");

			if (ui::imgui_button("Assemble", 2, "Assemble bScripts from asm-file and patch ROM")) try {

				auto xrom{
					fe::script::asm_bscripts(m_config,
						m_game->m_rom_data,
						klib::file::read_file_as_strings(get_script_path("bscript", "asm")),
						false,
						m_msg_callback)
				};

				m_game->m_rom_data = std::move(xrom);

				add_message("bScript assembly succeeded", fe::MsgType::Success);
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), fe::MsgType::Error);
			}

			ImGui::SeparatorText("bScript Disassembly");

			if (ui::imgui_button("Disassemble", 4, "Disassemble sprite behavior scripts from ROM and write to file (hold shift to overwrite existing file)")) try {
				fe::script::disasm_bscripts_to_file(m_config, m_game->m_rom_data,
					get_script_path("bscript", "asm"), l_shift, m_msg_callback);
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), fe::MsgType::Error);
			}

			ImGui::EndTabItem();
		}

		if (ui::imgui_tab("mScripts",
			"Low-level representation of the game's music, useful for inspection and precise editing")) {

			ImGui::SeparatorText("mScript Assembly");

			if (ui::imgui_button("Assemble", 2, "Assemble mScripts from asm-file and patch ROM")) try {

				auto xrom{
					fe::script::asm_mscripts(m_config,
						m_game->m_rom_data,
						klib::file::read_file_as_strings(get_script_path("mscript", "asm")),
						m_msg_callback)
				};

				m_game->m_rom_data = std::move(xrom);

				// this should truly be impossible
				if (!refresh_mscript_cache(m_game->m_rom_data))
					throw std::runtime_error("mScript assembly succeeded, but GUI cache refresh failed");

				add_message("mScript assembly succeeded", fe::MsgType::Success);
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), fe::MsgType::Error);
			}

			ImGui::SeparatorText("mScript Disassembly");

			if (ui::imgui_button("Disassemble", 4, "Disassemble music scripts from ROM and write to file (hold shift to overwrite existing file)")) try {
				fe::script::disasm_mscripts_to_file(m_config, m_game->m_rom_data,
					get_script_path("mscript", "asm"), ls_emit_mscript_notes, l_shift, m_msg_callback);
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), fe::MsgType::Error);
			}

			ui::imgui_checkbox("Emit note names", ls_emit_mscript_notes,
				"Use note names instead of raw hex values in the output asm-file");

			ImGui::EndTabItem();
		}

		if (ui::imgui_tab("Miscellaneous",
			"Assorted game data and parameters in a simple text format, including enemy attributes, item settings, gameplay values, and various text")) {

			ImGui::SeparatorText("Miscellaneous Build");

			if (ui::imgui_button("Build", 2, "Build misc data from txt-file and patch ROM")) try {

				auto xrom{
					fe::script::build_misc(m_config,
						m_game->m_rom_data,
						klib::file::read_file_as_strings(get_script_path("misc", "txt")),
						m_msg_callback)
				};

				m_game->m_rom_data = std::move(xrom);

				add_message("Miscellaneous data build succeeded", fe::MsgType::Success);
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), fe::MsgType::Error);
			}

			ImGui::SeparatorText("Miscellaneous Extraction");

			if (ui::imgui_button("Extract", 4, "Extract misc data from ROM and write to file (hold shift to overwrite existing file)")) try {
				fe::script::extract_misc_to_file(m_config, m_game->m_rom_data,
					get_script_path("misc", "txt"), ls_include_all_sprites, l_shift, m_msg_callback);
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), fe::MsgType::Error);
			}

			ui::imgui_checkbox("Include all sprites", ls_include_all_sprites,
				"Extract attributes for all sprites (otherwise only enemies and bosses)");

			ImGui::EndTabItem();
		}

		if (ui::imgui_tab("MML (music)",
			"Music Macro Language (MML), the high-level format recommended for composing and editing the game's music")) {

			ImGui::SeparatorText("MML Compilation");

			if (ui::imgui_button("Compile", 2, "Compile music from MML-file and patch ROM")) try {

				auto xrom{
					fe::script::compile_mml(m_config,
						m_game->m_rom_data,
						klib::file::read_file_as_strings(get_mml_path()),
						m_msg_callback)
				};

				m_game->m_rom_data = std::move(xrom);

				// this should truly be impossible
				if (!refresh_mscript_cache(m_game->m_rom_data))
					throw std::runtime_error("MML compilation succeeded, but GUI cache refresh failed");

				add_message("MML compilation succeeded", fe::MsgType::Success);
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), fe::MsgType::Error);
			}

			ImGui::SeparatorText("MML Decompilation");

			if (ui::imgui_button("Decompile", 4, "Decompile MML from ROM and write to file (hold shift to overwrite existing file)")) try {
				fe::script::decompile_mml_to_file(m_config, m_game->m_rom_data,
					get_mml_path(), l_shift, m_msg_callback);
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), fe::MsgType::Error);
			}

			ImGui::SeparatorText("MIDI");

			if (ui::imgui_button("ROM to MIDI", 2, "Generate MIDI files directly from ROM")) try {
				fe::script::rom_to_midi_files(m_config, m_game->m_rom_data,
					get_script_file_prefix(m_filename),
					m_msg_callback);
				add_message("MIDI files generated from ROM", fe::MsgType::Success);
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), fe::MsgType::Error);
			}

			ImGui::SameLine();

			if (ui::imgui_button("MML to MIDI", 2, "Generate MIDI files from MML")) try {
				fe::script::mml_to_midi_files(get_mml_path(),
					get_script_file_prefix(m_filename),
					m_msg_callback);
				add_message("MIDI files generated from MML", fe::MsgType::Success);
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), fe::MsgType::Error);
			}

			ImGui::SeparatorText("LilyPond");

			if (ui::imgui_button("ROM to LilyPond", 2, "Generate LilyPond files directly from ROM")) try {
				fe::script::rom_to_lilypond_files(m_config, m_game->m_rom_data,
					get_script_file_prefix(m_filename),
					ls_lilypond_percussion,
					m_msg_callback);
				add_message("LilyPond files generated from ROM", fe::MsgType::Success);
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), fe::MsgType::Error);
			}

			ImGui::SameLine();

			if (ui::imgui_button("MML to LilyPond", 2, "Generate LilyPond files from MML")) try {
				fe::script::mml_to_lilypond_files(get_mml_path(),
					get_script_file_prefix(m_filename),
					ls_lilypond_percussion,
					m_msg_callback);
				add_message("LilyPond files generated from MML", fe::MsgType::Success);
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), fe::MsgType::Error);
			}

			ui::imgui_checkbox("Add LilyPond percussion track", ls_lilypond_percussion);

			ImGui::EndTabItem();
		}

		ImGui::PopStyleColor(3);

		ImGui::EndTabBar();
	}

	ImGui::End();
}
