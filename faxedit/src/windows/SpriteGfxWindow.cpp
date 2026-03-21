#include "MainWindow.h"
#include "gfx.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include "Imgui_helper.h"
#include "./../fe/fe_constants.h"
#include "./../fe/fe_app_constants.h"
#include "./../common/klib/Kfile.h"
#include "./../fe/sprite/fe_sprite_constants.h"
#include <format>
#include <SDL3/SDL.h>
#include "./../fe/sprite/SpriteGUILoader.h"

using byte = unsigned char;

void fe::MainWindow::draw_sprite_gfx_window(SDL_Renderer* p_rnd) {
	static SpriteGfxEditMode editmode{ fe::SpriteGfxEditMode::Portraits };
	static std::size_t ls_sel_npc{ 0 };

	ui::imgui_screen("Sprite Graphics Editor",
		c::WIN_TILEMAP_X + 50, c::WIN_TILEMAP_Y + 50,
		c::WIN_TILEMAP_W, c::WIN_TILEMAP_H);

	try {
		ImGui::SeparatorText("Edit Mode");

		if (ImGui::RadioButton("NPCs++",
			editmode == fe::SpriteGfxEditMode::NPC))
			editmode = fe::SpriteGfxEditMode::NPC;
		ImGui::SameLine();
		if (ImGui::RadioButton("Player++",
			editmode == fe::SpriteGfxEditMode::Player))
			editmode = fe::SpriteGfxEditMode::Player;
		ImGui::SameLine();
		if (ImGui::RadioButton("Portraits",
			editmode == fe::SpriteGfxEditMode::Portraits))
			editmode = fe::SpriteGfxEditMode::Portraits;
		ImGui::SameLine();
		if (ImGui::RadioButton("Settings",
			editmode == fe::SpriteGfxEditMode::Settings))
			editmode = fe::SpriteGfxEditMode::Settings;

		ImGui::Separator();

		if (editmode == fe::SpriteGfxEditMode::Settings) {
			static std::size_t ls_sel_sprite;
			auto& lr_spr_first_frames{ m_game->m_sprite_gfx_manager.npc_start_frames };

			ui::imgui_slider_with_arrows("###sprssf",
				get_description(static_cast<byte>(ls_sel_sprite), m_labels_sprites),
				ls_sel_sprite, 0, m_sprite_count - 1, "", false, true);

			ui::imgui_slider_with_arrows("###sprsf", "", lr_spr_first_frames.at(ls_sel_sprite),
				0, m_game->m_sprite_gfx_manager.c_npcs.frames.size() - 1);

			ImGui::SeparatorText("Collection bmp export");

			if (ui::imgui_button("Export all NPC frames as bmps", 2)) {
				const auto& npccoll{ m_game->m_sprite_gfx_manager.c_npcs };
				for (std::size_t i{ 0 }; i <= m_sprite_count; ++i)
					export_sprite_frame_bmps(npccoll, c::KEY_COLL_NPCS, i);
			}
			if (ui::imgui_button("Export all player frames as bmps", 2)) {
				const auto& playercoll{ m_game->m_sprite_gfx_manager.c_player };
				export_sprite_frame_bmps(playercoll, c::KEY_COLL_PLAYER, c::KEY_BANK_ARMOR);
				export_sprite_frame_bmps(playercoll, c::KEY_COLL_PLAYER, c::KEY_BANK_WEAPONS);
			}
			if (ui::imgui_button("Export all portrait frames as bmps", 2)) {
				const auto& portraitcoll{ m_game->m_sprite_gfx_manager.c_portraits };
				export_sprite_frame_bmps(portraitcoll, c::KEY_COLL_PORTRAITS, c::KEY_BANK_PORTRAITS);
			}

			ImGui::SeparatorText("Collection bmp import");

			if (ui::imgui_button("Import all NPC frame bmps", 4)) {
				auto& npccoll{ m_game->m_sprite_gfx_manager.c_npcs };

				for (std::size_t i{ 0 }; i < m_sprite_count; ++i) {
					try {
						import_sprite_frame_bmps(npccoll, c::KEY_COLL_NPCS, i);
					}
					catch (const std::exception& ex) {
						add_message(ex.what(), 1);
					}

				}
			}

			if (ui::imgui_button("Import all player frame bmps", 4)) {
				auto& playercoll{ m_game->m_sprite_gfx_manager.c_player };
				import_sprite_frame_bmps(playercoll, c::KEY_COLL_PLAYER, c::KEY_BANK_ARMOR);
				import_sprite_frame_bmps(playercoll, c::KEY_COLL_PLAYER, c::KEY_BANK_WEAPONS);
			}
			if (ui::imgui_button("Import all portrait frame bmps", 4)) {
				auto& portraitcoll{ m_game->m_sprite_gfx_manager.c_portraits };
				import_sprite_frame_bmps(portraitcoll, c::KEY_COLL_PORTRAITS, 0);
			}

		}
		else if (editmode == fe::SpriteGfxEditMode::Portraits)
			show_sprite_gfx_editor(p_rnd, c::KEY_COLL_PORTRAITS, m_game->m_sprite_gfx_manager.c_portraits);
		else if (editmode == fe::SpriteGfxEditMode::Player)
			show_sprite_gfx_editor(p_rnd, c::KEY_COLL_PLAYER, m_game->m_sprite_gfx_manager.c_player);
		else if (editmode == fe::SpriteGfxEditMode::NPC) {
			show_sprite_gfx_editor(p_rnd, c::KEY_COLL_NPCS, m_game->m_sprite_gfx_manager.c_npcs);
		}

	}
	catch (const std::exception& ex) {
		add_message(ex.what(), 1);
	}

	ImGui::End();
}

void fe::MainWindow::show_sprite_gfx_editor(SDL_Renderer* p_rnd,
	std::size_t p_coll, fe::SpriteFrameCollection& p_collection) {
	static std::optional<std::size_t> ls_coll;
	static std::size_t ls_frame{ 0 }, ls_bank{ 0 }, ls_resolved_bank{ 0 };
	static int ls_sel_frame_x{ 0 }, ls_sel_frame_y{ 0 }, ls_sel_bank_x{ 0 }, ls_sel_bank_y{ 0 },
		ls_sel_bank_chr_x{ 0 }, ls_sel_bank_chr_y{ 0 };
	static bool ls_redraw_bank{ true }, ls_redraw_frame{ true };

	if (!ls_coll || *ls_coll != p_coll) {
		ls_redraw_frame = true;
		ls_redraw_bank = true;
		ls_coll = p_coll;
	}

	if (ls_frame >= p_collection.frames.size()) {
		ls_frame = p_collection.frames.size() - 1;
		ls_redraw_frame = true;
	}

	// get the chr bank ID - all frames must be associated with at least one bank
	if (ls_bank >= p_collection.frames[ls_frame].chrbanks.size()) {
		ls_bank = p_collection.frames[ls_frame].chrbanks.size() - 1;
		ls_resolved_bank = p_collection.frames.at(ls_frame).chrbanks.at(ls_bank);
		ls_redraw_bank = true;
	}
	else {
		std::size_t resolved_bank = p_collection.frames.at(ls_frame).chrbanks.at(ls_bank);
		if (resolved_bank != ls_resolved_bank) {
			ls_resolved_bank = resolved_bank;
			ls_redraw_bank = true;
		}
	}

	if (ls_redraw_bank) {
		m_gfx.gen_sprite_selected_chr_bank(p_rnd, p_collection.banks[ls_resolved_bank],
			m_game->m_palettes.at(p_coll < 2 ? 28 : 30));
		ls_redraw_bank = false;
		ls_redraw_frame = true;
	}

	if (ls_redraw_frame) {
		m_gfx.gen_sprite_selected_texture(p_rnd, p_collection.frames[ls_frame].frame,
			p_collection.banks[ls_resolved_bank], m_game->m_palettes.at(p_coll < 2 ? 28 : 30));
		ls_redraw_frame = false;
	}

	constexpr float FRAME_GFX_SCALE{ 3.0f };
	constexpr float FRAME_TILE_SIZE{ 8.0f * FRAME_GFX_SCALE };
	constexpr float BANK_GFX_SCALE{ 2.0f };
	constexpr float BANK_TILE_SIZE{ 8.0f * BANK_GFX_SCALE };

	// draw the frame itself
	if (!p_collection.frames.empty()) {
		if (ls_frame >= p_collection.frames.size())
			ls_frame = p_collection.frames.size() - 1;

		if (ui::imgui_slider_with_arrows("###selframe",
			std::format("Frame #{}/{} ({}x{})", ls_frame, p_collection.frames.size() - 1,
				p_collection.frames.at(ls_frame).frame.w(),
				p_collection.frames.at(ls_frame).frame.h()),
			ls_frame, 0, p_collection.frames.size() - 1, "", false, true))
			ls_redraw_frame = true;

		// get the chr bank ID - all frames must be associated with at least one bank
		if (ls_bank >= p_collection.frames[ls_frame].chrbanks.size()) {
			ls_bank = p_collection.frames[ls_frame].chrbanks.size() - 1;

			std::size_t resolved_bank{ p_collection.frames.at(ls_frame).chrbanks.at(ls_bank) };
			if (resolved_bank != ls_resolved_bank) {
				ls_resolved_bank = resolved_bank;
				ls_redraw_bank = true;
			}
		}

		auto& lr_frame{ p_collection.frames[ls_frame].frame };

		ImGui::SeparatorText("Frame Metadata");

		ui::imgui_slider_with_arrows("###sfofx", "x-offset", lr_frame.offset_x, -128, 127);
		ui::imgui_slider_with_arrows("###sfofy", "y-offset", lr_frame.offset_y, -128, 127);
		ui::imgui_slider_with_arrows("###sfpivx", "x-pivot", lr_frame.pivot_x, -128, 127);

		ImGui::SeparatorText("Dimensions");

		if (ui::imgui_button("+row", 2))
			if (lr_frame.add_row())
				ls_redraw_frame = true;
		ImGui::SameLine();
		if (ui::imgui_button("-row", 1))
			if (lr_frame.pop_row())
				ls_redraw_frame = true;

		if (ui::imgui_button("+col", 2))
			if (lr_frame.add_col())
				ls_redraw_frame = true;
		ImGui::SameLine();
		if (ui::imgui_button("-col", 1))
			if (lr_frame.pop_col())
				ls_redraw_frame = true;

		ImGui::SeparatorText("Rendered Frame");

		// draw the frame texture - we have a valid animation frame and bank index
		auto paint_tile{ imgui_select_tile_image(m_gfx.get_sprite_selected_texture(),
			FRAME_GFX_SCALE, ls_sel_frame_x, ls_sel_frame_y) };

		if (paint_tile) {
			std::size_t bank_idx{ static_cast<std::size_t>(ls_sel_bank_chr_y * 16 + ls_sel_bank_chr_x) };
			if (ls_resolved_bank < p_collection.banks.size() &&
				bank_idx < p_collection.banks[ls_resolved_bank].size() &&
				paint_tile->first < lr_frame.w() &&
				paint_tile->second < lr_frame.h()) {
				auto& tile{ lr_frame.tilemap.at(paint_tile->second).at(paint_tile->first) };
				if (tile) {
					if (tile->index != static_cast<byte>(bank_idx)) {
						tile->index = static_cast<byte>(bank_idx);
						ls_redraw_frame = true;
					}
				}
				else {
					tile = fe::SpriteFrameTile(static_cast<byte>(bank_idx), 0, false, false);
					ls_redraw_frame = true;
				}
			}
		}

		ImGui::SeparatorText("Selected Tile");

		if (static_cast<std::size_t>(ls_sel_frame_x) < lr_frame.w() &&
			static_cast<std::size_t>(ls_sel_frame_y) < lr_frame.h()) {
			auto& lr_tile{ lr_frame.tilemap.at(static_cast<std::size_t>(ls_sel_frame_y)).at(static_cast<std::size_t>(ls_sel_frame_x)) };

			if (!lr_tile) {
				ImGui::Text("No tile");
				if (ui::imgui_button("Add tile", 2))
					lr_tile = fe::SpriteFrameTile(0, 0, false, false);
			}
			else {
				if (ui::imgui_slider_with_arrows("###stchrind",
					std::format("chr-tile {}", lr_tile->index),
					lr_tile->index, 0, p_collection.banks.at(ls_resolved_bank).size()))
					ls_redraw_frame = true;

				if (ui::imgui_slider_with_arrows("###stsp",
					std::format("sub-palette {}", lr_tile->sub_palette),
					lr_tile->sub_palette, 0, 3))
					ls_redraw_frame = true;

				if (ui::imgui_checkbox("v-flip", lr_tile->v_flip))
					ls_redraw_frame = true;
				ImGui::SameLine();
				if (ui::imgui_checkbox("h-flip", lr_tile->h_flip))
					ls_redraw_frame = true;;

				if (ui::imgui_button("Clear tile", 1)) {
					lr_tile = std::nullopt;
					ls_redraw_frame = true;
				}
			}

		}
		else {
			imgui_text("No tile selected");
		}
	}
	else {
		imgui_text("Collection has no animation frames");
		return;
	}

	ImGui::Separator();

	if (ui::imgui_slider_with_arrows("###selbank",
		std::format("chr-bank #{}/{} ({}: {} tiles)", ls_bank, p_collection.frames[ls_frame].chrbanks.size() - 1,
			ls_resolved_bank, p_collection.banks[ls_resolved_bank].size()),
		ls_bank, 0, p_collection.frames[ls_frame].chrbanks.size() - 1, "", false, true))
		ls_redraw_bank = true;

	imgui_select_tile_image(m_gfx.get_sprite_selected_chr_bank(),
		BANK_GFX_SCALE, ls_sel_bank_chr_x, ls_sel_bank_chr_y);

	/*
	TODO: Decide if users should be allowed to canonicalize. bmp import will do this up to sorting
	if (ui::imgui_button("Canonicalize", 2)) {
		m_game->m_sprite_gfx_manager.canonsort_gfx_collection_chr_bank(p_collection, p_collection.frames[ls_frame].chrbanks[ls_bank]);
		ls_redraw_bank = true;
	}
	*/

	ImGui::SeparatorText("File Operations");

	if (ui::imgui_button("Export bmps", 2)) {
		export_sprite_frame_bmps(p_collection, p_coll, ls_resolved_bank);
	}

	ImGui::SameLine();

	if (ui::imgui_button("Import bmps", 4)) {
		import_sprite_frame_bmps(p_collection, p_coll, ls_resolved_bank);
		ls_redraw_bank = true;
	}

	if (ui::imgui_button("Export chr", 2))
		export_sprite_chr_bank(p_collection, p_coll, ls_resolved_bank);

	ImGui::SameLine();

	if (ui::imgui_button("Import chr", 4)) {
		import_sprite_chr_bank(p_collection, p_coll, ls_resolved_bank);
		ls_redraw_bank = true;
	}
}

std::optional<std::pair<int, int>> fe::MainWindow::imgui_select_tile_image(SDL_Texture* tex, float scale, int& sel_x, int& sel_y) const {
	std::optional<std::pair<int, int>> result;
	const float FRAME_TILE_SIZE{ 8.0f * scale };

	ImVec2 size = ImVec2(
		scale * static_cast<float>(tex->w),
		scale * static_cast<float>(tex->h)
	);

	ImVec2 pos = ImGui::GetCursorScreenPos();

	// time
	float t = static_cast<float>(ImGui::GetTime());
	float pulse = (sinf(t * 2.0f) + 1.0f) * 0.5f;

	// base colors
	ImVec4 c1 = ImVec4(0.1f, 0.2f, 0.8f, 1.0f);
	ImVec4 c2 = ImVec4(0.3f, 0.6f, 1.0f, 1.0f);

	ImVec4 c = ImVec4(
		c1.x + (c2.x - c1.x) * pulse,
		c1.y + (c2.y - c1.y) * pulse,
		c1.z + (c2.z - c1.z) * pulse,
		1.0f
	);

	ImU32 col = ImGui::ColorConvertFloat4ToU32(c);

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->AddRectFilled(
		pos,
		ImVec2(pos.x + size.x, pos.y + size.y),
		ImGui::ColorConvertFloat4ToU32(c)
	);

	ImGui::Image(tex, size);

	// image rect in screen coordinates
	const ImVec2 img_min{ ImGui::GetItemRectMin() };
	const ImVec2 img_max{ ImGui::GetItemRectMax() };

	// handle clicking on the frame
	if (ImGui::IsItemHovered() &&
		(ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
			ImGui::IsMouseDown(ImGuiMouseButton_Right))) {
		ImVec2 mouse = ImGui::GetIO().MousePos;

		int tile_x = static_cast<int>((mouse.x - img_min.x) / FRAME_TILE_SIZE);
		int tile_y = static_cast<int>((mouse.y - img_min.y) / FRAME_TILE_SIZE);

		const int tiles_w = tex->w / 8;
		const int tiles_h = tex->h / 8;

		if (tile_x >= 0 && tile_x < tiles_w && tile_y >= 0 && tile_y < tiles_h) {
			if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				sel_x = tile_x;
				sel_y = tile_y;
			}
			else
				result = std::make_pair(tile_x, tile_y);
		}
	}

	// draw selection rectangle
	{
		const int tiles_w = tex->w / 8;
		const int tiles_h = tex->h / 8;

		if (sel_x >= 0 && sel_x < tiles_w &&
			sel_y >= 0 && sel_y < tiles_h) {

			ImVec2 rect_min{
				img_min.x + sel_x * FRAME_TILE_SIZE,
				img_min.y + sel_y * FRAME_TILE_SIZE
			};
			ImVec2 rect_max{
				rect_min.x + FRAME_TILE_SIZE,
				rect_min.y + FRAME_TILE_SIZE
			};

			ImDrawList* draw = ImGui::GetWindowDrawList();
			draw->AddRect(rect_min, rect_max, IM_COL32(0, 0, 0, 255), 0.0f, 0, 3.0f);
			draw->AddRect(rect_min, rect_max, IM_COL32(255, 255, 0, 255), 0.0f, 0, 1.5f);
		}
	}

	return result;
}

void fe::MainWindow::export_sprite_chr_bank(const fe::SpriteFrameCollection& p_coll, std::size_t p_coll_id,
	std::size_t p_bank_id) {
	std::string bank_str_id{ std::format("{}-{:03}", get_sprite_gfx_file_prefix(p_coll_id), p_bank_id) };
	const auto tiles{ p_coll.get_chr_bank(p_bank_id, p_coll_id == c::KEY_COLL_NPCS) };
	save_chr(tiles, bank_str_id);
}

void fe::MainWindow::import_sprite_chr_bank(fe::SpriteFrameCollection& p_coll, std::size_t p_coll_id,
	std::size_t p_bank_id) {
	std::string in_file{ get_chr_file_path(std::format("{}-{:03}",
		get_sprite_gfx_file_prefix(p_coll_id), p_bank_id)) };

	const auto tiles_flat{ klib::file::read_file_as_bytes(in_file) };

	if (tiles_flat.size() % 16 != 0)
		throw std::runtime_error(std::format("File size of '{}' not a multiple of 16", in_file));
	std::size_t tile_count{ tiles_flat.size() / 0x10 };
	const auto tilerange{ m_game->m_sprite_gfx_manager.get_chr_tile_count_range(p_coll_id, p_bank_id) };

	if (tile_count < tilerange.first || tile_count > tilerange.second)
		throw std::runtime_error(std::format("File '{}' contains {} tiles, but tile count must be in range [{}-{}]",
			in_file, tile_count, tilerange.first, tilerange.second));

	std::vector<klib::NES_tile> imp_bank;
	for (std::size_t i{ 0 }; i < tiles_flat.size(); i += 16)
		imp_bank.push_back(klib::NES_tile(tiles_flat, i));

	p_coll.banks.at(p_bank_id) = imp_bank;
	if (p_coll_id == c::KEY_COLL_NPCS)
		p_coll.expand_bank_if_last(p_bank_id);

	const auto chrbytes{ klib::file::read_file_as_bytes(in_file) };
}

void fe::MainWindow::import_sprite_frame_bmps(fe::SpriteFrameCollection& p_coll, std::size_t p_coll_id,
	std::size_t p_bank_id) {

	const auto impact{ m_game->m_sprite_gfx_manager.analyze_bank_impact(p_coll, p_bank_id) };

	if (impact.frame_indexes.empty())
		add_message(std::format("No frames using chr bank {}", p_bank_id), 6);
	else if (impact.banks_identical) {
		auto impres{ m_gfx.import_sprite_frames_from_folder(get_bmp_path(), get_sprite_gfx_file_prefix(p_coll_id),
			p_bank_id, impact.frame_indexes, m_game->m_palettes.at(p_coll_id < 2 ? 28 : 30),
			255, 3) };

		// check overflow
		if (impres.approximated_tile_count > 0)
			throw std::runtime_error("Imported bmps required creating too many chr-tiles");

		for (std::size_t l_bank_id : impact.chr_bank_indexes) {
			const auto tilerange{ m_game->m_sprite_gfx_manager.get_chr_tile_count_range(p_coll_id, p_bank_id) };
			std::size_t tilecount{ impres.tiles.size() };

			if (tilecount < tilerange.first || tilecount > tilerange.second)
				throw std::runtime_error(std::format("bmp import generated {} tiles, but tile count must be in range [{}-{}]",
					tilecount, tilerange.first, tilerange.second));
		}

		if (p_coll_id == c::KEY_COLL_NPCS && p_bank_id == m_game->m_sprite_gfx_manager.c_npcs.banks.size() - 1)
			update_sgfx_result_for_common_bank(impres);

		for (std::size_t l_bank_id : impact.chr_bank_indexes) {
			p_coll.banks.at(l_bank_id) = impres.tiles;
		}
		for (std::size_t i{ 0 }; i < impres.frames.size(); ++i) {
			p_coll.frames.at(impact.frame_indexes.at(i)).frame.tilemap = impres.frames[i].tilemap;
		}

		add_message(std::format("Imported {} bmp files", impact.frame_indexes.size()), 2);
	}
	else {
		throw std::runtime_error(
			std::format("bmp import for bank {} not possible; frames referenced by more than one unique chr-bank",
				p_bank_id));
	}
}

void fe::MainWindow::export_sprite_frame_bmps(const fe::SpriteFrameCollection& p_coll,
	std::size_t p_coll_id, std::size_t p_bank_id) {

	const auto impact{ m_game->m_sprite_gfx_manager.analyze_bank_impact(p_coll, p_bank_id) };

	if (impact.frame_indexes.empty()) {
		add_message(std::format("No frames using chr bank {}", p_bank_id), 6);
	}
	else {
		m_gfx.save_sprite_frames_bmp(p_coll,
			p_bank_id,
			impact.frame_indexes,
			m_game->m_palettes.at(p_coll_id < 2 ? 28 : 30),
			get_bmp_path(),
			get_sprite_gfx_file_prefix(p_coll_id));

		add_message(std::format("Saved {} bmps as {}", impact.frame_indexes.size(),
			m_gfx.get_sprite_frame_bmp_wc_filpath(get_bmp_path(),
				get_sprite_gfx_file_prefix(p_coll_id), p_bank_id)), 2);
	}
}

// when importing common chr bank frames and bank, everything will be relative to 0
// we need to update all frame indexes and the chr bank itself to start at $40
void fe::MainWindow::update_sgfx_result_for_common_bank(fe::SpriteImportResult& import) {
	std::vector<klib::NES_tile> l_bank(256, klib::NES_tile());
	std::vector<fe::SpriteAnimationFrame> l_frames;

	if (import.tiles.size() > c::PPU_COMMON_TILE_COUNT - 1)
		throw std::runtime_error(std::format(
			"Common chr-bank import generated {} tiles, but at most {} are allowed (one slot is reserved for the sprite-0 tile)",
import.tiles.size(), c::PPU_COMMON_TILE_COUNT - 1));

		std::vector<byte> ZERO_HIT_CHR_BYTES(std::begin(c::SPRITE_0_HIT_CHR), std::end(c::SPRITE_0_HIT_CHR));
	klib::NES_tile ZERO_HIT_TILE(ZERO_HIT_CHR_BYTES);

	std::size_t forbidden_chr_idx{ c::SPRITE_0_PPU_IDX_REL_ZERO };
	std::size_t new_7f_index{ import.tiles.size() };
	if (import.tiles.size() > c::SPRITE_0_PPU_IDX_REL_ZERO) {
		// move the generated chr tile to the end
		l_bank[c::PPU_COMMON_TILE_START + new_7f_index] = import.tiles[forbidden_chr_idx];
	}

	for (std::size_t i{ 0 }; i < import.tiles.size(); ++i)
		l_bank.at(c::PPU_COMMON_TILE_START + i) = import.tiles[i];

	// set the zero-hit tile to its necessary location
	l_bank[c::PPU_COMMON_TILE_START + c::SPRITE_0_PPU_IDX_REL_ZERO] = ZERO_HIT_TILE;

	for (const auto& frame : import.frames)
		l_frames.push_back(fe::SpriteAnimationFrame(frame, c::PPU_COMMON_TILE_START,
			static_cast<byte>(forbidden_chr_idx), static_cast<byte>(new_7f_index)));
import.tiles = l_bank;
import.frames = l_frames;
}

std::string fe::MainWindow::get_sprite_gfx_file_prefix(std::size_t p_coll_key) const {
	if (p_coll_key == 0)
		return "npc";
	else if (p_coll_key == 1)
		return "player";
	else if (p_coll_key == 2)
		return "portraits";
	else
		return "unknown";
}

// only call this once the sprite data has been populated
void fe::MainWindow::generate_editor_sprite_gfx(SDL_Renderer* p_rnd) {
	fe::SpriteGUILoader gui_sprites;
	gui_sprites.load_sprites_for_gui(m_game->m_sprite_gfx_manager, m_game->m_rom_data,
		m_config.constant(c::ID_SPRITE_TYPE_OFFSET));

	m_sprite_dims = gui_sprites.get_animation_dimension_data();

	// pass the resolved frames on to the gfx handler to make sprite textures
	m_gfx.gen_sprites(p_rnd, gui_sprites, m_game->m_palettes.at(28));
}
