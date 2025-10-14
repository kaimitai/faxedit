#include "MainWindow.h"
#include "Imgui_helper.h"
#include "./../common/klib/Bitreader.h"
#include "./../fe/fe_constants.h"

void fe::MainWindow::draw_chunk_window(SDL_Renderer* p_rnd, fe::Game& p_game) {

	fe::ui::imgui_screen(std::format("World {} - {}###ww",
		m_sel_chunk,
		fe::c::LABELS_CHUNKS.at(m_sel_chunk))
	);

	if (fe::ui::imgui_slider_with_arrows("##ws", std::format("World #{} of {}", m_sel_chunk, p_game.m_chunks.size()),
		m_sel_chunk, static_cast<std::size_t>(0), p_game.m_chunks.size() - 1)) {
		m_sel_screen = 0;
		m_atlas_new_tileset_no = get_default_tileset_no(m_sel_chunk, m_sel_screen);
		m_atlas_new_palette_no = get_default_palette_no(p_game, m_sel_chunk, m_sel_screen);
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
	ImGui::End();
}
