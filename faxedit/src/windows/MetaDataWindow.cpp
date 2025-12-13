#include "MainWindow.h"
#include "Imgui_helper.h"
#include "./../common/klib/Bitreader.h"
#include "./../fe/fe_constants.h"
#include "./../fe/fe_app_constants.h"
#include <unordered_map>

void fe::MainWindow::draw_metadata_window(SDL_Renderer* p_rnd) {

	auto& l_chunk{ m_game->m_chunks.at(m_sel_chunk) };

	fe::ui::imgui_screen("World and Game Settings###cw", c::WIN_METADATA_X, c::WIN_METADATA_Y,
		c::WIN_METADATA_W, c::WIN_METADATA_H, 3);

	if (ImGui::BeginTabBar("Metadata")) {

		ImGui::PushStyleColor(ImGuiCol_Tab, ui::g_uiStyles[4].normal);
		ImGui::PushStyleColor(ImGuiCol_TabActive, ui::g_uiStyles[4].active);
		ImGui::PushStyleColor(ImGuiCol_TabHovered, ui::g_uiStyles[4].hovered);

		if (ImGui::BeginTabItem(std::format("World {} ({}) Metadata###wmdtabi", m_sel_chunk, m_labels_worlds[m_sel_chunk]).c_str())) {

			if (ImGui::BeginTabBar("WorldMetaTabs")) {

				ImGui::PushStyleColor(ImGuiCol_Tab, ui::g_uiStyles[2].normal);
				ImGui::PushStyleColor(ImGuiCol_TabActive, ui::g_uiStyles[2].active);
				ImGui::PushStyleColor(ImGuiCol_TabHovered, ui::g_uiStyles[2].hovered);

				// WORLD - METATILES - BEGIN
				show_mt_definition_tab(p_rnd, l_chunk);
				// WORLD - METATILES - BEGIN

				// CHUNK - SCENE - BEGIN
				if (ImGui::BeginTabItem("Scene")) {

					bool l_bldg{ m_sel_chunk == c::CHUNK_IDX_BUILDINGS };

					if (l_bldg) {
						show_scene(m_game->m_building_scenes.at(m_sel_screen), true);
						ImGui::Separator();
						imgui_text("Building world scenes are set per screen");
					}
					else
						show_scene(l_chunk.m_scene, false);

					ImGui::EndTabItem();
				}

				// CHUNK - SCENE - END

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

				if (ImGui::BeginTabItem("Cleanup")) {

					ImGui::SeparatorText("Remove unused assets");

					if (ui::imgui_button("Delete Unreferenced Metatiles", 1, "",
						!ImGui::IsKeyDown(ImGuiKey_ModShift))) {
						std::size_t l_del_cnt{ m_game->delete_unreferenced_metatiles(m_sel_chunk) };
						add_message(std::format("{} metatiles deleted from world {}",
							l_del_cnt, m_sel_chunk), 5);
						if (l_del_cnt > 0)
							generate_metatile_textures(p_rnd);
					}

					if (ui::imgui_button("Delete Unreferenced Screens", 1, "",
						m_sel_chunk == c::CHUNK_IDX_BUILDINGS || !ImGui::IsKeyDown(ImGuiKey_ModShift))) {
						std::size_t l_del_cnt{ m_game->delete_unreferenced_screens(m_sel_chunk) };
						add_message(std::format("{} screens deleted from world {}",
							l_del_cnt, m_sel_chunk), 5);
						if (m_sel_screen >= m_game->m_chunks.at(m_sel_chunk).m_screens.size())
							m_sel_screen = m_game->m_chunks.at(m_sel_chunk).m_screens.size() - 1;
					}

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

				if (ImGui::BeginTabItem("Stages")) {
					show_stages_data();
					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("Spawns")) {

					ImGui::SeparatorText("Spawn locations after dying or restoring from mantra");

					ui::imgui_slider_with_arrows("spawnloc", "",
						m_sel_spawn_location, 0, 7, "", false, true);

					ImGui::SeparatorText(std::format("Location for spawn point #{}", m_sel_spawn_location).c_str());

					auto& l_spawn{ m_game->m_spawn_locations.at(m_sel_spawn_location) };

					ui::imgui_slider_with_arrows("spawnworld",
						std::format("World: {}", m_labels_worlds.at(l_spawn.m_world)),
						l_spawn.m_world, 0, 7);

					ui::imgui_slider_with_arrows("spawnscr", "Screen",
						l_spawn.m_screen, 0, m_game->m_chunks.at(m_sel_chunk).m_screens.size() - 1);

					auto l_newpos = show_position_slider(l_spawn.m_x, l_spawn.m_y);

					if (l_newpos.has_value()) {
						l_spawn.m_x = l_newpos.value().first;
						l_spawn.m_y = l_newpos.value().second;
					}

					ImGui::SeparatorText("Stage Number");

					ui::imgui_slider_with_arrows("spawnstage", "",
						l_spawn.m_stage, 0, 5);

					ImGui::SeparatorText("Building Sprite Set for the spawn's Guru room");

					ui::imgui_slider_with_arrows("spawnspriteset", "",
						l_spawn.m_sprite_set, 0, m_game->m_npc_bundles.size() - 1);

					show_sprite_set_contents(l_spawn.m_sprite_set);

					ImGui::SeparatorText("Automatic Deduction");

					if (ui::imgui_button("Deduce", 4, "Try to deduce spawn locations")) {
						add_message(std::format("Fully deduced {} of 8 spawn points",
							m_game->calculate_spawn_locations_by_guru()),
							4);
					}

					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("Sprite Sets")) {

					show_sprite_npc_bundle_screen();

					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("Push-Block")) {
					auto& l_pb{ m_game->m_push_block };

					ImGui::Text("The parameters for the tilemap change when you push blocks and open the last spring.");
					ImGui::Text("Navigate to the world where the push-blocks reside to see metatile previews.");
					ImGui::Text("Don't forget to set the command-byte to 0 for the screens where you want");
					ImGui::Text("the animation to trigger automatically after quest completion.");
					ImGui::NewLine();
					ImGui::Text("Be careful about adding pushable blocks to different worlds,");
					ImGui::Text("as the metatile indexes might be out of bounds elsewhere.");

					ImGui::SeparatorText("Stage and screen where the quest-flag will be set when pusing blocks.");

					auto l_world{ m_game->m_stages.m_stages.at(l_pb.m_stage).m_world_id };
					bool l_sameworld{ m_sel_chunk == l_world };
					std::size_t l_mt_count{ m_game->m_chunks[l_world].m_metatiles.size() };

					ui::imgui_slider_with_arrows("###pbstage",
						std::format("Stage {} ({})",
							l_pb.m_stage, m_labels_worlds[l_world]),
						l_pb.m_stage, 0, 5, "");

					ui::imgui_slider_with_arrows("pbs",
						"Screen", l_pb.m_screen, 0, m_game->m_chunks.at(l_pb.m_stage).m_screens.size());

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
						l_pb.m_source_0, 0, m_game->m_chunks.at(l_pb.m_stage).m_metatiles.size() - 1,
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

					ImGui::SeparatorText("Deduce");

					if (ui::imgui_button("Deduce", 4, "Try to deduce as many parameters as possible")) {
						if (m_game->calculate_push_block_parameters())
							add_message("Push-block parameters deduced", 2);
						else
							add_message("Could not deduce Push-block parameters", 1);
					}

					ImGui::EndTabItem();
				}

				// GAME - JUMP-ON ANIMATION - BEGIN

				if (ImGui::BeginTabItem("Jump-On")) {
					auto& l_jo{ m_game->m_jump_on_animation };

					ImGui::Text("Define the four blocks cycled through when jumping on breakable floors");
					ImGui::Text("Breakable floors are metatiles with property code 5");
					ImGui::Separator();
					ImGui::Text("These are defined game-wide - adverse effects can occur if using this");
					ImGui::Text("on a world where not all these metatiles are defined.");
					ImGui::Separator();
					ImGui::Text("Rendering metatiles with metatiles from currently selected world");
					ImGui::Separator();
					for (std::size_t i{ 0 }; i < l_jo.size(); ++i) {
						ImGui::PushID(std::format("jo###{}", i).c_str());

						ui::imgui_slider_with_arrows("", std::format("Animation tile {}", i + 1),
							l_jo.at(i), 0, 255, "");

						if (l_jo.at(i) < l_chunk.m_metatiles.size()) {
							ImGui::SameLine();
							ImGui::Image(m_gfx.get_metatile_texture(l_jo.at(i)), ImVec2(32, 32));
						}
						else {
							ImGui::Text("Metatile not defined for currently selected world");
						}

						ImGui::PopID();
					}

					ImGui::EndTabItem();
				}

				// GAME - JUMP-ON ANIMATION - END

				if (ImGui::BeginTabItem("Pal2Mus")) {
					static std::size_t ls_p2m_slot{ 0 };
					auto& slots{ m_game->m_pal_to_music };

					ui::imgui_slider_with_arrows("###p2ms",
						"Slot", ls_p2m_slot, 0, slots.m_slots.size() - 1,
						"", false, true);

					ImGui::SeparatorText("Palette to Music mapping");

					auto& slot{ slots.m_slots[ls_p2m_slot] };

					ui::imgui_slider_with_arrows("###p2mp",
						std::format("Palette: {}",
							get_description(slot.m_palette, m_labels_palettes)),
						slot.m_palette, 0, m_game->m_palettes.size() - 1);

					ui::imgui_slider_with_arrows("###p2mm",
						std::format("Music: {}",
							get_description(slot.m_music, m_labels_music)),
						slot.m_music, 0, m_labels_music.size() - 1);

					ImGui::EndTabItem();
				}

				// GAME - FOG - BEGIN

				if (ImGui::BeginTabItem("Fog")) {
					auto& l_fog{ m_game->m_fog };

					imgui_text("World and palette combination for the fog to be in effect");
					imgui_text("Set to an invalid world number to disable fog entirely");

					ImGui::Separator();

					ui::imgui_slider_with_arrows("###fogw",
						std::format("World: {}",
							get_description(l_fog.m_world_no, m_labels_worlds)),
						l_fog.m_world_no, 0, m_game->m_chunks.size()
					);

					ui::imgui_slider_with_arrows("###fogp",
						std::format("Palette: {}",
							get_description(l_fog.m_palette_no, m_labels_palettes)),
						l_fog.m_palette_no, 0, m_game->m_palettes.size() - 1
					);

					ImGui::EndTabItem();
				}

				// GAME - FOG - END

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

void fe::MainWindow::show_stages_data(void) {
	auto& l_stages{ m_game->m_stages };

	ImGui::SeparatorText("Stage Data");

	ui::imgui_slider_with_arrows("###stagesel",
		std::format("Selected Stage: ", m_sel_stage),
		m_sel_stage, 0, 5, "", false, true);

	ImGui::Separator();

	auto& l_stage{ l_stages.m_stages[m_sel_stage] };

	std::size_t l_world_no{ l_stage.m_world_id };

	if (ui::imgui_slider_with_arrows("###stedw",
		std::format("Stage World: {} ({})", l_world_no,
			m_labels_worlds[l_world_no]),
		l_world_no, 0, 7,
		"Mapping from stage to world - currently the stage 0 world cannot be edited",
		m_sel_stage == 0))
		l_stages.set_stage_world(m_sel_stage, l_world_no);

	ImGui::SeparatorText("Next Stage Parameters");

	ui::imgui_slider_with_arrows("###stens",
		std::format("Next Stage: {} ({})",
			l_stage.m_next_stage,
			m_labels_worlds[l_stages.m_stages[l_stage.m_next_stage].m_world_id]),
		l_stage.m_next_stage, 0, 5);
	ui::imgui_slider_with_arrows("###stenscr", "Next Screen",
		l_stage.m_next_screen, 0, m_game->m_chunks.at(
			l_stages.m_stages[l_stage.m_next_stage].m_world_id
		).m_screens.size() - 1);
	ui::imgui_slider_with_arrows("###stenr", std::format("Next-Stage Door Requirement: {}",
		get_description(l_stage.m_next_requirement, m_labels_door_reqs)),
		l_stage.m_next_requirement, 0, m_labels_door_reqs.size() - 1);

	ImGui::SeparatorText("Previous Stage Parameters");

	ui::imgui_slider_with_arrows("###steps",
		std::format("Previous Stage: {} ({})",
			l_stage.m_prev_stage,
			m_labels_worlds[l_stages.m_stages[l_stage.m_prev_stage].m_world_id]),
		l_stage.m_prev_stage, 0, 5);
	ui::imgui_slider_with_arrows("###stepscr", "Previous Screen",
		l_stage.m_prev_screen, 0, m_game->m_chunks.at(
			l_stages.m_stages[l_stage.m_prev_stage].m_world_id
		).m_screens.size() - 1);
	ui::imgui_slider_with_arrows("###stepr", std::format("Previous-Stage Door Requirement: {}",
		get_description(l_stage.m_prev_requirement, m_labels_door_reqs)),
		l_stage.m_prev_requirement, 0, m_labels_door_reqs.size() - 1);

	ImGui::SeparatorText("Start Parameters");

	ui::imgui_slider_with_arrows("###stestascr", "Start Screen",
		l_stages.m_start_screen, 0, m_game->m_chunks.at(
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
		l_stages.m_start_hp, 0, 80);
}

void fe::MainWindow::show_screen_scroll_data(void) {
	auto& l_screen{ m_game->m_chunks[m_sel_chunk].m_screens[m_sel_screen] };
	std::size_t l_scr_count{ m_game->m_chunks[m_sel_chunk].m_screens.size() };

	const std::vector<std::string> lc_dirs{ "Left", "Right", "Up", "Down" };
	std::map<std::size_t, std::optional<byte>&> l_conns{
		{0, l_screen.m_scroll_left},
		{1, l_screen.m_scroll_right},
		{2, l_screen.m_scroll_up},
		{3, l_screen.m_scroll_down}
	};

	ImGui::SeparatorText("Screen-scroll connections");

	if (ImGui::BeginTable("ScrollTable", 3, ImGuiTableFlags_Borders)) {
		ImGui::TableSetupColumn("Direction", ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableSetupColumn("Destination Screen");
		ImGui::TableHeadersRow();

		for (auto& kv : l_conns) {
			ImGui::PushID(std::format("###scrdef{}", kv.first).c_str());

			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text(lc_dirs[kv.first].c_str());

			ImGui::TableSetColumnIndex(1);
			if (kv.second.has_value()) {

				if (ImGui::IsKeyDown(ImGuiKey_ModCtrl)) {
					if (ui::imgui_button("Go To", 4)) {
						m_sel_screen = kv.second.value();
					}
				}
				else if (ui::imgui_button("Delete", 1))
					kv.second = std::nullopt;
			}
			else {
				if (ui::imgui_button("Add", 2)) {
					kv.second = 0;
				}
			}

			ImGui::TableSetColumnIndex(2);
			if (kv.second.has_value()) {
				ui::imgui_slider_with_arrows("###scrolldef", "", kv.second.value(), 0, l_scr_count - 1);
			}
			else {
				ImGui::Text("Undefined");
			}

			ImGui::PopID();
		}

		ImGui::EndTable();
	}
}

void fe::MainWindow::show_mt_definition_tab(SDL_Renderer* p_rnd, fe::Chunk& p_chunk) {
	if (ImGui::BeginTabItem("Metatiles")) {

		if (m_sel_metatile >= p_chunk.m_metatiles.size())
			m_sel_metatile = 0;

		auto& l_mt_def{ p_chunk.m_metatiles[m_sel_metatile] };

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
					byte oldval{ l_mt_def.m_tilemap.at(quadrant / 2).at(quadrant % 2) };
					byte newval{ static_cast<byte>(m_sel_nes_tile) };

					if (oldval != newval) {
						l_mt_def.m_tilemap.at(quadrant / 2).at(quadrant % 2) = static_cast<byte>(m_sel_nes_tile);
						generate_metatile_textures(p_rnd, m_sel_metatile);
					}
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
			p_chunk.m_metatiles.size() - 1, "", false, true);

		ImGui::SeparatorText(std::format("Block property: {}",
			get_description(l_mt_def.m_block_property,
				m_labels_block_props)).c_str());

		ui::imgui_slider_with_arrows("mtblprop", "",
			l_mt_def.m_block_property, 0x00, 0x0f);

		std::size_t l_tileset_no{ m_game->get_default_tileset_no(m_sel_chunk, m_sel_screen) };
		std::size_t l_tileset_start{ m_tileset_start.at(l_tileset_no) };
		std::size_t l_tileset_end{ l_tileset_start + m_tileset_size.at(l_tileset_no) };

		ImGui::BeginChild("TilePicker", ImVec2(0, 400), true); // scrollable area
		auto l_atlas{ m_gfx.get_atlas() };

		int showcount{ 0 };

		for (std::size_t i{ 0 }; i < 256; ++i) {

			if (i < l_tileset_start && m_chr_picker_mode == fe::ChrPickerMode::Default)
				continue;
			else if (i < l_tileset_start && i >= c::CHR_HUD_TILE_COUNT && m_chr_picker_mode != fe::ChrPickerMode::All)
				continue;
			else if (i >= l_tileset_end && m_chr_picker_mode != fe::ChrPickerMode::All)
				continue;

			// Compute UVs for tile i
			float u0 = (i * 8.0f) / (float)l_atlas->w;
			float v0 = (8.0f * static_cast<float>(m_sel_tilemap_sub_palette)) / (float)l_atlas->h;
			float u1 = ((i + 1) * 8.0f) / (float)l_atlas->w;
			float v1 = 8.0f * static_cast<float>(m_sel_tilemap_sub_palette + 1) / (float)l_atlas->h;

			if (ImGui::ImageButton(std::format("###stile{}", i).c_str(),
				l_atlas, ImVec2(32.0f, 32.0f), ImVec2(u0, v0), ImVec2(u1, v1))) {
				m_sel_nes_tile = i;
			}

			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				imgui_text(std::format("chr-tile {}", i));
				ImGui::EndTooltip();
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
			if ((showcount++ + 1) % 16 != 0)
				ImGui::SameLine();
		}

		ImGui::EndChild();

		ImGui::SeparatorText("Sub-palettes per quadrant");

		if (ui::imgui_slider_with_arrows("mtdeftl", "Top-Left",
			l_mt_def.m_attr_tl, 0, 3))
			generate_metatile_textures(p_rnd, m_sel_metatile);
		ui::imgui_slider_with_arrows("mtdeftr", "Top-Right",
			l_mt_def.m_attr_tr, 0, 3);
		ui::imgui_slider_with_arrows("mtdefbl", "Bottom-Left",
			l_mt_def.m_attr_bl, 0, 3);
		ui::imgui_slider_with_arrows("mtdefbr", "Bottom-Right",
			l_mt_def.m_attr_br, 0, 3);

		ImGui::Separator();

		if (ui::imgui_button("Add metatile", 2)) {
			p_chunk.m_metatiles.push_back(fe::Metatile());
			generate_metatile_textures(p_rnd,
				p_chunk.m_metatiles.size() - 1);
		}

		ImGui::SameLine();

		if (ui::imgui_button("Delete metatile", 1)) {
			if (ImGui::IsKeyDown(ImGuiKey_ModShift)) {
				if (m_game->is_metatile_referenced(m_sel_chunk,
					m_sel_metatile))
					add_message("Metatile is in use", 1);
				else {
					m_game->delete_metatiles(m_sel_chunk, { static_cast<byte>(m_sel_metatile) });
					generate_metatile_textures(p_rnd);
				}
			}
			else
				add_message("Hold shift to delete metatile");
		}

		ImGui::SeparatorText("Source NES-tiles");

		if (ImGui::RadioButton("Default###mtchrdef", (m_chr_picker_mode == fe::ChrPickerMode::Default)))
			m_chr_picker_mode = fe::ChrPickerMode::Default;
		ImGui::SameLine();
		if (ImGui::RadioButton("HUD###mtchrhud", (m_chr_picker_mode == fe::ChrPickerMode::HUD)))
			m_chr_picker_mode = fe::ChrPickerMode::HUD;
		ImGui::SameLine();
		if (ImGui::RadioButton("All###mtchrall", (m_chr_picker_mode == fe::ChrPickerMode::All)))
			m_chr_picker_mode = fe::ChrPickerMode::All;

		ImGui::SeparatorText("Sub-palette for rendering chr-tiles");

		ui::imgui_slider_with_arrows("###mtsp", "", m_sel_tilemap_sub_palette, 0, 3, "",
			false, true);
		ImGui::EndTabItem();
	}
}

void fe::MainWindow::show_scene(fe::Scene& p_scene, bool p_show_pos) {

	if (fe::ui::imgui_slider_with_arrows("##wsp",
		std::format("Palette: {}", get_description(static_cast<byte>(p_scene.m_palette), m_labels_palettes)),
		p_scene.m_palette, 0,
		m_game->m_palettes.size() - 1,
		"Default palette used by all screens in this world. Can be overridden in-game by door and transition parameters.")) {
		m_atlas_new_palette_no = p_scene.m_palette;
	}

	if (fe::ui::imgui_slider_with_arrows("###wst",
		std::format("Tileset: {}", get_description(static_cast<byte>(p_scene.m_tileset), m_labels_tilesets)),
		p_scene.m_tileset, 0,
		m_game->m_tilesets.size() - 1))
		m_atlas_new_tileset_no = p_scene.m_tileset;

	fe::ui::imgui_slider_with_arrows("###wsm",
		std::format("Music: {}", get_description(static_cast<byte>(p_scene.m_music), m_labels_music)),
		p_scene.m_music, 0, m_labels_music.size() - 1);

	if (p_show_pos) {
		ImGui::SeparatorText("Entry Position");
		ImGui::PushID("scenepos");

		auto l_pos{ show_position_slider(p_scene.m_x, p_scene.m_y) };
		if (l_pos.has_value()) {
			p_scene.m_x = l_pos->first;
			p_scene.m_y = l_pos->second;
		}

		ImGui::PopID();
	}

}
