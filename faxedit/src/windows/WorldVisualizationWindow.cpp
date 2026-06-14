#include "MainWindow.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include "Imgui_helper.h"
#include "./../fe/fe_constants.h"
#include "./../fe/fe_app_constants.h"
#include "./../common/klib/Kfile.h"
#include <format>
#include <SDL3/SDL.h>
#include "./../fe/WorldVisualizer.h"
#include "./../fi/fi_constants.h"
#include <unordered_map>
#include <unordered_set>

void fe::MainWindow::export_world_png(const fe::WorldVisualizer& p_visualizer,
	std::size_t p_world, std::size_t p_screen,
	const fe::SpriteGUILoader& p_sprites,
	const fe::WorldVisualizationOptions& p_options) {
	const auto res{ p_visualizer.visualize_world(m_config, *m_game, p_world, p_screen,
	p_sprites, p_options) };

	std::string png_path{ std::format("{}/{}-png", m_path.string(), m_filename) };
	std::string png_filename{ std::format("world-{:02}", p_world) };

	m_gfx.save_world_visualizer_png(res, png_path, png_filename);

	add_message(std::format("Saved world {} as {}/{}.png", p_world,
		png_path, png_filename), 2);
}

void fe::MainWindow::draw_visualization_window(SDL_Renderer* p_rnd) {
	static fe::WorldVisualizationOptions options;
	static std::size_t l_world{ 0 }, l_screen{ 0 };

	ui::imgui_screen("World Visualizer",
		c::WIN_TILEMAP_X + 50, c::WIN_TILEMAP_Y + 50,
		c::WIN_TILEMAP_W / 2, c::WIN_TILEMAP_H + 50);

	try {

		ImGui::SeparatorText("World and start screen for png export");

		show_world_screen_slider(l_world, l_screen);

		ImGui::Separator();

		if (ui::imgui_button("Export png", 2, "Save the selected world as a PNG image. Hold Shift to export all worlds (starting from screen 0 in each world)")) {
			bool l_shift{ ImGui::IsKeyDown(ImGuiKey_ModShift) };

			fe::SpriteGUILoader visual_sprites;
			visual_sprites.load_sprites_for_gui(m_config,
				m_game->m_sprite_gfx_manager, m_game->m_rom_data);
			auto visualizer{ fe::WorldVisualizer(world_ppu_tilesets,
				extract_script_semantics()) };

			if (l_shift)
				for (std::size_t i{ 0 }; i < m_game->m_chunks.size(); ++i)
					export_world_png(visualizer, i, 0, visual_sprites, options);
			else
				export_world_png(visualizer, l_world, l_screen, visual_sprites, options);
		}

		ImGui::SeparatorText("Sprite Options");

		ui::imgui_checkbox("Enemies", options.draw_enemies);
		ImGui::SameLine();
		ui::imgui_checkbox("NPCs", options.draw_npcs);
		ImGui::SameLine();
		ui::imgui_checkbox("Items", options.draw_items);
		ImGui::SameLine();
		ui::imgui_checkbox("Other", options.draw_other);

		if (ImGui::RadioButton("First Frame",
			!options.use_last_anim_frame))
			options.use_last_anim_frame = false;
		ImGui::SameLine();
		if (ImGui::RadioButton("Last Frame",
			options.use_last_anim_frame))
			options.use_last_anim_frame = true;

		ImGui::SeparatorText("Door Options");

		ui::imgui_checkbox("Door Labels", options.show_door_labels);
		ImGui::SameLine();
		ui::imgui_checkbox("Door Requirements", options.show_door_requirements);

		ImGui::SeparatorText("Script Options");
		ui::imgui_checkbox("Gifts", options.show_gifts,
			"Render NPC gifts");
		ImGui::SameLine();
		ui::imgui_checkbox("Shops", options.show_shops,
			"Render shop contents");

		ImGui::SeparatorText("Other Options");

		ui::imgui_checkbox("Screen Numbers", options.show_screen_numbers);
		ui::imgui_checkbox("Other-World Transitions", options.show_ow_transitions);
		ui::imgui_checkbox("Stage Door Destinations", options.show_stage_door_dests);
		ui::imgui_checkbox("Scroll Connections", options.show_scroll_connections);
		ui::imgui_checkbox("Skip Unreferences Screens", options.skip_unreferenced_screens);

		ui::imgui_slider_with_arrows("###swtol", "SW-Transition tolerance",
			options.sameworld_trans_tolerance, 0, 4,
			"Same-world transition tiles within this distance from a screen edge are treated as scroll connections when building the world graph");
	}
	catch (const std::exception& ex) {
		add_message(ex.what(), 1);
	}

	ImGui::End();
}

std::unordered_map<byte, fe::ScriptSemanticInfo>
fe::MainWindow::extract_script_semantics(void) try {
	std::unordered_map<byte, fe::ScriptSemanticInfo> result;
	const auto& rom_bytes{ m_game->m_rom_data };

	fi::IScriptLoader loader(m_config, rom_bytes);

	for (std::size_t i{ 0 }; i < loader.get_script_count(); ++i) {
		const auto& instrs{ loader.parse_script_raw(rom_bytes, i) };

		for (const auto& kv : instrs) {
			if (!kv.second.operand)
				continue;

			auto op_value{ *kv.second.operand };

			byte opcode{ kv.second.opcode_byte };

			if (opcode == fi::c::OPCODE_GET_ITEM)
				result[static_cast<byte>(i)].gifts.push_back(
					static_cast<byte>(op_value)
				);
			else if (opcode == fi::c::OPCODE_SHOP_BUY ||
				opcode == fi::c::OPCODE_SHOP_SELL) {
				const auto& shops{ loader.get_shops() };


				if (op_value < shops.size())
					for (const auto& entry : shops[op_value].m_entries)
						result[static_cast<byte>(i)].shop_items.insert(entry.m_item);
			}
		}
	}

	return result;
}
catch (const std::exception& ex) {
	add_message(std::format("Script extraction failed: {}", ex.what()), 1);
	return {};
}
