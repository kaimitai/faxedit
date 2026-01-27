#include "GfxTilemapManager.h"
#include "fe_constants.h"
#include "Config.h"
#include <algorithm>
#include <format>
#include <stdexcept>
#include <vector>

using byte = unsigned char;

void fe::GfxTilemapManager::load_chr_bank(const std::string& p_chr_bank_id,
	const std::vector<byte>& p_rom, std::size_t p_rom_offset,
	std::size_t p_chr_tile_count) {

	if (chrbanks.find(p_chr_bank_id) == end(chrbanks)) {
		ChrTileBank banktiles;

		for (std::size_t i{ 0 }; i < p_chr_tile_count; ++i)
			banktiles.push_back(klib::NES_tile(p_rom, p_rom_offset + 16 * i));

		chrbanks.insert(std::make_pair(p_chr_bank_id, banktiles));
	}

}

void fe::GfxTilemapManager::load_chr_metadata(const std::string& p_gfx_id,
	const std::string& p_label, const std::string& p_chr_bank_id,
	std::size_t p_gfx_numeric_key, std::size_t p_mt_x, std::size_t p_mt_y,
	std::size_t p_mt_w, std::size_t p_mt_h,
	std::size_t p_rom_offset_tilemap,
	std::size_t p_rom_offset_attr, std::size_t p_rom_offset_pal,
	std::size_t p_chr_ppu_index,
	bool p_patch_attributes, bool p_patch_palette,
	bool p_fix_tile_0, bool p_add_alphanumeric) {
	metadata.insert(std::make_pair(p_gfx_id,
		fe::GfxTilemapMetadata{
			.m_label = p_label,
			.m_chr_bank_id = p_chr_bank_id,
			.m_gfx_key = p_gfx_numeric_key,
			.mt_x = p_mt_x,
			.mt_y = p_mt_y,
			.mt_w = p_mt_w,
			.mt_h = p_mt_h,
			.m_rom_offset_tilemap = p_rom_offset_tilemap,
			.m_rom_offset_attr = p_rom_offset_attr,
			.m_rom_offset_pal = p_rom_offset_pal,
			.m_chr_ppu_index = p_chr_ppu_index,
			.m_patch_attributes = p_patch_attributes,
			.m_patch_palette = p_patch_palette,
			.m_fix_tile_0 = p_fix_tile_0,
			.m_add_alphanumeric = p_add_alphanumeric
		}
	));
}

std::size_t fe::GfxTilemapManager::get_gfx_numeric_key(const std::string& p_gfx_id) const {
	const auto& md{ metadata.at(p_gfx_id) };
	return md.m_gfx_key;
}

std::string fe::GfxTilemapManager::get_label(const std::string& p_gfx_id) const {
	const auto& md{ metadata.at(p_gfx_id) };
	return md.m_label;
}

void fe::GfxTilemapManager::initialize(const fe::Config& p_config,
	const std::vector<byte>& p_rom) {

	// extract alphabet chr-tiles (A-Z + copyright symbol)
	load_chr_bank(c::CHR_BANK_ALPHA, p_rom,
		p_config.constant(c::ID_ALPHABET_CHR_OFFSET), c::CHR_BANK_ALPHA_COUNT);
	// extract numeric chr-tiles (0-9)
	load_chr_bank(c::CHR_BANK_NUMERIC, p_rom,
		p_config.constant(c::ID_NUMERIC_CHR_OFFSET), c::CHR_BANK_NUMERIC_COUNT);

	// title screen bank
	load_chr_bank(c::CHR_BANK_TITLE, p_rom,
		p_config.constant(c::ID_TITLE_CHR_OFFSET),
		p_config.constant(c::ID_TITLE_CHR_PPU_COUNT));
	// intro and outro screen banks
	load_chr_bank(c::CHR_BANK_INTRO_OUTRO, p_rom,
		p_config.constant(c::ID_INTRO_ANIM_CHR_OFFSET),
		p_config.constant(c::ID_INTRO_ANIM_CHR_PPU_COUNT));
	// item gfx banks
	load_chr_bank(c::CHR_BANK_ITEMS, p_rom,
		p_config.constant(c::ID_ITEM_VSCREEN_CHR_OFFSET),
		p_config.constant(c::ID_ITEM_VSCREEN_CHR_PPU_COUNT));

	// initialize metadata for all gfx tilemap objects and chr banks
	load_chr_metadata(c::CHR_GFX_ID_TITLE, c::CHR_GFX_LABEL_TITLE, c::CHR_BANK_TITLE,
		c::CHR_GFX_NUM_ID_TITLE,
		p_config.constant(c::ID_TITLE_TILEMAP_MT_X),
		p_config.constant(c::ID_TITLE_TILEMAP_MT_Y),
		p_config.constant(c::ID_TITLE_TILEMAP_MT_W),
		p_config.constant(c::ID_TITLE_TILEMAP_MT_H),
		p_config.constant(c::ID_TITLE_TILEMAP_OFFSET),
		p_config.constant(c::ID_TITLE_ATTRIBUTE_OFFSET),
		p_config.constant(c::ID_TITLE_PALETTE_OFFSET),
		p_config.constant(c::ID_TITLE_CHR_PPU_INDEX),
		true, true, true, true);
	load_chr_metadata(c::CHR_GFX_ID_INTRO, c::CHR_GFX_LABEL_INTRO, c::CHR_BANK_INTRO_OUTRO,
		c::CHR_GFX_NUM_ID_INTRO,
		p_config.constant(c::ID_INTRO_ANIM_TILEMAP_MT_X),
		p_config.constant(c::ID_INTRO_ANIM_TILEMAP_MT_Y),
		p_config.constant(c::ID_INTRO_ANIM_TILEMAP_MT_W),
		p_config.constant(c::ID_INTRO_ANIM_TILEMAP_MT_H),
		p_config.constant(c::ID_INTRO_ANIM_TILEMAP_OFFSET),
		p_config.constant(c::ID_INTRO_ANIM_ATTRIBUTE_OFFSET),
		p_config.constant(c::ID_INTRO_ANIM_PALETTE_OFFSET),
		p_config.constant(c::ID_INTRO_ANIM_CHR_PPU_INDEX),
		true, true, false, false);
	load_chr_metadata(c::CHR_GFX_ID_ITEMS, c::CHR_GFX_LABEL_ITEMS, c::CHR_BANK_ITEMS,
		c::CHR_GFX_NUM_ID_ITEMS,
		p_config.constant(c::ID_ITEM_VSCREEN_TILEMAP_MT_X),
		p_config.constant(c::ID_ITEM_VSCREEN_TILEMAP_MT_Y),
		p_config.constant(c::ID_ITEM_VSCREEN_TILEMAP_MT_W),
		p_config.constant(c::ID_ITEM_VSCREEN_TILEMAP_MT_H),
		p_config.constant(c::ID_ITEM_VSCREEN_TILEMAP_OFFSET),
		0, // no attributes
		0, // no palette
		p_config.constant(c::ID_ITEM_VSCREEN_CHR_PPU_INDEX),
		false, false, false, false);
	load_chr_metadata(c::CHR_GFX_ID_OUTRO, c::CHR_GFX_LABEL_OUTRO, c::CHR_BANK_INTRO_OUTRO,
		c::CHR_GFX_NUM_ID_OUTRO,
		p_config.constant(c::ID_OUTRO_ANIM_TILEMAP_MT_X),
		p_config.constant(c::ID_OUTRO_ANIM_TILEMAP_MT_Y),
		p_config.constant(c::ID_OUTRO_ANIM_TILEMAP_MT_W),
		p_config.constant(c::ID_OUTRO_ANIM_TILEMAP_MT_H),
		p_config.constant(c::ID_OUTRO_ANIM_TILEMAP_OFFSET),
		p_config.constant(c::ID_OUTRO_ANIM_ATTRIBUTE_OFFSET),
		p_config.constant(c::ID_OUTRO_ANIM_PALETTE_OFFSET),
		p_config.constant(c::ID_OUTRO_ANIM_CHR_PPU_INDEX),
		true, true, false, false);

	// needed when patching ROM
	chrbank_rom_offsets.insert(std::make_pair(c::CHR_BANK_TITLE,
		p_config.constant(c::ID_TITLE_CHR_OFFSET)));
	chrbank_rom_offsets.insert(std::make_pair(c::CHR_BANK_INTRO_OUTRO,
		p_config.constant(c::ID_INTRO_ANIM_CHR_OFFSET)));
	chrbank_rom_offsets.insert(std::make_pair(c::CHR_BANK_ITEMS,
		p_config.constant(c::ID_ITEM_VSCREEN_CHR_OFFSET)));

	// load tilemap data from rom - tilemaps, attributes and palettes
	load_tilemap_data_from_rom(p_rom, c::CHR_GFX_ID_TITLE);
	load_tilemap_data_from_rom(p_rom, c::CHR_GFX_ID_INTRO);
	load_tilemap_data_from_rom(p_rom, c::CHR_GFX_ID_OUTRO);
	load_tilemap_data_from_rom(p_rom, c::CHR_GFX_ID_ITEMS);
}

void fe::GfxTilemapManager::load_tilemap_data_from_rom(const std::vector<byte>& p_rom,
	const std::string& p_gfx_id) {
	verify_chr_gfx_id(p_gfx_id);
	load_rom_tilemap(p_rom, p_gfx_id);
	load_rom_attribute_map(p_rom, p_gfx_id);
	load_rom_palette(p_rom, p_gfx_id);
}

void fe::GfxTilemapManager::load_rom_tilemap(const std::vector<byte>& p_rom,
	const std::string& p_gfx_id) {
	if (!tilemapdata.contains(p_gfx_id) ||
		tilemapdata.at(p_gfx_id).tilemap.empty()) {
		const auto& md{ metadata.at(p_gfx_id) };

		std::size_t chr_w = 2 * md.mt_w;
		std::size_t chr_h = 2 * md.mt_h;
		std::size_t rom_offset{ md.m_rom_offset_tilemap };
		std::size_t cursor{ 0 };

		GfxTilemap tilemap;

		for (std::size_t j{ 0 }; j < chr_h; ++j) {
			std::vector<byte> tmp;
			for (std::size_t i{ 0 }; i < chr_w; ++i)
				tmp.push_back(p_rom.at(rom_offset + cursor++));
			tilemap.push_back(tmp);
		}

		tilemapdata[p_gfx_id].tilemap = tilemap;
	}
}

void fe::GfxTilemapManager::load_rom_attribute_map(const std::vector<byte>& p_rom,
	const std::string& p_gfx_id) {
	if (!tilemapdata.contains(p_gfx_id) ||
		tilemapdata.at(p_gfx_id).attrmap.empty()) {
		const auto& md{ metadata.at(p_gfx_id) };
		std::size_t attr_w = md.mt_w;
		std::size_t attr_h = md.mt_h;
		std::size_t rom_offset_attr{ md.m_rom_offset_attr };
		std::size_t cursor{ 0 };

		// we store the attribute table for the whole screen always (64 bytes)
		GfxAttrmap l_attrmap = std::vector<std::vector<byte>>(16, std::vector<byte>(16, 0));
		if (md.m_patch_attributes) {
			for (std::size_t ay = 0; ay < 8; ++ay) {
				for (std::size_t ax = 0; ax < 8; ++ax) {
					byte attr = p_rom.at(rom_offset_attr + ay * 8 + ax);

					l_attrmap[ay * 2 + 0][ax * 2 + 0] = (attr >> 0) & 0b11; // TL
					l_attrmap[ay * 2 + 0][ax * 2 + 1] = (attr >> 2) & 0b11; // TR
					l_attrmap[ay * 2 + 1][ax * 2 + 0] = (attr >> 4) & 0b11; // BL
					l_attrmap[ay * 2 + 1][ax * 2 + 1] = (attr >> 6) & 0b11; // BR
				}
			}
		}

		tilemapdata[p_gfx_id].attrmap = l_attrmap;
	}
}

void fe::GfxTilemapManager::load_rom_palette(const std::vector<byte>& p_rom, const std::string& p_gfx_id) {
	if (!tilemapdata.contains(p_gfx_id) ||
		tilemapdata.at(p_gfx_id).palette.empty()) {
		const auto& md{ metadata.at(p_gfx_id) };

		// load from rom
		if (md.m_patch_palette) {
			for (std::size_t i{ 0 }; i < 16; ++i)
				tilemapdata[p_gfx_id].palette.push_back(p_rom.at(md.m_rom_offset_pal + i));
		}
		// populate with a constant palette if not from rom
		else {
			tilemapdata[p_gfx_id].palette = {
			0x0f, 0x18, 0x26, 0x30,
			0x0f, 0x18, 0x26, 0x30,
			0x0f, 0x18, 0x26, 0x30,
			0x0f, 0x18, 0x26, 0x30 };
		}
	}
}

void fe::GfxTilemapManager::verify_chr_gfx_id(const std::string& p_gfx_id) const {
	if (!metadata.contains(p_gfx_id))
		throw std::runtime_error(std::format("Missing metadata definitions for chr graphic data: {}", p_gfx_id));
}

// helpers for xml import
void fe::GfxTilemapManager::set_chr_bank(const std::string& p_chr_bank_id,
	const std::vector<klib::NES_tile>& p_tiles) {
	chrbanks[p_chr_bank_id] = p_tiles;
}

void fe::GfxTilemapManager::set_tilemap(const std::string& p_gfx_id,
	const std::vector<std::vector<byte>>& p_tilemap) {
	tilemapdata[p_gfx_id].tilemap = p_tilemap;
}

void fe::GfxTilemapManager::set_attribute_table(const std::string& p_gfx_id,
	const std::vector<std::vector<byte>>& p_attribute_table) {
	tilemapdata[p_gfx_id].attrmap = p_attribute_table;
}

void fe::GfxTilemapManager::set_palette(const std::string& p_gfx_id,
	const std::vector<byte>& p_palette) {
	tilemapdata[p_gfx_id].palette = p_palette;
}

// helpers for bmp import and export
void fe::GfxTilemapManager::commit_import(const std::string& p_gfx_id,
	const fe::ChrTilemap& p_result) {
	auto& tilemap{ tilemapdata.at(p_gfx_id).tilemap };
	auto& attrmap{ tilemapdata.at(p_gfx_id).attrmap };
	const auto& md{ metadata.at(p_gfx_id) };
	auto& chrbank{ chrbanks.at(md.m_chr_bank_id) };

	const auto& rtm{ p_result.m_tilemap };

	// check the imported tilemap before we make any changes
	bool dims_ok{ true };
	if (rtm.size() * 2 != tilemap.size())
		dims_ok = false;
	else {
		for (std::size_t j{ 0 }; j < rtm.size(); ++j) {
			if (rtm[j].size() * 2 != tilemap[j].size())
				dims_ok = false;
			for (std::size_t i{ 0 }; i < rtm[j].size(); ++i)
				if (!rtm[j][i].has_value())
					dims_ok = false;
		}
	}

	if (!dims_ok)
		throw std::runtime_error("Import result does not have the right dimensions");

	// update tilemap
	for (std::size_t j{ 0 }; j < rtm.size(); ++j) {
		for (std::size_t i{ 0 }; i < rtm[j].size(); ++i) {
			tilemap[2 * j][2 * i] = static_cast<byte>(rtm[j][i]->m_idxs.at(0));
			tilemap[2 * j][2 * i + 1] = static_cast<byte>(rtm[j][i]->m_idxs.at(1));
			tilemap[2 * j + 1][2 * i] = static_cast<byte>(rtm[j][i]->m_idxs.at(2));
			tilemap[2 * j + 1][2 * i + 1] = static_cast<byte>(rtm[j][i]->m_idxs.at(3));

			// remember we keep the attr table for the whole screen,
			// but the result only contains the attributes for our sub-section
			if (md.m_patch_attributes)
				attrmap[j + md.mt_y][i + md.mt_x] = static_cast<byte>(rtm[j][i]->m_palette);
		}
	}

	// update chr-bank, we only keep the updatable tiles locally,
	// but the result gives us the whole 256-tile bank
	for (std::size_t t{ 0 }; t < chrbank.size(); ++t)
		chrbank[t] = p_result.m_tiles.at(t + md.m_chr_ppu_index);
}

std::vector<byte>& fe::GfxTilemapManager::get_bg_palette(const std::string& p_gfx_id) {
	return tilemapdata.at(p_gfx_id).palette;
}

fe::ChrTilemap fe::GfxTilemapManager::get_chrtilemap(const std::string& p_gfx_id) const {
	fe::ChrTilemap result;
	const auto& gfxdata{ tilemapdata.at(p_gfx_id) };
	const auto& md{ metadata.at(p_gfx_id) };
	const auto& chrbank{ chrbanks.at(md.m_chr_bank_id) };

	result.set_flat_palette(gfxdata.palette);

	// combine the tilemap and the attribute into vec<vec<chrmetatile>>
	for (std::size_t j{ 0 }; j < md.mt_h; ++j) {

		std::vector<std::optional<ChrMetaTile>> tmp;
		for (std::size_t i{ 0 }; i < md.mt_w; ++i) {
			std::vector<std::size_t> mt_quads;
			mt_quads.push_back(gfxdata.tilemap.at(2 * j).at(2 * i));
			mt_quads.push_back(gfxdata.tilemap.at(2 * j).at(2 * i + 1));
			mt_quads.push_back(gfxdata.tilemap.at(2 * j + 1).at(2 * i));
			mt_quads.push_back(gfxdata.tilemap.at(2 * j + 1).at(2 * i + 1));

			ChrMetaTile metatile;
			metatile.m_idxs = mt_quads;

			// for "virtual" tilemap we might be bigger than one screen
			// just send in whatever attr we have in the top left quad
			// as they will all be the same
			if (!md.m_patch_attributes) {
				metatile.m_palette = gfxdata.attrmap.at(0).at(0);
			}
			else {
				metatile.m_palette = gfxdata.attrmap.at(j + md.mt_y).at(i + md.mt_x);
			}
			tmp.push_back(metatile);
		}
		result.m_tilemap.push_back(tmp);
	}

	result.m_tiles = get_complete_chr_tileset(p_gfx_id);

	return result;
}

std::vector<fe::ChrGfxTile> fe::GfxTilemapManager::get_complete_chr_tileset_w_md(
	const std::string& p_gfx_id) const {
	std::vector<fe::ChrGfxTile> l_tiles;

	const auto& md{ metadata.at(p_gfx_id) };
	const auto& chrbank{ chrbanks.at(md.m_chr_bank_id) };

	// anything up to the ppu idx is disallowed empty tiles (read-only or not doesn't matter then)
	for (std::size_t i{ 0 }; i < md.m_chr_ppu_index; ++i)
		l_tiles.push_back(fe::ChrGfxTile(klib::NES_tile(), true, false));
	// anything in the chr bank is allowed, and not read-only. these can change
	for (std::size_t i{ 0 }; i < chrbank.size(); ++i)
		l_tiles.push_back(fe::ChrGfxTile(chrbank[i], false, true));
	// pad with more disallowed empty tiles at the end
	while (l_tiles.size() < 256)
		l_tiles.push_back(fe::ChrGfxTile(klib::NES_tile(), true, false));
	// tile 0 might be read-only
	if (md.m_fix_tile_0)
		l_tiles[0].m_readonly = true;

	// add alphanumeric tiles if required, these come after the editable tiles
	if (md.m_add_alphanumeric) {
		const auto& alphabank{ chrbanks.at(c::CHR_BANK_ALPHA) };
		const auto& numbank{ chrbanks.at(c::CHR_BANK_NUMERIC) };

		std::size_t l_chr_ppu_idx{ md.m_chr_ppu_index + chrbank.size() };

		// 0-9
		for (std::size_t i{ 0 }; i < numbank.size() &&
			l_chr_ppu_idx < 256; ++i)
			l_tiles[l_chr_ppu_idx++] = fe::ChrGfxTile(numbank[i], true, true);

		// A-Z + copyright symbol
		for (std::size_t i{ 0 }; i < alphabank.size() && l_chr_ppu_idx < 256; ++i)
			l_tiles[l_chr_ppu_idx++] = fe::ChrGfxTile(alphabank[i], true, true);
	}

	// next - lock all tiles referenced by other images using this char bank
	for (const auto& kv : metadata)
		if (kv.first != p_gfx_id && kv.second.m_chr_bank_id == md.m_chr_bank_id) {
			for (const auto& otherrow : tilemapdata.at(kv.first).tilemap)
				for (byte othertile : otherrow)
					l_tiles.at(othertile).m_readonly = true;
		}

	return l_tiles;
}

std::vector<klib::NES_tile> fe::GfxTilemapManager::get_complete_chr_tileset(
	const std::string& p_gfx_id) const {
	std::vector<klib::NES_tile> result;
	auto tiles{ get_complete_chr_tileset_w_md(p_gfx_id) };

	for (const auto& tile : tiles)
		result.push_back(tile.m_tile);

	return result;
}

// helpers for the xml import and export
bool fe::GfxTilemapManager::is_chr_bank_dynamic(const std::string& p_chr_bank_id) const {
	for (const auto& md : metadata)
		if (md.second.m_chr_bank_id == p_chr_bank_id)
			return true;
	return false;
}

bool fe::GfxTilemapManager::is_palette_dynamic(const std::string& p_gfx_id) const {
	return metadata.at(p_gfx_id).m_patch_palette;
}

bool fe::GfxTilemapManager::is_attr_table_dynamic(const std::string& p_gfx_id) const {
	return metadata.at(p_gfx_id).m_patch_attributes;
}

std::string fe::GfxTilemapManager::get_tilemap_chr_bank_id(const std::string& p_gfx_id) const {
	return metadata.at(p_gfx_id).m_chr_bank_id;
}

// ROM patching
void fe::GfxTilemapManager::patch_rom(std::vector<byte>& p_rom) const {
	patch_chr_banks(p_rom);
	patch_tilemap_data(p_rom);
}

void fe::GfxTilemapManager::patch_tilemap_data(std::vector<byte>& p_rom) const {
	for (const auto& kv : metadata) {

		// always patch tilemap
		const auto tmbytes{ tilemapdata.at(kv.first).tilemap };
		std::size_t l_offset{ kv.second.m_rom_offset_tilemap };
		for (const auto& row : tmbytes) {
			std::copy(begin(row), end(row), begin(p_rom) + l_offset);
			l_offset += row.size();
		}

		// patch attribute table if patchable
		if (kv.second.m_patch_attributes) {
			const auto attrbytes{ serialize_attr_table(tilemapdata.at(kv.first).attrmap) };
			std::copy(begin(attrbytes), end(attrbytes), begin(p_rom) + kv.second.m_rom_offset_attr);
		}
		
		// patch palette if patchable
		if (kv.second.m_patch_palette) {
			const auto& palbytes{ tilemapdata.at(kv.first).palette };
			std::copy(begin(palbytes), end(palbytes), begin(p_rom) + kv.second.m_rom_offset_pal);
		}
		
	}
}

std::vector<byte> fe::GfxTilemapManager::serialize_attr_table(
	const std::vector<std::vector<byte>>& p_attr_table) const {
	std::vector<byte> result(64, 0);
	std::size_t idx = 0;

	for (size_t my = 0; my < 16; my += 2) {
		for (size_t mx = 0; mx < 16; mx += 2) {

			byte tl = p_attr_table[my][mx] & 0x03;
			byte tr = p_attr_table[my][mx + 1] & 0x03;
			byte bl = p_attr_table[my + 1][mx] & 0x03;
			byte br = p_attr_table[my + 1][mx + 1] & 0x03;

			result[idx++] =
				(tl << 0) |
				(tr << 2) |
				(bl << 4) |
				(br << 6);
		}
	}

	return result;
}

void fe::GfxTilemapManager::patch_chr_banks(std::vector<byte>& p_rom) const {
	for (const auto& kv : chrbank_rom_offsets) {
		const auto& tiles{ chrbanks.at(kv.first) };
		for (std::size_t i{ 0 }; i < tiles.size(); ++i) {
			const auto& chrbytes{ tiles[i].to_bytes() };
			std::copy(begin(chrbytes), end(chrbytes), begin(p_rom) + kv.second + 16 * i);
		}
	}
}
