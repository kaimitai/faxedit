#include "MainWindow.h"
#include "Imgui_helper.h"
#include "./../common/klib/Bitreader.h"
#include "./../fe/fe_constants.h"

void fe::MainWindow::draw_metadata_window(SDL_Renderer* p_rnd, fe::Game& p_game) {

	auto& l_chunk{ p_game.m_chunks.at(m_sel_chunk) };

	fe::ui::imgui_screen("World and Game Settings###cw");

	if (ImGui::BeginTabBar("Metadata")) {

		ImGui::PushStyleColor(ImGuiCol_Tab, ui::g_uiStyles[4].normal);
		ImGui::PushStyleColor(ImGuiCol_TabActive, ui::g_uiStyles[4].active);
		ImGui::PushStyleColor(ImGuiCol_TabHovered, ui::g_uiStyles[4].hovered);

		if (ImGui::BeginTabItem(std::format("{} Metadata###wmdtabi", c::LABELS_CHUNKS[m_sel_chunk]).c_str())) {

			if (ImGui::BeginTabBar("WorldMetaTabs")) {

				ImGui::PushStyleColor(ImGuiCol_Tab, ui::g_uiStyles[2].normal);
				ImGui::PushStyleColor(ImGuiCol_TabActive, ui::g_uiStyles[2].active);
				ImGui::PushStyleColor(ImGuiCol_TabHovered, ui::g_uiStyles[2].hovered);

				// CHUNK - METATILES - BEGIN
				if (ImGui::BeginTabItem("Metatiles")) {

					if (m_sel_metatile >= l_chunk.m_metatiles.size())
						m_sel_metatile = 0;

					auto& l_mt_def{ l_chunk.m_metatiles[m_sel_metatile] };

					ImVec2 image_pos = ImGui::GetCursorScreenPos(); // capture position BEFORE drawing

					ImGui::Image(m_gfx.get_metatile_texture(m_sel_metatile), ImVec2(64.0f, 64.0f));


					ImVec2 mouse_pos = ImGui::GetMousePos();

					for (int y = 0; y < 2; ++y) {
						for (int x = 0; x < 2; ++x) {
							int quadrant = y * 2 + x;

							ImVec2 quad_pos = ImVec2(image_pos.x + x * 64.0f / 2, image_pos.y + y * 64.0f / 2);
							ImVec2 quad_end = ImVec2(quad_pos.x + 64.0f / 2, quad_pos.y + 64.0f / 2);

							bool hovered = mouse_pos.x >= quad_pos.x && mouse_pos.x < quad_end.x &&
								mouse_pos.y >= quad_pos.y && mouse_pos.y < quad_end.y;

							if (hovered && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
								l_mt_def.m_tilemap.at(quadrant / 2).at(quadrant % 2) = static_cast<byte>(m_sel_nes_tile);
								generate_metatile_textures(p_rnd, p_game);
							}
							else if (hovered && ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
								m_sel_nes_tile = l_mt_def.m_tilemap.at(quadrant / 2).at(quadrant % 2);

							}

							// Optional: draw outline on hover
							if (hovered) {
								ImGui::GetWindowDrawList()->AddRect(
									quad_pos, quad_end,
									IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.0f
								);
							}
						}
					}



					ImGui::SeparatorText(std::format("Selected metatile: {}", m_sel_metatile).c_str());

					ui::imgui_slider_with_arrows("Metatile###mtdef", "", m_sel_metatile, 0,
						l_chunk.m_metatiles.size() - 1);

					ImGui::SeparatorText(std::format("Block property: {}",
						get_description(l_mt_def.m_block_property,
							c::LABELS_BLOCK_PROPERTIES)).c_str());

					ui::imgui_slider_with_arrows("mtblprop", "",
						l_mt_def.m_block_property, 0x00, 0x0f);

					ImGui::BeginChild("TilePicker", ImVec2(0, 400), true); // scrollable area
					auto l_atlas{ m_gfx.get_atlas() };

					for (std::size_t i = 0x80; i <= 0xff; ++i) {
						// Compute UVs for tile i
						float u0 = (i * 8.0f) / (float)l_atlas->w;
						float v0 = 8.0f * static_cast<float>(m_sel_tilemap_sub_palette);
						float u1 = ((i + 1) * 8.0f) / (float)l_atlas->w;
						float v1 = 8.0f * static_cast<float>(m_sel_tilemap_sub_palette + 1) / (float)l_atlas->h;

						if (ImGui::ImageButton(std::format("###stile{}", i).c_str(),
							l_atlas, ImVec2(32.0f, 32.0f), ImVec2(u0, v0), ImVec2(u1, v1))) {
							m_sel_nes_tile = i;
						}

						// Highlight if selected
						if (i == m_sel_nes_tile) {
							ImVec2 button_pos = ImGui::GetItemRectMin();
							ImVec2 button_end = ImGui::GetItemRectMax();
							ImGui::GetWindowDrawList()->AddRect(
								button_pos, button_end,
								IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.0f
							);
						}

						// Layout: 8 buttons per row
						if ((i + 1) % 16 != 0)
							ImGui::SameLine();
					}

					ImGui::EndChild();

					ImGui::SeparatorText("Sub-palettes per quadrant");

					ui::imgui_slider_with_arrows("mtdeftl", "Top-Left",
						l_mt_def.m_attr_tl, 0, 3);
					ui::imgui_slider_with_arrows("mtdeftr", "Top-Right",
						l_mt_def.m_attr_tr, 0, 3);
					ui::imgui_slider_with_arrows("mtdefbl", "Bottom-Left",
						l_mt_def.m_attr_bl, 0, 3);
					ui::imgui_slider_with_arrows("mtdefbr", "Bottom-Right",
						l_mt_def.m_attr_br, 0, 3);

					ImGui::EndTabItem();
				}
				// CHUNK - METATILES - BEGIN

				// CHUNK - DOOR META - BEGIN
				if (ImGui::BeginTabItem("Door Metadata")) {

					if (l_chunk.m_door_connections.has_value()) {

						auto& l_conns{ l_chunk.m_door_connections.value() };

						ImGui::SeparatorText("Next-World door parameters");

						// next world chunk

						// generate allowed destinations
						std::vector<const char*> allowed_labels;
						for (std::size_t i{ 0 }; i < c::LABELS_CHUNKS.size(); ++i)
							allowed_labels.push_back(c::LABELS_CHUNKS[i].c_str());

						int selected_index = static_cast<int>(
							l_conns.m_next_chunk);

						ImGui::Combo("World###nxtw", &selected_index, allowed_labels.data(), static_cast<int>(allowed_labels.size()));

						bool l_allowed{ false };
						for (std::size_t lvl_no : c::MAP_CHUNK_LEVELS)
							if (lvl_no == static_cast<std::size_t>(selected_index)) {
								l_allowed = true;
								break;
							}

						if (l_allowed)
							l_conns.m_next_chunk = static_cast<byte>(selected_index);
						else
							add_message("Invalid destination world");

						ui::imgui_slider_with_arrows("nwdcs", "Screen",
							l_conns.m_next_screen, 0, p_game.m_chunks.at(l_conns.m_next_chunk).m_screens.size() - 1);

						ui::imgui_slider_with_arrows("nwdrq", "Requirement: " + get_description(l_conns.m_next_door_req, c::LABELS_DOOR_REQS),
							l_conns.m_next_door_req, 0, c::LABELS_DOOR_REQS.size() - 1);

						// previous chunk door connection

						ImGui::SeparatorText("Previous-World door parameters");

						selected_index = static_cast<int>(
							l_conns.m_prev_chunk);

						ImGui::Combo("World###prvw", &selected_index, allowed_labels.data(), static_cast<int>(allowed_labels.size()));

						l_allowed = false;
						for (std::size_t lvl_no : c::MAP_CHUNK_LEVELS)
							if (lvl_no == static_cast<std::size_t>(selected_index)) {
								l_allowed = true;
								break;
							}

						if (l_allowed)
							l_conns.m_prev_chunk = static_cast<byte>(selected_index);
						else
							add_message("Invalid destination world");

						ui::imgui_slider_with_arrows("pwdcs", "Screen",
							l_conns.m_prev_screen, 0, p_game.m_chunks.at(l_conns.m_prev_chunk).m_screens.size() - 1);

						ui::imgui_slider_with_arrows("pwdrq", "Requirement: " + get_description(l_conns.m_prev_door_req, c::LABELS_DOOR_REQS),
							l_conns.m_prev_door_req, 0, c::LABELS_DOOR_REQS.size() - 1);

					}
					else
						ImGui::Text("No door metadata for this world");

					ImGui::EndTabItem();
				}

				// CHUNK - DOOR META - BEGIN

				// CHUNK - PALETTE - BEGIN
				if (ImGui::BeginTabItem("Palette")) {
					if (fe::ui::imgui_slider_with_arrows("##cdp",
						std::format("Default Palette #{}", l_chunk.m_default_palette_no),
						l_chunk.m_default_palette_no, 0,
						p_game.m_palettes.size() - 1,
						"Default palette used by all screens in this world. Can be overridden in-game by door and transition parameters.")) {
						m_atlas_new_palette_no = l_chunk.m_default_palette_no;
					}

					ImGui::EndTabItem();
				}

				// CHUNK - PALETTE - END

				if (ImGui::BeginTabItem("Mattock Animation")) {

					ImGui::EndTabItem();
				}


				ImGui::EndTabBar();
				ImGui::PopStyleColor(3);
			}

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Game Metadata")) {

			if (ImGui::BeginTabBar("gamemetatabs")) {
				ImGui::PushStyleColor(ImGuiCol_Tab, ui::g_uiStyles[2].normal);
				ImGui::PushStyleColor(ImGuiCol_TabActive, ui::g_uiStyles[2].active);
				ImGui::PushStyleColor(ImGuiCol_TabHovered, ui::g_uiStyles[2].hovered);

				if (ImGui::BeginTabItem("Spawns")) {

					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("Building Parameters")) {

					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("Push-Block")) {

					ImGui::EndTabItem();
				}

				ImGui::EndTabBar();
				ImGui::PopStyleColor(3);
			}

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
		ImGui::PopStyleColor(3);
	}

	ImGui::End();
}


/*
if (ImGui::Button("Check")) {
	std::string l_output;

	for (std::size_t i{ 0 }; i < p_game.m_chunks.size(); ++i)
		for (std::size_t j{ 0 }; j < p_game.m_chunks[i].m_screens.size(); ++j)
			for (std::size_t s{ 0 }; s < p_game.m_chunks[i].m_screens[j].m_doors.size(); ++s) {

				const auto& l_door{ p_game.m_chunks[i].m_screens[j].m_doors[s] };

				if (l_door.m_door_type == fe::DoorType::Building ||
					l_door.m_door_type == fe::DoorType::SameWorld)
					l_output += "Chunk=" + std::to_string(i) +
					",Screen=" + std::to_string(j) +
					",Door=" + std::to_string(s) +
					",Requirement=" + std::to_string(l_door.m_requirement)
					+ "\n";
			}

}

ImGui::Separator();

const auto& l_screen{ p_game.m_chunks.at(m_sel_chunk).m_screens.at(m_sel_screen) };

if (ImGui::Button("Left") && l_screen.m_scroll_left.has_value())
	m_sel_screen = l_screen.m_scroll_left.value();
if (ImGui::Button("Right") && l_screen.m_scroll_right.has_value())
	m_sel_screen = l_screen.m_scroll_right.value();
if (ImGui::Button("Up") && l_screen.m_scroll_up.has_value())
	m_sel_screen = l_screen.m_scroll_up.value();
if (ImGui::Button("Down") && l_screen.m_scroll_down.has_value())
	m_sel_screen = l_screen.m_scroll_down.value();

std::size_t l_sprite_count{ l_screen.m_sprites.size() };

imgui_text("Screen sprite count: " + std::to_string(l_sprite_count));

for (std::size_t i{ 0 }; i < l_sprite_count; ++i) {
	const auto& l_sprite{ l_screen.m_sprites[i] };

	byte l_id{ l_sprite.m_id };
	byte l_x{ l_sprite.m_x };
	byte l_y{ l_sprite.m_y };
	std::string l_text{ l_sprite.m_text_id.has_value() ?
		klib::Bitreader::byte_to_hex(l_sprite.m_text_id.value()) :
		"None"
	};

	imgui_text("Sprite #" + std::to_string(i) +
		" [ID=" + klib::Bitreader::byte_to_hex(l_id) +
		"] - coords=("
		+ std::to_string(l_x) + "," + std::to_string(l_y) + "), Text: " + l_text);
}

ImGui::Separator();

if (l_screen.m_interchunk_scroll.has_value()) {
	const auto& l_is{ l_screen.m_interchunk_scroll };

	ImGui::Text("Inter-world scroll transition");
	imgui_text("Destination world=" + std::to_string(l_is.value().m_dest_chunk)
		+ ", screen=" + std::to_string(l_is.value().m_dest_screen)
		+ ", pos=(" + std::to_string(l_is.value().m_dest_x)
		+ "," + std::to_string(l_is.value().m_dest_y) + "), palette="
		+ klib::Bitreader::byte_to_hex(l_is.value().m_palette_id)
	);
}

ImGui::Separator();

if (l_screen.m_intrachunk_scroll.has_value()) {
	const auto& l_is{ l_screen.m_intrachunk_scroll };

	ImGui::Text("Intra-world scroll transition");
	imgui_text("Destination screen=" + std::to_string(l_is.value().m_dest_screen)
		+ ", pos=(" + std::to_string(l_is.value().m_dest_x)
		+ "," + std::to_string(l_is.value().m_dest_y) + "), palette="
		+ klib::Bitreader::byte_to_hex(l_is.value().m_palette_id)
	);
}

ImGui::Separator();

const auto& l_cconn{ p_game.m_chunks.at(m_sel_chunk).m_door_connections };

if (l_cconn.has_value()) {
	imgui_text("Next chunk and screen: " + std::to_string(l_cconn.value().m_next_chunk) + ", " + std::to_string(l_cconn.value().m_next_screen) +
		" - req: " + klib::Bitreader::byte_to_hex(l_cconn.value().m_next_door_req));
	imgui_text("Prev chunk and screen: " + std::to_string(l_cconn.value().m_prev_chunk) + ", " + std::to_string(l_cconn.value().m_prev_screen)
		+ " - req: " + klib::Bitreader::byte_to_hex(l_cconn.value().m_prev_door_req));
}
else
	imgui_text("This world has no concept of next and previous world");

ImGui::Separator();

imgui_text("Door count: " + std::to_string(l_screen.m_doors.size()));

for (std::size_t i{ 0 }; i < l_screen.m_doors.size(); ++i) {
	const auto& l_door{ l_screen.m_doors[i] };

	std::string l_doortxt{ "Door #" + std::to_string(i) + " at ("
	+ std::to_string(l_door.m_coords.first) + ","
		+ std::to_string(l_door.m_coords.second) + "), dest=("

	+ std::to_string(l_door.m_dest_coords.first) + ","
		+ std::to_string(l_door.m_dest_coords.second) + "), unknown="
		+ klib::Bitreader::byte_to_hex(l_door.m_unknown)
		+ ",type="
	};

	if (l_door.m_door_type == fe::DoorType::NextWorld)
		l_doortxt += "Next World";
	else if (l_door.m_door_type == fe::DoorType::PrevWorld)
		l_doortxt += "Previous World";
	else if (l_door.m_door_type == fe::DoorType::Building)
		l_doortxt += "Building, req=" + klib::Bitreader::byte_to_hex(l_door.m_requirement)
		+ "\nDest screen=" + std::to_string(l_door.m_dest_screen_id)
		+ ", NPC bundle=" + klib::Bitreader::byte_to_hex(l_door.m_npc_bundle);
	else
		l_doortxt += "IntraChunk, req=" + klib::Bitreader::byte_to_hex(l_door.m_requirement)
		+ "\nDest screen=" + std::to_string(l_door.m_dest_screen_id)
		+ ", Palette=" + klib::Bitreader::byte_to_hex(l_door.m_dest_palette_id);


	imgui_text(l_doortxt);
}
*/
