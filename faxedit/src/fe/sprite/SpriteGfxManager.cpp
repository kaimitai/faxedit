#include "SpriteGfxManager.h"
#include "fe_sprite_constants.h"
#include "./../Config.h"
#include "./../ROM_Manager.h"
#include <algorithm>
#include <map>
#include <format>
#include <set>

void fe::SpriteGfxManager::merge_portrait_collection(const SpriteGfxCollection& coll) {
	auto newcoll{ coll };
	for (std::size_t i{ 0 }; i < portraits.frames.size(); ++i) {
		newcoll.frames.at(i).offset_x = portraits.frames[i].offset_x;
		newcoll.frames.at(i).offset_y = portraits.frames[i].offset_y;
		newcoll.frames.at(i).pivot_x = portraits.frames[i].pivot_x;
	}

	while (newcoll.tiles.size() < 255)
		newcoll.tiles.push_back(klib::NES_tile());

	portraits = newcoll;
}

void fe::SpriteGfxManager::load_rom_data(const fe::Config& p_config, const std::vector<byte>& p_rom,
	const fe::ROM_Manager& p_rom_mgr) {
	load_portrait_data(p_config, p_rom, p_rom_mgr);
	load_player_data(p_config, p_rom, p_rom_mgr);
	load_npc_data(p_config, p_rom, p_rom_mgr);
}

void fe::SpriteGfxManager::load_npc_data(const fe::Config& p_config, const std::vector<byte>& p_rom,
	const fe::ROM_Manager& p_rom_mgr) {
	// read this from ROM
	const std::size_t SPRITE_CUTOFF{ 55 };

	std::size_t sprite_count{ p_config.constant(c::ID_SPRITE_COUNT) };

	// load common sprite chr (ppu $40-$90)
	std::vector<klib::NES_tile> common_chr(c::PPU_COMMON_TILE_START, klib::NES_tile());

	auto common_chr_data{ extract_chr_tiles(p_rom,
		p_rom_mgr.get_ptr_to_rom_offset(p_rom, p_config.pointer(c::ID_GFX_COMMON_CHR_PTR)),
		c::PPU_COMMON_TILE_COUNT) };

	common_chr.insert(end(common_chr), begin(common_chr_data), end(common_chr_data));

	auto ppu_tile_counts{ p_rom_mgr.read_bytes(p_rom, p_config.constant(c::ID_GFX_NPC_TILE_COUNT_OFFSET), sprite_count) };
	auto anim_frame_starts{ p_rom_mgr.read_bytes(
		p_rom, p_config.constant(c::ID_GFX_NPC_ANIM_IDX_OFFSET), sprite_count) };

	auto bank6_chr_master_ptr{ p_config.pointer(c::ID_GFX_NPC_CHR_BANK6_PTR) };
	auto bank7_chr_master_ptr{ p_config.pointer(c::ID_GFX_NPC_CHR_BANK7_PTR) };
	auto anim_frames_master_ptr{ p_config.pointer(c::ID_GFX_NPC_ANIM_FRAME_PTR) };
	auto bank6_chr_ptr_table_offset{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, bank6_chr_master_ptr) };
	auto bank7_chr_ptr_table_offset{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, bank7_chr_master_ptr) };

	auto anim_frame_ptr_table_offset{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, anim_frames_master_ptr) };

	const auto sprite_frame_counts{ p_config.bmap_numeric(c::ID_GFX_NPC_FRAME_COUNT) };

	std::map<std::size_t, std::vector<std::pair<std::size_t, std::size_t>>> framerefs;
	std::map<std::size_t, std::vector<std::pair<bool, std::size_t>>> chrrefs;

	

	for (std::size_t i{ 0 }; i < sprite_count; ++i) {
		bool bank6{ i < SPRITE_CUTOFF };
		std::size_t ptr_table_entry{ bank6 ? i : i - SPRITE_CUTOFF };

		std::size_t zero_addr{ bank6 ? bank6_chr_master_ptr.second : bank7_chr_master_ptr.second };
		std::size_t chr_ptr_addr{ bank6 ?
			bank6_chr_ptr_table_offset + 2 * ptr_table_entry :
			bank7_chr_ptr_table_offset + 2 * ptr_table_entry };

		std::size_t pputilecount{ ppu_tile_counts[i] };
		std::vector<klib::NES_tile> chrbank;

		if (pputilecount > 1) {
			chrbank = extract_chr_tiles(p_rom,
				p_rom_mgr.get_ptr_to_rom_offset(p_rom, chr_ptr_addr, zero_addr),
				ppu_tile_counts[i]);
			chrrefs[p_rom_mgr.get_ptr_to_rom_offset(p_rom, chr_ptr_addr, zero_addr)].push_back(
				{bank6, i}
			);
		}
		else
			chrbank = common_chr;

		std::size_t framecount{ sprite_frame_counts.contains(static_cast<byte>(i)) ?
			sprite_frame_counts.at(static_cast<byte>(i)) : 1 };
		std::vector<fe::SpriteAnimationFrame> frames;

		for (std::size_t fr{ 0 }; fr < framecount; ++fr) {
			auto framestartaddr{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, anim_frame_ptr_table_offset +
				2 * (anim_frame_starts.at(i) + fr),
	anim_frames_master_ptr.second) };
			frames.push_back(fe::SpriteAnimationFrame(p_rom, framestartaddr));

			framerefs[framestartaddr].push_back({ i,fr });
		}

		npcs.push_back({ chrbank, frames });
	}

	std::string reusereport;
	for (const auto& kv : framerefs)
		if (kv.second.size() != 1) {
			reusereport += std::format("{}: ", kv.first);
			for (const auto& ppp : kv.second)
				reusereport += std::format("({},{}) ", ppp.first, ppp.second);
			reusereport += "\n";
		}

	reusereport += "*******************\n";

	for (const auto& kv : chrrefs)
		if (kv.second.size() != 1) {
			reusereport += std::format("{}: ", kv.first);
			for (const auto& ppp : kv.second)
				reusereport += std::format("({},{}) ", ppp.first, ppp.second);
			reusereport += "\n";
		}

	std::size_t totalframecnt{ 0 };
	for (const auto& fr : npcs)
		totalframecnt += fr.frames.size();

	std::size_t must_remove{ totalframecnt - 256 };
}

void fe::SpriteGfxManager::load_player_data(const fe::Config& p_config, const std::vector<byte>& p_rom,
	const fe::ROM_Manager& p_rom_mgr) {

	const auto player_count{ p_config.constant(c::ID_GFX_PLAYER_COUNT) };
	std::vector<std::size_t> loadlist_sizes;
	std::vector<std::vector<byte>> loadlists;

	// load list sizes seem to be one less than the actual size
	for (std::size_t i{ 0 }; i < player_count; ++i)
		loadlist_sizes.push_back(p_rom.at(p_config.constant(c::ID_GFX_PLAYER_TILE_COUNT_OFFSET) + i));

	auto loadlist_master_ptr{ p_config.pointer(c::ID_GFX_PLAYER_LOOKUP_TABLE_PTR) };
	auto loadlist_ptr{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, loadlist_master_ptr) };

	for (std::size_t i{ 0 }; i < 8; ++i) {
		auto loadlist_addr{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, loadlist_ptr + 2 * i, loadlist_master_ptr.second) };
		std::vector<byte> loadlist;
		for (std::size_t j{ 0 }; j <= loadlist_sizes[i]; ++j)
			loadlist.push_back(p_rom.at(loadlist_addr + j));
		loadlists.push_back(loadlist);
	}

	// final load lists are ready, we now load the chr tiles - assume count to be the maximum 255 until proven otherwise
	// we can't know up front how many chr tiles are in the bank before consulting the animation frames
	auto chrdata_ptr{ p_config.pointer(c::ID_GFX_PLAYER_CHR_PTR) };
	auto chrdata_addr{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, chrdata_ptr) };
	auto chrbank{ extract_chr_tiles(p_rom, chrdata_addr, 255) };

	std::vector<fe::SpriteAnimationFrame> frames;

	auto frameptr{ p_config.pointer(c::ID_GFX_PLAYER_ANIM_FRAME_PTR) };
	auto frame_ptr_table_addr{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, frameptr) };

	for (std::size_t j{ 0 }; j < player_count; ++j) {
		const auto loadlist{ loadlists[j] };

		for (std::size_t i{ 0 }; i < 8; ++i) {
			auto frame_addr{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, frame_ptr_table_addr, frameptr.second) };
			auto frame{ fe::SpriteAnimationFrame(p_rom, frame_addr) };
			normalize_frame(loadlist, frame, chrbank.size());
			frames.push_back(frame);
			frame_ptr_table_addr += 2;
		}
	}

	player = fe::SpriteGfxCollection(chrbank, frames);
}

void fe::SpriteGfxManager::load_portrait_data(const fe::Config& p_config,
	const std::vector<byte>& p_rom, const fe::ROM_Manager& p_rom_mgr) {

	auto portrait_chr_offset{ p_rom_mgr.get_ptr_to_rom_offset(p_rom,
		p_config.pointer(c::ID_GFX_PORTRAIT_CHR_PTR)) };

	portraits.tiles = extract_chr_tiles(p_rom, portrait_chr_offset, p_config.constant(c::ID_GFX_PORTRAIT_CHR_TILE_COUNT));

	auto portrait_frames_ptr_offset{ p_rom_mgr.get_ptr_to_rom_offset(p_rom,
		p_config.pointer(c::ID_GFX_PORTRAIT_ANIM_FRAME_PTR)) };

	for (std::size_t i{ 0 }; i < p_config.constant(c::ID_GFX_PORTRAIT_TOTAL_FRAME_COUNT); ++i) {
		auto frame_offset{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, portrait_frames_ptr_offset + 2 * i,
			p_config.pointer(c::ID_GFX_PORTRAIT_ANIM_FRAME_PTR).second) };
		portraits.frames.push_back(fe::SpriteAnimationFrame(p_rom, frame_offset));
	}

	auto portrait_frames_lookup_offset{ p_rom_mgr.get_ptr_to_rom_offset(p_rom,
		p_config.pointer(c::ID_GFX_PORTRAIT_LOOKUP_TABLE_PTR)) };

	std::size_t frames_per_portrait{ p_config.constant(c::ID_GFX_PORTRAIT_TOTAL_FRAME_COUNT) / p_config.constant(c::ID_GFX_PORTRAIT_COUNT) };
	for (std::size_t i{ 0 }; i < p_config.constant(c::ID_GFX_PORTRAIT_COUNT); ++i) {
		auto lookup_offset{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, portrait_frames_lookup_offset + 2 * i,
			p_config.pointer(c::ID_GFX_PORTRAIT_LOOKUP_TABLE_PTR).second) };

		auto ppu_order{ extract_ppu_idx_lookup_ff_delim(p_rom, lookup_offset) };
		std::set<byte> ppu_set;
		for (byte b : ppu_order)
			ppu_set.insert(b);

		for (std::size_t j{ 0 }; j < frames_per_portrait; ++j)
			normalize_frame(ppu_order, portraits.frames.at(i * frames_per_portrait + j), portraits.tiles.size());
	}
}

// makes frame tile numbers relative to chr bank and not ppu order
// return number of invalid references (for debugging)
int fe::SpriteGfxManager::normalize_frame(const std::vector<byte>& ppu_order,
	fe::SpriteAnimationFrame& frame, std::size_t p_chr_bank_size) {
	int result{ 0 };

	for (std::size_t y{ 0 }; y < frame.tilemap.size(); ++y)
		for (std::size_t x{ 0 }; x < frame.tilemap[y].size(); ++x) {
			auto& tile{ frame.tilemap[y][x] };
			if (tile) {
				if (tile->index < ppu_order.size())
					tile->index = ppu_order[tile->index];
				else {
					tile = std::nullopt;
					++result;
				}
			}
		}

	return result;
}

std::vector<byte> fe::SpriteGfxManager::extract_ppu_idx_lookup_ff_delim(const std::vector<byte>& p_rom,
	std::size_t p_offset) const {
	std::vector<byte> result;

	for (std::size_t i{ 0 }; p_rom.at(p_offset + i) != 0xff; ++i)
		result.push_back(p_rom.at(p_offset + i));

	return result;
}

std::vector<klib::NES_tile> fe::SpriteGfxManager::extract_chr_tiles(const std::vector<byte>& p_rom,
	std::size_t p_offset, std::size_t p_count) const {
	std::vector<klib::NES_tile> result;
	for (std::size_t i{ 0 }; i < p_count; ++i)
		result.push_back(klib::NES_tile(p_rom, p_offset + 16 * i));
	return result;
}

std::vector<byte> fe::SpriteGfxManager::calc_portrait_ppu_load_list(std::size_t p_portrait_no) const {
	std::map<byte, int> portrait_tile_usage;

	for (std::size_t i{ 0 }; i < 5; ++i) {
		const auto usagemap{ portraits.frames.at(5 * p_portrait_no + i).get_tile_usage() };
		for (const auto& kv : usagemap)
			portrait_tile_usage[kv.first] += kv.second;
	}

	std::vector<byte> result;
	for (const auto& kv : portrait_tile_usage)
		result.push_back(kv.first);

	result.push_back(0xff); // delimiter - no frame could even theoretically have this as a chr-tile index

	return result;
}

void fe::SpriteGfxManager::patch_rom(const fe::Config& p_config, std::vector<byte>& p_rom,
	const fe::ROM_Manager& p_rom_mgr) {
	patch_portrait_data(p_config, p_rom, p_rom_mgr);

	std::vector<std::vector<byte>> npc_frames;

	for (const auto& npc : npcs)
		for (const auto& frame : npc.frames)
			npc_frames.push_back(frame.to_bytes());

}

void fe::SpriteGfxManager::patch_portrait_data(const fe::Config& p_config, std::vector<byte>& p_rom,
	const fe::ROM_Manager& p_rom_mgr) {

	canonicalize_gfx_collection(portraits);

	std::vector<std::vector<byte>> loadlists;
	std::vector<byte> chrdata;
	std::vector<std::vector<byte>> frame_data;

	for (std::size_t pp{ 0 }; pp < 11; ++pp) {
		const auto loadlist{ calc_portrait_ppu_load_list(pp) };
		loadlists.push_back(loadlist);

		std::map<byte, byte> remap;
		for (std::size_t i{ 0 }; i < loadlist.size(); ++i)
			remap.insert(std::make_pair(loadlist[i], static_cast<byte>(i)));

		for (std::size_t i{ 0 }; i < 5; ++i)
			frame_data.push_back(portraits.frames.at(pp * 5 + i).to_bytes(remap));
	}

	for (const auto& tile : portraits.tiles) {
		const auto chrbytes{ tile.to_bytes() };
		chrdata.insert(end(chrdata), begin(chrbytes), end(chrbytes));
	}

	const auto ptr_ll{ p_config.pointer(c::ID_GFX_PORTRAIT_LOOKUP_TABLE_PTR) };
	const auto ptr_chr{ p_config.pointer(c::ID_GFX_PORTRAIT_CHR_PTR) };
	const auto ptr_frame{ p_config.pointer(c::ID_GFX_PORTRAIT_ANIM_FRAME_PTR) };

	std::size_t b8offset{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, ptr_ll) };

	const auto ll_w_ptr{ p_rom_mgr.build_pointer_table_and_data(
		b8offset,
		ptr_ll.second,
		loadlists) };

	p_rom_mgr.patch_bytes(ll_w_ptr, p_rom, b8offset);

	b8offset += ll_w_ptr.size();

	p_rom_mgr.patch_bytes(chrdata, p_rom, b8offset);

	// update ptr to chr data
	p_rom_mgr.patch_ptr(p_rom, ptr_chr.first, b8offset - ptr_chr.second);

	// finally patch frames
	std::size_t b7offset{ p_rom_mgr.get_ptr_to_rom_offset(p_rom, ptr_frame) };

	const auto frames_w_ptr{ p_rom_mgr.build_pointer_table_and_data(
	b7offset,
	ptr_frame.second,
	frame_data) };

	p_rom_mgr.patch_bytes(frames_w_ptr, p_rom, b7offset);
}

void fe::SpriteGfxManager::canonicalize_gfx_collection(fe::SpriteGfxCollection& coll) {
	canonicalize_gfx_collection_chr_bank(coll);
	dedup_gfx_collection(coll);
	sort_gfx_collection(coll);
}

void fe::SpriteGfxManager::canonicalize_gfx_collection_chr_bank(fe::SpriteGfxCollection& coll) {
	// 1. canonicalize each tile
	std::vector<klib::CanonChoice> choices(coll.tiles.size());

	for (std::size_t i = 0; i < coll.tiles.size(); ++i)
		choices[i] = coll.tiles[i].canonicalize();

	// 2. Apply the reverse action to each tile instance in each frame
	//    (for flips, inverse == itself, so just toggle those bits)
	for (auto& frame : coll.frames) {
		for (auto& row : frame.tilemap) {
			for (auto& cell : row) {
				if (!cell) continue;

				const byte idx = cell->index;
				if (idx >= choices.size())
					throw std::runtime_error("Frame references CHR index out of range during canonicalization");

				const auto& c = choices[idx];
				if (c.h) cell->h_flip = !cell->h_flip;
				if (c.v) cell->v_flip = !cell->v_flip;
			}
		}
	}
}

void fe::SpriteGfxManager::dedup_gfx_collection(fe::SpriteGfxCollection& coll) {
	const std::size_t N = coll.tiles.size();

	std::map<klib::NES_tile, std::size_t> tileToNewIndex;
	std::vector<std::size_t> oldToNew(N, 0);
	std::vector<klib::NES_tile> newBank;
	newBank.reserve(N);

	for (std::size_t i = 0; i < N; ++i) {
		const auto& tile = coll.tiles[i];

		auto it = tileToNewIndex.find(tile);
		if (it == tileToNewIndex.end()) {
			std::size_t newIdx = newBank.size();
			newBank.push_back(tile);
			tileToNewIndex.emplace(tile, newIdx);
			oldToNew[i] = newIdx;
		}
		else {
			oldToNew[i] = it->second;
		}
	}

	// update frame indices
	for (auto& frame : coll.frames) {
		for (auto& row : frame.tilemap) {
			for (auto& cell : row) {
				if (!cell) continue;
				cell->index = static_cast<byte>(oldToNew[cell->index]);
			}
		}
	}

	// 0. clear unused tiles
	std::map<byte, int> tileusage;
	for (const auto& frame : coll.frames) {
		const auto frameusage{ frame.get_tile_usage() };
		for (const auto& kv : frameusage)
			tileusage[kv.first] += kv.second;
	}

	for (std::size_t i{ 0 }; i < newBank.size(); ++i)
		if (!tileusage.contains(static_cast<byte>(i)))
			newBank[i] = klib::NES_tile();

	while (newBank.size() < coll.tiles.size())
		newBank.push_back(klib::NES_tile());

	coll.tiles = std::move(newBank);
}

void fe::SpriteGfxManager::sort_gfx_collection(fe::SpriteGfxCollection& coll) {
	const std::size_t targetSize = coll.tiles.size();

	if (coll.tiles.size() > targetSize)
		throw std::runtime_error("Portrait CHR bank exceeds 255 tiles.");

	// 1) Build sort order of existing (deduped) tiles
	std::vector<std::size_t> order(coll.tiles.size());
	for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;

	// 2) Sort: non-empty first, empty last; descending among non-empty
	std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
		const bool ae = coll.tiles[a].is_empty();
		const bool be = coll.tiles[b].is_empty();

		if (ae != be) return be;          // non-empty (false) comes first
		if (ae && be) return false;       // both empty: keep relative order (doesn't matter)

		// descending: a before b if chrbank[b] < chrbank[a]
		return coll.tiles[b] < coll.tiles[a];
		});

	// 3) Build old->new index mapping
	std::vector<byte> oldToNew2(coll.tiles.size(), 0);
	for (std::size_t newIdx = 0; newIdx < order.size(); ++newIdx) {
		oldToNew2[order[newIdx]] = static_cast<byte>(newIdx);
	}

	// 4) Remap all frame indices
	for (auto& frame : coll.frames) {
		for (auto& row : frame.tilemap) {
			for (auto& cell : row) {
				if (!cell) continue;

				const byte oldIdx = cell->index;
				if (oldIdx >= oldToNew2.size())
					throw std::runtime_error("Frame references CHR index out of range during sort/pad.");

				cell->index = oldToNew2[oldIdx];
			}
		}
	}

	// 5) Rebuild bank in sorted order
	std::vector<klib::NES_tile> newBank2;
	newBank2.reserve(targetSize);

	for (std::size_t i = 0; i < order.size(); ++i)
		newBank2.push_back(coll.tiles[order[i]]);

	// 6) Pad to 255 with empty tiles (all zeros)
	while (newBank2.size() < targetSize)
		newBank2.push_back(klib::NES_tile{});

	coll.tiles = std::move(newBank2);
}
