#include "MainWindow.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include "Imgui_helper.h"
#include "./../fe/fe_app_constants.h"

void fe::MainWindow::draw_screen_tilemap_window(SDL_Renderer* p_rnd) {
	auto& l_chunk = m_game->m_chunks.at(m_sel_chunk);
	auto& l_metatiles = l_chunk.m_metatiles;
	auto& l_screen = l_chunk.m_screens.at(m_sel_screen);
	auto l_screen_tilemap{ m_gfx.get_screen_texture() };

	const std::string l_win_label{ std::format("{} > Screen {} > {}",
		m_cache.m_labels_worlds[m_sel_chunk], m_sel_screen,
		get_editmode_as_string()) +
		(m_emode == fe::EditMode::TilemapEditMode ?
		std::format(" > Position ({},{})", m_sel_tile_x, m_sel_tile_y) : "")
	+ "###stmw" };

	// set window color to indicate a command byte
	int l_wstyle{ 0 };
	if (l_screen.m_sprite_set.m_command_byte.has_value()) {
		byte l_cbyte{ l_screen.m_sprite_set.m_command_byte.value() };
		if (l_cbyte == 0x00)
			l_wstyle = 2;
		else if (l_cbyte == 1)
			l_wstyle = 1;
		else if (l_cbyte == 2)
			l_wstyle = 4;
	}

	ui::imgui_screen(l_win_label, c::WIN_TILEMAP_X, c::WIN_TILEMAP_Y, c::WIN_TILEMAP_W, c::WIN_TILEMAP_H,
		l_wstyle);

	// Layout constants
	const float rightPanelWidth = 500.0f;
	const float sharedHeight = 200.0f;

	const float tileSize = 16.0f;

	// Available space
	const float availableWidth = ImGui::GetContentRegionAvail().x;
	const float availableHeight = ImGui::GetContentRegionAvail().y;

	const float leftPanelWidth = availableWidth - rightPanelWidth;
	const float panelHeight = availableHeight - sharedHeight;

	// Viewport fills entire panel
	const float viewportWidth = leftPanelWidth;
	const float viewportHeight = panelHeight;
	const auto vp = get_viewport();

	// --- Left Panel: Tilemap ---
	if (ImGui::BeginChild("Tilemap", ImVec2(leftPanelWidth, panelHeight))) {
		ImVec2 childAvail{ ImGui::GetContentRegionAvail() };

		const float viewAspect{ vp.visible_w_px / vp.visible_h_px };

		ImVec2 imageSize{ childAvail.x, childAvail.y };

		if (imageSize.x / imageSize.y > viewAspect)
			imageSize.x = imageSize.y * viewAspect;
		else
			imageSize.y = imageSize.x / viewAspect;

		// center image in remaining space
		const ImVec2 offset{
			(childAvail.x - imageSize.x) * 0.5f,
			(childAvail.y - imageSize.y) * 0.5f
		};

		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset.x);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offset.y);

		ImVec2 imagePos = ImGui::GetCursorScreenPos();

		ImVec2 uv0{
			vp.src_x0_px / c::TILEMAP_VIEW_PX_W,
			vp.src_y0_px / c::TILEMAP_VIEW_PX_H
		};

		ImVec2 uv1{
			vp.src_x1_px / c::TILEMAP_VIEW_PX_W,
			vp.src_y1_px / c::TILEMAP_VIEW_PX_H
		};

		ImGui::Image(
			m_gfx.get_screen_texture(),
			imageSize,
			uv0,
			uv1);

		ImVec2 mousePos = ImGui::GetMousePos();
		bool insideImage =
			mousePos.x >= imagePos.x && mousePos.x < imagePos.x + imageSize.x &&
			mousePos.y >= imagePos.y && mousePos.y < imagePos.y + imageSize.y;

		bool l_mouse_left_down{ ImGui::IsMouseDown(ImGuiMouseButton_Left) };
		bool l_mouse_right_down{ ImGui::IsMouseDown(ImGuiMouseButton_Right) };
		bool l_win_active{ ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) };

		constexpr float tilemapPixelWidth{ static_cast<float>(c::TILEMAP_VIEW_PX_W) };
		constexpr float tilemapPixelHeight{ static_cast<float>(c::TILEMAP_VIEW_PX_H) };

		if (insideImage) {
			const float u{ (mousePos.x - imagePos.x) / imageSize.x };
			const float v{ (mousePos.y - imagePos.y) / imageSize.y };
			const float textureX{ vp.src_x0_px + u * vp.visible_w_px };
			const float textureY{ vp.src_y0_px + v * vp.visible_h_px };

			const auto [worldXpx, worldYpx] {view_px_to_world_px(textureX, textureY)};
			float localX{ worldXpx };
			float localY{ worldYpx };

			const int worldTileX{ static_cast<int>(std::floor(worldXpx / tileSize)) };
			const int worldTileY{ static_cast<int>(std::floor(worldYpx / tileSize)) };

			const bool insideEditableScreen{
				worldTileX >= 0 &&
				worldTileX < c::TILEMAP_SCREEN_MT_W &&
				worldTileY >= 0 &&
				worldTileY < c::TILEMAP_SCREEN_MT_H
			};

			std::size_t tileX{ 0 };
			std::size_t tileY{ 0 };

			if (insideEditableScreen) {
				tileX = static_cast<std::size_t>(worldTileX);
				tileY = static_cast<std::size_t>(worldTileY);
			}

			const bool l_ctrl{ ImGui::IsKeyDown(ImGuiMod_Ctrl) };
			const bool l_mouse_left_dragging{ ImGui::IsMouseDragging(ImGuiMouseButton_Left) };
			const bool pan_with_middle{ ImGui::IsMouseDragging(ImGuiMouseButton_Middle) };

			bool zoom_or_pan{ ImGui::IsKeyDown(ImGuiKey_Space) };

			const bool pan_with_space{ zoom_or_pan && l_mouse_left_dragging };

			if (pan_with_space || pan_with_middle) {
				const ImGuiMouseButton drag_button{ pan_with_middle ?
					ImGuiMouseButton_Middle : ImGuiMouseButton_Left };

				const ImVec2 drag_delta{ ImGui::GetMouseDragDelta(drag_button) };
				const float tex_per_screen_x{ vp.visible_w_px / imageSize.x };
				const float tex_per_screen_y{ vp.visible_h_px / imageSize.y };

				camera.pan(
					-drag_delta.x * tex_per_screen_x,
					-drag_delta.y * tex_per_screen_y,
					vp);

				ImGui::ResetMouseDragDelta(drag_button);
				zoom_or_pan = true;
			}
			else if (l_ctrl) {
				float wheel{ ImGui::GetIO().MouseWheel };

				if (wheel != 0.0f) {
					if (m_settings.m_invert_zoom)
						wheel = -wheel;

					// Mouse position within displayed image [0..1]
					const float u{ (mousePos.x - imagePos.x) / imageSize.x };
					const float v{ (mousePos.y - imagePos.y) / imageSize.y };

					// Texture pixel currently under cursor
					const float anchor_x{ vp.src_x0_px + u * vp.visible_w_px };
					const float anchor_y{ vp.src_y0_px + v * vp.visible_h_px };

					camera.zoom_at(wheel, anchor_x, anchor_y, u, v, vp);

					zoom_or_pan = true;
				}
			}

			// show tile position as tooltip
			if (insideEditableScreen) {
				ImGui::SetNextWindowPos(ImVec2(5, 5));
				ImGui::Begin("TilemapTooltip", nullptr,
					ImGuiWindowFlags_NoDecoration |
					ImGuiWindowFlags_AlwaysAutoResize |
					ImGuiWindowFlags_NoSavedSettings |
					ImGuiWindowFlags_NoMove |
					ImGuiWindowFlags_Tooltip);
				ImGui::Text("(%d, %d)", tileX, tileY);
				ImGui::End();
			}

			if (insideEditableScreen && !zoom_or_pan &&
				l_win_active && (l_mouse_left_down || l_mouse_right_down)) {

				if (m_emode == fe::EditMode::TilemapEditMode) {
					if (l_mouse_left_down) {
						// ctrl + left mouse; color picker
						if (ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
							m_sel_metatile = l_screen.m_tilemap.at(tileY).at(tileX);
						}
						else if (ImGui::IsKeyDown(ImGuiMod_Shift)) {
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
							m_undo->apply_tilemap_edit(m_sel_chunk, m_sel_screen,
								tileX, tileY, static_cast<byte>(m_sel_metatile));
					}

				}
				else {
					bool l_shift{ ImGui::IsKeyDown(ImGuiMod_Shift) };
					bool l_ctrl{ ImGui::IsKeyDown(ImGuiMod_Ctrl) };

					bool l_building{ m_sel_chunk == c::CHUNK_IDX_BUILDINGS };
					bool l_allow_sprite_edit{ !l_building || m_settings.m_show_sprite_sets_in_buildings };

					if (l_allow_sprite_edit &&
						m_emode == fe::EditMode::Sprites &&
						l_mouse_left_down) {

						std::size_t l_spr_index{ l_building ?
						m_sel_npc_bundle_sprite : m_sel_sprite };
						fe::Sprite_set& l_spr_set{ l_building ?
						m_game->m_npc_bundles.at(m_sel_npc_bundle) :
						l_screen.m_sprite_set };

						if (l_shift &&
							l_spr_index < l_spr_set.size()) {
							l_spr_set.m_sprites[l_spr_index].m_x = static_cast<byte>(tileX);
							l_spr_set.m_sprites[l_spr_index].m_y = static_cast<byte>(tileY);
						}
						else {
							for (std::size_t s{ 0 }; s < l_spr_set.size(); ++s) {
								const auto& chksprite{ l_spr_set.m_sprites[s] };
								// need to look for hit with pixel precision as some sprites
								// can be narrower than a metatile

								if (localX >= 16.0f * chksprite.m_x &&
									localX < 16.0f * chksprite.m_x + m_cache.m_sprite_dims[chksprite.m_id].w &&
									localY >= 16.0f * chksprite.m_y &&
									localY < 16.0f * chksprite.m_y + m_cache.m_sprite_dims[chksprite.m_id].h) {

									if (l_building)
										m_sel_npc_bundle_sprite = s;
									else
										m_sel_sprite = s;
									break;
								}
							}
						}
					}

					else if (m_emode == fe::EditMode::Doors &&
						l_mouse_left_down) {
						if (l_shift && m_sel_door < l_screen.m_doors.size()) {
							l_screen.m_doors[m_sel_door].m_coords =
							{ static_cast<byte>(tileX), static_cast<byte>(tileY) };
						}
						else if (l_ctrl && m_sel_door < l_screen.m_doors.size()) {
							l_screen.m_doors[m_sel_door].m_dest_coords =
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
					else if (m_emode == fe::EditMode::Transitions &&
						l_mouse_left_down) {

						if (l_ctrl && l_screen.m_interchunk_scroll.has_value()) {
							l_screen.m_interchunk_scroll->m_dest_x = static_cast<byte>(tileX);
							l_screen.m_interchunk_scroll->m_dest_y = static_cast<byte>(tileY);
						}
						else if (l_shift && l_screen.m_intrachunk_scroll.has_value()) {
							l_screen.m_intrachunk_scroll->m_dest_x = static_cast<byte>(tileX);
							l_screen.m_intrachunk_scroll->m_dest_y = static_cast<byte>(tileY);
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
				m_emode = fe::EditMode::TilemapEditMode;

				ImGui::SeparatorText("Selected Metatile");

				if (m_sel_metatile >= l_chunk.m_metatiles.size())
					m_sel_metatile = 0;

				imgui_text(std::format("Metatile #{} with property: {}", m_sel_metatile,
					get_description(l_metatiles[m_sel_metatile].m_block_property,
						m_cache.m_labels_block_props)));

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

				ImGui::EndTabItem();
			}
			// TAB TILEMAP - END


			// TAB SPRITES - BEGIN
			if (ImGui::BeginTabItem("Sprites")) {
				m_emode = fe::EditMode::Sprites;

				ImGui::PushID("screensprites");

				if (m_sel_chunk != c::CHUNK_IDX_BUILDINGS)
					show_sprite_screen(l_screen.m_sprite_set, m_sel_sprite);
				else if (m_settings.m_show_sprite_sets_in_buildings)
					show_sprite_npc_bundle_screen();

				ImGui::PopID();

				if (m_sel_chunk == c::CHUNK_IDX_BUILDINGS)
					ui::imgui_checkbox("Edit Building Sprite Sets (advanced)",
						m_settings.m_show_sprite_sets_in_buildings,
						"Check the documentation before using this functionality");

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
						m_sel_door, 0, l_door_cnt - 1, "", false, true);

					ImGui::Separator();

					// door type
					int l_newtype{ static_cast<int>(l_dtype) };

					if (ImGui::BeginCombo("Door Type", c::LABELS_DOOR_TYPES[l_dtype])) {
						for (std::size_t i = 0; i < 4; ++i) {

							bool is_selected = (l_dtype == i);

							if (ImGui::Selectable(c::LABELS_DOOR_TYPES[i], is_selected)) {
								l_door.m_door_type = static_cast<fe::DoorType>(i);
								l_door.m_dest_screen_id = 0;
							}

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
					else if (l_door.m_door_type == fe::DoorType::SameWorld &&
						m_cache.m_randomizer_doors) {
						show_randomizer_sameworld_door_data(l_door);
					}
					else {
						// requirements - common to buildings and sameworld doors
						byte l_req{ l_door.m_requirement };

						(ui::imgui_slider_with_arrows("doorreqs",
							std::format("Requirement: {0}", get_description(l_req, m_cache.m_labels_door_reqs)),
							l_door.m_requirement, 0, m_cache.m_labels_door_reqs.size() - 1));

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

						// now do the same for destination screens
						byte l_dest_screen;
						l_dest_tooltip.clear();

						l_dest_screen = l_door.m_dest_screen_id;

						if (ui::imgui_slider_with_arrows("doordestscreen",
							l_door.m_door_type == fe::DoorType::Building ?
							std::format("Destination Screen: {}", get_description(l_dest_screen, m_cache.m_labels_buildings)) :
							"Destination Screen"
							, l_dest_screen, 0, m_game->m_chunks.at(l_dest_world).m_screens.size() - 1,
							l_dest_tooltip))
							l_door.m_dest_screen_id = l_dest_screen;

						// now to the same for destination screens
						byte l_dest_palette;
						l_dest_tooltip.clear();

						if (l_dtype == fe::DoorType::Building) {
							l_dest_palette = static_cast<byte>(m_game->get_default_palette_no(c::CHUNK_IDX_BUILDINGS, l_door.m_dest_screen_id));
						}
						else {
							l_dest_palette = l_door.m_dest_palette_id;
						}

						if (l_dtype == fe::DoorType::SameWorld) {
							if (ui::imgui_slider_with_arrows("doordestpal",
								std::format("Destination palette/music: {}",
									get_description(l_dest_palette, m_cache.m_labels_palettes)), l_dest_palette, 0, 30,
								l_dest_tooltip))
								l_door.m_dest_palette_id = l_dest_palette;
						}
						else if (l_dtype == fe::DoorType::Building) {
							ui::imgui_slider_with_arrows("doornpcs",
								std::format("Sprite set {}", l_door.m_npc_bundle),
								l_door.m_npc_bundle, 0, m_game->m_npc_bundles.size() - 1,
								"A paramater defining which sprites appear inside the building");

							ImGui::SeparatorText("Sprite Set content");

							show_sprite_set_contents(l_door.m_npc_bundle);
						}

						ImGui::Separator();

						// if the padding byte is enabled - show it
						if (m_settings.m_door_pad_byte) {
							ui::imgui_slider_with_arrows("###padbyte", "Padding Byte",
								l_door.m_unknown, 0, 255, "Not used for anything in the base game");
						}

						ImGui::Separator();
					}

				}

				enter_door_button(l_screen);

				ImGui::SeparatorText("Add or remove door");

				if (ui::imgui_button("Add door###adddoor", 2, "Add a new door to this screen")) {
					l_screen.m_doors.push_back(fe::Door());

					m_sel_door = l_screen.m_doors.size() - 1;
				}

				ImGui::SameLine();

				if (ui::imgui_button("Delete door###delseldoor", 1, "Delete current door",
					m_sel_door >= l_door_cnt)) {
					l_doors.erase(begin(l_doors) + m_sel_door);
					--m_sel_door;
				}

				ImGui::EndTabItem();
			}
			// TAB DOORS - END


			// TAB SCROLLING - BEGIN

			if (ImGui::BeginTabItem("Scrolling")) {
				m_emode = fe::EditMode::Scrolling;
				show_screen_scroll_data();
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
							"Destination world: " + m_cache.m_labels_worlds.at(l_iwt_data.m_dest_chunk), l_iwt_data.m_dest_chunk,
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
							std::format("Destination palette: {}",
								get_description(l_iwt_data.m_palette_id, m_cache.m_labels_palettes)),
							l_iwt_data.m_palette_id,
							0, m_game->m_palettes.size());

						ImGui::Separator();

						transition_ow_button(l_screen);

						ImGui::Separator();

						if (ui::imgui_button("Remove Transition###owiwtdel", 1))
							l_screen.m_intrachunk_scroll.reset();
					}
					else {
						if (ui::imgui_button("Add Transition###owiwtadd", 2))
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
							std::format("Destination palette: {}",
								get_description(l_ict_data.m_palette_id, m_cache.m_labels_palettes)),
							l_ict_data.m_palette_id,
							0, m_game->m_palettes.size());

						ImGui::Separator();

						transition_sw_button(l_screen);

						ImGui::Separator();

						if (ui::imgui_button("Remove Transition###swiwtdel", 1))
							l_screen.m_interchunk_scroll.reset();
					}
					else {
						if (ui::imgui_button("Add Transition###swiwtadd", 2))
							l_screen.m_interchunk_scroll = fe::InterChunkScroll(
								0, 0, static_cast<byte>(l_chunk.m_scene.m_palette)
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
	ImVec2 availbottom = ImGui::GetContentRegionAvail();

	if (ImGui::BeginChild("BottomAll", ImVec2(0.0f, sharedHeight), true)) {

		if (ImGui::BeginChild("SharedBottom", ImVec2(availbottom.x * 0.7f, 0.0f), true)) {

			ImGui::SeparatorText(std::format("Selected World: {} (#{} of {})",
				m_cache.m_labels_worlds.at(m_sel_chunk),
				m_sel_chunk, m_game->m_chunks.size()).c_str());

			if (fe::ui::imgui_slider_with_arrows("##ws", "",
				m_sel_chunk, static_cast<std::size_t>(0), m_game->m_chunks.size() - 1)) {
				m_sel_screen = 0;
				m_atlas_new_tileset_no = m_game->get_default_tileset_no(m_sel_chunk, m_sel_screen);
				m_atlas_new_palette_no = m_game->get_default_palette_no(m_sel_chunk, m_sel_screen);
			}

			ImGui::SeparatorText(std::format("Selected Screen: #{} of {}", m_sel_screen, m_game->m_chunks.at(m_sel_chunk).m_screens.size()).c_str());

			if (fe::ui::imgui_slider_with_arrows("##ss", "",
				m_sel_screen, static_cast<std::size_t>(0), m_game->m_chunks.at(m_sel_chunk).m_screens.size() - 1)) {
				m_atlas_new_tileset_no = m_game->get_default_tileset_no(m_sel_chunk, m_sel_screen);
				if (m_sel_chunk == c::CHUNK_IDX_BUILDINGS)
					m_atlas_new_palette_no = m_game->get_default_palette_no(m_sel_chunk, m_sel_screen);
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

			ImGui::SeparatorText("Add / Remove Screens");

			if (ui::imgui_button("Add Screen", 2, "", l_chunk.m_screens.size() == 0xff)) {

				if (m_sel_chunk == c::CHUNK_IDX_BUILDINGS) {
					if (m_sel_screen < l_chunk.m_screens.size()) {
						l_chunk.m_screens.push_back(l_chunk.m_screens.at(m_sel_screen));
						m_game->m_building_scenes.push_back(m_game->m_building_scenes.at(m_sel_screen));
						add_message("New building screen created (copied from the previously selected screen)", 2, true);
					}
				}
				else {
					l_chunk.m_screens.push_back(fe::Screen());
					l_chunk.m_screens.back().initialize_tilemap();
					add_message(std::format("Added new screen to world {}", m_sel_chunk), 2, true);
				}

				m_sel_screen = l_chunk.m_screens.size() - 1;
			}

			ImGui::SameLine();

			if (ui::imgui_button("Delete Screen", 1, "",
				l_chunk.m_screens.size() <= 1 || !ImGui::IsKeyDown(ImGuiKey_ModShift))) try {
				std::size_t l_scr_ref_count{ m_game->get_screen_reference_count(
				m_sel_chunk, m_sel_screen
				) };

				if (l_scr_ref_count > 0)
					add_message(std::format("Cannot delete screen ({} references)", l_scr_ref_count), 1);
				else {
					m_game->delete_screens(m_sel_chunk, { static_cast<byte>(m_sel_screen) });
					m_undo->clear_history(m_sel_chunk);

					if (m_sel_screen >= l_chunk.m_screens.size())
						--m_sel_screen;

					if (m_sel_chunk == c::CHUNK_IDX_BUILDINGS)
						set_atlas_update_values();

					add_message("Screen deleted", 2);
				}
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), 1);
			}

			ImGui::SameLine();

			if (ui::imgui_button("List References", 4, "")) try {
				const auto screenrefs{ m_game->get_refs_to_screen(m_sel_chunk, m_sel_screen) };

				if (screenrefs.empty())
					add_message("Screen has no references", 6);
				else {
					if (screenrefs.size() > 10)
						add_message(std::format("...and {} more", screenrefs.size() - 10), 6);

					for (std::size_t i{ 0 }; i < screenrefs.size() && i < 10; ++i)
						add_message(screenrefs[i].to_string(), 6);

					add_message(std::format("References to World {}, Screen {} ({})",
						m_sel_chunk, m_sel_screen, screenrefs.size()), 4);
				}
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), 1);
			}

		}
		ImGui::EndChild();

		ImGui::SameLine();

		if (ImGui::BeginChild("Renderoptions", ImVec2(0.0f, 0.0f), true)) {

			ui::imgui_checkbox("Animate", m_settings.m_animate);
			ImGui::SameLine();
			ui::imgui_checkbox("Grid", m_settings.m_show_grid);
			ImGui::SameLine();
			ui::imgui_checkbox("Adjacent", m_settings.m_show_adjacent_screens,
				"Render adjacent screens");

			ImGui::SeparatorText("Block-Property Icon Overlays");

			for (std::size_t j{ 0 }; j < 2; ++j) {

				for (std::size_t i{ 0 }; i < 8; ++i) {
					ui::imgui_checkbox(std::format("###io{}", j * 8 + i),
						m_settings.m_overlays[j * 8 + i],
						get_description(static_cast<byte>(j * 8 + i),
							m_cache.m_labels_block_props));
					ImGui::SameLine();
				}

				ImGui::Spacing();
				ImGui::SameLine();
				if (j == 0) {
					ui::imgui_checkbox("Mattock-Breakable", m_settings.m_mattock_overlay);
				}
				else if (j == 1) {
					ui::imgui_checkbox("Door Requirements", m_settings.m_door_req_overlay);
				}

				ImGui::NewLine();
			}

			ImGui::SeparatorText(std::format("Render Palette: {}",
				get_description(static_cast<byte>(m_atlas_new_palette_no), m_cache.m_labels_palettes)).c_str());

			ui::imgui_slider_with_arrows("###renderpal",
				"", m_atlas_new_palette_no, 0, m_game->m_palettes.size() - 1,
				"", false, true);

		}
		ImGui::EndChild();
	}
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
			"Stage ID for world {} can not be deduced.\nCan not show metadata.",
			m_cache.m_labels_worlds[m_sel_chunk]).c_str());
	}
	else {
		bool l_next{ p_door.m_door_type == fe::DoorType::NextWorld };
		const auto& l_stg{ l_stage.value() };
		std::size_t l_dest_stage{ l_next ? l_stg->m_next_stage : l_stg->m_prev_stage };
		std::size_t l_dest_world{ l_stages[l_dest_stage].m_world_id };
		std::size_t l_dest_screen{ l_next ? l_stg->m_next_screen : l_stg->m_prev_screen };
		byte l_dest_req{ l_next ? l_stg->m_next_requirement : l_stg->m_prev_requirement };

		ImGui::Text(std::format("Requirement: {}",
			get_description(l_dest_req, m_cache.m_labels_door_reqs)).c_str());
		ImGui::Text(std::format("Destination stage: {} ({})",
			l_dest_stage, m_cache.m_labels_worlds[l_dest_world]).c_str());
		ImGui::Text(std::format("Destination screen: {}",
			l_dest_screen).c_str());
		ImGui::Text(std::format("Destination palette: {}",
			get_description(static_cast<byte>(m_game->get_default_palette_no(l_dest_world, 0)),
				m_cache.m_labels_palettes)).c_str());
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
	if (ui::imgui_button("Enter Door###ed", 4, "",
		m_sel_door >= p_screen.m_doors.size())) {

		const auto& l_door{ p_screen.m_doors[m_sel_door] };

		if (l_door.m_door_type == fe::SameWorld) {
			if (m_cache.m_randomizer_doors) {
				enter_randomizer_stage_door(l_door);
			}
			else {
				m_sel_screen = l_door.m_dest_screen_id;
				m_atlas_new_palette_no = l_door.m_dest_palette_id;
			}
		}
		else if (l_door.m_door_type == fe::Building) {
			m_sel_screen = l_door.m_dest_screen_id;
			m_sel_chunk = c::CHUNK_IDX_BUILDINGS;
			m_atlas_new_palette_no = m_game->get_default_palette_no(m_sel_chunk, m_sel_screen);
			m_atlas_new_tileset_no = m_game->get_default_tileset_no(m_sel_chunk, m_sel_screen);
			m_sel_npc_bundle = l_door.m_npc_bundle;
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
				m_atlas_new_palette_no = m_game->get_default_palette_no(m_sel_chunk, m_sel_screen);
				m_atlas_new_tileset_no = m_game->get_default_tileset_no(m_sel_chunk, m_sel_screen);
			}
		}

	}

	if (m_sel_screen >= m_game->m_chunks[m_sel_chunk].m_screens.size()) {
		add_message("Invalid door destination", 1);
		m_sel_screen = 0;
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
		m_atlas_new_tileset_no = m_game->get_default_tileset_no(m_sel_chunk, m_sel_screen);
	}
}

void fe::MainWindow::show_sprite_set_contents(std::size_t p_sprite_set) {
	std::size_t l_spr_count{ m_game->m_npc_bundles.at(p_sprite_set).m_sprites.size() };
	for (std::size_t s{ 0 }; s < l_spr_count; ++s) {
		const auto& sprite{ m_game->m_npc_bundles.at(p_sprite_set).m_sprites[s] };

		imgui_text(get_sprite_label(sprite.m_id));

		if (sprite.m_text_id.has_value()) {
			ImGui::SameLine();
			if (ui::imgui_button(
				std::format("Script: {}", sprite.m_text_id.value()), 4,
				"View Script")) {
				m_sel_iscript = sprite.m_text_id.value();
				m_iscript_window = true;
				m_iscript_win_set_focus = true;
			}
		}

		if (l_spr_count - s != 1)
			ImGui::Separator();
	}
}

void fe::MainWindow::set_atlas_update_values(void) {
	m_atlas_new_palette_no = m_game->get_default_palette_no(m_sel_chunk, m_sel_screen);
	m_atlas_new_tileset_no = m_game->get_default_tileset_no(m_sel_chunk, m_sel_screen);
}

// randomizer helpers
void fe::MainWindow::show_randomizer_sameworld_door_data(fe::Door& p_door) {
	byte l_req{ p_door.m_requirement };
	byte l_subreq{ static_cast<byte>(l_req % 16) };
	byte l_deststage{ static_cast<byte>(l_req / 16) };
	const auto& stages{ m_game->m_stages.m_stages };

	byte l_world_no{ static_cast<byte>(stages.at(l_deststage).m_world_id) };
	const auto& worlds{ m_game->m_chunks };

	ui::imgui_slider_with_arrows("rndreq",
		std::format("Requirement: {}", get_description(l_subreq, m_cache.m_labels_door_reqs)),
		l_subreq, 0, m_cache.m_labels_door_reqs.size() - 1);

	ui::imgui_slider_with_arrows("rndpal",
		std::format("Palette: {}", get_description(p_door.m_dest_palette_id, m_cache.m_labels_palettes)),
		p_door.m_dest_palette_id, 0, m_game->m_palettes.size() - 1);

	ImGui::Separator();

	ui::imgui_slider_with_arrows("rndstg",
		std::format("Stage: {} {}", l_deststage, get_description(l_world_no, m_cache.m_labels_worlds)),
		l_deststage, 0, stages.size() - 1);

	ui::imgui_slider_with_arrows("rndscr",
		std::format("Screen: {}", p_door.m_dest_screen_id),
		p_door.m_dest_screen_id, 0, m_game->m_chunks.at(l_world_no).m_screens.size() - 1);

	ImGui::Separator();

	ImGui::Text("Note: This is a randomizer stage-door");

	p_door.m_requirement = static_cast<byte>(l_deststage * 16) + static_cast<byte>(l_subreq);
}

void fe::MainWindow::enter_randomizer_stage_door(const fe::Door& p_door) {
	m_sel_screen = p_door.m_dest_screen_id;
	m_sel_chunk = m_game->m_stages.m_stages.at(p_door.m_requirement / 16).m_world_id;
	m_atlas_new_palette_no = p_door.m_dest_palette_id;
	m_atlas_new_tileset_no = m_game->get_default_tileset_no(m_sel_chunk, m_sel_screen);
}
