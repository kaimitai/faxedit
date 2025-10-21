#include "MainWindow.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include "Imgui_helper.h"

void fe::MainWindow::draw_screen_tilemap_window(SDL_Renderer* p_rnd, fe::Game& p_game) {
	auto& l_chunk = p_game.m_chunks.at(m_sel_chunk);
	auto& l_metatiles = l_chunk.m_metatiles;
	auto& l_screen = l_chunk.m_screens.at(m_sel_screen);
	auto l_screen_tilemap{ m_gfx.get_screen_texture() };

	const std::string l_win_label{ std::format("{} > Screen {} > Position ({},{})",
		c::LABELS_CHUNKS[m_sel_chunk], m_sel_screen,
		m_sel_tile_x, m_sel_tile_y) + "###sms" };

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
	}
	ImGui::EndChild();

	// --- Right Panel: Metatile Selector ---
	ImGui::SameLine();

	if (ImGui::BeginChild("RightScreenPanel", ImVec2(rightPanelWidth, panelHeight), true)) {

		if (ImGui::BeginTabBar("ScreenTabs")) {
			// TAB TILEMAP - BEGIN
			if (ImGui::BeginTabItem("Tilemap")) {

				ImGui::SeparatorText("Selected Metatile");

				if (m_sel_metatile >= l_chunk.m_metatiles.size())
					m_sel_metatile = 0;

				imgui_text(std::format("Metatile #{} with property: {}", m_sel_metatile,
					get_description(l_metatiles[m_sel_metatile].m_block_property, c::LABELS_BLOCK_PROPERTIES)));

				ImGui::SeparatorText("Metatiles");
				for (int i = 0; i < l_metatiles.size(); ++i) {
					if (ImGui::ImageButton(std::format("###mt{}", i).c_str(), m_gfx.get_metatile_texture(i), ImVec2(32, 32))) {
						m_sel_metatile = i;
					}
					if ((i + 1) % 10)
						ImGui::SameLine();
				}

				ImGui::NewLine();
				ImGui::SeparatorText("Sub-palette for rendering metatiles");
				if (ui::imgui_slider_with_arrows("###mtsp", "", m_sel_tilemap_sub_palette, 0, 3))
					generate_metatile_textures(p_rnd, p_game);

				ImGui::EndTabItem();
			}
			// TAB TILEMAP - END


			// TAB SPRITES - BEGIN
			if (ImGui::BeginTabItem("Sprites")) {

				std::size_t l_sprite_cnt{ l_screen.m_sprites.size() };

				auto& l_sprites{ l_screen.m_sprites };

				if (l_sprites.empty()) {
					imgui_text("No sprites defined for this screen");
				}
				else {
					std::size_t l_sprite_cnt{ l_sprites.size() };

					if (m_sel_sprite >= l_sprite_cnt)
						m_sel_sprite = l_sprite_cnt - 1;

					auto& l_sprite{ l_sprites[m_sel_sprite] };

					ui::imgui_slider_with_arrows("###spritesel",
						std::format("Selected sprite: #{}/{}", m_sel_door, l_sprite_cnt),
						m_sel_sprite, 0, l_sprite_cnt - 1);

					ImGui::SeparatorText("Sprite ID");

					ui::imgui_slider_with_arrows("###spriteid",
						"Sprite ID: " + get_description(l_sprite.m_id, c::LABELS_SPRITES),
						l_sprite.m_id, 0, 0x79);

					ImGui::SeparatorText("Position");

					auto l_new_pos{ show_position_slider(l_sprite.m_x, l_sprite.m_y) };

					if (l_new_pos.has_value()) {
						l_sprite.m_x = l_new_pos.value().first;
						l_sprite.m_y = l_new_pos.value().second;
					}

					ImGui::SeparatorText("Dialogue");

					if (!l_sprite.m_text_id.has_value()) {
						imgui_text("No dialogue defined");

						if (ui::imgui_button("Add dialogue", 2, "Define a dialogue for the sprite"))
							l_sprite.m_text_id = 0;
					}
					else {
						ui::imgui_slider_with_arrows("###sprdiag",
							"Dialogue: " + get_description(l_sprite.m_text_id.value(), c::LABELS_DIALOGUE),
							l_sprite.m_text_id.value(), 0, 0x50);

						if (ui::imgui_button("Remove dialogue"))
							l_sprite.m_text_id.reset();
					}

					ImGui::Separator();

					if (ui::imgui_button("Remove sprite"))
						l_sprites.erase(begin(l_sprites) + m_sel_sprite--);
				}

				ImGui::Separator();
				if (ui::imgui_button("Add sprite")) {
					l_screen.add_sprite(0x2a, 0, 0);
					++m_sel_sprite;
				}

				ImGui::EndTabItem();
			}
			// TAB SPRITES - END



			// TAB DOORS - BEGIN
			if (ImGui::BeginTabItem("Doors")) {

				auto& l_doors{ l_screen.m_doors };
				std::size_t l_door_cnt{ l_doors.size() };

				if (l_door_cnt == 0)
					imgui_text("No doors defined for this screen");
				else {
					if (m_sel_door >= l_door_cnt)
						m_sel_door = l_door_cnt - 1;

					auto& l_door{ l_doors[m_sel_door] };
					auto l_dtype{ l_door.m_door_type };

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

							// disable next and prev world for chunks 2 and 6
							// disable sameworld for chunk 6
							// TODO: Check if chunk 6 even supports any of it
							bool is_disabled = ((i >= 2 && !l_chunk.m_door_connections.has_value())
								|| (i == 0 && m_sel_chunk == 2));

							if (is_disabled) ImGui::BeginDisabled();

							bool is_selected = (l_dtype == i);

							if (ImGui::Selectable(c::LABELS_DOOR_TYPES[i], is_selected) && !is_disabled) {
								l_door.m_door_type = static_cast<fe::DoorType>(i);
							}

							if (is_disabled) ImGui::EndDisabled();
						}
						ImGui::EndCombo();
					}

					ImGui::SeparatorText("Coordinates");

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

					// do an error check here - we should never have next/prev-world
					// doors in chunks that don't support them
					if ((l_dtype == fe::DoorType::NextWorld ||
						l_dtype == fe::DoorType::PrevWorld) &&
						!l_chunk.m_door_connections.has_value())
						throw std::runtime_error("Intra-chunk door defined in unsupported chunk");

					// type-specific data
					const std::string C_DEFINED_IN_MD{ "Defined in this world's metadata" };

					// requirements
					byte l_req{ l_door.m_requirement };
					bool l_is_disabled{ false };

					if (l_dtype == fe::DoorType::NextWorld) {
						l_is_disabled = true;
						l_req = l_chunk.m_door_connections.value().m_next_door_req;
					}
					else if (l_dtype == fe::DoorType::PrevWorld) {
						l_is_disabled = true;
						l_req = l_chunk.m_door_connections.value().m_prev_door_req;
					}

					if (ui::imgui_slider_with_arrows("doorreqs",
						std::format("Requirement: {0}", get_description(l_req, c::LABELS_DOOR_REQS)),
						l_req, 0, c::LABELS_DOOR_REQS.size() - 1,
						l_is_disabled ? C_DEFINED_IN_MD : "",
						l_is_disabled
					))
						l_door.m_requirement = l_req;

					// destination world, can never be edited from here
					// but we show it
					std::string l_dest_tooltip;
					byte l_dest_world;

					if (l_dtype == fe::DoorType::Building) {
						l_dest_world = 0x06;
						l_dest_tooltip = "Building doors go to the buildings world";
					}
					else if (l_dtype == fe::DoorType::SameWorld) {
						l_dest_world = static_cast<byte>(m_sel_chunk);
						l_dest_tooltip = "Same-World door";
					}
					else if (l_dtype == fe::DoorType::NextWorld &&
						l_chunk.m_door_connections.has_value()) {
						l_dest_world = l_chunk.m_door_connections.value().m_next_chunk;
						l_dest_tooltip = C_DEFINED_IN_MD;
					}
					else if (l_dtype == fe::DoorType::PrevWorld &&
						l_chunk.m_door_connections.has_value()) {
						l_dest_world = l_chunk.m_door_connections.value().m_prev_chunk;
						l_dest_tooltip = C_DEFINED_IN_MD;
					}

					ui::imgui_slider_with_arrows("doordestchunk",
						"Destination world", l_dest_world, 0, p_game.m_chunks.size() - 1,
						l_dest_tooltip, true);

					// now to the same for destination screens
					byte l_dest_screen;
					l_dest_tooltip.clear();

					if (l_dtype == fe::DoorType::Building ||
						l_dtype == fe::DoorType::SameWorld) {
						l_dest_screen = l_door.m_dest_screen_id;
						l_is_disabled = false;
					}
					else if (l_dtype == fe::DoorType::NextWorld &&
						l_chunk.m_door_connections.has_value()) {
						l_dest_screen = l_chunk.m_door_connections.value().m_next_screen;
						l_dest_tooltip = C_DEFINED_IN_MD;
						l_is_disabled = true;
					}
					else if (l_dtype == fe::DoorType::PrevWorld &&
						l_chunk.m_door_connections.has_value()) {
						l_dest_screen = l_chunk.m_door_connections.value().m_prev_screen;
						l_dest_tooltip = C_DEFINED_IN_MD;
						l_is_disabled = true;
					}

					if (ui::imgui_slider_with_arrows("doordestscreen",
						"Destination screen", l_dest_screen, 0, p_game.m_chunks.at(l_dest_world).m_screens.size() - 1,
						l_dest_tooltip, l_is_disabled))
						l_door.m_dest_screen_id = l_dest_screen;

					// now to the same for destination screens
					byte l_dest_palette;
					l_dest_tooltip.clear();

					if (l_dtype == fe::DoorType::Building) {
						l_dest_palette = static_cast<byte>(get_default_palette_no(p_game, c::IDX_CHUNK_NPC_BUNDLES, l_door.m_dest_screen_id));
						l_dest_tooltip = "Defined by destination screen";
						l_is_disabled = true;
					}
					else if (l_dtype == fe::DoorType::NextWorld &&
						l_chunk.m_door_connections.has_value()) {
						l_dest_palette = p_game.m_chunks.at(l_chunk.m_door_connections.value().m_next_chunk).m_default_palette_no;
						l_dest_tooltip = "Uses the default palette for the destination world";
						l_is_disabled = true;
					}
					else if (l_dtype == fe::DoorType::PrevWorld &&
						l_chunk.m_door_connections.has_value()) {
						l_dest_palette = p_game.m_chunks.at(l_chunk.m_door_connections.value().m_prev_chunk).m_default_palette_no;
						l_dest_tooltip = "Uses the default palette for the destination world";
						l_is_disabled = true;
					}
					else {
						l_dest_palette = l_door.m_dest_palette_id;
					}

					if (ui::imgui_slider_with_arrows("doordestpal",
						"Destination palette/music", l_dest_palette, 0, 30,
						l_dest_tooltip, l_is_disabled))
						l_door.m_dest_palette_id = l_dest_palette;

					if (l_dtype == fe::DoorType::Building)
						ui::imgui_slider_with_arrows("doornpcs",
							"NPCs: " + get_description(l_door.m_npc_bundle, c::LABELS_NPC_BUNDLES),
							l_door.m_npc_bundle, 0, 70 - 1,
							"A paramater defining which sprites appear inside the building");

					ImGui::Separator();

					// trigger a possible redraw of the gfx atlas
					if (ui::imgui_button("Enter door", 4, "Takes you to the door destination screen with the corresponding palette")) {
						m_sel_chunk = l_dest_world;
						m_sel_screen = l_dest_screen;
						m_atlas_new_palette_no = l_dest_palette;
						m_atlas_new_tileset_no = get_default_tileset_no(m_sel_chunk, m_sel_screen);
					}

					ImGui::Separator();

					if (ui::imgui_button("Delete###delseldoor", 1, "Delete current door")) {
						l_doors.erase(begin(l_doors) + m_sel_door);
						--m_sel_door;
					}
				}

				ImGui::Separator();

				if (ui::imgui_button("Add###adddoor", 2, "Add a new door to this screen")) {
					l_screen.m_doors.push_back(fe::Door());

					m_sel_door = l_screen.m_doors.size() - 1;
				}

				ImGui::EndTabItem();
			}
			// TAB DOORS - END


			// TAB SCROLLING - BEGIN

			if (ImGui::BeginTabItem("Scrolling")) {

				l_screen.m_scroll_left = show_screen_scroll_section("Left", l_chunk.m_screens.size(), l_screen.m_scroll_left);
				l_screen.m_scroll_right = show_screen_scroll_section("Right", l_chunk.m_screens.size(), l_screen.m_scroll_right);
				l_screen.m_scroll_up = show_screen_scroll_section("Up", l_chunk.m_screens.size(), l_screen.m_scroll_up);
				l_screen.m_scroll_down = show_screen_scroll_section("Down", l_chunk.m_screens.size(), l_screen.m_scroll_down);

				ImGui::EndTabItem();
			}


			// TAB SCROLLING - END



			// TAB TRANSITIONS - BEGIN


			if (ImGui::BeginTabItem("Transitions")) {

				bool l_iwt{ l_screen.m_intrachunk_scroll.has_value() };

				if (ui::collapsing_header(
					std::format("Other-world transitions ({})###swiwt",
						l_iwt ? "enabled" : "disabled"
					))) {

					if (l_iwt) {

						auto& l_iwt_data{ l_screen.m_intrachunk_scroll.value() };

						ui::imgui_slider_with_arrows("###cintrat",
							"Destination world: " + c::LABELS_CHUNKS.at(l_iwt_data.m_dest_chunk), l_iwt_data.m_dest_chunk,
							0, l_chunk.m_screens.size() - 1);

						ui::imgui_slider_with_arrows("###sintrat",
							"Destination screen", l_iwt_data.m_dest_screen,
							0, p_game.m_chunks.at(l_iwt_data.m_dest_chunk).m_screens.size() - 1);

						ImGui::SeparatorText("Destination Coordinates");

						auto l_coords{ show_position_slider(l_iwt_data.m_dest_x, l_iwt_data.m_dest_y) };

						if (l_coords.has_value()) {
							l_iwt_data.m_dest_x = l_coords.value().first;
							l_iwt_data.m_dest_y = l_coords.value().second;
						}

						ui::imgui_slider_with_arrows("###sintrap",
							"Destination palette/music", l_iwt_data.m_palette_id,
							0, p_game.m_palettes.size());

						ImGui::Separator();

						if (ui::imgui_button("Go To###intracgo")) {
							m_atlas_new_palette_no = l_iwt_data.m_palette_id;
							m_sel_chunk = l_iwt_data.m_dest_chunk;
							m_sel_screen = l_iwt_data.m_dest_screen;
							m_atlas_new_tileset_no = get_default_tileset_no(m_sel_chunk,
								m_sel_screen);
						}

						ImGui::Separator();

						if (ImGui::Button("Remove Transition###swiwtdel"))
							l_screen.m_intrachunk_scroll.reset();
					}
					else {
						if (ImGui::Button("Add Transition###swiwtadd"))
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

						auto l_coords{ show_position_slider(l_ict_data.m_dest_x, l_ict_data.m_dest_y) };

						if (l_coords.has_value()) {
							l_ict_data.m_dest_x = l_coords.value().first;
							l_ict_data.m_dest_y = l_coords.value().second;
						}

						ui::imgui_slider_with_arrows("###sinterp",
							"Destination palette/music", l_ict_data.m_palette_id,
							0, p_game.m_palettes.size());

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
	}

	ImGui::EndChild();

	// --- Bottom Panel ---

	ImGui::SeparatorText(std::format("Selected World: {} (#{} of {})",
		c::LABELS_CHUNKS.at(m_sel_chunk),
		m_sel_chunk, p_game.m_chunks.size()).c_str());

	if (fe::ui::imgui_slider_with_arrows("##ws", "",
		m_sel_chunk, static_cast<std::size_t>(0), p_game.m_chunks.size() - 1)) {
		m_sel_screen = 0;
		m_atlas_new_tileset_no = get_default_tileset_no(m_sel_chunk, m_sel_screen);
		m_atlas_new_palette_no = get_default_palette_no(p_game, m_sel_chunk, m_sel_screen);
	}

	if (ImGui::BeginChild("SharedBottom", ImVec2(availableWidth, sharedHeight), false)) {

		ImGui::SeparatorText(std::format("Selected Screen: #{} of {}", m_sel_screen, p_game.m_chunks.at(m_sel_chunk).m_screens.size()).c_str());

		if (fe::ui::imgui_slider_with_arrows("##ss", "",
			m_sel_screen, static_cast<std::size_t>(0), p_game.m_chunks.at(m_sel_chunk).m_screens.size() - 1)) {
			m_atlas_new_tileset_no = get_default_tileset_no(m_sel_chunk, m_sel_screen);
			m_atlas_new_palette_no = get_default_palette_no(p_game, m_sel_chunk, m_sel_screen);
		}


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
