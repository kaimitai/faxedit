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

	const auto message_callback = [this](const std::string& p_message) -> void {
		add_message(p_message, 6);
		};

	const auto mark_last_message_success = [this](void) -> void {
		if (!m_messages.empty())
			m_messages.front().status = 2;
		};

	const auto get_script_dir = [this](void) -> std::filesystem::path {
		const std::filesystem::path dir{ m_path / std::format("scripts-{}", m_filename) };

		klib::file::create_directories(dir);
		return dir;
		};

	const auto get_script_path = [&get_script_dir](const std::string& p_name,
		const std::string& p_extension) -> std::string {
			return (get_script_dir() / std::format("{}.{}", p_name, p_extension)).string();
		};


	bool l_shift{ ImGui::IsKeyDown(ImGuiMod_Shift) };

	ui::imgui_screen("Scripting",
		c::WIN_TILEMAP_X + 70, c::WIN_TILEMAP_Y + 70,
		c::WIN_TILEMAP_W - 400, c::WIN_TILEMAP_H + 50);

	if (ImGui::BeginTabBar("scripting-tabs")) {

		ImGui::PushStyleColor(ImGuiCol_Tab, ui::g_uiStyles[2].normal);
		ImGui::PushStyleColor(ImGuiCol_TabActive, ui::g_uiStyles[2].active);
		ImGui::PushStyleColor(ImGuiCol_TabHovered, ui::g_uiStyles[2].hovered);

		if (ImGui::BeginTabItem("iScripts")) {

			ImGui::SeparatorText("iScript Assembly");

			if (ui::imgui_button("Assemble", 2, "Assemble iScripts from asm-file and patch ROM")) try {
				const auto opcode_info{ fi::load_iscript_opcodes_from_config(m_config.bmap_dense(fe::c::ID_ISCRIPT_OPCODES),
					m_config.str_map(fe::c::ID_ISCRIPT_OPCODE_IMPLS)) };

				auto xrom{
					fe::script::asm_iscripts(m_config,
						m_game->m_rom_data,
						klib::file::read_file_as_strings(get_script_path("iscript", "asm")),
						opcode_info,
						false,
						message_callback)
				};

				m_game->m_rom_data = std::move(xrom);

				// TODO: Parse each entrypoint independently during static analysis; whole-layer validation may not
				// exercise every control-flow path from every entrypoint
				if (!refresh_iscript_cache(m_game->m_rom_data))
					throw std::runtime_error("iScript assembly succeeded, but GUI cache refresh failed");

				add_message("iScript assembly succeeded", 2);
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), 1);
			}

			ImGui::SeparatorText("iScript Disassembly");

			if (ui::imgui_button("Disassemble", 2, "Disassemble iScripts from ROM and write to file (hold shift to overwrite existing file)")) try {
				fe::script::disasm_iscripts_to_file(m_config, m_game->m_rom_data,
					get_script_path("iscript", "asm"), ls_shop_data_as_comments, l_shift, message_callback);
				mark_last_message_success();
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), 1);
			}

			ui::imgui_checkbox("Shop Contents as Comments", ls_shop_data_as_comments,
				"Append shop contents as comments to shop-related code in the output asm-file");

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("bScripts")) {

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("mScripts")) {

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Miscellaneous")) {

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("MML (music)")) {

			ImGui::EndTabItem();
		}

		ImGui::PopStyleColor(3);

		ImGui::EndTabBar();
	}

	ImGui::End();
}
