#include "MainWindow.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include "Imgui_helper.h"

void fe::MainWindow::draw_screen_tilemap_window(SDL_Renderer* p_rnd) {
	auto& l_chunk = m_game->m_chunks.at(m_sel_chunk);
	auto& l_metatiles = l_chunk.m_metatiles;
	auto& l_screen = l_chunk.m_screens.at(m_sel_screen);
	auto l_screen_tilemap{ m_gfx.get_screen_texture() };

	const std::string l_win_label{ std::format("{} > Screen {} > {}",
		c::LABELS_CHUNKS[m_sel_chunk], m_sel_screen,
		get_editmode_as_string()) +
		(m_emode == fe::EditMode::Tilemap ?
		std::format(" > Position ({},{})", m_sel_tile_x, m_sel_tile_y) : "")
	+ "###stmw" };

	ImGui::Begin(l_win_label.c_str());

	// Layout constants
	const float rightPanelWidth = 500.0f;
	const float sharedHeight = 200.0f;
	const float tilemapPixelWidth = static_cast<float>(l_screen_tilemap->w); // 256.0f;
	const float tilemapPixelHeight = static_cast<float>(l_screen_tilemap->h); // 208.0f;
	const float tileSize = 16.0f;
	const float tilemapAspect = tilemapPixelWidth / tilemapPixelHeight;

	// Available space
	const float availableWidth = ImGui::GetContentRegionAvail().x;
	const float availableHeight = ImGui::GetContentRegionAvail().y;
	const float leftPanelWidth = availableWidth - rightPanelWidth;
	const float panelHeight = availableHeight - sharedHeight;

	// Scale tilemap to fit left panel
	float scaledWidth = leftPanelWidth;
	float scaledHeight = scaledWidth / tilemapAspect;
	if (scaledHeight > panelHeight) {
		scaledHeight = panelHeight;
		scaledWidth = scaledHeight * tilemapAspect;
	}

	// --- Left Panel: Tilemap ---
	if (ImGui::BeginChild("Tilemap", ImVec2(leftPanelWidth, panelHeight))) {
		ImVec2 imagePos = ImGui::GetCursorScreenPos();
		ImVec2 imageSize = ImVec2(scaledWidth, scaledHeight);
		ImGui::Image(m_gfx.get_screen_texture(), imageSize);

		ImVec2 mousePos = ImGui::GetMousePos();
		bool insideImage =
			mousePos.x >= imagePos.x && mousePos.x < imagePos.x + imageSize.x &&
			mousePos.y >= imagePos.y && mousePos.y < imagePos.y + imageSize.y;

		bool l_mouse_left_down{ ImGui::IsMouseDown(ImGuiMouseButton_Left) };
		bool l_mouse_right_down{ ImGui::IsMouseDown(ImGuiMouseButton_Right) };

		if (insideImage && (l_mouse_left_down || l_mouse_right_down)) {
			float scaleX = imageSize.x / tilemapPixelWidth;
			float scaleY = imageSize.y / tilemapPixelHeight;

			float localX = (mousePos.x - imagePos.x) / scaleX;
			float localY = (mousePos.y - imagePos.y) / scaleY;

			std::size_t tileX = std::min(static_cast<std::size_t>(localX / tileSize), std::size_t(15));
			std::size_t tileY = std::min(static_cast<std::size_t>(localY / tileSize), std::size_t(12));

			if (m_emode == fe::EditMode::Tilemap) {
				if (l_mouse_left_down) {
					// ctrl + left mouse; color picker
					if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
						m_sel_metatile = l_screen.m_tilemap.at(tileY).at(tileX);
					}
					else if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
						m_sel_tile_x2 = tileX;
						m_sel_tile_y2 = tileY;
					}
					// left mouse; select tile coords
					else {
						m_sel_tile_x = tileX;
						m_sel_tile_y = tileY;
						// clear selection rectangle
						m_sel_tile_x2 = 16;
					}
				}

				else if (l_mouse_right_down) {
					if (m_sel_metatile < l_metatiles.size())
						l_screen.m_tilemap.at(tileY).at(tileX) = static_cast<byte>(m_sel_metatile);
				}

			}
			else {
				bool l_shift{ ImGui::IsKeyDown(ImGuiKey_LeftShift) };

				if (m_emode == fe::EditMode::Sprites &&
					l_mouse_left_down) {

					if (l_shift &&
						m_sel_sprite < l_screen.m_sprite_set.size()) {
						l_screen.m_sprite_set.m_sprites[m_sel_sprite].m_x = static_cast<byte>(tileX);
						l_screen.m_sprite_set.m_sprites[m_sel_sprite].m_y = static_cast<byte>(tileY);
					}
					else {
						for (std::size_t s{ 0 }; s < l_screen.m_sprite_set.size(); ++s)
							if (tileX == l_screen.m_sprite_set.m_sprites[s].m_x &&
								tileY == l_screen.m_sprite_set.m_sprites[s].m_y) {
								m_sel_sprite = s;
								break;
							}
					}
				}

				else if (m_emode == fe::EditMode::Doors &&
					l_mouse_left_down) {
					if (l_shift && m_sel_door < l_screen.m_doors.size()) {
						l_screen.m_doors[m_sel_door].m_coords =
						{ static_cast<byte>(tileX), static_cast<byte>(tileY) };
					}
					else {
						for (std::size_t d{ 0 }; d < l_screen.m_doors.size(); ++d)
							if (tileX == l_screen.m_doors[d].m_coords.first &&
								tileY == l_screen.m_doors[d].m_coords.second) {
								m_sel_door = d;
								break;
							}
					}
				}
			}
		}
	}
	ImGui::EndChild();

	// --- Right Panel: Metatile Selector ---
	ImGui::SameLine();

	if (ImGui::BeginChild("RightScreenPanel", ImVec2(rightPanelWidth, panelHeight), true)) {

		ImGui::PushStyleColor(ImGuiCol_Tab, ui::g_uiStyles[0].normal);
		ImGui::PushStyleColor(ImGuiCol_TabActive, ui::g_uiStyles[2].active);
		ImGui::PushStyleColor(ImGuiCol_TabHovered, ui::g_uiStyles[2].hovered);

		if (ImGui::BeginTabBar("ScreenTabs")) {
			// TAB TILEMAP - BEGIN
			if (ImGui::BeginTabItem("Tilemap")) {
				m_emode = fe::EditMode::Tilemap;

				ImGui::SeparatorText("Selected Metatile");

				if (m_sel_metatile >= l_chunk.m_metatiles.size())
					m_sel_metatile = 0;

				imgui_text(std::format("Metatile #{} with property: {}", m_sel_metatile,
					get_description(l_metatiles[m_sel_metatile].m_block_property, c::LABELS_BLOCK_PROPERTIES)));

				ImGui::SeparatorText("Metatiles");
				for (std::size_t i = 0; i < l_metatiles.size(); ++i) {
					if (ImGui::ImageButton(std::format("###mt{}", i).c_str(), m_gfx.get_metatile_texture(i), ImVec2(32, 32))) {
						m_sel_metatile = i;
					}

					if (i == m_sel_metatile) {
						ImVec2 button_pos = ImGui::GetItemRectMin();
						ImVec2 button_end = ImGui::GetItemRectMax();
						ImGui::GetWindowDrawList()->AddRect(
							button_pos, button_end,
							IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.0f
						);
					}

					if ((i + 1) % 10)
						ImGui::SameLine();
				}

				ImGui::NewLine();
				ImGui::SeparatorText("Sub-palette for rendering metatiles");
				if (ui::imgui_slider_with_arrows("###mtsp", "", m_sel_tilemap_sub_palette, 0, 3))
					generate_metatile_textures(p_rnd);

				ImGui::EndTabItem();
			}
			// TAB TILEMAP - END


			// TAB SPRITES - BEGIN
			if (ImGui::BeginTabItem("Sprites")) {
				m_emode = fe::EditMode::Sprites;

				ImGui::PushID("screensprites");

				show_sprite_screen(l_screen.m_sprite_set, m_sel_sprite);

				ImGui::PopID();

				ImGui::EndTabItem();
			}
			// TAB SPRITES - END



			// TAB DOORS - BEGIN
			if (ImGui::BeginTabItem("Doors")) {
				m_emode = fe::EditMode::Doors;

				auto& l_doors{ l_screen.m_doors };
				std::size_t l_door_cnt{ l_doors.size() };

				if (l_door_cnt == 0)
					imgui_text("No doors defined for this screen");
				else {
					if (m_sel_door >= l_door_cnt)
						m_sel_door = l_door_cnt - 1;

					auto& l_door{ l_doors[m_sel_door] };
					auto l_dtype{ l_door.m_door_type };

					// screen door selection
					ui::imgui_slider_with_arrows("###sdoorsel",
						std::format("Selected door: #{}/{}", m_sel_door, l_door_cnt),
						m_sel_door, 0, l_door_cnt - 1);

					ImGui::Separator();

					// door type
					int l_newtype{ static_cast<int>(l_dtype) };

					// chunks 2 (towns) and 6 (buildings) do not have
					// the metadata needed for prev and next world doors
					// and should be impossible to select in the dropdown

					if (ImGui::BeginCombo("Door Type", c::LABELS_DOOR_TYPES[l_dtype])) {
						for (std::size_t i = 0; i < 4; ++i) {

							// can't make same-world door for the town world
							// the door destination index is reduced by 0x20 before indexing
							bool is_disabled = (i == 0 && m_sel_chunk == c::CHUNK_IDX_TOWNS);

							if (is_disabled) ImGui::BeginDisabled();

							bool is_selected = (l_dtype == i);

							if (ImGui::Selectable(c::LABELS_DOOR_TYPES[i], is_selected) && !is_disabled) {
								l_door.m_door_type = static_cast<fe::DoorType>(i);
							}

							if (is_disabled) ImGui::EndDisabled();
						}
						ImGui::EndCombo();
					}

					ImGui::SeparatorText("Door Coordinates");

					ImGui::PushID("srccoords");
					auto l_tmp_pos{ show_position_slider(l_door.m_coords.first,
						l_door.m_coords.second) };
					ImGui::PopID();

					if (l_tmp_pos.has_value())
						l_door.m_coords = l_tmp_pos.value();

					ImGui::SeparatorText("Destination coordinates");

					ImGui::PushID("destcoords");
					l_tmp_pos = show_position_slider(l_door.m_dest_coords.first,
						l_door.m_dest_coords.second);
					ImGui::PopID();

					if (l_tmp_pos.has_value())
						l_door.m_dest_coords = l_tmp_pos.value();

					ImGui::SeparatorText("Other Parameters");

					if (l_door.m_door_type == fe::NextWorld || l_door.m_door_type == fe::PrevWorld) {
						show_stage_door_data(l_door);
					}
					else {
						// requirements - common to buildings and sameworld doors
						byte l_req{ l_door.m_requirement };

						(ui::imgui_slider_with_arrows("doorreqs",
							std::format("Requirement: {0}", get_description(l_req, c::LABELS_DOOR_REQS)),
							l_door.m_requirement, 0, c::LABELS_DOOR_REQS.size() - 1));

						// destination world, can never be edited from here
						// but we show it with a tooltip
						std::string l_dest_tooltip;
						byte l_dest_world;

						if (l_dtype == fe::DoorType::Building) {
							l_dest_world = c::CHUNK_IDX_BUILDINGS;
							l_dest_tooltip = "Building doors go to the buildings world";
						}
						else {
							l_dest_world = static_cast<byte>(m_sel_chunk);
							l_dest_tooltip = "Same-World door";
						}

						ui::imgui_slider_with_arrows("doordestchunk",
							"Destination world", l_dest_world, 0, m_game->m_chunks.size() - 1,
							l_dest_tooltip, true);



						// now to the same for destination screens
						byte l_dest_screen;
						l_dest_tooltip.clear();

						l_dest_screen = l_door.m_dest_screen_id;

						if (ui::imgui_slider_with_arrows("doordestscreen",
							l_door.m_door_type == fe::DoorType::Building ?
							c::LABELS_BUILDINGS[l_dest_screen] : "Destination Screen"
							, l_dest_screen, 0, m_game->m_chunks.at(l_dest_world).m_screens.size() - 1,
							l_dest_tooltip))
							l_door.m_dest_screen_id = l_dest_screen;

						// now to the same for destination screens
						byte l_dest_palette;
						l_dest_tooltip.clear();

						if (l_dtype == fe::DoorType::Building) {
							l_dest_palette = static_cast<byte>(get_default_palette_no(c::CHUNK_IDX_BUILDINGS, l_door.m_dest_screen_id));
							l_dest_tooltip = "Defined by destination screen";
						}
						else {
							l_dest_palette = l_door.m_dest_palette_id;
						}

						if (ui::imgui_slider_with_arrows("doordestpal",
							std::format("Destination palette/music: {}",
								get_description(l_dest_palette, c::LABELS_PALETTES)), l_dest_palette, 0, 30,
							l_dest_tooltip))
							l_door.m_dest_palette_id = l_dest_palette;

						if (l_dtype == fe::DoorType::Building)
							ui::imgui_slider_with_arrows("doornpcs",
								"NPCs: " + get_description(l_door.m_npc_bundle, c::LABELS_NPC_BUNDLES),
								l_door.m_npc_bundle, 0, 70 - 1,
								"A paramater defining which sprites appear inside the building");

						ImGui::Separator();

						// trigger a possible redraw of the gfx atlas
						if (ui::imgui_button("Enter door", 4, "Takes you to the door destination screen with the corresponding palette")) {
							/*
							m_sel_chunk = l_dest_world;
							m_sel_screen = l_dest_screen;
							m_atlas_new_palette_no = l_dest_palette;
							m_atlas_new_tileset_no = get_default_tileset_no(m_sel_chunk, m_sel_screen);
							*/
						}
					}

					ImGui::Separator();

					if (ui::imgui_button("Delete door###delseldoor", 1, "Delete current door")) {
						l_doors.erase(begin(l_doors) + m_sel_door);
						--m_sel_door;
					}
				}

				ImGui::Separator();

				if (ui::imgui_button("Add door###adddoor", 2, "Add a new door to this screen")) {
					l_screen.m_doors.push_back(fe::Door());

					m_sel_door = l_screen.m_doors.size() - 1;
				}

				ImGui::EndTabItem();
			}
			// TAB DOORS - END


			// TAB SCROLLING - BEGIN

			if (ImGui::BeginTabItem("Scrolling")) {
				m_emode = fe::EditMode::Scrolling;

				l_screen.m_scroll_left = show_screen_scroll_section("Left", l_chunk.m_screens.size(), l_screen.m_scroll_left);
				l_screen.m_scroll_right = show_screen_scroll_section("Right", l_chunk.m_screens.size(), l_screen.m_scroll_right);
				l_screen.m_scroll_up = show_screen_scroll_section("Up", l_chunk.m_screens.size(), l_screen.m_scroll_up);
				l_screen.m_scroll_down = show_screen_scroll_section("Down", l_chunk.m_screens.size(), l_screen.m_scroll_down);

				ImGui::EndTabItem();
			}


			// TAB SCROLLING - END



			// TAB TRANSITIONS - BEGIN


			if (ImGui::BeginTabItem("Transitions")) {
				m_emode = fe::EditMode::Transitions;

				bool l_iwt{ l_screen.m_intrachunk_scroll.has_value() };

				if (ui::collapsing_header(
					std::format("Other-world transitions ({})###owiwt",
						l_iwt ? "enabled" : "disabled"
					))) {

					if (l_iwt) {

						auto& l_iwt_data{ l_screen.m_intrachunk_scroll.value() };

						ui::imgui_slider_with_arrows("###cintrat",
							"Destination world: " + c::LABELS_CHUNKS.at(l_iwt_data.m_dest_chunk), l_iwt_data.m_dest_chunk,
							0, 7);

						ui::imgui_slider_with_arrows("###sintrat",
							"Destination screen", l_iwt_data.m_dest_screen,
							0, m_game->m_chunks.at(l_iwt_data.m_dest_chunk).m_screens.size() - 1);

						ImGui::SeparatorText("Destination Coordinates");

						ImGui::PushID("intradc");
						auto l_coords{ show_position_slider(l_iwt_data.m_dest_x, l_iwt_data.m_dest_y) };
						ImGui::PopID();

						if (l_coords.has_value()) {
							l_iwt_data.m_dest_x = l_coords.value().first;
							l_iwt_data.m_dest_y = l_coords.value().second;
						}

						ui::imgui_slider_with_arrows("###sintrap",
							"Destination palette/music", l_iwt_data.m_palette_id,
							0, m_game->m_palettes.size());

						ImGui::Separator();

						if (ui::imgui_button("Go To###intracgo")) {
							m_atlas_new_palette_no = l_iwt_data.m_palette_id;
							m_sel_chunk = l_iwt_data.m_dest_chunk;
							m_sel_screen = l_iwt_data.m_dest_screen;
							m_atlas_new_tileset_no = get_default_tileset_no(m_sel_chunk,
								m_sel_screen);
						}

						ImGui::Separator();

						if (ImGui::Button("Remove Transition###owiwtdel"))
							l_screen.m_intrachunk_scroll.reset();
					}
					else {
						if (ImGui::Button("Add Transition###owiwtadd"))
							l_screen.m_intrachunk_scroll = fe::IntraChunkScroll(
								0, 0, 0, 0
							);
					}

				}

				ImGui::Separator();

				bool l_ict{ l_screen.m_interchunk_scroll.has_value() };

				if (ui::collapsing_header(
					std::format("Same-world transitions ({})###swict",
						l_ict ? "enabled" : "disabled"
					))) {

					if (l_ict) {

						auto& l_ict_data{ l_screen.m_interchunk_scroll.value() };

						ui::imgui_slider_with_arrows("###sintert",
							"Destination screen", l_ict_data.m_dest_screen,
							0, l_chunk.m_screens.size() - 1);

						ImGui::SeparatorText("Destination Coordinates");

						ImGui::PushID("interdc");
						auto l_coords{ show_position_slider(l_ict_data.m_dest_x, l_ict_data.m_dest_y) };
						ImGui::PopID();

						if (l_coords.has_value()) {
							l_ict_data.m_dest_x = l_coords.value().first;
							l_ict_data.m_dest_y = l_coords.value().second;
						}

						ui::imgui_slider_with_arrows("###sinterp",
							"Destination palette/music", l_ict_data.m_palette_id,
							0, m_game->m_palettes.size());

						ImGui::Separator();

						if (ui::imgui_button("Go To###intercgo")) {
							m_atlas_new_palette_no = l_ict_data.m_palette_id;
							m_sel_screen = l_ict_data.m_dest_screen;
						}

						ImGui::Separator();

						if (ImGui::Button("Remove Transition###swiwtdel"))
							l_screen.m_interchunk_scroll.reset();
					}
					else {
						if (ImGui::Button("Add Transition###swiwtadd"))
							l_screen.m_interchunk_scroll = fe::InterChunkScroll(
								0, 0, l_chunk.m_default_palette_no
							);
					}

				}

				ImGui::EndTabItem();
			}

			// TAB TRANSITIONS - END


			ImGui::EndTabBar();
		}

		ImGui::PopStyleColor(3);
	}

	ImGui::EndChild();

	// --- Bottom Panel ---

	ImGui::SeparatorText(std::format("Selected World: {} (#{} of {})",
		c::LABELS_CHUNKS.at(m_sel_chunk),
		m_sel_chunk, m_game->m_chunks.size()).c_str());

	if (fe::ui::imgui_slider_with_arrows("##ws", "",
		m_sel_chunk, static_cast<std::size_t>(0), m_game->m_chunks.size() - 1)) {
		m_sel_screen = 0;
		m_atlas_new_tileset_no = get_default_tileset_no(m_sel_chunk, m_sel_screen);
		m_atlas_new_palette_no = get_default_palette_no(m_sel_chunk, m_sel_screen);
	}

	if (ImGui::BeginChild("SharedBottom", ImVec2(availableWidth, sharedHeight), false)) {

		ImGui::SeparatorText(std::format("Selected Screen: #{} of {}", m_sel_screen, m_game->m_chunks.at(m_sel_chunk).m_screens.size()).c_str());

		if (fe::ui::imgui_slider_with_arrows("##ss", "",
			m_sel_screen, static_cast<std::size_t>(0), m_game->m_chunks.at(m_sel_chunk).m_screens.size() - 1)) {
			m_atlas_new_tileset_no = get_default_tileset_no(m_sel_chunk, m_sel_screen);
			m_atlas_new_palette_no = get_default_palette_no(m_sel_chunk, m_sel_screen);
		}


	}

	ImGui::SeparatorText("Navigation");

	scroll_left_button(l_screen);
	ImGui::SameLine();
	scroll_right_button(l_screen);
	ImGui::SameLine();
	scroll_up_button(l_screen);
	ImGui::SameLine();
	scroll_down_button(l_screen);

	ImGui::SameLine();

	ImGui::Spacing();

	ImGui::SameLine();

	enter_door_button(l_screen);

	ImGui::SameLine();

	ImGui::Spacing();

	ImGui::SameLine();

	transition_sw_button(l_screen);

	ImGui::SameLine();

	transition_ow_button(l_screen);

	ImGui::EndChild();

	ImGui::End();
}


std::optional<byte> fe::MainWindow::show_screen_scroll_section(const std::string& p_direction,
	std::size_t p_screen_count, std::optional<byte> p_scroll_data) {

	std::optional<byte> l_result{ p_scroll_data };

	ImGui::PushID(p_direction.c_str());

	imgui_text(p_direction);

	ImGui::SameLine();

	if (p_scroll_data.has_value()) {
		byte l_scroll_val{ p_scroll_data.value() };

		if (ui::imgui_slider_with_arrows(p_direction.c_str(),
			"", l_scroll_val, 0, p_screen_count - 1))
			l_result = l_scroll_val;

		ImGui::SameLine();

		if (ui::imgui_button("Delete###dsd", 1, "Remove connection"))
			l_result = std::nullopt;

		ImGui::SameLine();

		if (ui::imgui_button("Go To###dsgt", 4, "Go to screen"))
			// just change index, no palette change when scrolling
			m_sel_screen = p_scroll_data.value();
	}
	else {
		imgui_text(": No scrolling defined");

		ImGui::SameLine();

		if (ui::imgui_button("Add###dsgt", 2, "Create connection"))
			// default to 0
			l_result = 0;
	}

	ImGui::PopID();

	return l_result;
}

void fe::MainWindow::show_stage_door_data(fe::Door& p_door) {
	const std::string C_DEFINED_IN_MD{ "Defined in the stage metadata" };

	const auto& l_stages{ m_game->m_stages.m_stages };
	const auto& l_stage{ m_game->m_stages.get_stage_from_world(m_sel_chunk) };
	bool l_unique_stage{ l_stage.has_value() };

	ImGui::SeparatorText("Stage door metadata");

	if (!l_unique_stage) {
		imgui_text(std::format(
			"Stage ID for world {} can not be deduced. Can not show metadata.",
			c::LABELS_CHUNKS[m_sel_chunk]).c_str());
	}
	else {
		bool l_next{ p_door.m_door_type == fe::DoorType::NextWorld };
		const auto& l_stg{ l_stage.value() };
		std::size_t l_dest_stage{ l_next ? l_stg->m_next_stage : l_stg->m_prev_stage };
		std::size_t l_dest_world{ l_stages[l_dest_stage].m_world_id };
		std::size_t l_dest_screen{ l_next ? l_stg->m_next_screen : l_stg->m_prev_screen };
		byte l_dest_req{ l_next ? l_stg->m_next_requirement : l_stg->m_prev_requirement };

		ImGui::Text(std::format("Requirement: {}",
			get_description(l_dest_req, c::LABELS_DOOR_REQS)).c_str());
		ImGui::Text(std::format("Destination stage: {} ({})",
			l_dest_stage, c::LABELS_CHUNKS[l_dest_world]).c_str());
		ImGui::Text(std::format("Destination screen: {}",
			l_dest_screen).c_str());
		ImGui::Text(std::format("Destination palette: {}",
			get_description(m_game->m_chunks.at(l_dest_world).m_default_palette_no,
				c::LABELS_PALETTES)).c_str());
	}
}

void fe::MainWindow::scroll_button(std::string p_button_text, std::optional<byte> p_scroll_dest) {
	if (ui::imgui_button(p_button_text, 4, "", !p_scroll_dest.has_value()))
		m_sel_screen = p_scroll_dest.value();
}

void fe::MainWindow::scroll_left_button(const fe::Screen& p_screen) {
	scroll_button("Scroll Left", p_screen.m_scroll_left);
}

void fe::MainWindow::scroll_right_button(const fe::Screen& p_screen) {
	scroll_button("Scroll Right", p_screen.m_scroll_right);
}

void fe::MainWindow::scroll_up_button(const fe::Screen& p_screen) {
	scroll_button("Scroll Up", p_screen.m_scroll_up);
}

void fe::MainWindow::scroll_down_button(const fe::Screen& p_screen) {
	scroll_button("Scroll Down", p_screen.m_scroll_down);
}

void fe::MainWindow::enter_door_button(const fe::Screen& p_screen) {
	if (ui::imgui_button("Enter Door", 4, "",
		m_sel_door >= p_screen.m_doors.size())) {

		const auto& l_door{ p_screen.m_doors[m_sel_door] };

		if (l_door.m_door_type == fe::SameWorld) {
			m_sel_screen = l_door.m_dest_screen_id;
			m_atlas_new_palette_no = l_door.m_dest_palette_id;
		}
		else if (l_door.m_door_type == fe::Building) {
			m_sel_screen = l_door.m_dest_screen_id;
			m_sel_chunk = c::CHUNK_IDX_BUILDINGS;
			m_atlas_new_palette_no = get_default_palette_no(m_sel_chunk, m_sel_screen);
			m_atlas_new_tileset_no = get_default_tileset_no(m_sel_chunk, m_sel_screen);
		}
		else {
			const auto& l_stage{ m_game->m_stages.get_stage_from_world(m_sel_chunk) };

			if (!l_stage.has_value())
				add_message("Could not deduce stage number from current world", 1);
			else {
				bool l_next{ l_door.m_door_type == fe::NextWorld };
				std::size_t l_dest_stage{ l_next ? l_stage.value()->m_next_stage :
				l_stage.value()->m_prev_stage };

				m_sel_chunk = m_game->m_stages.m_stages[l_dest_stage].m_world_id;
				m_sel_screen = (l_next ? l_stage.value()->m_next_screen : l_stage.value()->m_prev_screen);
				m_atlas_new_palette_no = get_default_palette_no(m_sel_chunk, m_sel_screen);
				m_atlas_new_tileset_no = get_default_tileset_no(m_sel_chunk, m_sel_screen);
			}
		}

	}
}

void fe::MainWindow::transition_sw_button(const fe::Screen& p_screen) {
	if (ui::imgui_button("Transition (SW)", 4, "Same-World",
		!p_screen.m_interchunk_scroll.has_value())) {
		m_sel_screen = p_screen.m_interchunk_scroll.value().m_dest_screen;
		m_atlas_new_palette_no = p_screen.m_interchunk_scroll.value().m_palette_id;
	}
}

void fe::MainWindow::transition_ow_button(const fe::Screen& p_screen) {
	if (ui::imgui_button("Transition (OW)", 4, "Other-World",
		!p_screen.m_intrachunk_scroll.has_value())) {
		m_sel_chunk = p_screen.m_intrachunk_scroll.value().m_dest_chunk;
		m_sel_screen = p_screen.m_intrachunk_scroll.value().m_dest_screen;
		m_atlas_new_palette_no = p_screen.m_intrachunk_scroll.value().m_palette_id;
		m_atlas_new_tileset_no = get_default_tileset_no(m_sel_chunk, m_sel_screen);
	}
}
