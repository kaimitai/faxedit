#include "MainWindow.h"
#include "gfx.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"
#include "Imgui_helper.h"
#include "fe/fe_constants.h"
#include "fe/fe_app_constants.h"
#include "common/klib/Kfile.h"
#include "fe/game/game_gfx.h"

using byte = unsigned char;

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
			std::format("World: {}", m_cache.m_labels_worlds.at(m_sel_gfx_ts_world)), m_sel_gfx_ts_world, 0, 7);

		if (m_sel_gfx_ts_screen >= m_game->m_chunks.at(m_sel_gfx_ts_world).m_screens.size())
			m_sel_gfx_ts_screen = 0;

		if (m_sel_gfx_ts_world == c::CHUNK_IDX_BUILDINGS)
			ui::imgui_slider_with_arrows("###tss",
				get_description(static_cast<byte>(m_sel_gfx_ts_screen), m_cache.m_labels_buildings), m_sel_gfx_ts_screen,
				0, m_game->m_chunks.at(m_sel_gfx_ts_world).m_screens.size() - 1);

		std::size_t l_ts_no{ m_game->get_default_tileset_no(m_sel_gfx_ts_world, m_sel_gfx_ts_screen) };
		std::size_t l_palette_no{ m_game->get_default_palette_no(m_sel_gfx_ts_world, m_sel_gfx_ts_screen) };

		imgui_text(std::format("Tileset {}: {}", l_ts_no, get_description(static_cast<byte>(l_ts_no), m_cache.m_labels_tilesets)));
		imgui_text(std::format("Palette: {}",
			get_description(static_cast<byte>(l_palette_no), m_cache.m_labels_palettes)
		));

		ImGui::Separator();

		std::size_t l_pass_screen{ m_sel_gfx_ts_world == c::CHUNK_IDX_BUILDINGS ?
			m_sel_gfx_ts_screen : 0 };

		std::size_t l_gfx_key{
			m_sel_gfx_ts_world == c::CHUNK_IDX_BUILDINGS ?
			m_sel_gfx_ts_world * 100 + l_pass_screen :
			m_sel_gfx_ts_world
		};

		auto txt{ m_gfx.get_tileset_txt(l_gfx_key) };

		if (txt == nullptr)
			imgui_text("Graphics not yet extracted");
		else {
			ImGui::Image(txt, ImVec2(static_cast<float>(2 * txt->w), static_cast<float>(2 * txt->h)));
		}

		ImGui::Separator();

		if (ui::imgui_button("Refresh", 4)) try {
			refresh_world_gfx(p_rnd, l_gfx_key, fe::game::gfx::get_world_tileset_gfx_def(
				m_config, *m_game, m_sel_gfx_ts_world, l_pass_screen));
		}
		catch (const std::exception& ex) {
			add_message(ex.what(), fe::MsgType::Error);
		}

		ImGui::SameLine();

		if (ui::imgui_button("Save bmp", 2)) try {
			save_world_gfx(l_gfx_key, fe::game::gfx::get_world_tileset_gfx_def(
				m_config, *m_game, m_sel_gfx_ts_world, l_pass_screen));
		}
		catch (const std::exception& ex) {
			add_message(ex.what(), fe::MsgType::Error);
		}

		if (ui::imgui_button("Load bmp", 2)) try {
			load_world_gfx(p_rnd, l_gfx_key,
				fe::game::gfx::get_world_tileset_gfx_def(m_config, *m_game,
					m_sel_gfx_ts_world, l_pass_screen),
				ls_dedup_strat);
		}
		catch (const std::exception& ex) {
			add_message(ex.what(), fe::MsgType::Error);
		}

		ImGui::SameLine();

		bool l_res_pending{ m_gfx.has_tilemap_import_result(l_gfx_key) };

		if (ui::imgui_button("Commit to ROM",
			l_res_pending ? 2 : 4, "Commit imported graphics to ROM", !l_res_pending)) try {
			commit_world_gfx(l_gfx_key, fe::game::gfx::get_world_tileset_gfx_def(
				m_config, *m_game, m_sel_gfx_ts_world, l_pass_screen));
		}
		catch (const std::exception& ex) {
			add_message(ex.what(), fe::MsgType::Error);
		}

		if (m_gfx.has_tilemap_import_result(l_gfx_key)) {
			static std::size_t ls_rerender_pal{ 0 };

			ImGui::SeparatorText(
				std::format("Preview result under palette: {}",
					get_description(static_cast<byte>(ls_rerender_pal),
						m_cache.m_labels_palettes)).c_str()
			);

			ui::imgui_slider_with_arrows("###gfxrerender",
				"", ls_rerender_pal, 0, m_game->m_palettes.size() - 1,
				"", false, true);

			if (ui::imgui_button("Re-Render", 4))
				m_gfx.re_render_tilemap_result(p_rnd, l_gfx_key,
					m_game->m_palettes.at(ls_rerender_pal));
			ImGui::Separator();
		}

		if (ImGui::CollapsingHeader("Advanced: Custom Import Definition"))
			draw_custom_world_gfx(p_rnd, ls_dedup_strat);
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

		if (ui::imgui_button("Refresh", 4)) {
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
				get_bmp_filepath(l_gfx_key)), fe::MsgType::Success);
		}
		catch (const std::exception& ex) {
			add_message(ex.what(), fe::MsgType::Error);
		}

		if (ui::imgui_button("Load bmp", 2)) try {
			auto l_tmp_tiles{ gfxman.get_complete_chr_tileset_w_md(gfxkey) };
			auto l_image{ m_gfx.load_image_from_bmp_file(get_bmp_path(),
				get_bmp_filename(l_gfx_key)) };

			auto bmpimportres{ fe::game::gfx::import_tilemap_image(l_image,
				l_tmp_tiles, fe::game::gfx::flat_pal_to_2d_pal(
					gfxman.tilemapdata.at(gfxkey).palette),
				m_cache.m_nes_palette, ls_dedup_strat) };

			m_gfx.set_tilemap_import_result(l_gfx_key, bmpimportres.tilemap);
			// TODO: defer until next frame to avoid flicker
			m_gfx.gen_tilemap_texture(p_rnd, bmpimportres.image, std::nullopt, l_gfx_key);

			add_message(std::format("Loaded {}", get_bmp_filepath(l_gfx_key)),
				fe::MsgType::Success);

			add_message(std::format("{} chr-tiles to spare, {} chr-tiles approximated",
				bmpimportres.leftoverChrCount, bmpimportres.overflowChrCount),
				bmpimportres.overflowChrCount == 0 ? fe::MsgType::Success : fe::MsgType::Warning);
		}
		catch (const std::exception& ex) {
			add_message(ex.what(), fe::MsgType::Error);
		}

		ImGui::SameLine();

		bool l_res_pending{ m_gfx.has_tilemap_import_result(l_gfx_key) };
		if (ui::imgui_button("Commit to ROM",
			l_res_pending ? 2 : 4, "Commit imported graphics to ROM", !l_res_pending)) try {
			const auto gfxres{ m_gfx.get_tilemap_import_result(l_gfx_key) };

			gfxman.commit_import(gfxkey, gfxres);

			// make life easy for ourselves and wipe all staging data on commit
			m_gfx.clear_all_tilemap_import_results();

			add_message("Graphics committed to ROM", fe::MsgType::Success);
		}
		catch (const std::exception& ex) {
			add_message(ex.what(), fe::MsgType::Error);
		}

	}

	else if (m_gfx_emode == fe::GfxEditMode::WorldPalettes) {
		// selected game palette no
		static std::size_t ls_sel_wpal{ 0 };
		auto& wpal{ m_game->m_palettes.at(ls_sel_wpal) };

		ui::imgui_slider_with_arrows("###wpal",
			std::format("Palette: {}", get_description(static_cast<byte>(ls_sel_wpal),
				m_cache.m_labels_palettes)),
			ls_sel_wpal, 0, m_game->m_palettes.size() - 1,
			"", false, true);

		auto spal_iter{ m_cache.m_shared_palettes.find(ls_sel_wpal) };
		if (spal_iter != end(m_cache.m_shared_palettes)) {
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

		const auto regen_hud = [this](SDL_Renderer* pl_rnd,
			std::size_t p_palette_no) -> void {
				m_hud_tilemap.set_flat_palette(m_game->m_palettes.at(p_palette_no));
				const auto& attrs{
					m_game->m_hud_attributes.m_hud_attributes.at(
						m_game->m_hud_attributes.m_palette_to_hud_idx.at(p_palette_no)
					)
				};
				m_hud_tilemap.populate_attribute(attrs.m_tl, attrs.m_tr, attrs.m_bl, attrs.m_br);
				m_gfx.gen_tilemap_texture(pl_rnd, m_hud_tilemap, HUD_GFX_KEY);
			};

		static std::size_t ls_sel_wpal{ 0 };
		auto& hud_attrs{ m_game->m_hud_attributes };

		if (ui::imgui_slider_with_arrows("###hpal",
			std::format("Palette: {}", get_description(static_cast<byte>(ls_sel_wpal),
				m_cache.m_labels_palettes)),
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
		try {
			show_world_chr_bank_screen(p_rnd);
		}
		catch (const std::exception& ex) {
			add_message(ex.what(), fe::MsgType::Error);
		}
	}
	else if (m_gfx_emode == fe::GfxEditMode::GfxChrBank) {
		try {
			show_gfx_chr_bank_screen(p_rnd);
		}
		catch (const std::exception& ex) {
			add_message(ex.what(), fe::MsgType::Error);
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

bool fe::MainWindow::show_palette_window(std::size_t p_pal_key, std::vector<byte>& p_palette) {
	bool was_changed{ false };

	// selected palette index
	static std::size_t ls_sel_pal_idx{ 1 };
	static bool ls_edit_bg_col{ false };

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
		m_clip_manager.copy_palette(p_palette);

	ImGui::SameLine();

	if (ui::imgui_button("Paste###palpaste", 4, "Paste entire palette from clipboard")) try {
		auto newpalette{ m_clip_manager.paste_palette() };
		was_changed = m_undo->apply_palette_edit(p_pal_key, p_palette,
			newpalette);
	}
	catch (const std::exception& ex) {
		add_message(ex.what(), fe::MsgType::Error);
	}

	return was_changed;
}

std::vector<byte> fe::MainWindow::update_pal_bg_idx(std::vector<byte>& p_palette,
	byte p_nes_pal_idx) {
	auto newpal{ p_palette };
	for (std::size_t i{ 0 }; i < p_palette.size(); i += 4)
		newpal[i] = p_nes_pal_idx;
	return newpal;
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
		add_message(std::format("Could not generate door requirement graphics: {}", ex.what()), fe::MsgType::Error);
	}
	catch (const std::exception& ex) {
		add_message(std::format("Could not generate door requirement graphics: {}", ex.what()), fe::MsgType::Error);
	}
	catch (...) {
		add_message("Could not generate door requirement graphics: Unknown exception", fe::MsgType::Error);
	}

}

void fe::MainWindow::show_gfx_chr_bank_screen(SDL_Renderer* p_rnd) {
	static const std::vector<std::string> lcs_chr_banks{ c::CHR_BANK_TITLE, c::CHR_BANK_INTRO_OUTRO, c::CHR_BANK_ITEMS };
	static std::size_t ls_sel_bank{ 0 };
	static std::unordered_map<std::string, std::vector<klib::NES_tile>> undo_tiles;

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
			static_cast<float>(3 * banktxt->w),
			static_cast<float>(3 * banktxt->h)
		));
	}

	ImGui::Separator();

	if (ui::imgui_button("Re-render", 2) || banktxt == nullptr) {
		auto completebank{ bank_chr_w_metadata(bank_id) };
		m_gfx.gen_bank_chr_gfx(p_rnd, bank_id,
			completebank.first, completebank.second);
	}

	if (ui::imgui_button("Export chr", 4)) {
		save_chr(m_game->m_gfx_manager.chrbanks.at(bank_id), bank_id);
	}
	ImGui::SameLine();
	if (ui::imgui_button("Import chr", 4)) {
		auto tmpundo{ m_game->m_gfx_manager.chrbanks.at(bank_id) };
		m_game->m_gfx_manager.set_chr_bank(bank_id,
			load_chr(bank_id, m_game->m_gfx_manager.chrbanks.at(bank_id).size())
		);
		undo_tiles[bank_id] = tmpundo;
		auto completebank{ bank_chr_w_metadata(bank_id) };
		m_gfx.gen_bank_chr_gfx(p_rnd, bank_id,
			completebank.first, completebank.second);
	}
	/*
	ImGui::SameLine();

	bool has_undo{ undo_tiles.contains(bank_id) };
	if (ui::imgui_button("Undo import", 4, "", !has_undo)) {
		m_game->m_gfx_manager.set_chr_bank(bank_id, undo_tiles.at(bank_id));
		undo_tiles.erase(bank_id);
		auto completebank{ bank_chr_w_metadata(bank_id) };
		m_gfx.gen_bank_chr_gfx(p_rnd, bank_id,
			completebank.first, completebank.second);
	}
	*/
	ImGui::Separator();

	imgui_text("Warning! This will re-index tilemaps - only use if you want deterministic chr ordering");

	bool l_shift{ ImGui::IsKeyDown(ImGuiMod_Shift) };

	if (ui::imgui_button("Canonicalize", 4, "Sort and deduplicate the editable portion of the chr bank",
		!l_shift)) {
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
	static std::unordered_map<std::string, std::vector<klib::NES_tile>> undo_tiles;

	ui::imgui_slider_with_arrows("###wchrb", std::format("Tileset {}: {}", ls_tileset_no,
		get_description(static_cast<byte>(ls_tileset_no), m_cache.m_labels_tilesets)).c_str(),
		ls_tileset_no, 0, m_game->m_tilesets.size() - 1, "", false, true);

	std::string bank_id{ std::format("tileset-{}", ls_tileset_no) };

	auto banktxt{ m_gfx.get_bank_chr_gfx(bank_id) };

	if (banktxt != nullptr) {
		ImGui::Image(banktxt, ImVec2(
			static_cast<float>(3 * banktxt->w),
			static_cast<float>(3 * banktxt->h)
		));
	}

	if (ui::imgui_button("Re-render", 2) || banktxt == nullptr) {
		auto completebank{ get_complete_world_tileset_w_metadata(ls_tileset_no) };
		m_gfx.gen_bank_chr_gfx(p_rnd, bank_id,
			completebank.first, completebank.second);
	}

	if (ui::imgui_button("Export chr", 4)) {
		save_chr(m_game->m_tilesets.at(ls_tileset_no).tiles, bank_id);
	}

	ImGui::SameLine();

	if (ui::imgui_button("Import chr", 4)) {
		auto tmpundo{ m_game->m_tilesets.at(ls_tileset_no).tiles };
		set_world_tileset_tiles(p_rnd, ls_tileset_no, load_chr(bank_id, m_game->m_tilesets.at(ls_tileset_no).tiles.size()));
		undo_tiles[bank_id] = tmpundo;
		auto completebank{ get_complete_world_tileset_w_metadata(ls_tileset_no) };
		m_gfx.gen_bank_chr_gfx(p_rnd, bank_id,
			completebank.first, completebank.second);
	}
	/*
	ImGui::SameLine();

	bool has_undo{ undo_tiles.contains(bank_id) };
	if (ui::imgui_button("Undo import", 4, "", !has_undo)) {
		set_world_tileset_tiles(p_rnd, ls_tileset_no, undo_tiles.at(bank_id));
		undo_tiles.erase(bank_id);
		auto completebank{ get_complete_world_tileset_w_metadata(ls_tileset_no) };
		m_gfx.gen_bank_chr_gfx(p_rnd, bank_id,
			completebank.first, completebank.second);
	}
	*/
	ImGui::Separator();

	imgui_text("Warning! This will re-index tilemaps - only use if you want deterministic chr ordering");

	bool l_shift{ ImGui::IsKeyDown(ImGuiMod_Shift) };

	if (ui::imgui_button("Canonicalize", 4, "Sort and deduplicate the editable portion of the chr bank", !l_shift)) {
		auto chrbank{ get_world_tileset_w_metadata(ls_tileset_no) };
		set_world_tileset_tiles(p_rnd, ls_tileset_no, reorder_chr_tiles(chrbank.first, chrbank.second));
		auto completebank{ get_complete_world_tileset_w_metadata(ls_tileset_no) };
		m_gfx.gen_bank_chr_gfx(p_rnd, bank_id,
			completebank.first, completebank.second);
		m_undo->clear_metatile_history();
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

// only call this if the size of the chr-tile vector has been confirmed
void fe::MainWindow::set_world_tileset_tiles(SDL_Renderer* p_rnd, std::size_t p_tileset_no,
	const std::vector<klib::NES_tile>& p_tiles) {
	m_game->m_tilesets.at(p_tileset_no).tiles = p_tiles;

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

	const auto& tiles{ m_cache.m_world_ppu_tilesets.at(p_tileset_no) };
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

std::string fe::MainWindow::get_chr_folder(void) const {
	return std::format("{}/{}-chr", m_path.string(), m_filename);
}

std::string fe::MainWindow::get_chr_file_path(const std::string& p_bank_id) const {
	return std::format("{}/{}.chr", get_chr_folder(), p_bank_id);
}

void fe::MainWindow::save_chr(const std::vector<klib::NES_tile>& tiles, const std::string& p_bank_id) {
	klib::file::create_directories(get_chr_folder());
	std::vector<byte> out_data;
	for (const auto& tile : tiles) {
		auto tilebytes{ tile.to_bytes() };
		out_data.insert(end(out_data), begin(tilebytes), end(tilebytes));
	}
	std::string out_file{ get_chr_file_path(p_bank_id) };
	klib::file::write_bytes_to_file(out_data, out_file);
	add_message(std::format("chr-data written to '{}' ({} chr-tiles, {} bytes)", out_file, tiles.size(), out_data.size()), fe::MsgType::Success);
}

std::vector<klib::NES_tile> fe::MainWindow::load_chr(const std::string& p_bank_id, std::size_t p_chr_tile_count) {
	std::vector<klib::NES_tile> out_tiles;
	std::string in_file{ get_chr_file_path(p_bank_id) };
	auto chrbytes{ klib::file::read_file_as_bytes(in_file) };
	if (chrbytes.size() != 16 * p_chr_tile_count)
		throw std::runtime_error(std::format("Expected file size '{}' to be {} ({} chr-tiles), but actual size was {}",
			in_file, 16 * p_chr_tile_count, p_chr_tile_count, chrbytes.size()));

	for (std::size_t i{ 0 }; i < chrbytes.size(); i += 16)
		out_tiles.push_back(klib::NES_tile(chrbytes, i));

	add_message(std::format("Loaded {} chr-tiles from '{}'", out_tiles.size(), in_file), fe::MsgType::Success);

	return out_tiles;
}

void fe::MainWindow::generate_world_tilesets(void) {
	m_cache.m_world_ppu_tilesets = fe::game::gfx::gen_world_tilesets(*m_game, m_config);
}

void fe::MainWindow::refresh_world_gfx(SDL_Renderer* p_rnd, std::size_t p_gfx_key,
	const fe::WorldTilesetGfxDef& p_gfx_def) {
	m_gfx.gen_tilemap_texture(p_rnd,
		fe::game::gfx::get_world_mt_tilemap(*m_game, p_gfx_def),
		p_gfx_key);
	m_gfx.clear_tilemap_import_result(p_gfx_key);
}

void fe::MainWindow::save_world_gfx(std::size_t p_gfx_key, const fe::WorldTilesetGfxDef& p_gfx_def) {
	m_gfx.save_tilemap_bmp(fe::game::gfx::get_world_mt_tilemap(*m_game, p_gfx_def),
		get_bmp_path(), get_bmp_filename(p_gfx_key));
	add_message(std::format("Saved {}", get_bmp_filepath(p_gfx_key)),
		fe::MsgType::Success);
}

void fe::MainWindow::load_world_gfx(SDL_Renderer* p_rnd, std::size_t p_gfx_key,
	fe::WorldTilesetGfxDef p_gfx_def, fe::ChrDedupMode p_dedup_mode) {
	auto l_image{ m_gfx.load_image_from_bmp_file(
	get_bmp_path(), get_bmp_filename(p_gfx_key)) };

	auto importres{ fe::game::gfx::import_tilemap_image(l_image, p_gfx_def.chr_tiles,
		p_gfx_def.palette, m_cache.m_nes_palette,p_dedup_mode) };
	m_gfx.set_tilemap_import_result(p_gfx_key, importres.tilemap);
	// TODO: defer until next frame to avoid flicker
	m_gfx.gen_tilemap_texture(p_rnd, importres.image,
		fe::game::gfx::get_hot_pink(), p_gfx_key);
	add_message(std::format("{} chr-tiles to spare, {} chr-tiles approximated",
		importres.leftoverChrCount, importres.overflowChrCount),
		importres.overflowChrCount == 0 ?
		fe::MsgType::Success : fe::MsgType::Warning);
}

void fe::MainWindow::commit_world_gfx(std::size_t p_gfx_key, const fe::WorldTilesetGfxDef& p_gfx_def) {
	const auto gfxres{ m_gfx.get_tilemap_import_result(p_gfx_key) };

	fe::game::gfx::apply_world_tileset_gfx(*m_game, p_gfx_def, gfxres);

	// the world tileset chr-tiles were updated, update the ui cache
	generate_world_tilesets();

	// redraw current metatile atlas if this import affects it
	if (p_gfx_def.world_no == m_sel_chunk)
		m_atlas_force_update = true;

	// make life easy for ourselves and wipe all staging and undo data on commit
	m_gfx.clear_all_tilemap_import_results();
	m_undo->clear_metatile_history();

	add_message("Imported graphics committed to ROM", fe::MsgType::Success);
}

void fe::MainWindow::draw_custom_world_gfx(SDL_Renderer* p_rnd, fe::ChrDedupMode p_dedup_mode) {
	constexpr std::size_t l_gfx_key{ c::CHR_GFX_NUM_ID_CUSTOM_WORLD };
	static std::size_t ls_sel_custom_gfx_world{ 0 };
	static std::size_t ls_sel_custom_gfx_tileset{ 0 };
	static std::size_t ls_sel_custom_gfx_palette{ 0 };
	static std::size_t ls_sel_custom_gfx_mt_start{ 0 };
	static std::size_t ls_sel_custom_gfx_mt_end{ 255 };
	static std::size_t ls_sel_custom_gfx_chr_start{ 0 };
	static std::size_t ls_sel_custom_gfx_chr_end{ 255 };

	// selections may have become stale due to edits elsewhere
	ls_sel_custom_gfx_world = std::min(ls_sel_custom_gfx_world, m_game->m_chunks.size() - 1);
	ls_sel_custom_gfx_tileset = std::min(ls_sel_custom_gfx_tileset, m_game->m_tilesets.size() - 1);
	ls_sel_custom_gfx_palette = std::min(ls_sel_custom_gfx_palette, m_game->m_palettes.size() - 1);

	// select the top level container indexes; world, tileset and palette
	ui::imgui_slider_with_arrows("###customgfxworld",
		std::format("World: {}", m_cache.m_labels_worlds.at(ls_sel_custom_gfx_world)),
		ls_sel_custom_gfx_world, 0, m_game->m_chunks.size() - 1);
	ui::imgui_slider_with_arrows("###customgfxts",
		std::format("Tileset: {}", get_description(static_cast<byte>(ls_sel_custom_gfx_tileset),
			m_cache.m_labels_tilesets)),
		ls_sel_custom_gfx_tileset, 0, m_game->m_tilesets.size() - 1);
	ui::imgui_slider_with_arrows("###customgfxpal",
		std::format("Palette: {}", get_description(static_cast<byte>(ls_sel_custom_gfx_palette),
			m_cache.m_labels_palettes)),
		ls_sel_custom_gfx_palette, 0, m_game->m_palettes.size() - 1);

	// get current bounds after the selectors, since they may have changed this frame
	const auto& l_chunk{ m_game->m_chunks.at(ls_sel_custom_gfx_world) };
	const auto& l_tileset{ m_game->m_tilesets.at(ls_sel_custom_gfx_tileset) };

	const std::size_t l_mt_end{ l_chunk.m_metatiles.size() - 1 };
	const std::size_t l_chr_start{ l_tileset.start_idx };
	const std::size_t l_chr_end{ l_tileset.end_index() - 1 };

	// keep stale ranges in bounds
	ls_sel_custom_gfx_mt_start = std::min(ls_sel_custom_gfx_mt_start, l_mt_end);
	ls_sel_custom_gfx_mt_end = std::min(ls_sel_custom_gfx_mt_end, l_mt_end);
	ls_sel_custom_gfx_chr_start = std::clamp(ls_sel_custom_gfx_chr_start, l_chr_start, l_chr_end);
	ls_sel_custom_gfx_chr_end = std::clamp(ls_sel_custom_gfx_chr_end, l_chr_start, l_chr_end);


	ui::imgui_slider_with_arrows("###customgfxchrstart",
		std::format("Writable CHR start: ${:02X}", ls_sel_custom_gfx_chr_start),
		ls_sel_custom_gfx_chr_start, l_chr_start, l_chr_end);
	ui::imgui_slider_with_arrows("###customgfxchrend",
		std::format("Writable CHR end: ${:02X}", ls_sel_custom_gfx_chr_end),
		ls_sel_custom_gfx_chr_end, l_chr_start, l_chr_end);
	ui::imgui_slider_with_arrows("###customgfxmtstart",
		std::format("Metatile start: {}", ls_sel_custom_gfx_mt_start),
		ls_sel_custom_gfx_mt_start, 0, l_mt_end);
	ui::imgui_slider_with_arrows("###customgfxmtend",
		std::format("Metatile end: {}", ls_sel_custom_gfx_mt_end),
		ls_sel_custom_gfx_mt_end, 0, l_mt_end);

	auto get_gfx_def = [&](void) {
		return fe::game::gfx::get_custom_world_tileset_gfx_def(
			m_config, *m_game,
			ls_sel_custom_gfx_world,
			ls_sel_custom_gfx_tileset,
			ls_sel_custom_gfx_palette,
			ls_sel_custom_gfx_mt_start,
			ls_sel_custom_gfx_mt_end + 1,
			ls_sel_custom_gfx_chr_start,
			ls_sel_custom_gfx_chr_end + 1);
		};

	ImGui::Separator();

	auto txt{ m_gfx.get_tileset_txt(l_gfx_key) };

	if (txt == nullptr)
		imgui_text("Graphics not yet extracted");
	else
		ImGui::Image(txt,
			ImVec2(static_cast<float>(2 * txt->w),
				static_cast<float>(2 * txt->h)));
	ImGui::Separator();

	if (ui::imgui_button("Refresh###cgref", 4)) try {
		refresh_world_gfx(p_rnd, l_gfx_key, get_gfx_def());
	}
	catch (const std::exception& ex) {
		add_message(ex.what(), fe::MsgType::Error);
	}

	ImGui::SameLine();

	if (ui::imgui_button("Save bmp##cgsf", 2)) try {
		save_world_gfx(l_gfx_key, get_gfx_def());
	}
	catch (const std::exception& ex) {
		add_message(ex.what(), fe::MsgType::Error);
	}

	if (ui::imgui_button("Load bmp##cglf", 2)) try {
		load_world_gfx(p_rnd, l_gfx_key, get_gfx_def(), p_dedup_mode);
	}
	catch (const std::exception& ex) {
		add_message(ex.what(), fe::MsgType::Error);
	}

	ImGui::SameLine();

	bool l_res_pending{ m_gfx.has_tilemap_import_result(l_gfx_key) };

	if (ui::imgui_button("Commit to ROM##cgcomm",
		l_res_pending ? 2 : 4, "Commit imported graphics to ROM", !l_res_pending)) try {
		commit_world_gfx(l_gfx_key, get_gfx_def());
	}
	catch (const std::exception& ex) {
		add_message(ex.what(), fe::MsgType::Error);
	}
}
