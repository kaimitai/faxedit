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
	if (ImGui::RadioButton("Palettes",
		m_gfx_emode == fe::GfxEditMode::Palettes))
		m_gfx_emode = fe::GfxEditMode::Palettes;

	ImGui::Separator();

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

	auto txt{ m_gfx.get_tileset_txt(m_sel_gfx_ts_world, l_pass_screen) };

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
			m_sel_gfx_ts_world, l_pass_screen);

	ImGui::SameLine();

	if (ui::imgui_button("Save bmp", 2, "", txt == nullptr)) try {

		m_gfx.save_tilemap_bmp(get_world_mt_tilemap(m_sel_gfx_ts_world,
			l_pass_screen),
			m_path.string() + "/" + m_filename + "-bmp",
			std::format("tileset-{}-{}.bmp", m_sel_gfx_ts_world, l_pass_screen)
		);

		add_message("Tileset/metatile bmp saved", 2);
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

		auto gfxres{ m_gfx.import_tilemap_bmp(l_tiles,
			flat_pal_to_2d_pal(m_game->m_palettes.at(m_game->get_default_palette_no(m_sel_gfx_ts_world,
				l_pass_screen))),
			ls_dedup_strat,
			m_path.string() + "/" + m_filename + "-bmp",
			std::format("tileset-{}-{}.bmp", m_sel_gfx_ts_world, l_pass_screen))
		};

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
	}
	catch (const std::runtime_error& ex) {
		add_message(ex.what(), 1);
	}
	catch (const std::exception& ex) {
		add_message(ex.what(), 1);
	}

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

	ImGui::End();
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
