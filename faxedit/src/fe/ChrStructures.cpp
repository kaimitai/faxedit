#include "ChrStructures.h"

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

void fe::ChrTilemap::set_flat_palette(const std::vector<byte>& p_palette) {
	m_palette = fe::ChrTilemap::flat_pal_to_2d_pal(p_palette);
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
	if (p_mt_y < m_tilemap.size() &&
		p_mt_x < m_tilemap[p_mt_y].size() &&
		m_tilemap[p_mt_y][p_mt_x].has_value()) {
		m_tilemap[p_mt_y][p_mt_x]->m_palette = p_attr;
	}
}

std::vector<std::vector<byte>> fe::ChrTilemap::flat_pal_to_2d_pal(
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
