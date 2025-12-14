#include "GameGfxTilemap.h"
#include <algorithm>
#include <stdexcept>

fe::GameGfxTilemap::GameGfxTilemap(const std::string& p_gfx_name,
	std::size_t p_mtx, std::size_t p_mty,
	std::size_t p_mtw, std::size_t p_mth,
	std::size_t p_tilemap_rom_offset,
	std::size_t p_chr_rom_offset,
	std::size_t p_ppu_index,
	std::size_t p_ppu_count,
	bool p_fix_tile_0,
	bool p_patch_attr,
	bool p_patch_palette,
	std::size_t p_attr_rom_offset,
	std::size_t p_pal_rom_offset,
	bool p_add_alphanumeric,
	std::size_t p_alpha_chr_offset,
	std::size_t p_numeric_chr_offset) :
	m_gfx_name{ p_gfx_name },
	mt_x{ p_mtx },
	mt_y{ p_mty },
	mt_w{ p_mtw },
	mt_h{ p_mth },
	m_rom_offset_tilemap{ p_tilemap_rom_offset },
	m_rom_offset_chr{ p_chr_rom_offset },
	m_chr_ppu_index{ p_ppu_index },
	m_chr_ppu_count{ p_ppu_count },
	m_fix_tile_0{ p_fix_tile_0 },
	m_rom_offset_attr{ p_attr_rom_offset },
	m_rom_offset_pal{ p_pal_rom_offset },
	m_patch_attributes{ p_patch_attr },
	m_patch_palette{ p_patch_palette },
	m_add_alphanumeric{ p_add_alphanumeric },
	m_alpha_chr_offset{ p_alpha_chr_offset },
	m_numeric_chr_offset{ p_numeric_chr_offset },
	m_loaded{ false }
{
}

void fe::GameGfxTilemap::commit_import(const fe::ChrTilemap& p_result) {
	const auto& rtm{ p_result.m_tilemap };

	// check the imported tilemap before we make any changes
	bool dims_ok{ true };
	if (rtm.size() != m_tilemap.size())
		dims_ok = false;
	else {
		for (std::size_t j{ 0 }; j < rtm.size(); ++j) {
			if (m_tilemap[j].size() != rtm[j].size())
				dims_ok = false;
			for (std::size_t i{ 0 }; i < rtm[j].size(); ++i)
				if (!rtm[j][i].has_value())
					dims_ok = false;
		}
	}

	if (!dims_ok)
		throw std::runtime_error("Import result does not have the right dimensions");

	for (std::size_t j{ 0 }; j < rtm.size(); ++j) {
		for (std::size_t i{ 0 }; i < rtm[j].size(); ++i) {
			m_tilemap[j][i].m_idxs = rtm[j][i]->m_idxs;
			m_tilemap[j][i].m_palette = rtm[j][i]->m_palette;
			m_attributes[mt_y + j][mt_x + i] = rtm[j][i]->m_palette;
		}
	}

	for (std::size_t t{ 0 }; t < p_result.m_tiles.size(); ++t)
		m_chr_tiles.at(t).m_tile = p_result.m_tiles[t];
}

void fe::GameGfxTilemap::load_from_rom(const std::vector<byte>& p_rom) {
	// extract palette if we get it from ROM
	if (m_patch_palette) {
		m_palette.clear();
		for (std::size_t i{ 0 }; i < 16; ++i)
			m_palette.push_back(p_rom.at(m_rom_offset_pal + i));
	}
	else {
		m_palette = { 0x0f, 0x18, 0x26, 0x30,
			0x0f, 0x18, 0x26, 0x30,
			0x0f, 0x18, 0x26, 0x30,
			0x0f, 0x18, 0x26, 0x30 };
	}

	if (m_patch_attributes) {
		m_attributes = std::vector<std::vector<std::size_t>>(15, std::vector<std::size_t>(16, 0));

		for (std::size_t ay = 0; ay < 8; ++ay) {
			for (std::size_t ax = 0; ax < 8; ++ax) {
				byte attr = p_rom.at(m_rom_offset_attr + ay * 8 + ax);

				m_attributes[ay * 2 + 0][ax * 2 + 0] = (attr >> 0) & 0b11; // TL
				m_attributes[ay * 2 + 0][ax * 2 + 1] = (attr >> 2) & 0b11; // TR

				if (ay * 2 + 1 < 15) {
					m_attributes[ay * 2 + 1][ax * 2 + 0] = (attr >> 4) & 0b11; // BL
					m_attributes[ay * 2 + 1][ax * 2 + 1] = (attr >> 6) & 0b11; // BR
				}
			}
		}
	}
	else
		initialize_attributes(0);

	// extract the tilemap itself
	std::vector<std::vector<fe::ChrMetaTile>> l_tilemap;
	const std::size_t tiles_per_row = mt_w * 2; // tight pack: width in tiles

	for (std::size_t my{ 0 }; my < mt_h; ++my) {
		std::vector<fe::ChrMetaTile> l_row;

		const std::size_t tile_y = my * 2;
		const std::size_t row_base = m_rom_offset_tilemap + tiles_per_row * tile_y;

		for (std::size_t mx{ 0 }; mx < mt_w; ++mx) {
			const std::size_t tile_x{ mx * 2 };
			const std::size_t mt_offset{ row_base + tile_x };

			fe::ChrMetaTile tmptile;
			tmptile.m_idxs.push_back(p_rom.at(mt_offset));
			tmptile.m_idxs.push_back(p_rom.at(mt_offset + 1));
			tmptile.m_idxs.push_back(p_rom.at(mt_offset + tiles_per_row));
			tmptile.m_idxs.push_back(p_rom.at(mt_offset + tiles_per_row + 1));

			tmptile.m_palette = m_attributes[mt_y + my][mt_x + mx];

			l_row.push_back(tmptile);
		}
		l_tilemap.push_back(l_row);
	}
	m_tilemap = l_tilemap;

	// extract the chr tiles
	std::vector<fe::ChrGfxTile> l_tiles;
	for (std::size_t i{ 0 }; i < m_chr_ppu_index; ++i)
		l_tiles.push_back(fe::ChrGfxTile(klib::NES_tile(), true, false));

	for (std::size_t i{ 0 }; i < m_chr_ppu_count; ++i)
		l_tiles.push_back(fe::ChrGfxTile(
			klib::NES_tile(p_rom, m_rom_offset_chr + 16 * i),
			false, true));

	while (l_tiles.size() < 256)
		l_tiles.push_back(fe::ChrGfxTile(klib::NES_tile(), true, false));

	if (m_fix_tile_0)
		l_tiles[0].m_readonly = true;

	// add alphanumeric tiles if required
	if (m_add_alphanumeric) {
		std::size_t l_chr_ppu_idx{ m_chr_ppu_index + m_chr_ppu_count };

		// 0-9
		for (std::size_t i{ 0 }; i < 10 && l_chr_ppu_idx < 256; ++i)
			l_tiles[l_chr_ppu_idx++] = fe::ChrGfxTile(klib::NES_tile(
				p_rom, m_numeric_chr_offset + 16 * i), true, true);

		// A-Z + copyright symbol
		for (std::size_t i{ 0 }; i < 27 && l_chr_ppu_idx < 256; ++i)
			l_tiles[l_chr_ppu_idx++] = fe::ChrGfxTile(klib::NES_tile(
				p_rom, m_alpha_chr_offset + 16 * i), true, true);
	}

	m_chr_tiles = l_tiles;

	m_loaded = true;
}

void fe::GameGfxTilemap::initialize_attributes(std::size_t p_attr) {
	m_attributes = std::vector<std::vector<std::size_t>>(mt_h, std::vector<std::size_t>(mt_h, 0));

	for (auto& row : m_attributes)
		for (auto& idx : row)
			idx = p_attr;
}

fe::ChrTilemap fe::GameGfxTilemap::get_chrtilemap(void) const {
	fe::ChrTilemap l_tilemap;
	l_tilemap.m_palette = flat_pal_to_2d_pal(m_palette);

	for (const auto& tile : m_chr_tiles)
		l_tilemap.m_tiles.push_back(tile.m_tile);

	for (std::size_t j{ 0 }; j < m_tilemap.size(); ++j) {
		std::vector<std::optional<fe::ChrMetaTile>> mtvec;

		for (std::size_t i{ 0 }; i < m_tilemap[j].size(); ++i) {
			fe::ChrMetaTile tmpmt;
			tmpmt.m_palette = m_tilemap[j][i].m_palette;
			for (std::size_t b : m_tilemap[j][i].m_idxs)
				tmpmt.m_idxs.push_back(b);
			mtvec.push_back(tmpmt);
		}
		l_tilemap.m_tilemap.push_back(mtvec);
	}

	return l_tilemap;
}

std::vector<std::vector<byte>> fe::GameGfxTilemap::flat_pal_to_2d_pal(
	const std::vector<byte>& p_flat_pal) {
	std::vector<std::vector<byte>> result;

	std::vector<byte> l_sub_pal;
	for (std::size_t i{ 0 }; i < p_flat_pal.size(); ++i) {
		l_sub_pal.push_back(p_flat_pal[i]);
		if (l_sub_pal.size() % 4 == 0) {
			result.push_back(l_sub_pal);
			l_sub_pal.clear();
		}
	}

	return result;
}

// helper structs
fe::ChrGfxTile::ChrGfxTile(const klib::NES_tile& p_tile,
	bool p_readonly, bool p_allowed) :
	m_tile{ p_tile },
	m_readonly{ p_readonly }, m_allowed{ p_allowed }
{
}

fe::ChrGfxTile::ChrGfxTile(void) :
	m_readonly{ false },
	m_allowed{ false }
{
}

fe::ChrMetaTile::ChrMetaTile(void) :
	m_palette{ 0 }
{
}

// ROM patching routines
void fe::GameGfxTilemap::patch_rom(std::vector<byte>& p_rom) const {
	if (!m_loaded)
		return;

	if (m_patch_palette)
		patch_palette(p_rom);
	if (m_patch_attributes)
		patch_attributes(p_rom);
	patch_chr_tiles(p_rom);
	patch_tilemap(p_rom);
}

void fe::GameGfxTilemap::patch_palette(std::vector<byte>& p_rom) const {
	for (std::size_t j{ 0 }; j < m_palette.size(); ++j)
		p_rom.at(m_rom_offset_pal + j) = m_palette[j];
}

void fe::GameGfxTilemap::patch_attributes(std::vector<byte>& p_rom) const {
	std::vector<uint8_t> attr_bytes(64, 0);
	for (std::size_t i{ 0 }; i < 64; ++i)
		attr_bytes[i] = p_rom.at(m_rom_offset_attr + i);

	for (std::size_t my = 0; my < 15; ++my) {
		for (std::size_t mx = 0; mx < 16; ++mx) {
			uint8_t pal = static_cast<byte>(m_attributes[my][mx]); // already 0–3

			auto [byte_index, quadrant] = mt_coords_to_attribute(mx, my);

			uint8_t mask = 0x03 << (quadrant * 2);
			attr_bytes[byte_index] = (attr_bytes[byte_index] & ~mask) | (pal << (quadrant * 2));
		}
	}

	for (std::size_t i = 0; i < attr_bytes.size(); ++i) {
		p_rom.at(m_rom_offset_attr + i) = attr_bytes[i];
	}
}

void fe::GameGfxTilemap::patch_tilemap(std::vector<byte>& p_rom) const {
	std::size_t tilemap_stride = mt_w * 2; // width in tiles

	for (std::size_t j = 0; j < m_tilemap.size(); ++j) {
		for (std::size_t i = 0; i < m_tilemap[j].size(); ++i) {
			const auto& tmbytes = m_tilemap[j][i].m_idxs;

			// top-left
			std::size_t off0 = m_rom_offset_tilemap + (j * 2) * tilemap_stride + (i * 2);
			p_rom.at(off0) = static_cast<byte>(tmbytes.at(0));

			// top-right
			std::size_t off1 = m_rom_offset_tilemap + (j * 2) * tilemap_stride + (i * 2 + 1);
			p_rom.at(off1) = static_cast<byte>(tmbytes.at(1));

			// bottom-left
			std::size_t off2 = m_rom_offset_tilemap + (j * 2 + 1) * tilemap_stride + (i * 2);
			p_rom.at(off2) = static_cast<byte>(tmbytes.at(2));

			// bottom-right
			std::size_t off3 = m_rom_offset_tilemap + (j * 2 + 1) * tilemap_stride + (i * 2 + 1);
			p_rom.at(off3) = static_cast<byte>(tmbytes.at(3));
		}
	}
}

void fe::GameGfxTilemap::patch_chr_tiles(std::vector<byte>& p_rom) const {
	for (std::size_t i{ 0 }; i < m_chr_ppu_count; ++i) {
		auto bytes{ m_chr_tiles.at(m_chr_ppu_index + i).m_tile.to_bytes() };
		for (std::size_t j{ 0 }; j < 16; ++j)
			p_rom.at(m_rom_offset_chr + 16 * i + j) = bytes.at(j);
	}
}

std::pair<std::size_t, std::size_t> fe::GameGfxTilemap::mt_coords_to_attribute(
	std::size_t mtx, std::size_t mty) {

	// Each attribute byte covers 2x2 metatiles.
	std::size_t byte_x = mtx / 2;   // 0..7
	std::size_t byte_y = mty / 2;   // 0..7
	std::size_t byte_index = byte_y * 8 + byte_x; // 0..63

	// Quadrant inside the byte: 0=top-left, 1=top-right, 2=bottom-left, 3=bottom-right
	std::size_t quadrant = (mty % 2) * 2 + (mtx % 2);

	return std::make_pair(byte_index, quadrant);
}

// helpers for chrtilemap updates
void fe::ChrTilemap::set_flat_palette(const std::vector<byte>& p_palette) {
	m_palette = fe::GameGfxTilemap::flat_pal_to_2d_pal(p_palette);
}

void fe::ChrTilemap::populate_attribute(byte p_tl, byte p_tr, byte p_bl, byte p_br) {
	for (std::size_t j{ 0 }; j < m_tilemap.size(); j += 2)
		for (std::size_t i{ 0 }; i < m_tilemap[j].size(); i += 2) {
			set_attribute(i, j, p_tl);
			set_attribute(i + 1, j, p_tr);
			set_attribute(i, j + 1, p_bl);
			set_attribute(i + 1, j + 1, p_br);
		}
}

void fe::ChrTilemap::set_attribute(std::size_t p_mt_x, std::size_t p_mt_y, byte p_attr) {
	if (p_mt_y < m_tilemap.size()  &&
		p_mt_x < m_tilemap[p_mt_y].size() &&
		m_tilemap[p_mt_y][p_mt_x].has_value()) {
		m_tilemap[p_mt_y][p_mt_x]->m_palette = p_attr;
	}
}
