#include "MainWindow.h"
#include "gfx.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include "Imgui_helper.h"
#include "./../fe/fe_constants.h"
#include "./../fe/fe_app_constants.h"

using byte = unsigned char;

static std::vector<std::vector<byte>> flat_pal_to_2d_pal(const std::vector<byte>& pal) {
	std::vector<std::vector<byte>> result;

	for (std::size_t j{ 0 }; j < 4; ++j) {
		std::vector<byte> l_subpal;
		for (std::size_t i{ 0 }; i < 4; ++i)
			l_subpal.push_back(pal.at(4 * j + i));
		result.push_back(l_subpal);
	}

	return result;
}

void fe::MainWindow::draw_gfx_window(SDL_Renderer* p_rnd) {
	static fe::ChrDedupMode ls_dedup_strat{
	fe::ChrDedupMode::PalIndex_Eq
	};

	ui::imgui_screen("Graphics Editor",
		c::WIN_ISCRIPT_X, c::WIN_ISCRIPT_Y,
		c::WIN_ISCRIPT_W, c::WIN_ISCRIPT_H);

	ImGui::SeparatorText("Edit Mode");

	if (ImGui::RadioButton("Tilesets and Metatiles",
		m_gfx_emode == fe::GfxEditMode::WorldChr))
		m_gfx_emode = fe::GfxEditMode::WorldChr;
	ImGui::SameLine();
	if (ImGui::RadioButton("BG Graphics",
		m_gfx_emode == fe::GfxEditMode::BgGraphics))
		m_gfx_emode = fe::GfxEditMode::BgGraphics;
	ImGui::SameLine();
	if (ImGui::RadioButton("Palettes",
		m_gfx_emode == fe::GfxEditMode::Palettes))
		m_gfx_emode = fe::GfxEditMode::Palettes;

	ImGui::Separator();

	if (m_gfx_emode == fe::GfxEditMode::WorldChr) {

		ui::imgui_slider_with_arrows("###tsw",
			std::format("World: {}", m_labels_worlds.at(m_sel_gfx_ts_world)), m_sel_gfx_ts_world, 0, 7);

		if (m_sel_gfx_ts_world == c::CHUNK_IDX_BUILDINGS)
			ui::imgui_slider_with_arrows("###tss",
				m_labels_buildings.at(m_sel_gfx_ts_screen), m_sel_gfx_ts_screen, 0, c::WORLD_BUILDINGS_SCREEN_COUNT - 1);

		std::size_t l_ts_no{ m_game->get_default_tileset_no(m_sel_gfx_ts_world, m_sel_gfx_ts_screen) };
		std::size_t l_palette_no{ m_game->get_default_palette_no(m_sel_gfx_ts_world, m_sel_gfx_ts_screen) };

		imgui_text(std::format("Tileset {}: {}", l_ts_no, m_labels_tilesets.at(l_ts_no)));
		imgui_text(std::format("Palette: {}",
			get_description(static_cast<byte>(l_palette_no), m_labels_palettes)
		));

		ImGui::Separator();

		std::size_t l_pass_screen{ m_sel_gfx_ts_world == c::CHUNK_IDX_BUILDINGS ?
			m_sel_gfx_ts_screen : 0 };

		std::size_t l_gfx_key{
			m_sel_gfx_ts_world == c::CHUNK_IDX_BUILDINGS ?
			m_sel_gfx_ts_world * c::WORLD_BUILDINGS_SCREEN_COUNT + l_pass_screen :
			m_sel_gfx_ts_world
		};

		auto txt{ m_gfx.get_tileset_txt(l_gfx_key) };

		if (txt == nullptr)
			imgui_text("Graphics not yet extracted");
		else {
			ImGui::Image(txt, ImVec2(
				static_cast<float>(2 * txt->w),
				static_cast<float>(2 * txt->h)
			));
		}

		ImGui::Separator();

		if (ui::imgui_button("Extract", 4))
			m_gfx.gen_tilemap_texture(p_rnd,
				get_world_mt_tilemap(m_sel_gfx_ts_world, l_pass_screen),
				l_gfx_key);

		ImGui::SameLine();

		if (ui::imgui_button("Save bmp", 2, "", txt == nullptr)) try {

			m_gfx.save_tilemap_bmp(get_world_mt_tilemap(m_sel_gfx_ts_world,
				l_pass_screen),
				get_bmp_path(),
				get_bmp_filename(l_gfx_key)
			);

			add_message(std::format("Saved {}", get_bmp_filepath(l_gfx_key)), 2);
		}
		catch (const std::runtime_error& ex) {
			add_message(ex.what(), 1);
		}
		catch (const std::exception& ex) {
			add_message(ex.what(), 1);
		}

		if (ui::imgui_button("Load bmp", 4)) try {
			std::set<std::size_t> l_res_idx;
			std::size_t l_tileset_start{ m_tileset_start.at(l_ts_no) };
			std::size_t l_tileset_end{ l_tileset_start + m_tileset_size.at(l_ts_no) };

			for (std::size_t i{ 0 }; i < l_tileset_start; ++i)
				l_res_idx.insert(i);
			for (std::size_t i{ l_tileset_end }; i < 256; ++i)
				l_res_idx.insert(i);

			// fix any other chr refs from worlds
			gen_read_only_chr_idx_non_building(l_ts_no, m_sel_gfx_ts_world, l_res_idx);
			// fix any other chr refs from buildings screens
			gen_read_only_chr_idx_building(l_ts_no, m_sel_gfx_ts_world,
				l_pass_screen, l_res_idx);

			// if we are in the buildings world, ensure we do not update metatile refs
			// used by screens with another tileset than this one
			std::set<std::size_t> l_read_only_mts;
			if (m_sel_gfx_ts_world == c::CHUNK_IDX_BUILDINGS)
				gen_fixed_building_metatiles(l_ts_no, l_read_only_mts);

			std::vector<fe::ChrGfxTile> l_tiles;
			const auto& chrtiles{ m_game->m_tilesets.at(l_ts_no) };

			for (std::size_t i{ 0 }; i < chrtiles.size(); ++i)
				l_tiles.push_back(fe::ChrGfxTile(chrtiles[i],
					l_res_idx.find(i) != end(l_res_idx),
					(i < l_tileset_end && i >= l_tileset_start) ||
					i < c::CHR_HUD_TILE_COUNT
				));

			m_gfx.import_tilemap_bmp(p_rnd,
				l_tiles,
				flat_pal_to_2d_pal(m_game->m_palettes.at(m_game->get_default_palette_no(m_sel_gfx_ts_world,
					l_pass_screen))),
				ls_dedup_strat,
				get_bmp_path(),
				get_bmp_filename(l_gfx_key),
				l_gfx_key);

			add_message(std::format("Loaded {}", get_bmp_filepath(l_gfx_key)), 2);
		}
		catch (const std::runtime_error& ex) {
			add_message(ex.what(), 1);
		}
		catch (const std::exception& ex) {
			add_message(ex.what(), 1);
		}

		ImGui::SameLine();

		bool l_res_pending{ m_gfx.has_tilemap_import_result(l_gfx_key) };
		if (ui::imgui_button("Commit to ROM",
			l_res_pending ? 2 : 4, "Commit imported graphics to ROM", !l_res_pending)) {
			const auto gfxres{ m_gfx.get_tilemap_import_result(l_gfx_key) };

			m_game->m_tilesets.at(l_ts_no) = gfxres.m_tiles;

			auto& mts{ m_game->m_chunks.at(m_sel_gfx_ts_world).m_metatiles };
			const auto& restm{ gfxres.m_tilemap };

			auto l_use_mts{ gen_metatile_usage(m_sel_gfx_ts_world,
				l_pass_screen, mts.size()) };

			auto allowediter{ begin(l_use_mts) };

			for (std::size_t j{ 0 }; j < restm.size(); ++j)
				for (std::size_t i{ 0 }; i < restm[j].size(); ++i) {
					if (allowediter == end(l_use_mts))
						break;
					std::size_t mtno{ *allowediter };

					if (restm[j][i].has_value() && mtno < mts.size()) {
						auto& umt{ mts.at(mtno) };
						auto& umt_tm{ umt.m_tilemap };
						const auto& idxs{ restm[j][i]->m_idxs };
						umt.m_attr_tl = static_cast<byte>(restm[j][i]->m_palette);
						umt.m_attr_tr = static_cast<byte>(restm[j][i]->m_palette);
						umt.m_attr_bl = static_cast<byte>(restm[j][i]->m_palette);
						umt.m_attr_br = static_cast<byte>(restm[j][i]->m_palette);

						umt_tm.at(0).at(0) = static_cast<byte>(idxs.at(0));
						umt_tm.at(0).at(1) = static_cast<byte>(idxs.at(1));
						umt_tm.at(1).at(0) = static_cast<byte>(idxs.at(2));
						umt_tm.at(1).at(1) = static_cast<byte>(idxs.at(3));

						++allowediter;
					}
				}

			if (l_ts_no == m_game->get_default_tileset_no(m_sel_chunk, m_sel_screen))
				m_atlas_force_update = true;

			m_gfx.clear_tilemap_import_result(l_gfx_key);

			add_message("Imported graphics committed to ROM", 2);
		}

	}
	else if (m_gfx_emode == fe::GfxEditMode::BgGraphics) {
		static std::size_t ls_sel_bg_game_gfx{ 0 };
		std::size_t l_gfx_key{ 900 + ls_sel_bg_game_gfx };

		ui::imgui_slider_with_arrows("###sgbg",
			std::format("Graphic: {}", m_game->m_game_gfx.at(ls_sel_bg_game_gfx).m_gfx_name),
			ls_sel_bg_game_gfx, 0, m_game->m_game_gfx.size() - 1,
			"", false, true);

		auto txt{ m_gfx.get_tileset_txt(l_gfx_key) };

		if (txt != nullptr) {
			ImGui::Image(txt, ImVec2(
				static_cast<float>(2 * txt->w),
				static_cast<float>(2 * txt->h)
			));
		}

		if (ui::imgui_button("Extract", 4)) {
			auto& gamegfx{ m_game->m_game_gfx.at(ls_sel_bg_game_gfx) };

			if (!gamegfx.m_loaded)
				gamegfx.load_from_rom(m_game->m_rom_data);

			m_gfx.gen_tilemap_texture(p_rnd,
				m_game->m_game_gfx.at(ls_sel_bg_game_gfx).get_chrtilemap(), l_gfx_key);
		}
		ImGui::SameLine();
		if (ui::imgui_button("Save bmp", txt == nullptr ? 4 : 2,
			"", txt == nullptr)) {
			m_gfx.save_tilemap_bmp(
				m_game->m_game_gfx.at(ls_sel_bg_game_gfx).get_chrtilemap(),
				get_bmp_path(),
				get_bmp_filename(l_gfx_key)
			);

			add_message(std::format("Saved {}",
				get_bmp_filepath(l_gfx_key)), 2);
		}

		if (ui::imgui_button("Load bmp", txt == nullptr ? 4 : 2,
			"", txt == nullptr)) {

			try {
				const auto& l_ggfx{ m_game->m_game_gfx };
				std::set<std::size_t> l_lock_ind;

				for (std::size_t gf{ 0 }; gf < l_ggfx.size(); ++gf)
					if (gf != ls_sel_bg_game_gfx &&
						l_ggfx[gf].m_rom_offset_chr == l_ggfx[ls_sel_bg_game_gfx].m_rom_offset_chr) {
						if (!l_ggfx[gf].m_loaded)
							throw std::runtime_error(std::format(
								"Shared chr tiles detected - extract \"{}\" before importing bmp to this image",
								l_ggfx[gf].m_gfx_name)
							); else {
							const auto& ggchrs{ l_ggfx[gf].m_tilemap };
							for (const auto& row : ggchrs)
								for (const auto& col : row)
									for (const auto& idx : col.m_idxs)
										l_lock_ind.insert(idx);
						}
					}

				auto l_tmp_tiles{ m_game->m_game_gfx.at(ls_sel_bg_game_gfx).m_chr_tiles };
				for (std::size_t i : l_lock_ind)
					l_tmp_tiles.at(i).m_readonly = true;

				m_gfx.import_tilemap_bmp(p_rnd,
					l_tmp_tiles,
					m_game->m_game_gfx.at(ls_sel_bg_game_gfx).m_palette,
					ls_dedup_strat,
					get_bmp_path(),
					get_bmp_filename(l_gfx_key),
					l_gfx_key);
			}
			catch (const std::runtime_error& ex) {
				add_message(ex.what(), 1);
			}

		}

		ImGui::SameLine();

		bool l_res_pending{ m_gfx.has_tilemap_import_result(l_gfx_key) };
		if (ui::imgui_button("Commit to ROM",
			l_res_pending ? 2 : 4, "Commit imported graphics to ROM", !l_res_pending)) {
			const auto gfxres{ m_gfx.get_tilemap_import_result(l_gfx_key) };

			m_game->m_game_gfx.at(ls_sel_bg_game_gfx).commit_import(
				gfxres
			);

			// copy the chr tiles to the other bg gfx using this tileset
			// to keep them in synch
			auto& l_ggfx{ m_game->m_game_gfx };

			for (std::size_t gf{ 0 }; gf < l_ggfx.size(); ++gf)
				if (gf != ls_sel_bg_game_gfx &&
					l_ggfx[gf].m_rom_offset_chr == l_ggfx[ls_sel_bg_game_gfx].m_rom_offset_chr &&
					l_ggfx[gf].m_loaded) { // must be true here
					l_ggfx[gf].m_chr_tiles.clear();
					for (const auto& tile : l_ggfx[ls_sel_bg_game_gfx].m_chr_tiles)
						l_ggfx[gf].m_chr_tiles.push_back(tile);
				}

			m_gfx.clear_tilemap_import_result(l_gfx_key);

			add_message("Graphics committed to ROM", 2);
		}

	}

	else if (m_gfx_emode == fe::GfxEditMode::Palettes) {
		// selected game palette no
		static std::size_t ls_sel_wpal{ 0 };
		auto& wpal{ m_game->m_palettes.at(ls_sel_wpal) };

		ui::imgui_slider_with_arrows("###wpal",
			std::format("Palette: {}", get_description(static_cast<byte>(ls_sel_wpal),
				m_labels_palettes)),
			ls_sel_wpal, 0, m_game->m_palettes.size() - 1,
			"", false, true);

		if (show_palette_window(wpal)) {
			if (m_atlas_palette_no == ls_sel_wpal)
				m_atlas_force_update = true;
		}
	}


	if (m_gfx_emode == fe::GfxEditMode::WorldChr ||
		m_gfx_emode == fe::GfxEditMode::BgGraphics) {
		ImGui::SeparatorText("chr-tile deduplication strategy");

		if (ImGui::RadioButton("Sub-Palette",
			ls_dedup_strat == ChrDedupMode::PalIndex_Eq))
			ls_dedup_strat = ChrDedupMode::PalIndex_Eq;

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Compare tiles strictly by palette indices (strict)");

		ImGui::SameLine();
		if (ImGui::RadioButton("NES-Palette",
			ls_dedup_strat == ChrDedupMode::NESPalIndex_Eq))
			ls_dedup_strat = ChrDedupMode::NESPalIndex_Eq;

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Compare tiles by the NES color the palette indices resolve to");

		ImGui::SameLine();
		if (ImGui::RadioButton("RGB",
			ls_dedup_strat == ChrDedupMode::rgb_Eq))
			ls_dedup_strat = ChrDedupMode::rgb_Eq;

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Compare tiles by the rgb-value they resolve to (loose)");
	}

	ImGui::End();
}

bool fe::MainWindow::show_palette_window(std::vector<byte>& p_palette) {
	bool was_changed{ false };

	// selected palette index
	static std::size_t ls_sel_pal_idx{ 0 };
	const auto nescols{ m_gfx.get_nes_palette() };

	ImGui::SeparatorText("Palette Colors");

	for (std::size_t i{ 0 }; i < 16; ++i) {
		ImVec4 col = SDL_Color_to_imgui(nescols->colors[p_palette.at(i)]);
		ImGui::PushStyleColor(ImGuiCol_Button, col);

		if (ImGui::Button(std::format("###wpidx{}", i).c_str(),
			ImVec2(32, 32))) {
			ls_sel_pal_idx = i;
		}

		// Outline if selected
		if (ls_sel_pal_idx == i) {
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			ImVec2 p_min = ImGui::GetItemRectMin();
			ImVec2 p_max = ImGui::GetItemRectMax();
			draw_list->AddRect(p_min, p_max, IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.0f);
		}

		ImGui::PopStyleColor();
		if ((i + 1) % 4 != 0)
			ImGui::SameLine();
	}

	ImGui::SeparatorText("NES-Palette");

	std::size_t l_nes_pal_idx_resolve{ p_palette.at(ls_sel_pal_idx) };

	for (std::size_t i{ 0 }; i < 64; ++i) {
		ImVec4 col = SDL_Color_to_imgui(nescols->colors[i]);
		ImGui::PushStyleColor(ImGuiCol_Button, col);

		if (i == 0x0d)
			ImGui::BeginDisabled();

		if (ImGui::Button(std::format("###wpcol{}", i).c_str(),
			ImVec2(32, 32))) {
			if (ls_sel_pal_idx % 4 == 0)
				was_changed = update_pal_bg_idx(p_palette, static_cast<byte>(i));
			else if (i != l_nes_pal_idx_resolve) {
				was_changed = true;
				p_palette.at(ls_sel_pal_idx) = static_cast<byte>(i);
			}
		}

		// Outline if selected
		if (l_nes_pal_idx_resolve == i) {
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			ImVec2 p_min = ImGui::GetItemRectMin();
			ImVec2 p_max = ImGui::GetItemRectMax();
			draw_list->AddRect(p_min, p_max, IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.0f);
		}

		// Tooltip with hex value
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			imgui_text(std::format("${:02x}", i));
			ImGui::EndTooltip();
		}

		// disable the "forbidden" glitch color
		if (i == 0x0d)
			ImGui::EndDisabled();

		ImGui::PopStyleColor();
		if ((i + 1) % 16 != 0)
			ImGui::SameLine();
	}

	return was_changed;
}

bool fe::MainWindow::update_pal_bg_idx(std::vector<byte>& p_palette,
	byte p_nes_pal_idx) const {
	bool any_change{ false };

	for (std::size_t i{ 0 }; i < p_palette.size(); i += 4) {
		if (p_palette[i] != p_nes_pal_idx) {
			p_palette[i] = p_nes_pal_idx;
			any_change = true;
		}
	}

	return any_change;
}

void fe::MainWindow::gen_read_only_chr_idx_non_building(std::size_t p_tileset_no,
	std::size_t p_world_no, std::set<std::size_t>& p_idxs) const {

	// let us reserve chr indexes which are used by metatile
	// definitions for other worlds using this tileset so we don't make
	// any changes to them, while ignoring buildings
	for (std::size_t i{ 0 }; i < 8; ++i) {
		if (i == c::CHUNK_IDX_BUILDINGS ||
			i == p_world_no ||
			m_game->get_default_tileset_no(i, 0) != p_tileset_no)
			continue;

		const auto& other_mts{ m_game->m_chunks.at(i).m_metatiles };

		for (const auto& omt : other_mts)
			for (const auto& row : omt.m_tilemap)
				for (byte b : row)
					p_idxs.insert(static_cast<std::size_t>(b));
	}

}

void fe::MainWindow::gen_read_only_chr_idx_building(std::size_t p_tileset_no,
	std::size_t p_world_no, std::size_t p_screen_no,
	std::set<std::size_t>& p_idxs) const {

	std::set<std::size_t> l_used_mts;

	for (std::size_t i{ 0 }; i < c::WORLD_BUILDINGS_SCREEN_COUNT; ++i) {

		if (p_world_no == c::CHUNK_IDX_BUILDINGS && i == p_screen_no)
			continue;
		if (m_game->get_default_tileset_no(c::CHUNK_IDX_BUILDINGS, i) != p_tileset_no)
			continue;

		// we have a buildings screen with the tileset we want to protect
		l_used_mts = gen_metatile_usage(c::CHUNK_IDX_BUILDINGS, i, 0);
		break;
	}

	const auto& building_mts{ m_game->m_chunks.at(c::CHUNK_IDX_BUILDINGS).m_metatiles };

	for (std::size_t i{ 0 }; i < building_mts.size(); ++i)
		if (l_used_mts.contains(i))
			for (const auto& row : building_mts[i].m_tilemap)
				for (byte b : row)
					p_idxs.insert(static_cast<std::size_t>(b));
}

std::set<std::size_t> fe::MainWindow::gen_metatile_usage(std::size_t p_world_no,
	std::size_t p_screen_no,
	std::size_t p_total_metatile_count) const {
	std::set<std::size_t> result;

	if (p_world_no == c::CHUNK_IDX_BUILDINGS) {
		std::size_t l_tileset_no{ m_game->get_default_tileset_no(p_world_no,
			p_screen_no) };

		for (std::size_t s{ 0 }; s < c::WORLD_BUILDINGS_SCREEN_COUNT; ++s)
			if (m_game->get_default_tileset_no(p_world_no, s) == l_tileset_no) {
				const auto& scr{ m_game->m_chunks.at(c::CHUNK_IDX_BUILDINGS).m_screens.at(s) };
				for (std::size_t j{ 0 }; j < 13; ++j)
					for (std::size_t i{ 0 }; i < 16; ++i)
						result.insert(scr.get_mt_at_pos(i, j));
			}
	}
	else {
		for (std::size_t i{ 0 }; i < p_total_metatile_count; ++i)
			result.insert(i);
	}

	return result;
}

void fe::MainWindow::gen_fixed_building_metatiles(std::size_t p_tileset_no,
	std::set<std::size_t>& p_idxs) const {

	std::set<std::size_t> result;

	for (std::size_t i{ 0 }; i < c::WORLD_BUILDINGS_SCREEN_COUNT; ++i)
		if (m_game->get_default_tileset_no(c::CHUNK_IDX_BUILDINGS, i) != p_tileset_no) {
			auto l_fixed_idxs = gen_metatile_usage(c::CHUNK_IDX_BUILDINGS, i, 0);
			result.insert(begin(l_fixed_idxs), end(l_fixed_idxs));
		}

	result = p_idxs;
}

fe::ChrTilemap fe::MainWindow::get_world_mt_tilemap(std::size_t p_world_no,
	std::size_t p_screen_no) const {
	fe::ChrTilemap result;
	std::size_t l_tileset_no{ m_game->get_default_tileset_no(
		p_world_no, p_screen_no) };

	const auto& mts{ m_game->m_chunks.at(p_world_no).m_metatiles };

	std::set<std::size_t> use_mt_idx{ gen_metatile_usage(p_world_no,
		p_screen_no, mts.size()) };

	// set palette
	result.m_palette = flat_pal_to_2d_pal(m_game->m_palettes.at(m_game->get_default_palette_no(p_world_no, p_screen_no)));

	// generate tilemap
	std::vector<std::optional<fe::ChrMetaTile>> resrow;
	for (std::size_t i{ 0 }; i < mts.size(); ++i) {
		if (!use_mt_idx.contains(i))
			continue;

		fe::ChrMetaTile tile;
		tile.m_palette = mts[i].get_palette_attribute(0, 0);

		for (const auto& col : mts[i].m_tilemap)
			for (byte b : col)
				tile.m_idxs.push_back(static_cast<std::size_t>(b));

		resrow.push_back(tile);

		if (resrow.size() % 16 == 0) {
			result.m_tilemap.push_back(resrow);
			resrow.clear();
		}
	}

	if (!resrow.empty()) {
		while (resrow.size() % 16 != 0)
			resrow.push_back(std::nullopt);
		result.m_tilemap.push_back(resrow);
	}

	// get chr tiles with the correct metadata
	result.m_tiles = m_game->m_tilesets.at(
		m_game->get_default_tileset_no(p_world_no, p_screen_no)
	);

	return result;
}

std::string fe::MainWindow::get_bmp_path(void) const {
	return std::format("{}/{}-bmp", m_path.string(), m_filename);
}

std::string fe::MainWindow::get_bmp_filename(std::size_t p_gfx_key) const {
	return std::format("tilemap-{}.bmp", p_gfx_key);
}

std::string fe::MainWindow::get_bmp_filepath(std::size_t p_gfx_key) const {
	return std::format("{}/{}", get_bmp_path(),
		get_bmp_filename(p_gfx_key));
}

ImVec4 fe::MainWindow::SDL_Color_to_imgui(const SDL_Color& c) const {
	return ImVec4(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
}
