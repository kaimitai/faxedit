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

	if (ImGui::RadioButton("World Gfx",
		m_gfx_emode == fe::GfxEditMode::WorldChr))
		m_gfx_emode = fe::GfxEditMode::WorldChr;
	ImGui::SameLine();
	if (ImGui::RadioButton("World Palettes",
		m_gfx_emode == fe::GfxEditMode::WorldPalettes))
		m_gfx_emode = fe::GfxEditMode::WorldPalettes;
	ImGui::SameLine();
	if (ImGui::RadioButton("BG Gfx",
		m_gfx_emode == fe::GfxEditMode::BgGraphics))
		m_gfx_emode = fe::GfxEditMode::BgGraphics;
	ImGui::SameLine();
	if (ImGui::RadioButton("BG Palettes",
		m_gfx_emode == fe::GfxEditMode::GfxPalettes))
		m_gfx_emode = fe::GfxEditMode::GfxPalettes;
	ImGui::SameLine();
	if (ImGui::RadioButton("HUD",
		m_gfx_emode == fe::GfxEditMode::HUDAttributes))
		m_gfx_emode = fe::GfxEditMode::HUDAttributes;
	ImGui::SameLine();
	if (ImGui::RadioButton("World Chr",
		m_gfx_emode == fe::GfxEditMode::WorldChrBank))
		m_gfx_emode = fe::GfxEditMode::WorldChrBank;
	ImGui::SameLine();
	if (ImGui::RadioButton("Bg Chr",
		m_gfx_emode == fe::GfxEditMode::GfxChrBank))
		m_gfx_emode = fe::GfxEditMode::GfxChrBank;

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

		if (ui::imgui_button("Extract from ROM", 4)) {
			m_gfx.gen_tilemap_texture(p_rnd,
				get_world_mt_tilemap(m_sel_gfx_ts_world, l_pass_screen),
				l_gfx_key);

			m_gfx.clear_tilemap_import_result(l_gfx_key);
		}

		ImGui::SameLine();

		if (ui::imgui_button("Save bmp", 2)) try {

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
			std::size_t l_tileset_start{ m_game->m_tilesets.at(l_ts_no).start_idx };
			std::size_t l_tileset_end{ m_game->m_tilesets.at(l_ts_no).end_index() };

			for (std::size_t i{ 0 }; i < l_tileset_start; ++i)
				l_res_idx.insert(i);
			for (std::size_t i{ l_tileset_end }; i < 256; ++i)
				l_res_idx.insert(i);

			// if this is the fog world we should fix chr indexes for fog tiles
			if (m_sel_gfx_ts_world == m_game->m_fog.m_world_no) {
				const auto fogtiles{ m_config.vset_as_set(c::ID_FOG_RESERVED_CHR_IDXS) };
				for (const auto b : fogtiles)
					l_res_idx.insert(b);
			}

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
			const auto& chrtiles{ world_ppu_tilesets.at(l_ts_no) };

			for (std::size_t i{ 0 }; i < chrtiles.size(); ++i)
				l_tiles.push_back(fe::ChrGfxTile(chrtiles[i],
					l_res_idx.find(i) != end(l_res_idx),
					(i < l_tileset_end && i >= l_tileset_start) ||
					i < c::CHR_HUD_TILE_COUNT
				));

			auto bmpimportres = m_gfx.import_tilemap_bmp(p_rnd,
				l_tiles,
				flat_pal_to_2d_pal(m_game->m_palettes.at(m_game->get_default_palette_no(m_sel_gfx_ts_world,
					l_pass_screen))),
				ls_dedup_strat,
				get_bmp_path(),
				get_bmp_filename(l_gfx_key),
				l_gfx_key);

			add_message(std::format("Loaded {}", get_bmp_filepath(l_gfx_key)), 2);
			add_message(std::format("{} chr-tiles to spare, {} chr-tiles approximated",
				bmpimportres.first, bmpimportres.second), 6);
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
			l_res_pending ? 2 : 4, "Commit imported graphics to ROM", !l_res_pending)) try {
			const auto gfxres{ m_gfx.get_tilemap_import_result(l_gfx_key) };

			std::size_t l_start_idx{ m_game->m_tilesets.at(l_ts_no).start_idx };
			std::size_t l_end_idx{ m_game->m_tilesets.at(l_ts_no).end_index() };
			std::size_t l_tcount{ l_end_idx - l_start_idx };

			for (std::size_t i{ 0 }; i < l_tcount; ++i)
				m_game->m_tilesets.at(l_ts_no).tiles.at(i) = gfxres.m_tiles.at(l_start_idx + i);
			// the world tileset chr-tiles were updated, update the ui cache
			generate_world_tilesets();

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

			// make life easy for ourselves and wipe all staging data on commit
			m_gfx.clear_all_tilemap_import_results();

			add_message("Imported graphics committed to ROM", 2);
		}
		catch (const std::runtime_error& ex) {
			add_message(ex.what(), 1);
		}
		catch (const std::exception& ex) {
			add_message(ex.what(), 1);
		}

		if (m_gfx.has_tilemap_import_result(l_gfx_key)) {
			static std::size_t ls_rerender_pal{ 0 };

			ImGui::SeparatorText(
				std::format("Preview result under palette: {}",
					get_description(static_cast<byte>(ls_rerender_pal),
						m_labels_palettes)).c_str()
			);

			ui::imgui_slider_with_arrows("###gfxrerender",
				"", ls_rerender_pal, 0, m_game->m_palettes.size() - 1,
				"", false, true);

			if (ui::imgui_button("Re-Render", 4))
				m_gfx.re_render_tilemap_result(p_rnd, l_gfx_key,
					m_game->m_palettes.at(ls_rerender_pal));
			ImGui::Separator();
		}

	}
	else if (m_gfx_emode == fe::GfxEditMode::BgGraphics) {
		static std::size_t ls_sel_bg_game_gfx{ 0 };

		auto& gfxman{ m_game->m_gfx_manager };
		const auto& gfxkey{ c::CHR_GFX_IDS[ls_sel_bg_game_gfx] };
		std::size_t l_gfx_key{ gfxman.get_gfx_numeric_key(gfxkey) };

		ui::imgui_slider_with_arrows("###sgbg",
			std::format("Graphic: {}", gfxman.get_label(gfxkey)),
			ls_sel_bg_game_gfx, 0, c::CHR_GFX_IDS.size() - 1,
			"", false, true);

		auto txt{ m_gfx.get_tileset_txt(l_gfx_key) };

		if (txt != nullptr) {
			ImGui::Image(txt, ImVec2(
				static_cast<float>(2 * txt->w),
				static_cast<float>(2 * txt->h)
			));
		}

		if (ui::imgui_button("Extract from ROM", 4)) {
			m_gfx.gen_tilemap_texture(p_rnd,
				gfxman.get_chrtilemap(gfxkey), l_gfx_key);

			m_gfx.clear_tilemap_import_result(l_gfx_key);
		}
		ImGui::SameLine();
		if (ui::imgui_button("Save bmp", txt == nullptr ? 4 : 2,
			"", txt == nullptr)) try {
			m_gfx.save_tilemap_bmp(
				gfxman.get_chrtilemap(gfxkey),
				get_bmp_path(),
				get_bmp_filename(l_gfx_key)
			);

			add_message(std::format("Saved {}",
				get_bmp_filepath(l_gfx_key)), 2);
		}
		catch (const std::runtime_error& ex) {
			add_message(ex.what(), 1);
		}
		catch (const std::exception& ex) {
			add_message(ex.what(), 1);
		}

		if (ui::imgui_button("Load bmp", 2)) {

			try {
				auto l_tmp_tiles{ gfxman.get_complete_chr_tileset_w_md(gfxkey) };

				auto bmpimportres = m_gfx.import_tilemap_bmp(p_rnd,
					l_tmp_tiles,
					flat_pal_to_2d_pal(gfxman.tilemapdata.at(gfxkey).palette),
					ls_dedup_strat,
					get_bmp_path(),
					get_bmp_filename(l_gfx_key),
					l_gfx_key);

				add_message(std::format("Loaded {}", get_bmp_filepath(l_gfx_key)), 2);
				add_message(std::format("{} chr-tiles to spare, {} chr-tiles approximated",
					bmpimportres.first, bmpimportres.second), 6);
			}
			catch (const std::runtime_error& ex) {
				add_message(ex.what(), 1);
			}
			catch (const std::exception& ex) {
				add_message(ex.what(), 1);
			}

		}

		ImGui::SameLine();

		bool l_res_pending{ m_gfx.has_tilemap_import_result(l_gfx_key) };
		if (ui::imgui_button("Commit to ROM",
			l_res_pending ? 2 : 4, "Commit imported graphics to ROM", !l_res_pending)) try {
			const auto gfxres{ m_gfx.get_tilemap_import_result(l_gfx_key) };

			gfxman.commit_import(gfxkey, gfxres);

			// make life easy for ourselves and wipe all staging data on commit
			m_gfx.clear_all_tilemap_import_results();

			add_message("Graphics committed to ROM", 2);
		}
		catch (const std::runtime_error& ex) {
			add_message(ex.what(), 1);
		}
		catch (const std::exception& ex) {
			add_message(ex.what(), 1);
		}

	}

	else if (m_gfx_emode == fe::GfxEditMode::WorldPalettes) {
		// selected game palette no
		static std::size_t ls_sel_wpal{ 0 };
		auto& wpal{ m_game->m_palettes.at(ls_sel_wpal) };

		ui::imgui_slider_with_arrows("###wpal",
			std::format("Palette: {}", get_description(static_cast<byte>(ls_sel_wpal),
				m_labels_palettes)),
			ls_sel_wpal, 0, m_game->m_palettes.size() - 1,
			"", false, true);

		auto spal_iter{ m_shared_palettes.find(ls_sel_wpal) };
		if (spal_iter != end(m_shared_palettes)) {
			imgui_text(std::format("This palette is used by gfx \"{}\" - Edit under BG Palettes", spal_iter->second));
		}
		else if (show_palette_window(ls_sel_wpal, wpal)) {
			if (m_atlas_palette_no == ls_sel_wpal)
				m_atlas_force_update = true;
		}
	}
	else if (m_gfx_emode == fe::GfxEditMode::GfxPalettes) {
		static std::size_t ls_sel_game_gfx{ 0 };

		auto& gfxman{ m_game->m_gfx_manager };
		const auto& gfxkey{ c::CHR_GFX_IDS[ls_sel_game_gfx] };

		ui::imgui_slider_with_arrows("###sgbg",
			std::format("Graphic: {}", m_game->m_gfx_manager.get_label(gfxkey)),
			ls_sel_game_gfx, 0, c::CHR_GFX_IDS.size() - 1,
			"", false, true);

		show_palette_window(gfxman.get_gfx_numeric_key(gfxkey),
			m_game->m_gfx_manager.get_bg_palette(gfxkey)
		);

		if (!m_game->m_gfx_manager.is_palette_dynamic(gfxkey)) {
			ImGui::Separator();
			imgui_text("This palette will not be saved to ROM.\nIt is only used for bmp export and import.");
		}
	}
	else if (m_gfx_emode == fe::GfxEditMode::HUDAttributes) {
		const std::size_t HUD_GFX_KEY{ 1000 };

		const auto regen_hud = [this](SDL_Renderer* p_rnd,
			std::size_t p_palette_no) -> void {
				m_hud_tilemap.set_flat_palette(m_game->m_palettes.at(p_palette_no));
				const auto& attrs{
					m_game->m_hud_attributes.m_hud_attributes.at(
						m_game->m_hud_attributes.m_palette_to_hud_idx.at(p_palette_no)
					)
				};
				m_hud_tilemap.populate_attribute(attrs.m_tl, attrs.m_tr, attrs.m_bl, attrs.m_br);
				m_gfx.gen_tilemap_texture(p_rnd, m_hud_tilemap, HUD_GFX_KEY);
			};

		static std::size_t ls_sel_wpal{ 0 };
		auto& hud_attrs{ m_game->m_hud_attributes };

		if (ui::imgui_slider_with_arrows("###hpal",
			std::format("Palette: {}", get_description(static_cast<byte>(ls_sel_wpal),
				m_labels_palettes)),
			ls_sel_wpal, 0, m_game->m_palettes.size() - 1,
			"", false, true))
			regen_hud(p_rnd, ls_sel_wpal);

		ImGui::Separator();

		if (ui::imgui_slider_with_arrows("###hamap",
			"HUD Attribute Index", hud_attrs.m_palette_to_hud_idx.at(ls_sel_wpal),
			0, hud_attrs.m_hud_attributes.size() - 1))
			regen_hud(p_rnd, ls_sel_wpal);

		auto& attrs{ hud_attrs.m_hud_attributes.at(hud_attrs.m_palette_to_hud_idx.at(ls_sel_wpal)) };

		ImGui::SeparatorText("HUD Attribute Values");

		ImGui::Text("Note: Altering these values will impact all palettes using this HUD attribute index");

		ImGui::Separator();

		if (ui::imgui_slider_with_arrows("###hatl", "Top Left",
			attrs.m_tl, 0, 3))
			regen_hud(p_rnd, ls_sel_wpal);
		if (ui::imgui_slider_with_arrows("###hatr", "Top Right",
			attrs.m_tr, 0, 3))
			regen_hud(p_rnd, ls_sel_wpal);
		if (ui::imgui_slider_with_arrows("###habl", "Bottom Left",
			attrs.m_bl, 0, 3))
			regen_hud(p_rnd, ls_sel_wpal);
		if (ui::imgui_slider_with_arrows("###habr", "Bottom Right",
			attrs.m_br, 0, 3))
			regen_hud(p_rnd, ls_sel_wpal);

		auto txt{ m_gfx.get_tileset_txt(HUD_GFX_KEY) };

		ImGui::SeparatorText("HUD Preview");

		if (txt != nullptr) {
			ImGui::Image(txt, ImVec2(
				static_cast<float>(2 * txt->w),
				static_cast<float>(2 * txt->h)
			));
		}
		else {
			regen_hud(p_rnd, ls_sel_wpal);
		}
	}
	else if (m_gfx_emode == fe::GfxEditMode::WorldChrBank) {
		show_world_chr_bank_screen(p_rnd);
	}
	else if (m_gfx_emode == fe::GfxEditMode::GfxChrBank) {
		show_gfx_chr_bank_screen(p_rnd);
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

bool fe::MainWindow::show_palette_window(std::size_t p_pal_key, std::vector<byte>& p_palette) {
	bool was_changed{ false };

	// selected palette index
	static std::size_t ls_sel_pal_idx{ 1 };
	static bool ls_edit_bg_col{ false };
	static std::vector<byte> ls_pal_clipboard;

	const auto nescols{ m_gfx.get_nes_palette() };

	ImGui::SeparatorText("Palette Colors");

	for (std::size_t i{ 0 }; i < 16; ++i) {
		bool l_disabled{ !ls_edit_bg_col && i % 4 == 0 };

		ImVec4 col = SDL_Color_to_imgui(nescols->colors[p_palette.at(i)]);
		ImGui::PushStyleColor(ImGuiCol_Button, col);

		if (l_disabled)
			ImGui::BeginDisabled();

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

		// Tooltip with hex value of the NES-palette index
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			imgui_text(std::format("${:02x}", p_palette.at(i)));
			ImGui::EndTooltip();
		}

		if (l_disabled)
			ImGui::EndDisabled();

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
				was_changed = m_undo->apply_palette_edit(p_pal_key, p_palette,
					update_pal_bg_idx(p_palette, static_cast<byte>(i)));
			else was_changed = m_undo->apply_palette_edit(p_pal_key, p_palette,
				ls_sel_pal_idx, static_cast<byte>(i));
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

	ImGui::Separator();

	ui::imgui_checkbox("Allow editing bg-color", ls_edit_bg_col,
		"Faxanadu will override the bg-color with NES palette index $0f (black) in-game");

	ImGui::Separator();

	if (ui::imgui_button("Undo###palundo", 4, "", !m_undo->has_palette_undo(p_pal_key))) {
		m_undo->undo_palette(p_pal_key, p_palette);
		was_changed = true;
	}

	ImGui::SameLine();

	if (ui::imgui_button("Redo###palredo", 4, "", !m_undo->has_palette_redo(p_pal_key))) {
		m_undo->redo_palette(p_pal_key, p_palette);
		was_changed = true;
	}

	if (ui::imgui_button("Copy###palcpy", 4, "Copy entire palette to clipboard"))
		ls_pal_clipboard = p_palette;

	ImGui::SameLine();

	if (ui::imgui_button("Paste###palpaste", 4, "Paste entire palette from clipboard",
		ls_pal_clipboard.empty()))
		was_changed = m_undo->apply_palette_edit(p_pal_key, p_palette, ls_pal_clipboard);

	return was_changed;
}

std::vector<byte> fe::MainWindow::update_pal_bg_idx(std::vector<byte>& p_palette,
	byte p_nes_pal_idx) {
	auto newpal{ p_palette };
	for (std::size_t i{ 0 }; i < p_palette.size(); i += 4)
		newpal[i] = p_nes_pal_idx;
	return newpal;
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

	// the bmp metatile-width should match the metatile picker
	const std::size_t lc_metatile_width{ 10 };
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

		if (resrow.size() % lc_metatile_width == 0) {
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
	result.m_tiles = world_ppu_tilesets.at(
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

void fe::MainWindow::initialize_hud_tilemap(void) {
	std::size_t l_hud_chr_offset{ m_config.constant(c::ID_CHR_HUD_TILE_OFFSET) };

	m_hud_tilemap.m_tiles.clear();
	for (std::size_t i{ 0 }; i < 256; ++i) {
		m_hud_tilemap.m_tiles.push_back(
			klib::NES_tile(m_game->m_rom_data, l_hud_chr_offset + 16 * i)
		);
	}

	auto tileidx{ m_config.bmap_as_numeric_vectors(c::ID_HUD_TILEMAP) };
	std::vector<std::vector<byte>> l_hudtiles;

	for (std::size_t i{ 0 }; i < 4; ++i) {
		auto iter{ tileidx.find(static_cast<byte>(i)) };
		if (iter == end(tileidx))
			l_hudtiles.push_back(std::vector<byte>(32, 0));
		else {
			l_hudtiles.push_back(iter->second);
			while (l_hudtiles.back().size() < 32)
				l_hudtiles.back().push_back(0);
		}
	}

	m_hud_tilemap.m_tilemap.clear();

	for (std::size_t j{ 0 }; j < 4; j += 2) {
		std::vector<std::optional<fe::ChrMetaTile>> row;
		for (std::size_t i{ 0 }; i < 32; i += 2) {
			row.push_back(fe::ChrMetaTile());
			row.back()->m_idxs = {
				l_hudtiles[j][i],
				l_hudtiles[j][i + 1],
				l_hudtiles[j + 1][i],
				l_hudtiles[j + 1][i + 1]
			};
		}
		m_hud_tilemap.m_tilemap.push_back(row);
	}

	m_hud_tilemap.set_flat_palette(std::vector<byte>(16, 0));
}

// generate the door requirement graphics based on the items image
void fe::MainWindow::generate_door_req_gfx(SDL_Renderer* p_rnd) {
	// mapping from door requirement no (treat as 1-indexed) to item graphic no in the item gfx tilemap
	const auto DOOR_REQ_ITEMS{ m_config.bmap_numeric(c::ID_DOOR_REQ_ITEM_GFX) };
	const auto itemgfx{ m_game->m_gfx_manager.get_chrtilemap(c::CHR_GFX_ID_ITEMS) };

	try {
		for (const auto& kv : DOOR_REQ_ITEMS) {
			std::size_t drgfx{ kv.second };

			// very defensive check here
			if (drgfx < itemgfx.m_tilemap.size() && !itemgfx.m_tilemap.at(drgfx).empty() &&
				itemgfx.m_tilemap.at(drgfx).at(0).has_value()) {
				const auto& metatile{ itemgfx.m_tilemap.at(drgfx).at(0).value() };

				std::vector<klib::NES_tile> tiles;
				for (std::size_t chridx : metatile.m_idxs)
					tiles.push_back(itemgfx.m_tiles.at(chridx));

				m_gfx.gen_door_req_gfx(p_rnd, kv.first, tiles, itemgfx.m_palette.at(0));
			}
		}
	}
	catch (const std::runtime_error& ex) {
		add_message(std::format("Could not generate door requirement graphics: {}", ex.what()));
	}
	catch (const std::exception& ex) {
		add_message(std::format("Could not generate door requirement graphics: {}", ex.what()));
	}
	catch (...) {
		add_message("Could not generate door requirement graphics: Unknown exception");
	}

}

void fe::MainWindow::show_gfx_chr_bank_screen(SDL_Renderer* p_rnd) {
	static const std::vector<std::string> lcs_chr_banks{ c::CHR_BANK_TITLE, c::CHR_BANK_INTRO_OUTRO, c::CHR_BANK_ITEMS };
	static std::size_t ls_sel_bank{ 0 };

	const auto bank_chr_w_metadata = [this](const std::string& p_bank_id) ->
		std::pair<std::vector<fe::ChrGfxTile>, std::set<std::size_t>> {
		std::set<std::size_t> fixed_tiles;
		auto tiles{ this->m_game->m_gfx_manager.get_complete_bank_chr_tileset_w_md(p_bank_id) };
		if (this->m_game->m_gfx_manager.is_bank_tile_0_fixed(p_bank_id)) {
			fixed_tiles.insert(0);
			tiles.at(0).m_readonly = false;
		}

		return std::make_pair(tiles, fixed_tiles);
		};

	ImGui::SeparatorText("chr-bank");

	for (std::size_t i{ 0 }; i < lcs_chr_banks.size(); ++i) {
		if (ImGui::RadioButton(lcs_chr_banks[i].c_str(),
			ls_sel_bank == i))
			ls_sel_bank = i;
		ImGui::SameLine();
	}

	ImGui::NewLine();

	const std::string bank_id{ lcs_chr_banks[ls_sel_bank] };
	auto banktxt{ m_gfx.get_bank_chr_gfx(bank_id) };
	if (banktxt != nullptr) {
		ImGui::Image(banktxt, ImVec2(
			static_cast<float>(4 * banktxt->w),
			static_cast<float>(4 * banktxt->h)
		));
	}

	ImGui::Separator();

	if (ui::imgui_button("Re-render", 2) || banktxt == nullptr) {
		auto completebank{ bank_chr_w_metadata(bank_id) };
		m_gfx.gen_bank_chr_gfx(p_rnd, bank_id,
			completebank.first, completebank.second);
	}

	if (ui::imgui_button("Export chr", 4)) {

	}
	ImGui::SameLine();
	if (ui::imgui_button("Import chr", 4)) {

	}

	ImGui::NewLine();

	if (ui::imgui_button("Canonicalize", 4, "Sort and deduplicate the editable portion of the chr bank")) {
		auto completebank{ bank_chr_w_metadata(bank_id) };
		std::vector<klib::NES_tile> banktiles;
		for (const auto& mdtile : completebank.first)
			if (mdtile.m_allowed && !mdtile.m_readonly)
				banktiles.push_back(mdtile.m_tile);

		m_game->m_gfx_manager.apply_canonicalization(bank_id, reorder_chr_tiles(banktiles, completebank.second));
		completebank = bank_chr_w_metadata(bank_id);
		m_gfx.gen_bank_chr_gfx(p_rnd, bank_id,
			completebank.first, completebank.second);
	}

}

void fe::MainWindow::show_world_chr_bank_screen(SDL_Renderer* p_rnd) {
	static std::size_t ls_tileset_no{ 0 };

	ui::imgui_slider_with_arrows("###wchrb", std::format("Tileset {}: {}", ls_tileset_no,
		get_description(static_cast<byte>(ls_tileset_no), m_labels_tilesets)).c_str(),
		ls_tileset_no, 0, m_game->m_tilesets.size() - 1, "", false, true);

	std::string bank_id{ std::format("tileset-{}", ls_tileset_no) };

	auto banktxt{ m_gfx.get_bank_chr_gfx(bank_id) };

	if (banktxt != nullptr) {
		ImGui::Image(banktxt, ImVec2(
			static_cast<float>(4 * banktxt->w),
			static_cast<float>(4 * banktxt->h)
		));
	}

	if (ui::imgui_button("Re-render", 2) || banktxt == nullptr) {
		auto completebank{ get_complete_world_tileset_w_metadata(ls_tileset_no) };
		m_gfx.gen_bank_chr_gfx(p_rnd, bank_id,
			completebank.first, completebank.second);
	}

	if (ui::imgui_button("Canonicalize", 4)) {
		auto chrbank{ get_world_tileset_w_metadata(ls_tileset_no) };
		set_world_tileset_tiles(p_rnd, ls_tileset_no, reorder_chr_tiles(chrbank.first, chrbank.second));
		auto completebank{ get_complete_world_tileset_w_metadata(ls_tileset_no) };
		m_gfx.gen_bank_chr_gfx(p_rnd, bank_id,
			completebank.first, completebank.second);
	}

}

void fe::MainWindow::set_world_tileset_tiles(SDL_Renderer* p_rnd, std::size_t p_tileset_no,
	const fe::ChrReorderResult& p_result) {
	auto& wtileset{ m_game->m_tilesets.at(p_tileset_no) };
	auto& wtiles{ wtileset.tiles };

	if (wtiles.size() == p_result.tiles.size())
		wtiles = p_result.tiles;
	else
		throw std::runtime_error(std::format("Invalid chr tile count {} for world tileset {} - expected {}",
			p_result.tiles.size(), p_tileset_no, wtiles.size()));

	std::size_t ppu_start{ wtileset.start_idx };
	std::size_t ppu_end{ wtileset.end_index() };

	// re-index metatiles for all worlds using this tileset (except buildings)
	for (std::size_t i{ 0 }; i < m_game->m_chunks.size(); ++i)
		if (i != c::CHUNK_IDX_BUILDINGS) {
			if (m_game->get_default_tileset_no(i, 0) == p_tileset_no) {
				for (auto& mt : m_game->m_chunks[i].m_metatiles) {
					for (auto& row : mt.m_tilemap)
						for (auto& b : row) {
							if (static_cast<std::size_t>(b) >= ppu_start &&
								static_cast<std::size_t>(b) < ppu_end)
								b = static_cast<byte>(p_result.idx_old_to_new.at(
									static_cast<std::size_t>(b) - ppu_start
								) + static_cast<byte>(ppu_start));
						}
				}
			}
		}

	// re-index all metatiles in the buildings world used by a screen connected to this tileset, if any
	std::set<std::size_t> building_mts;
	auto& scrs{ m_game->m_chunks[c::CHUNK_IDX_BUILDINGS].m_screens };
	for (std::size_t i{ 0 }; i < scrs.size(); ++i) {
		if (m_game->get_default_tileset_no(c::CHUNK_IDX_BUILDINGS, i) == p_tileset_no)
			for (const auto& row : scrs[i].m_tilemap)
				for (byte b : row)
					building_mts.insert(static_cast<std::size_t>(b));
	}

	auto& bmts{ m_game->m_chunks[c::CHUNK_IDX_BUILDINGS].m_metatiles };
	for (std::size_t mtno : building_mts) {
		for (auto& row : bmts[mtno].m_tilemap)
			for (auto& b : row) {
				if (static_cast<std::size_t>(b) >= ppu_start &&
					static_cast<std::size_t>(b) < ppu_end)
					b = static_cast<byte>(p_result.idx_old_to_new.at(
						static_cast<std::size_t>(b) - ppu_start
					) + static_cast<byte>(ppu_start));
			}
	}

	generate_world_tilesets();
	if (m_game->get_default_tileset_no(m_sel_chunk, m_sel_screen) == p_tileset_no) {
		generate_metatile_textures(p_rnd);
		m_atlas_force_update = true;
	}
}

std::pair<std::vector<klib::NES_tile>, std::set<std::size_t>> fe::MainWindow::get_world_tileset_w_metadata(
	std::size_t p_tileset_no) const {
	const auto totalset{ get_complete_world_tileset_w_metadata(p_tileset_no, true) };
	std::vector<klib::NES_tile> tiles;
	for (const auto& tile : totalset.first)
		tiles.push_back(tile.m_tile);

	return std::make_pair(tiles, totalset.second);
}

std::pair<std::vector<fe::ChrGfxTile>, std::set<std::size_t>> fe::MainWindow::get_complete_world_tileset_w_metadata(
	std::size_t p_tileset_no, bool p_normalize) const {
	std::vector<fe::ChrGfxTile> result;

	const auto& tiles{ world_ppu_tilesets.at(p_tileset_no) };
	std::size_t ppu_start{ m_game->m_tilesets.at(p_tileset_no).start_idx };
	std::size_t ppu_end{ m_game->m_tilesets.at(p_tileset_no).end_index() };

	for (std::size_t i{ 0 }; i < tiles.size(); ++i) {
		auto chrtile{ tiles[i] };
		if (i < c::CHR_HUD_TILE_COUNT && !p_normalize)
			result.push_back(fe::ChrGfxTile(chrtile, true, true));
		else if (i >= ppu_start && i < ppu_end)
			result.push_back(fe::ChrGfxTile(chrtile, false, true));
		else if (!p_normalize)
			result.push_back(fe::ChrGfxTile(chrtile, false, false));
	}

	std::set<std::size_t> reservedidx;
	if (m_game->m_fog.m_world_no < m_game->m_chunks.size() &&
		m_game->get_default_tileset_no(m_game->m_fog.m_world_no, 0) == p_tileset_no) {
		const auto fogchr{ m_config.vset_as_set(c::ID_FOG_RESERVED_CHR_IDXS) };
		for (byte b : fogchr)
			reservedidx.insert(p_normalize ?
				static_cast<std::size_t>(b) - static_cast<std::size_t>(ppu_start) :
				static_cast<std::size_t>(b));
	}

	return std::make_pair(result, reservedidx);
}

fe::ChrReorderResult fe::MainWindow::reorder_chr_tiles(const std::vector<klib::NES_tile>& tiles,
	const std::set<std::size_t>& fixed_indexes) const {
	fe::ChrReorderResult out;
	std::size_t N{ tiles.size() };

	// 1. prepare output and mark fixed slots
	// set output to all empty
	// any fixed tile map to their own positions
	// anything else stays empty until we place the movables we end up keeping
	out.tiles.assign(N, klib::NES_tile{});             // all empty initially
	out.idx_old_to_new.assign(N, std::size_t(-1));     // fill later

	std::vector<bool> is_fixed(N, false);
	for (std::size_t fi : fixed_indexes) {
		assert(fi < N && "fixed index out of range");
		is_fixed[fi] = true;
		out.tiles[fi] = tiles[fi]; // fixed preserved verbatim
	}

	// 2. decide representatives (global deduplication)
	struct TileLess {
		bool operator()(const klib::NES_tile& a, const klib::NES_tile& b) const {
			return a < b;
		}
	};

	std::map<klib::NES_tile, std::size_t, TileLess> first_rep_old;
	std::vector<std::size_t> rep_old(N, std::size_t(-1));
	std::vector<bool> kept_movable(N, false);

	auto consider_index = [&](std::size_t i) {
		auto it = first_rep_old.find(tiles[i]);
		if (it == first_rep_old.end()) {
			first_rep_old.emplace(tiles[i], i);
			rep_old[i] = i;
			if (!is_fixed[i]) kept_movable[i] = true; // unique movable, keep it
		}
		else {
			rep_old[i] = it->second; // duplicate, points to rep
		}
		};

	// seed fixed first (so fixed wins as rep)
	for (std::size_t fi : fixed_indexes) consider_index(fi);

	// then process movables
	for (std::size_t i = 0; i < N; ++i)
		if (!is_fixed[i]) consider_index(i);

	struct Kept { klib::NES_tile tile; std::size_t old_idx; };

	std::vector<Kept> kept;
	kept.reserve(N);

	for (std::size_t i = 0; i < N; ++i) {
		if (kept_movable[i]) kept.push_back({ tiles[i], i });
	}

	std::sort(kept.begin(), kept.end(),
		[](const Kept& a, const Kept& b) {
			if (b.tile < a.tile) return true;   // a before b when a is greater
			if (a.tile < b.tile) return false;  // a after b when a is smaller
			return a.old_idx < b.old_idx;       // deterministic tie-break
		});

	std::vector<std::size_t> movable_positions;
	movable_positions.reserve(N);
	for (std::size_t i = 0; i < N; ++i)
		if (!is_fixed[i]) movable_positions.push_back(i);

	std::vector<std::size_t> new_pos_for_old_kept(N, std::size_t(-1));

	for (std::size_t k = 0; k < kept.size(); ++k) {
		const std::size_t dest = movable_positions[k];
		out.tiles[dest] = kept[k].tile;
		new_pos_for_old_kept[kept[k].old_idx] = dest;
	}

	for (std::size_t i = 0; i < N; ++i) {
		if (is_fixed[i]) {
			out.idx_old_to_new[i] = i; // fixed never reindex
			continue;
		}

		const std::size_t rep = rep_old[i];
		assert(rep != std::size_t(-1));

		if (is_fixed[rep]) {
			out.idx_old_to_new[i] = rep; // maps into fixed rep
		}
		else {
			const std::size_t new_pos = new_pos_for_old_kept[rep];
			assert(new_pos != std::size_t(-1));
			out.idx_old_to_new[i] = new_pos; // maps into moved kept tile
		}
	}

	// TODO: Remove this test once the code is trusted
	// test - begin
	for (std::size_t i = 0; i < N; ++i) {
		const auto& old_tile = tiles[i];
		const auto& new_tile = out.tiles[out.idx_old_to_new[i]];
		// Fixed indices themselves must stay the same
		if (is_fixed[i]) {
			assert(out.idx_old_to_new[i] == i);
			assert(old_tile == out.tiles[i]);
		}
		// Everything must map to an identical tile value
		assert(old_tile == new_tile);
	}
	// test - end

	return out;
}
