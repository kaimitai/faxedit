#include "MainWindow.h"
#include "gfx.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include "./../common/imguifiledialog/ImGuiFileDialog.h"
#include <algorithm>
#include <filesystem>
#include <map>
#include "./../common/klib/Bitreader.h"
#include "Imgui_helper.h"
#include "./../fe/fe_constants.h"
#include "./../fe/fe_app_constants.h"
#include "./../fe/xml/Xml_helper.h"
#include "./../common/klib/Kfile.h"

fe::MainWindow::MainWindow(SDL_Renderer* p_rnd, const std::string& p_filepath,
	const std::string& p_region) :
	m_sel_chunk{ 0 }, m_sel_screen{ 0 }, m_sel_door{ 0 },
	m_sel_sprite{ 0 }, m_sel_tile_x{ 0 }, m_sel_tile_y{ 0 },
	m_sel_tile_x2{ 16 }, m_sel_tile_y2{ 0 },
	m_gfx{ fe::gfx(p_rnd) },
	m_atlas_palette_no{ 1 },
	m_atlas_tileset_no{ 1 },
	m_atlas_new_tileset_no{ 0 },
	m_atlas_new_palette_no{ 0 },
	m_atlas_force_update{ true },
	m_sel_metatile{ 0 },
	m_sel_tilemap_sub_palette{ 0 },
	m_sel_nes_tile{ 0x80 },
	m_sel_spawn_location{ 0 },
	m_sel_npc_bundle{ 0 },
	m_sel_stage{ 0 },
	m_sel_iscript{ 0 },
	m_sel_gfx_ts_world{ 0 },
	m_sel_gfx_ts_screen{ 0 },
	m_emode{ fe::EditMode::Tilemap },
	m_chr_picker_mode{ fe::ChrPickerMode::HUD },
	m_gfx_emode{ fe::GfxEditMode::WorldChr },
	m_pulse_color{ 255, 255, 102, 255 }, // light yellow },
	m_pulse_time{ 0.0f },
	m_anim_time{ 0.0f },
	m_anim_frame{ 0 },
	m_iscript_window{ false },
	m_gfx_window{ false },
	m_iscript_win_set_focus{ false },
	m_animate{ true },
	m_mattock_overlay{ false },
	m_door_req_overlay{ true },
	m_show_grid{ false },
	m_overlays{ std::vector<char>(16, false) },
	m_show_sprite_sets_in_buildings{ false },
	// config variables, will be loaded with the config xml
	m_sprite_count{ 0 },
	m_iscript_count{ 0 },
	// exit handler variables
	m_exit_app_requested{ false },
	m_exit_app_granted{ false }
{
	add_message("It is recommended to read the documentation for usage tips", 5);
	add_message("For script and music editing try FaxIScripts (https://github.com/kaimitai/FaxIScripts)", 2);
	add_message("Transitions Mode: Shift+Left Click to move OW-transition destinations, Ctrl+Left Click to move SW-transition destinations", 4);
	add_message("Sprites Mode: Shift+Left Click to move sprites", 4);
	add_message("Doors Mode: Shift+Left Click to move doors, Ctrl+Left Click to move destionation position", 4);
	add_message("              Ctrl+Z (undo), Ctrl+Y (redo)", 4);
	add_message("Tilemap Mode: Ctrl+C (copy), Ctrl+V (paste), Shift+V (Show selection), Ctrl+Left Click to \"tile pick\", Right Click to paint", 4);
	add_message(std::format("Build date: {} {} CET",
		__DATE__, __TIME__), 5);
	add_message(std::format("Version: {}", c::APP_VERSION), 5);
	add_message("https://github.com/kaimitai/faxedit", 2);
	add_message("Welcome to Echoes of Eolis by Kai E. Froeland <kai.froland@gmail.com>", 2);

	if (!p_filepath.empty())
		load_rom(p_rnd, p_filepath, p_region);

	m_gfx.generate_icon_overlays(p_rnd);
}

void fe::MainWindow::generate_textures(SDL_Renderer* p_rnd) {

	// ensure the atlas will be generated
	m_atlas_new_tileset_no = m_game->get_default_tileset_no(0, 0);
	m_atlas_new_palette_no = m_game->get_default_palette_no(0, 0);

	// TODO: generate sprite textures
}

void fe::MainWindow::draw(SDL_Renderer* p_rnd) {

	if (m_exit_app_requested) {
		if (!m_game.has_value())
			m_exit_app_granted = true;
		else
			draw_exit_app_window(p_rnd);
	}
	else if (m_game.has_value()) {

		const SDL_Color lc_pulse_start{ 153, 153, 0, 255 };   // dark yellow
		const SDL_Color lc_pulse_end{ 255, 255, 102, 255 }; // light yellow

		// update pulsating color and animation frame counter
		float deltatime{ ImGui::GetIO().DeltaTime };
		m_pulse_time += deltatime;
		m_anim_time += deltatime;
		float t = 0.5f * (1.0f + sinf(m_pulse_time * 5.0f));
		if (m_anim_time > 0.25f) {
			m_anim_time = 0;
			++m_anim_frame;
		}

		m_pulse_color.r = static_cast<Uint8>(lc_pulse_start.r + (lc_pulse_end.r - lc_pulse_start.r) * t);
		m_pulse_color.g = static_cast<Uint8>(lc_pulse_start.g + (lc_pulse_end.g - lc_pulse_start.g) * t);
		m_pulse_color.b = static_cast<Uint8>(lc_pulse_start.b + (lc_pulse_end.b - lc_pulse_start.b) * t);
		m_pulse_color.a = 255;

		// input handling, move to separate function later
		if (m_emode == fe::EditMode::Tilemap) {
			if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_C)) {
				clipboard_copy();
			}
			else if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_V)) {
				clipboard_paste();
			}
			else if (ImGui::IsKeyDown(ImGuiMod_Shift) && ImGui::IsKeyPressed(ImGuiKey_V)) {
				clipboard_paste(false);
			}

			else if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_Z)) {
				if (!m_undo->undo(m_sel_chunk, m_sel_screen))
					add_message("No tilemap changes to undo", 4);
			}
			else if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_Y)) {
				if (!m_undo->redo(m_sel_chunk, m_sel_screen))
					add_message("No tilemap changes to redo", 4);
			}
		}

		// input handling end
		regenerate_atlas_if_needed(p_rnd);

		SDL_SetRenderDrawColor(p_rnd, 0, 33, 71, 0);
		SDL_RenderClear(p_rnd);

		ImGui_ImplSDLRenderer3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		const auto& l_chunk{ m_game->m_chunks.at(m_sel_chunk) };
		const auto& l_screen{ m_game->m_chunks.at(m_sel_chunk).m_screens.at(m_sel_screen) };
		const auto& l_tilemap{ l_screen.m_tilemap };
		std::size_t l_tileset{ m_game->get_default_tileset_no(m_sel_chunk, m_sel_screen) };
		byte mattock_mt_id{ l_chunk.m_mattock_animation.at(0) };

		for (int y{ 0 }; y < 13; ++y)
			for (int x{ 0 }; x < 16; ++x) {

				byte mt_no = l_tilemap.at(y).at(x);
				byte blockprop{ l_chunk.m_metatiles[mt_no].m_block_property };

				const auto& l_metatile{ m_game->m_chunks.at(m_sel_chunk).m_metatiles.at(mt_no) };
				const auto& l_mt_tilemap{ l_metatile.m_tilemap };
				byte l_pal_no{ l_metatile.get_palette_attribute(x, y) };

				m_gfx.blit_to_screen(p_rnd, l_mt_tilemap.at(0).at(0), l_pal_no, 2 * x, 2 * y);
				m_gfx.blit_to_screen(p_rnd, l_mt_tilemap.at(0).at(1), l_pal_no, 2 * x + 1, 2 * y);
				m_gfx.blit_to_screen(p_rnd, l_mt_tilemap.at(1).at(0), l_pal_no, 2 * x, 2 * y + 1);
				m_gfx.blit_to_screen(p_rnd, l_mt_tilemap.at(1).at(1), l_pal_no, 2 * x + 1, 2 * y + 1);

				// draw overlay
				if (m_overlays[blockprop])
					m_gfx.draw_icon_overlay(p_rnd, x, y, blockprop);
				if (mt_no == mattock_mt_id && m_mattock_overlay)
					m_gfx.draw_icon_overlay(p_rnd, x, y, 16);
			}

		// draw grid if enabled
		if (m_show_grid)
			m_gfx.draw_gridlines_on_screen(p_rnd);

		// draw selected rectangle

		if (m_emode == fe::EditMode::Tilemap) {
			if (m_sel_tile_x2 < 16) {
				const auto l_rect{ get_selection_dims() };

				m_gfx.draw_rect_on_screen(p_rnd, SDL_Color(255, 120, 0, 255),
					static_cast<int>(l_rect.x), static_cast<int>(l_rect.y),
					static_cast<int>(l_rect.w), static_cast<int>(l_rect.h));
			}
			else {
				m_gfx.draw_rect_on_screen(p_rnd, SDL_Color(255, 255, 0, 255),
					static_cast<int>(m_sel_tile_x),
					static_cast<int>(m_sel_tile_y), 1, 1);
			}
		}
		else if (m_emode == fe::EditMode::Sprites) {
			bool l_building{ m_sel_chunk == c::CHUNK_IDX_BUILDINGS };
			bool l_showsprites{ !l_building || m_show_sprite_sets_in_buildings };

			if (l_showsprites)
				draw_sprites(p_rnd,
					l_building ? m_game->m_npc_bundles.at(m_sel_npc_bundle).m_sprites : l_screen.m_sprite_set.m_sprites,
					l_building ? m_sel_npc_bundle_sprite : m_sel_sprite);
		}
		else if (m_emode == fe::EditMode::Doors) {
			// draw placeholder rectangles for doors
			for (std::size_t d{ 0 }; d < l_screen.m_doors.size(); ++d) {
				const auto& l_door{ l_screen.m_doors[d] };

				m_gfx.draw_rect_on_screen(p_rnd,
					d == m_sel_door ? m_pulse_color : SDL_Color(70, 100, 160, 255),
					l_door.m_coords.first, l_door.m_coords.second,
					1, 1
				);

				// draw requirement
				if (m_door_req_overlay) {
					byte l_dreq{ 0 };
					if (l_door.m_door_type == fe::DoorType::Building ||
						l_door.m_door_type == fe::DoorType::SameWorld) {
						l_dreq = l_door.m_requirement;
					}
					else {
						auto l_stage{ m_game->m_stages.get_stage_from_world(m_sel_chunk) };
						if (l_stage.has_value()) {
							if (l_door.m_door_type == fe::DoorType::NextWorld)
								l_dreq = l_stage.value()->m_next_requirement;
							else
								l_dreq = l_stage.value()->m_prev_requirement;
						}
					}

					m_gfx.draw_door_req(p_rnd, l_door.m_coords.first,
						l_door.m_coords.second, l_dreq);
				}

			}
		}

		draw_screen_tilemap_window(p_rnd);
		draw_control_window(p_rnd);
		draw_metadata_window(p_rnd);

		if (m_iscript_window)
			draw_iscript_window(p_rnd);

		if (m_gfx_window)
			draw_gfx_window(p_rnd);

		ImGui::Render();
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), p_rnd);
	}
	else {
		draw_filepicker_window(p_rnd);
	}
}

void fe::MainWindow::imgui_text(const std::string& p_str) const {
	ImGui::Text(p_str.c_str());
}

void fe::MainWindow::show_output_messages(void) const {
	ImGui::SeparatorText("Output Messages");
	for (const auto& msg : m_messages) {
		ImGui::PushStyleColor(ImGuiCol_Text, ui::g_uiStyles[msg.status].active);
		ImGui::TextUnformatted(msg.text.c_str());
		ImGui::PopStyleColor();
	}
}

void fe::MainWindow::draw_metatile_info(std::size_t p_sel_chunk, std::size_t p_sel_screen,
	std::size_t p_sel_x, std::size_t p_sel_y) {

	byte l_metatile_id{ m_game->m_chunks.at(m_sel_chunk).m_screens.at(m_sel_screen).m_tilemap.at(p_sel_y).at(p_sel_x) };

	ImGui::Begin("Metatile##mt");

	imgui_text("Metatile ID: " +
		klib::Bitreader::byte_to_hex(l_metatile_id));

	imgui_text("Position: " + std::to_string(p_sel_x) + "," + std::to_string(p_sel_y));

	imgui_text("Property: " + klib::Bitreader::byte_to_hex(m_game->m_chunks.at(m_sel_chunk).m_metatiles.at(l_metatile_id).m_block_property));

	ImGui::End();

}

// 1) extract hud tiles (ppu index 0-59)
// 2) inject empty tiles until we hit the world-specific tileset index
// 3) inject the world-specific tileset
// 4( inject empty tiles until we have 256 chr-tiles in total
void fe::MainWindow::generate_world_tilesets(void) {
	std::vector<std::vector<klib::NES_tile>> new_tiles;
	const auto& gtilesets{ m_game->m_tilesets };

	auto hud_tiles{ m_game->get_hud_chr_tiles(m_config) };

	for (const auto& wtileset : gtilesets) {
		std::vector<klib::NES_tile> wpputileset{ hud_tiles };

		while (wpputileset.size() < wtileset.start_idx)
			wpputileset.push_back(klib::NES_tile());

		for (const auto& wtile : wtileset.tiles)
			wpputileset.push_back(wtile);

		while (wpputileset.size() < 256)
			wpputileset.push_back(klib::NES_tile());

		new_tiles.push_back(wpputileset);
	}

	world_ppu_tilesets = new_tiles;
}

void fe::MainWindow::regenerate_atlas_if_needed(SDL_Renderer* p_rnd) {

	if (m_atlas_new_tileset_no != m_atlas_tileset_no ||
		m_atlas_new_palette_no != m_atlas_palette_no ||
		m_atlas_force_update) {

		m_gfx.generate_atlas(p_rnd, world_ppu_tilesets.at(m_atlas_new_tileset_no),
			m_game->m_palettes.at(m_atlas_new_palette_no));
		generate_metatile_textures(p_rnd);

		m_atlas_tileset_no = m_atlas_new_tileset_no;
		m_atlas_palette_no = m_atlas_new_palette_no;
		m_atlas_force_update = false;
	}
}

// can be regenerated independently of the atlas
void fe::MainWindow::generate_metatile_textures(SDL_Renderer* p_rnd,
	std::size_t p_mt_no) {
	const auto& mts{ m_game->m_chunks.at(m_sel_chunk).m_metatiles };

	if (p_mt_no == 256) {
		for (std::size_t i{ 0 }; i < mts.size(); ++i)
			m_gfx.generate_mt_texture(p_rnd,
				m_game->m_chunks.at(m_sel_chunk).m_metatiles.at(i).m_tilemap,
				i, mts[i].m_attr_tl);
	}
	else if (p_mt_no < mts.size())
		m_gfx.generate_mt_texture(p_rnd,
			m_game->m_chunks.at(m_sel_chunk).m_metatiles.at(p_mt_no).m_tilemap,
			p_mt_no, mts[p_mt_no].m_attr_tl);
}

std::string fe::MainWindow::get_description(byte p_index,
	const std::map<byte, std::string>& p_map) const {
	const auto& iter{ p_map.find(p_index) };

	if (iter == end(p_map))
		return std::format("Unknown ({})", klib::Bitreader::byte_to_hex(p_index));
	else
		return std::format("{} ({})", iter->second, klib::Bitreader::byte_to_hex(p_index));
}

std::string fe::MainWindow::get_description(byte p_index,
	const std::vector<std::string>& p_vec) const {
	if (p_index < p_vec.size())
		return std::format("{} ({})", p_vec[p_index], klib::Bitreader::byte_to_hex(p_index));
	else
		return std::format("Unknown ({})", klib::Bitreader::byte_to_hex(p_index));
}

std::string fe::MainWindow::get_sprite_label(std::size_t p_sprite_id) const {
	return std::format("{} ({})",
		m_labels_sprites[p_sprite_id],
		fe::Sprite_gfx_definiton::SpriteCatToString(m_sprite_dims[p_sprite_id].m_cat));
}

void fe::MainWindow::add_message(const std::string& p_msg, int p_status) {
	if (m_messages.size() > 50)
		m_messages.pop_back();

	if (m_messages.empty() || m_messages.front().text != p_msg)
		m_messages.push_front(fe::Message(p_msg, p_status));
}

// remember to push and pop IDs before calling this function
std::optional<std::pair<byte, byte>> fe::MainWindow::show_position_slider(byte p_x, byte p_y) {

	byte l_x{ p_x }, l_y{ p_y };
	bool l_updated{ false };

	imgui_text("x");
	ImGui::SameLine();
	if (ui::imgui_slider_with_arrows("pslx", "", l_x, 0, 15))
		l_updated = true;

	imgui_text("y");
	ImGui::SameLine();
	if (ui::imgui_slider_with_arrows("psly", "", l_y, 0, 12))
		l_updated = true;

	if (l_updated)
		return std::make_pair(l_x, l_y);
	else
		return std::nullopt;
}

void fe::MainWindow::show_sprite_screen(fe::Sprite_set& p_sprites, std::size_t& p_sel_sprite) {
	std::size_t l_sprite_cnt{ p_sprites.size() };

	auto& l_sprites{ p_sprites.m_sprites };

	if (l_sprites.empty()) {
		imgui_text("No sprites defined");
	}
	else {
		std::size_t l_sprite_cnt{ l_sprites.size() };

		if (p_sel_sprite >= l_sprite_cnt)
			p_sel_sprite = l_sprite_cnt - 1;

		auto& l_sprite{ l_sprites[p_sel_sprite] };

		ui::imgui_slider_with_arrows("###spritesel",
			std::format("Selected sprite: #{}/{}", p_sel_sprite, l_sprite_cnt),
			p_sel_sprite, 0, l_sprite_cnt - 1, "", false, true);

		ImGui::SeparatorText(std::format("Sprite ID: {}", l_sprite.m_id).c_str());

		ui::imgui_slider_with_arrows("###spriteid",
			get_sprite_label(l_sprite.m_id),
			l_sprite.m_id, 0, m_sprite_count - 1);

		ImGui::SeparatorText("Position");

		auto l_new_pos{ show_position_slider(l_sprite.m_x, l_sprite.m_y) };

		if (l_new_pos.has_value()) {
			l_sprite.m_x = l_new_pos.value().first;
			l_sprite.m_y = l_new_pos.value().second;
		}

		ImGui::SeparatorText("Script");

		if (!l_sprite.m_text_id.has_value()) {
			imgui_text("No script defined");

			if (ui::imgui_button("Add script", 2, "Define a script for the sprite"))
				l_sprite.m_text_id = 0;
		}
		else {
			ui::imgui_slider_with_arrows("###sprdiag",
				std::format("Script: {}", l_sprite.m_text_id.value()),
				l_sprite.m_text_id.value(), 0, m_iscript_count);

			if (ui::imgui_button("View script", 4)) {
				m_sel_iscript = l_sprite.m_text_id.value();
				m_iscript_window = true;
				m_iscript_win_set_focus = true;
			}
			ImGui::SameLine();
			if (ui::imgui_button("Remove script", 1))
				l_sprite.m_text_id.reset();
		}

		ImGui::Separator();
	}

	ImGui::SeparatorText("Add or remove sprites");

	if (ui::imgui_button("Add sprite", 2, "", l_sprites.size() == 0xff))
		p_sprites.push_back(fe::Sprite(0x2a, 0, 0));

	if (!p_sprites.empty()) {
		ImGui::SameLine();

		if (ui::imgui_button("Remove sprite", 1))
			l_sprites.erase(begin(l_sprites) + p_sel_sprite--);
	}

	ImGui::SeparatorText("Command Byte");
	auto& l_cb{ p_sprites.m_command_byte };

	if (l_cb.has_value()) {
		ui::imgui_slider_with_arrows("###sscb",
			std::format("Value: {}", get_description(l_cb.value(), m_labels_cmd_byte)),
			p_sprites.m_command_byte.value(), 0, 2, "Special events for the screen");
		if (ui::imgui_button("Delete command byte", 1))
			p_sprites.m_command_byte.reset();
	}
	else {
		if (ui::imgui_button("Add command byte", 2, ""))
			p_sprites.m_command_byte = 0x01;
	}
}

void fe::MainWindow::show_sprite_npc_bundle_screen(void) {
	auto l_bset{ m_game->m_npc_bundles };
	if (m_sel_npc_bundle >= l_bset.size())
		m_sel_npc_bundle = l_bset.size() - 1;

	ImGui::SeparatorText(std::format("Building Sprite Set: {}", m_sel_npc_bundle).c_str());

	ui::imgui_slider_with_arrows("###npcbsel", "", m_sel_npc_bundle,
		0, l_bset.size() - 1, "", false, true);

	ImGui::SeparatorText("Building Parameter Sprites");

	ImGui::PushID("###bldparam");

	show_sprite_screen(m_game->m_npc_bundles.at(m_sel_npc_bundle),
		m_sel_npc_bundle_sprite);

	auto sbp{ m_labels_spec_sprite_sets.find(static_cast<byte>(m_sel_npc_bundle)) };

	if (sbp != end(m_labels_spec_sprite_sets)) {
		ImGui::Separator();
		imgui_text(std::format("Special Significance: {}", sbp->second));
	}

	ImGui::PopID();

	ImGui::SeparatorText("Add or Remove Building Sprite Sets");

	if (ui::imgui_button("Add Sprite Set", 2, "", l_bset.size() == 0xff))
		m_game->m_npc_bundles.push_back(fe::Sprite_set());

	ImGui::SameLine();

	// don't delete below 70 - there are some hard coded references in the game code
	if (ui::imgui_button("Remove Sprite Set", 1, "", !ImGui::IsKeyDown(ImGuiKey_ModShift)
		|| l_bset.size() <= 70 || m_sel_npc_bundle != m_game->m_npc_bundles.size() - 1)) {

		bool l_bset_used{ false };
		for (const auto& chunk : m_game->m_chunks)
			for (const auto& scr : chunk.m_screens)
				for (const auto& door : scr.m_doors)
					if (door.m_door_type == fe::DoorType::Building &&
						door.m_npc_bundle == m_sel_npc_bundle)
						l_bset_used = true;

		if (l_bset_used)
			add_message("Building Sprite Set has references", 1);
		else {
			m_game->m_npc_bundles.erase(begin(m_game->m_npc_bundles) + m_sel_npc_bundle);
			if (m_sel_npc_bundle >= m_game->m_npc_bundles.size())
				m_sel_npc_bundle = m_game->m_npc_bundles.size() - 1;
			if (m_sel_npc_bundle_sprite >= m_game->m_npc_bundles.at(m_sel_npc_bundle).size())
				m_sel_npc_bundle_sprite = m_game->m_npc_bundles.at(m_sel_npc_bundle).size() - 1;
		}
	}
}

std::string fe::MainWindow::get_editmode_as_string(void) const {
	if (m_emode == fe::EditMode::Tilemap)
		return "Tilemap";
	else if (m_emode == fe::EditMode::Sprites)
		return "Sprites";
	else if (m_emode == fe::EditMode::Doors)
		return "Doors";
	else if (m_emode == fe::EditMode::Transitions)
		return "Transitions";
	else if (m_emode == fe::EditMode::Scrolling)
		return "Scrolling";
	else
		return "Other";
}

fe::Size4 fe::MainWindow::get_selection_dims(void) const {
	if (m_sel_tile_x2 == 16)
		return Size4(m_sel_tile_x, m_sel_tile_y, 1, 1);
	else
		return Size4(
			std::min(m_sel_tile_x, m_sel_tile_x2),
			std::min(m_sel_tile_y, m_sel_tile_y2),
			std::max(m_sel_tile_x, m_sel_tile_x2) - std::min(m_sel_tile_x, m_sel_tile_x2) + 1,
			std::max(m_sel_tile_y, m_sel_tile_y2) - std::min(m_sel_tile_y, m_sel_tile_y2) + 1
		);
}

void fe::MainWindow::clipboard_copy(void) {
	const auto l_rect{ get_selection_dims() };
	const auto& l_tm{ m_game->m_chunks.at(m_sel_chunk).m_screens.at(m_sel_screen).m_tilemap };

	std::vector<std::vector<byte>> l_clip;

	for (std::size_t j{ l_rect.y }; j < l_rect.y + l_rect.h; ++j) {
		std::vector<byte> l_tmp;

		for (std::size_t i{ l_rect.x }; i < l_rect.x + l_rect.w; ++i)
			l_tmp.push_back(l_tm.at(j).at(i));

		l_clip.push_back(l_tmp);
	}

	m_clipboard[m_sel_chunk] = l_clip;

	add_message(std::format("Copied {}x{} rectangle to world {} clipboard",
		l_clip.at(0).size(), l_clip.size(), m_sel_chunk));
}

void fe::MainWindow::clipboard_paste(bool l_update_data) {
	const auto& l_clip{ m_clipboard[m_sel_chunk] };

	if (l_clip.empty())
		add_message(std::format("Clipboard for world {} is empty", m_sel_chunk));
	else if (m_sel_tile_y + l_clip.size() > 13 ||
		m_sel_tile_x + l_clip[0].size() > 16)
		add_message("Clipboard data does not fit.");
	// all good, paste or at least show selection rectangle
	else {
		if (l_update_data) {
			m_undo->apply_tilemap_edit(m_sel_chunk, m_sel_screen,
				m_sel_tile_x, m_sel_tile_y, l_clip);
		}

		m_sel_tile_x2 = m_sel_tile_x + l_clip[0].size() - 1;
		m_sel_tile_y2 = m_sel_tile_y + l_clip.size() - 1;

		if (l_update_data)
			add_message("Clipboard data pasted");
	}
}

bool fe::MainWindow::check_patched_size(const std::string& p_data_type, std::size_t p_patch_data_size, std::size_t p_max_data_size) {
	bool l_ok{ p_patch_data_size <= p_max_data_size };

	add_message(std::format("Patching {} {}: Used {} of {} available bytes ({:.2f}%)",
		p_data_type,
		(l_ok ? "succeeded" : "failed"),
		p_patch_data_size, p_max_data_size,
		100.0f * (static_cast<float>(p_patch_data_size) / static_cast<float>(p_max_data_size))), l_ok ? 2 : 1);

	return l_ok;
}

void fe::MainWindow::draw_exit_app_window(SDL_Renderer* p_rnd) {
	SDL_SetRenderDrawColor(p_rnd, 96, 96, 255, 0);
	SDL_RenderClear(p_rnd);

	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	ui::imgui_screen("Close Application?", c::WIN_ROM_X, c::WIN_ROM_Y, c::WIN_ROM_W, c::WIN_ROM_H,
		4);

	if (ui::imgui_button("Return to editor", 2))
		m_exit_app_requested = false;

	ImGui::SameLine();

	if (ui::imgui_button("Exit", 1))
		m_exit_app_granted = true;

	ImGui::End();

	ImGui::Render();
	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), p_rnd);
}

void fe::MainWindow::draw_filepicker_window(SDL_Renderer* p_rnd) {
	SDL_SetRenderDrawColor(p_rnd, 96, 96, 255, 0);
	SDL_RenderClear(p_rnd);

	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	ui::imgui_screen("ROM select", c::WIN_CONTROLS_X, c::WIN_CONTROLS_Y, c::WIN_CONTROLS_W, c::WIN_CONTROLS_H,
		4);

	if (ui::imgui_button("Load nes ROM file", 4, "Load Faxanadu (U).nes")) {
		IGFD::FileDialogConfig config;
		config.path = ".";
		config.flags = ImGuiFileDialogFlags_HideColumnDate;

		ImGuiFileDialog::Instance()->SetFileStyle(
			IGFD_FileStyleByTypeDir, "", ImVec4(0.4f, 0.7f, 1.0f, 1.0f) // Light blue for folders
		);

		ImGuiFileDialog::Instance()->SetFileStyle(
			IGFD_FileStyleByTypeFile, ".nes", ImVec4(1.0f, 0.6f, 0.2f, 1.0f) // Orange for .nes files
		);

		ImGuiFileDialog::Instance()->OpenDialog(
			"ChooseROM",
			"Choose a .nes file",
			".nes", config
		);
	}

	ImGui::SetNextWindowPos(ImVec2(static_cast<float>(c::WIN_ROM_X), static_cast<float>(c::WIN_ROM_Y)), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(static_cast<float>(c::WIN_ROM_W), static_cast<float>(c::WIN_ROM_H)), ImGuiCond_FirstUseEver);

	if (ImGuiFileDialog::Instance()->Display("ChooseROM")) {
		if (ImGuiFileDialog::Instance()->IsOk()) {
			std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();

			load_rom(p_rnd, filePath);
		}
		ImGuiFileDialog::Instance()->Close();
	}

	show_output_messages();

	ImGui::End();

	ImGui::Render();
	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), p_rnd);
}

void fe::MainWindow::load_rom(SDL_Renderer* p_rnd, const std::string& p_filepath,
	const std::string& p_region) {
	std::string l_config_xml_path;

	const char* basePath{ SDL_GetBasePath() };
	if (!basePath) {
		l_config_xml_path = "./";
	}
	else {
		l_config_xml_path = basePath;
	}

	l_config_xml_path += c::CONFIG_FILE_NAME;

	add_message("Attempting to load file " + p_filepath, 5);

	// Load file as bytes and create game
	try {
		auto bytes = klib::file::read_file_as_bytes(p_filepath);

		m_config.clear();
		m_config.load_definitions(l_config_xml_path);

		if (p_region.empty()) {
			m_config.determine_region(bytes);
			add_message(std::format("ROM region detected: '{}'",
				m_config.get_region()), 4);
		}
		else {
			add_message(std::format("Region specified as '{}'", p_region));
			m_config.set_region(p_region);
		}

		m_config.load_config_data(l_config_xml_path);
		cache_config_variables();

		m_game = fe::Game(m_config, bytes);
		m_shared_palettes = m_game->get_shared_palettes(m_config);
		m_game->generate_tilesets(m_config);
		// the game object has world tilesets, let us make a cache of 256
		// tile big tilesets for the UI to send to the renderer
		generate_world_tilesets();
		initialize_hud_tilemap();

		std::filesystem::path romPath(p_filepath);

		m_path = romPath.parent_path();
		m_filename = romPath.stem().string();

		// extract sprite definitions
		const auto& gfx_def{ m_rom_manager.extract_sprite_data(bytes) };

		// use it to extract sprite sizes before it is sent off and discarded
		// sprite sizes are given here in nes tiles, not metatiles
		for (const auto& kv : gfx_def) {
			std::vector<std::pair<int, int>> l_frame_offsets;
			for (const auto& frame : kv.second.m_frames) {
				if (frame.m_disabled)
					continue;

				l_frame_offsets.push_back(std::make_pair(
					static_cast<int>(frame.m_offset_x),
					static_cast<int>(frame.m_offset_y)
				));
			}

			m_sprite_dims.at(kv.first).w = kv.second.w;
			m_sprite_dims[kv.first].h = kv.second.h;
			m_sprite_dims[kv.first].offsets = l_frame_offsets;
			m_sprite_dims[kv.first].m_cat = kv.second.m_category;
		}

		// gen NES palette
		m_gfx.set_nes_palette(m_config.bmap_as_numeric_vec(c::ID_NES_PALETTE, 64));

		// pass it on the gfx handler to make sprite textures
		m_gfx.gen_sprites(p_rnd, gfx_def);

		// extract door requirement gfx
		auto req_gfx{ m_rom_manager.extract_door_req_gfx(bytes) };
		m_gfx.gen_door_req_gfx(p_rnd, req_gfx);

		// extract scripts
		try {
			fi::IScriptLoader loader(m_config, bytes);

			for (std::size_t i{ 0 }; i < m_iscript_count; ++i) {
				try {
					m_iscripts[i] = loader.parse_script(bytes, i);
				}
				catch (...) {
					add_message(std::format("Unable to parse iScript #{}", i));
				}
			}

			m_game->m_spawn_to_script_no = loader.m_spawn_scripts;
		}
		catch (...) {
			add_message("Malformed script section", 1);
		}

		if (m_game->m_chunks.size() > 0)
			m_atlas_new_palette_no = m_game->get_default_palette_no(0, 0);

		m_undo.reset();
		m_undo.emplace(m_game.value());

		add_message("Loaded " + p_filepath, 2);
	}
	catch (const std::runtime_error& ex) {
		add_message(ex.what(), 1);
	}
	catch (const std::exception& ex) {
		add_message(ex.what(), 1);
	}
	catch (...) {
		add_message("Unknown error occurred", 1);
	}

}

// copy some config vars to the GUI so we don't need to look them up
// every draw frame
void fe::MainWindow::cache_config_variables(void) {
	// maps that can be sparse or even empty
	m_labels_cmd_byte = m_config.bmap(c::ID_CMD_BYTE_LABELS);
	m_labels_door_reqs = m_config.bmap(c::ID_DOOR_REQ_LABELS);
	m_labels_block_props = m_config.bmap(c::ID_BLOCK_PROP_LABELS);
	m_labels_palettes = m_config.bmap(c::ID_PALETTE_LABELS);
	m_labels_spec_sprite_sets = m_config.bmap(c::ID_SPECIAL_SPRITE_SET_LABELS);
	m_labels_music = m_config.bmap(c::ID_MUSIC_LABELS);

	// constants
	m_iscript_count = m_config.constant(c::ID_ISCRIPT_COUNT);
	m_sprite_count = m_config.constant(c::ID_SPRITE_COUNT);

	// maps we convert to vectors
	m_labels_worlds = m_config.bmap_as_vec(c::ID_WORLD_LABELS, 8);
	m_labels_sprites = m_config.bmap_as_vec(c::ID_SPRITE_LABELS, m_sprite_count);
	m_labels_buildings = m_config.bmap_as_vec(c::ID_BUILDING_LABELS, c::WORLD_BUILDINGS_SCREEN_COUNT);
	m_labels_tilesets = m_config.bmap_as_vec(c::ID_TILESET_LABELS,
		m_config.constant(c::ID_WORLD_TILESET_COUNT));

	// need sprite count to populate the sprite dimension metadata
	m_sprite_dims = std::vector<fe::AnimationGUIData>(m_sprite_count,
		fe::AnimationGUIData(8, 8, fe::SpriteCategory::Glitched));
}

std::string fe::MainWindow::get_ips_path(void) const {
	return get_filepath("ips", false);
}

std::string fe::MainWindow::get_xml_path(void) const {
	return get_filepath("xml", false);
}

std::string fe::MainWindow::get_nes_path(void) const {
	return get_filepath("nes", true);
}

std::string fe::MainWindow::get_filepath(const std::string& p_ext, bool p_add_out) const {
	std::string stemLower = m_filename;
	std::filesystem::path outputPath;

	if (!p_add_out) {
		outputPath = m_path / (m_filename + "." + p_ext);
	}
	else {
		outputPath = m_path / (m_filename + "-out" + "." + p_ext);
	}

	return outputPath.string();
}

// screen element draw routines
void fe::MainWindow::draw_sprites(SDL_Renderer* p_rnd,
	const std::vector<fe::Sprite>& p_sprites,
	std::size_t p_sel_sprite_no) const {

	for (std::size_t i{ 0 }; i < p_sprites.size(); ++i) {
		std::size_t l_anim_frame{ m_animate ?
			m_anim_frame % m_gfx.get_anim_frame_count(p_sprites[i].m_id) :
			m_gfx.get_anim_frame_count(p_sprites[i].m_id) - 1
		};
		std::size_t l_spriteid{ static_cast<std::size_t>(p_sprites[i].m_id) };

		m_gfx.draw_sprite_on_screen(p_rnd,
			l_spriteid,
			l_anim_frame,
			16 * static_cast<int>(p_sprites[i].m_x) +
			m_sprite_dims[l_spriteid].offsets[l_anim_frame].first,
			16 * static_cast<int>(p_sprites[i].m_y) +
			m_sprite_dims[l_spriteid].offsets[l_anim_frame].second
		);
	}

	// draw rectangle for selected sprite
	if (p_sel_sprite_no < p_sprites.size()) {
		const auto& l_sprite{ p_sprites[p_sel_sprite_no] };

		m_gfx.draw_pixel_rect_on_screen(p_rnd, m_pulse_color,
			16 * static_cast<int>(l_sprite.m_x),
			16 * static_cast<int>(l_sprite.m_y),
			m_sprite_dims[l_sprite.m_id].w,
			m_sprite_dims[l_sprite.m_id].h
		);
	}

}
