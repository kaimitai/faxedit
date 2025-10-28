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

		if (ImGui::BeginTabItem(std::format("World {} ({}) Metadata###wmdtabi", m_sel_chunk, c::LABELS_CHUNKS[m_sel_chunk]).c_str())) {

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
						float v0 = (8.0f * static_cast<float>(m_sel_tilemap_sub_palette)) / (float)l_atlas->h;
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

					ImGui::Separator();

					if (ui::imgui_button("Add metatile", 2)) {
						l_chunk.m_metatiles.push_back(fe::Metatile());
						generate_metatile_textures(p_rnd, p_game);
					}

					ImGui::SameLine();

					if (ui::imgui_button("Delete metatile", 1)) {
						if (ImGui::IsKeyDown(ImGuiKey_ModShift)) {
							if (p_game.is_metatile_referenced(m_sel_chunk,
								m_sel_metatile))
								add_message("Metatile is in use", 1);
							else {
								p_game.delete_metatiles(m_sel_chunk, { static_cast<byte>(m_sel_metatile) });
								generate_metatile_textures(p_rnd, p_game);
							}
						}
						else
							add_message("Hold shift to delete metatile");
					}

					ImGui::EndTabItem();
				}
				// CHUNK - METATILES - BEGIN

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

				// CHUNK - MATTOCK ANIMATION - BEGIN

				if (ImGui::BeginTabItem("Mattock Animation")) {
					auto& l_ma{ l_chunk.m_mattock_animation };

					ImGui::Text("Define the four blocks cycled through when using the mattock");

					ui::imgui_slider_with_arrows("ma0", "Breakable block",
						l_ma.at(0), 0, l_chunk.m_metatiles.size() - 1, "");

					ImGui::SameLine();
					ImGui::Image(m_gfx.get_metatile_texture(l_ma.at(0)), ImVec2(32, 32));

					ui::imgui_slider_with_arrows("ma1", "Intermedite block #1",
						l_ma.at(1), 0, l_chunk.m_metatiles.size() - 1, "");

					ImGui::SameLine();
					ImGui::Image(m_gfx.get_metatile_texture(l_ma.at(1)), ImVec2(32, 32));

					ui::imgui_slider_with_arrows("ma2", "Intermedite block #2",
						l_ma.at(2), 0, l_chunk.m_metatiles.size() - 1, "");

					ImGui::SameLine();
					ImGui::Image(m_gfx.get_metatile_texture(l_ma.at(2)), ImVec2(32, 32));

					ui::imgui_slider_with_arrows("ma3", "Destination block",
						l_ma.at(3), 0, l_chunk.m_metatiles.size() - 1, "");

					ImGui::SameLine();
					ImGui::Image(m_gfx.get_metatile_texture(l_ma.at(3)), ImVec2(32, 32));

					ImGui::EndTabItem();
				}

				// CHUNK - MATTOCK ANIMATION - END

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

				if (ImGui::BeginTabItem("Stages")) {
					show_stages_data(p_game);
					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("Spawns")) {

					ImGui::SeparatorText("Spawn locations after dying or talking to a spawn-setting Guru");

					ui::imgui_slider_with_arrows("spawnloc", "",
						m_sel_spawn_location, 0, 7);

					ImGui::SeparatorText(std::format("Location for spawn point #{}", m_sel_spawn_location).c_str());

					auto& l_spawn{ p_game.m_spawn_locations.at(m_sel_spawn_location) };

					ui::imgui_slider_with_arrows("spawnworld",
						std::format("World: {}", c::LABELS_CHUNKS.at(l_spawn.m_world)),
						l_spawn.m_world, 0, 7);

					ui::imgui_slider_with_arrows("spawnscr", "Screen",
						l_spawn.m_screen, 0, p_game.m_chunks.at(m_sel_chunk).m_screens.size() - 1);

					auto l_newpos = show_position_slider(l_spawn.m_x, l_spawn.m_y);

					if (l_newpos.has_value()) {
						l_spawn.m_x = l_newpos.value().first;
						l_spawn.m_y = l_newpos.value().second;
					}

					ImGui::SeparatorText("Stage Number");

					ui::imgui_slider_with_arrows("spawnstage", "The number of next-world doors traversed before this spawn",
						l_spawn.m_stage, 0, 5);

					ImGui::SeparatorText("Automatic Deduction");

					if (ui::imgui_button("Deduce", 3, "Try to deduce spawn locations by spawn-setting Guru door entrances")) {
						bool l_deduced_spawns{ p_game.calculate_spawn_locations_by_guru() };
						if (l_deduced_spawns)
							add_message("All spawn point data deduced OK", 2);
						else
							add_message("Unable to deduce all spawn points", 1);
					}

					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("Building Parameters")) {

					ImGui::SeparatorText(std::format("Building Parameter: {})",
						get_description(static_cast<byte>(m_sel_npc_bundle), c::LABELS_NPC_BUNDLES)).c_str());

					ui::imgui_slider_with_arrows("###npcbsel", "", m_sel_npc_bundle,
						0, p_game.m_npc_bundles.size() - 1);

					ImGui::SeparatorText("Building Parameter Sprites");

					ImGui::PushID("###bldparam");

					show_sprite_screen(p_game.m_npc_bundles.at(m_sel_npc_bundle),
						m_sel_npc_bundle_sprite);

					ImGui::PopID();

					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("Push-Block")) {
					auto& l_pb{ p_game.m_push_block };

					ImGui::Text("The parameters for the tilemap change when you push blocks and open the last spring.");
					ImGui::Text("Navigate to the world where the push-blocks reside to see metatile previews.");
					ImGui::Text("Don't forget to set the command-byte to 0 for the screens where you want");
					ImGui::Text("the animation to trigger automatically after quest completion.");
					ImGui::NewLine();
					ImGui::Text("Be careful about adding pushable blocks to different worlds,");
					ImGui::Text("as the metatile indexes might be out of bounds elsewhere.");

					ImGui::SeparatorText("Stage and screen where the quest-flag will be set when pusing blocks.");

					auto l_world{ p_game.m_stages.m_stages.at(l_pb.m_stage).m_world_id };
					bool l_sameworld{ m_sel_chunk == l_world };
					std::size_t l_mt_count{ p_game.m_chunks[l_world].m_metatiles.size() };

					ui::imgui_slider_with_arrows("###pbstage",
						std::format("Stage {} ({})",
							l_pb.m_stage, c::LABELS_CHUNKS[l_world]),
						l_pb.m_stage, 0, 5, "");

					ui::imgui_slider_with_arrows("pbs",
						"Screen", l_pb.m_screen, 0, p_game.m_chunks.at(l_pb.m_stage).m_screens.size());

					ImGui::SeparatorText("Line-drawing function");

					ImGui::Text("Start position");

					ImGui::PushID("pbsp");

					auto l_newpos{ show_position_slider(
						l_pb.m_x, l_pb.m_y) };
					if (l_newpos.has_value()) {
						l_pb.m_x = l_newpos.value().first;
						l_pb.m_y = l_newpos.value().second;
					}

					ImGui::PopID();

					ui::imgui_slider_with_arrows("pbdelta",
						"Delta-x", l_pb.m_pos_delta, 0, 255, "Change in x-position per block. Make sure you don't draw off screen. 255 and just below are negative deltas.");

					ui::imgui_slider_with_arrows("pbdcount",
						"Draw Count", l_pb.m_block_count, 0, 100, "Number of blocks to draw");

					ui::imgui_slider_with_arrows("pbblock", "Draw-block",
						l_pb.m_draw_block, 0, l_mt_count - 1,
						"The block that will be used to draw the line");

					if (l_sameworld && l_pb.m_draw_block < l_mt_count) {
						ImGui::SameLine();
						ImGui::Image(m_gfx.get_metatile_texture(l_pb.m_draw_block), ImVec2(32, 32));
					}

					ImGui::SeparatorText("Fountain Cover Position");

					ImGui::PushID("fctcp");

					auto l_newcoverpos{ show_position_slider(
						l_pb.m_cover_x, l_pb.m_cover_y) };
					if (l_newcoverpos.has_value()) {
						l_pb.m_cover_x = l_newcoverpos.value().first;
						l_pb.m_cover_y = l_newcoverpos.value().second;
					}

					ImGui::PopID();

					ImGui::SeparatorText("Pushable Blocks Animation - Source");

					ui::imgui_slider_with_arrows("pbs0", "Source Top",
						l_pb.m_source_0, 0, p_game.m_chunks.at(l_pb.m_stage).m_metatiles.size() - 1,
						"The block that will replace the top of the pushed block");

					if (l_sameworld && l_pb.m_source_0 < l_mt_count) {
						ImGui::SameLine();
						ImGui::Image(m_gfx.get_metatile_texture(l_pb.m_source_0), ImVec2(32, 32));
					}

					ui::imgui_slider_with_arrows("pbs1", "Source Bottom",
						l_pb.m_source_1, 0, l_mt_count - 1,
						"The block that will replace the bottom of the pushed block");

					if (l_sameworld && l_pb.m_source_1 < l_mt_count) {
						ImGui::SameLine();
						ImGui::Image(m_gfx.get_metatile_texture(l_pb.m_source_1), ImVec2(32, 32));
					}

					ImGui::SeparatorText("Pushable Blocks Animation - Target");

					ui::imgui_slider_with_arrows("pbt0", "Target Top",
						l_pb.m_target_0, 0, l_mt_count - 1,
						"The block that will replace the top of the destination of the pushed block");

					if (l_sameworld && l_pb.m_target_0 < l_mt_count) {
						ImGui::SameLine();
						ImGui::Image(m_gfx.get_metatile_texture(l_pb.m_target_0), ImVec2(32, 32));
					}

					ui::imgui_slider_with_arrows("pbt1", "Target Bottom",
						l_pb.m_target_1, 0, l_mt_count - 1,
						"The block that will replace the bottom of the destination of the pushed block");

					if (l_sameworld && l_pb.m_target_1 < l_mt_count) {
						ImGui::SameLine();
						ImGui::Image(m_gfx.get_metatile_texture(l_pb.m_target_1), ImVec2(32, 32));
					}

					ImGui::SeparatorText("Deduce world and screen");

					if (ui::imgui_button("Deduce", 1, "Find the stage and screen with the pushable blocks (takes first match) and deduce as much information as possible")) {

					}

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

void fe::MainWindow::show_stages_data(fe::Game& p_game) {
	auto& l_stages{ p_game.m_stages };

	ImGui::SeparatorText("Stage Data");

	ui::imgui_slider_with_arrows("###stagesel",
		std::format("Selected Stage: ", m_sel_stage),
		m_sel_stage, 0, 5);

	ImGui::Separator();

	auto& l_stage{ l_stages.m_stages[m_sel_stage] };

	std::size_t l_world_no{ l_stage.m_world_id };

	if (ui::imgui_slider_with_arrows("###stedw",
		std::format("Stage World: {} ({})", l_world_no,
			c::LABELS_CHUNKS[l_world_no]),
		l_world_no, 0, 7,
		"Mapping from stage to world - currently the stage 0 world cannot be edited",
		m_sel_stage == 0))
		l_stages.set_stage_world(m_sel_stage, l_world_no);

	ImGui::SeparatorText("Next Stage Parameters");

	ui::imgui_slider_with_arrows("###stens",
		std::format("Next Stage: {} ({})",
		l_stage.m_next_stage,
			c::LABELS_CHUNKS[l_stages.m_stages[l_stage.m_next_stage].m_world_id]),
		l_stage.m_next_stage, 0, 5);
	ui::imgui_slider_with_arrows("###stenscr", "Next Screen",
		l_stage.m_next_screen, 0, p_game.m_chunks.at(
			l_stages.m_stages[l_stage.m_next_stage].m_world_id
		).m_screens.size() - 1);
	ui::imgui_slider_with_arrows("###stenr", std::format("Next-Stage Door Requirement: {}",
		get_description(l_stage.m_next_requirement, c::LABELS_DOOR_REQS)),
		l_stage.m_next_requirement, 0, c::LABELS_DOOR_REQS.size() - 1);

	ImGui::SeparatorText("Previous Stage Parameters");

	ui::imgui_slider_with_arrows("###steps",
		std::format("Previous Stage: {} ({})",
			l_stage.m_prev_stage,
			c::LABELS_CHUNKS[l_stages.m_stages[l_stage.m_prev_stage].m_world_id]),
		l_stage.m_prev_stage, 0, 5);
	ui::imgui_slider_with_arrows("###stepscr", "Previous Screen",
		l_stage.m_prev_screen, 0, p_game.m_chunks.at(
			l_stages.m_stages[l_stage.m_prev_stage].m_world_id
		).m_screens.size() - 1);
	ui::imgui_slider_with_arrows("###stepr", std::format("Previous-Stage Door Requirement: {}",
		get_description(l_stage.m_prev_requirement, c::LABELS_DOOR_REQS)),
		l_stage.m_prev_requirement, 0, c::LABELS_DOOR_REQS.size() - 1);

	ImGui::SeparatorText("Start Parameters");

	ui::imgui_slider_with_arrows("###stestascr", "Start Screen",
		l_stages.m_start_screen, 0, p_game.m_chunks.at(
			l_stages.m_stages[0].m_world_id
		).m_screens.size() - 1);

	ImGui::PushID("###stestapos");
	auto l_startpos{ show_position_slider(l_stages.m_start_x, l_stages.m_start_y) };
	if (l_startpos.has_value()) {
		l_stages.m_start_x = l_startpos.value().first;
		l_stages.m_start_y = l_startpos.value().second;
	}
	ImGui::PopID();

	ui::imgui_slider_with_arrows("###stestahp", "Starting Health",
		l_stages.m_start_hp, 0, 255);
}
