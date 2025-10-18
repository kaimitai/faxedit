#include "MainWindow.h"
#include "Imgui_helper.h"
#include "./../common/klib/Bitreader.h"
#include "./../fe/fe_constants.h"

void fe::MainWindow::draw_chunk_window(SDL_Renderer* p_rnd, fe::Game& p_game) {

	auto& l_chunk{ p_game.m_chunks.at(m_sel_chunk) };

	fe::ui::imgui_screen(std::format("World Settings: {}###cw",
		fe::c::LABELS_CHUNKS.at(m_sel_chunk))
	);

	if (ui::collapsing_header("Palette", "")) {

		if (fe::ui::imgui_slider_with_arrows("##cdp",
			std::format("Default Palette #{}", l_chunk.m_default_palette_no),
			l_chunk.m_default_palette_no, 0,
			p_game.m_palettes.size() - 1,
			"Default palette used by all screens in this world. Can be overridden in-game by door and transition parameters.")) {
			m_atlas_new_palette_no = l_chunk.m_default_palette_no;
		}

	}

	if (l_chunk.m_door_connections.has_value() &&
		ui::collapsing_header("Intra-World Door Connections",
			"Parameters for the next-world and previous-world doors")) {

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
