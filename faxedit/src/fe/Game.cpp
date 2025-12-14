#include "Game.h"
#include "Metatile.h"
#include "fe_constants.h"
#include "./../common/klib/Kutil.h"
#include "Config.h"
#include <algorithm>
#include <utility>

fe::Game::Game(void) :
	m_jump_on_animation{ 0x34, 0x2c, 0x5c, 0x13 },
	m_push_block(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
{
}

fe::Game::Game(const fe::Config& p_config, const std::vector<byte>& p_rom_data) :
	m_rom_data{ p_rom_data }
{
	// start of 8-byte map from world to bank no
	std::size_t l_world_to_bank{ p_config.constant(c::ID_WORLD_TILEMAP_MD) };
	// start of 8-byte map from world to ptr in that bank
	std::size_t l_world_to_bank_ptr_idx{ l_world_to_bank + 8 };
	// map of bank number to ROM offset
	auto l_ptr_tilebamp_bank_rom_offsets{ p_config.bmap_numeric(c::ID_TILEMAP_BANK_OFFSETS) };

	// extract all tilemaps for each world
	for (std::size_t world{ 0 }; world < 8; ++world) {
		m_chunks.push_back(fe::Chunk());
		std::size_t l_master_ptr{
			l_ptr_tilebamp_bank_rom_offsets.at(p_rom_data.at(l_world_to_bank + world)) +
			static_cast<std::size_t>(p_rom_data.at(l_world_to_bank_ptr_idx + world) * 2) };

		auto l_screen_ptrs{ get_screen_pointers(l_master_ptr) };

		for (auto l_idx : l_screen_ptrs)
			m_chunks.back().decompress_and_add_screen(p_rom_data, l_idx);

	}

	// extract various
	for (std::size_t i{ 0 }; i < 8; ++i)
		set_various(p_config, i);

	// extract sprites
	auto l_sprite_ptr{ p_config.pointer(c::ID_SPRITE_PTR) };
	for (std::size_t i{ 0 }; i < 8; ++i) {
		if (i == c::CHUNK_IDX_BUILDINGS) {
			// this is not regular sprite data, it is the npc bundle masterdata
			// referred to by the parameter byte in building doors
			// read until we reach another master pointer value, that may not come
			// in increasing order! (we assume tightly packed data however)
			// the original game places this data last of all just to be nasty to us,
			// so we fall back to stop parsing if we get an empty sprite set
			std::size_t l_ptr_to_bundles{ get_pointer_address(l_sprite_ptr.first + 2 * i, l_sprite_ptr.second) };
			std::size_t l_ptr_to_data{ get_pointer_address(l_ptr_to_bundles, l_sprite_ptr.second) };

			for (std::size_t npcb{ 0 }; ; ++npcb) {
				std::size_t l_ptr_to_set{ get_pointer_address(l_ptr_to_bundles + 2 * npcb, l_sprite_ptr.second) };

				if (l_ptr_to_bundles + 2 * npcb == l_ptr_to_data)
					break;

				m_npc_bundles.push_back(extract_sprite_set(m_rom_data, l_ptr_to_set));
			}
		}
		else
			set_sprites(i, l_sprite_ptr);
	}
	// extract inter-chunk transitions
	for (std::size_t i{ 0 }; i < 8; ++i) {
		set_interchunk_scrolling(p_config, i);
		set_intrachunk_scrolling(p_config, i);
	}

	m_stages = fe::StageManager(p_rom_data,
		p_config.constant(c::ID_STAGE_TO_WORLD_OFFSET),
		p_config.constant(c::ID_STAGE_CONN_OFFSET),
		p_config.constant(c::ID_STAGE_SCREEN_OFFSET),
		p_config.constant(c::ID_STAGE_REQ_OFFSET),
		p_config.constant(c::ID_GAME_START_SCREEN_OFFSET),
		p_config.constant(c::ID_GAME_START_POS_OFFSET),
		p_config.constant(c::ID_GAME_START_HP_OFFSET));

	extract_scenes_if_empty(p_config);
	extract_palette_to_music(p_config);
	extract_fog_parameters(p_config);
	extract_hud_attributes(p_config);

	std::size_t l_palette_offset{ p_config.constant(c::ID_PALETTE_OFFSET) };
	std::size_t l_palette_count{ p_config.constant(c::ID_PALETTE_COUNT) };
	for (std::size_t i{ 0 }; i < l_palette_count; ++i) {
		NES_Palette l_tmp_palette;
		for (std::size_t pidx{ l_palette_offset + 16 * i }; pidx < l_palette_offset + 16 * i + 16; ++pidx)
			l_tmp_palette.push_back(p_rom_data[pidx]);

		m_palettes.push_back(l_tmp_palette);
	}

	// get the 8 spawn locations
	std::size_t l_spawn_loc_data_start{ p_config.constant(c::ID_SPAWN_LOC_DATA_START) };
	for (std::size_t i{ 0 }; i < 8; ++i) {
		m_spawn_locations.push_back(fe::Spawn_location(
			m_rom_data.at(l_spawn_loc_data_start + i),
			m_rom_data.at(l_spawn_loc_data_start + 5 * 8 + i),
			m_rom_data.at(l_spawn_loc_data_start + 4 * 8 + i),
			m_rom_data.at(l_spawn_loc_data_start + 8 + i) >> 4,
			m_rom_data.at(l_spawn_loc_data_start + 2 * 8 + i) >> 4,
			m_rom_data.at(l_spawn_loc_data_start + 3 * 8 + i)
		));
	}

	// extract mattock animations
	std::size_t l_mattock_anim_offset{ p_config.constant(c::ID_MATTOCK_ANIM_OFFSET) };
	for (std::size_t i{ 0 }; i < 8; ++i) {
		m_chunks.at(i).m_mattock_animation = {
			m_rom_data.at(l_mattock_anim_offset + 4 * i),
			m_rom_data.at(l_mattock_anim_offset + 4 * i + 1),
			m_rom_data.at(l_mattock_anim_offset + 4 * i + 2),
			m_rom_data.at(l_mattock_anim_offset + 4 * i + 3)
		};
	}

	// extract "push-block" parameters
	m_push_block = fe::Push_block_parameters(
		m_rom_data.at(p_config.constant(c::ID_PTM_STAGE_NO_OFFSET)),
		m_rom_data.at(p_config.constant(c::ID_PTM_SCREEN_NO_OFFSET)),
		m_rom_data.at(p_config.constant(c::ID_PTM_START_POS_OFFSET)) % 16,
		m_rom_data.at(p_config.constant(c::ID_PTM_START_POS_OFFSET)) / 16,
		m_rom_data.at(p_config.constant(c::ID_PTM_BLOCK_COUNT_OFFSET)),
		m_rom_data.at(p_config.constant(c::ID_PTM_REPLACE_TILE_OFFSET)),
		m_rom_data.at(p_config.constant(c::ID_PTM_REPLACE_TILE_OFFSET) + 1),
		m_rom_data.at(p_config.constant(c::ID_PTM_REPLACE_TILE_OFFSET) + 2),
		m_rom_data.at(p_config.constant(c::ID_PTM_REPLACE_TILE_OFFSET) + 3),
		m_rom_data.at(p_config.constant(c::ID_PTM_POS_DELTA_OFFSET)),
		m_rom_data.at(p_config.constant(c::ID_PTM_TILE_NO_OFFSET)),
		m_rom_data.at(p_config.constant(c::ID_PTM_COVER_POS_OFFSET)) % 16,
		m_rom_data.at(p_config.constant(c::ID_PTM_COVER_POS_OFFSET)) / 16
	);

	// extract "jump-on" animation
	std::size_t l_jump_on_anim_offset{ p_config.constant(c::ID_JUMP_ON_ANIM_OFFSET) };
	for (std::size_t i{ 0 }; i < 4; ++i)
		m_jump_on_animation.push_back(m_rom_data.at(l_jump_on_anim_offset + i));

	initialize_game_gfx_metadata(p_config);
}

void fe::Game::generate_tilesets(const fe::Config& p_config,
	std::vector<std::size_t>& p_tileset_start,
	std::vector<std::size_t>& p_tileset_size) {
	std::size_t l_tileset_count{ p_config.constant(c::ID_WORLD_TILESET_COUNT) };
	std::size_t l_chr_hud_offset{ p_config.constant(c::ID_CHR_HUD_TILE_OFFSET) };
	std::size_t l_chr_wtile_offset{ p_config.constant(c::ID_CHR_WORLD_TILE_OFFSET) };
	std::size_t l_tileset_to_addr{ p_config.constant(c::ID_WORLD_TILESET_TO_ADDR_OFFSET) };
	std::size_t l_tileset_chr_ppu_start_offset{ l_tileset_to_addr + 2 * l_tileset_count };
	std::size_t l_tileset_chr_count_offset{ l_tileset_chr_ppu_start_offset + l_tileset_count };

	// HUD header is the same for all tilesets
	std::vector<klib::NES_tile> l_hud_tiles;
	for (std::size_t i{ 0 }; i < c::CHR_HUD_TILE_COUNT; ++i) {
		l_hud_tiles.push_back(klib::NES_tile(m_rom_data,
			l_chr_hud_offset + 16 * i));
	}

	m_tilesets.clear();
	p_tileset_size.clear();
	p_tileset_start.clear();

	for (std::size_t i{ 0 }; i < l_tileset_count; ++i) {
		auto l_tileset{ l_hud_tiles };

		auto l_local_addr_lo{ l_tileset_to_addr + 2 * i };
		auto l_local_addr_hi{ l_tileset_to_addr + 2 * i + 1 };
		std::size_t l_local_addr{
			256 * static_cast<std::size_t>(m_rom_data.at(l_local_addr_hi)) +
			static_cast<std::size_t>(m_rom_data.at(l_local_addr_lo)) +
			l_chr_wtile_offset - 0x8000
		};

		std::size_t l_tileset_start{ m_rom_data.at(l_tileset_chr_ppu_start_offset + i) };
		l_tileset_start *= 0x100;
		l_tileset_start -= 0x1000;
		l_tileset_start /= 0x10;

		if (l_tileset_start > 256)
			throw std::exception("Invalid tile index");

		p_tileset_start.push_back(l_tileset_start);

		std::size_t l_tile_count{ m_rom_data.at(l_tileset_chr_count_offset + i) };
		l_tile_count *= 0x10;

		p_tileset_size.push_back(l_tile_count);

		while (l_tileset.size() < l_tileset_start)
			l_tileset.push_back(klib::NES_tile());

		for (std::size_t tidx{ 0 }; tidx < l_tile_count; ++tidx)
			l_tileset.push_back(klib::NES_tile(m_rom_data,
				l_local_addr + 16 * tidx));

		while (l_tileset.size() < 256)
			l_tileset.push_back(klib::NES_tile());

		m_tilesets.push_back(l_tileset);
	}

}

std::size_t fe::Game::get_pointer_address(std::size_t p_offset,
	std::size_t p_zero_addr_rom_offset) const {

	std::size_t l_value{
	static_cast<std::size_t>(m_rom_data.at(p_offset + 1)) * 256 +
		static_cast<std::size_t>(m_rom_data.at(p_offset))
	};

	std::size_t l_total_offset{
		p_zero_addr_rom_offset == 0 ?
		0x10 + (p_offset / 0x4000) * 0x4000 :
		p_zero_addr_rom_offset
	};

	return l_total_offset + l_value;
}

std::vector<std::size_t> fe::Game::get_screen_pointers(std::size_t p_world_ptr) const {
	std::vector<std::size_t> l_result;

	std::size_t l_ptr{ get_pointer_address(p_world_ptr) };
	std::size_t l_start_dest_offset{ get_pointer_address(l_ptr) };
	std::size_t l_offset{ l_start_dest_offset };

	// we don't know a priori how many screens are defined
	// but the screen layout data begins immediately after the pointer table
	while (l_ptr < l_start_dest_offset) {
		l_result.push_back(l_offset);
		l_ptr += 2;
		l_offset = get_pointer_address(l_ptr);
	}

	return l_result;
}

std::vector<std::size_t> fe::Game::get_screen_pointers(const std::vector<std::size_t>& p_offsets, std::size_t p_chunk_no) const {
	return get_screen_pointers(p_offsets[p_chunk_no]);
	/*
	std::vector<std::size_t> l_result;

	std::size_t l_ptr{ get_pointer_address(p_offsets[p_chunk_no]) };
	std::size_t l_start_dest_offset{ get_pointer_address(l_ptr) };
	std::size_t l_offset{ l_start_dest_offset };

	// we don't know a priori how many screens are defined
	// but the screen layout data begins immediately after the pointer table
	while (l_ptr < l_start_dest_offset) {
		l_result.push_back(l_offset);
		l_ptr += 2;
		l_offset = get_pointer_address(l_ptr);
	}

	return l_result;
	*/
}

void fe::Game::set_various(const fe::Config& p_config, std::size_t p_chunk_no) {
	auto l_md_ptr{ p_config.pointer(c::ID_METADATA_PTR) };

	// get address from the 16-bit metatable relating to the chunk we want
	std::size_t l_table_offset{ get_pointer_address(l_md_ptr.first) + 2 * p_chunk_no };
	// go to the metadata pointer table for our chunk
	std::size_t l_chunk_offset{ get_pointer_address(l_table_offset) };

	// metadata, 2 bytes: points 2 bytes forward - but why?
	std::size_t l_chunk_attributes{ get_pointer_address(l_chunk_offset) };
	// properties per metatile: #metatile bytes
	std::size_t l_block_properties{ get_pointer_address(l_chunk_offset + 2) };
	// 4 bytes per screen, defining which screen each edge scrolls to
	// order: left, right, up, down
	std::size_t l_chunk_scroll_data{ get_pointer_address(l_chunk_offset + 4) };
	// 4 bytes per door: screen id, yx-coords, byte in door dest data to use?, exit yx-coords
	std::size_t l_chunk_door_data{ get_pointer_address(l_chunk_offset + 6) };
	// door destination screen ids? referenced to from the previous data?
	std::size_t l_chunk_door_dest_data{ get_pointer_address(l_chunk_offset + 8) };
	// palettes, 1 byte per metatile defining its palette - 2 bits per quadrant
	std::size_t l_chunk_palette_attr{ get_pointer_address(l_chunk_offset + 10) };

	std::size_t l_tsa_top_left{ get_pointer_address(l_chunk_offset + 12) };
	std::size_t l_tsa_top_right{ get_pointer_address(l_chunk_offset + 14) };
	std::size_t l_tsa_bottom_left{ get_pointer_address(l_chunk_offset + 16) };
	std::size_t l_tsa_bottom_right{ get_pointer_address(l_chunk_offset + 18) };

	// the meta-tile definition counts seem to vary between chunks
	std::size_t l_metatile_count{ l_tsa_top_right - l_tsa_top_left };

	m_chunks[p_chunk_no].set_screen_scroll_properties(m_rom_data, l_chunk_scroll_data);
	m_chunks[p_chunk_no].add_metatiles(m_rom_data, l_metatile_count, l_tsa_top_left, l_tsa_top_right, l_tsa_bottom_left, l_tsa_bottom_right, l_chunk_palette_attr, l_block_properties);

	// the doors for the town chunk offsets the index by 0x20 (hard coded game logic)
	// probably to save space since all the doors there go to buildings
	m_chunks[p_chunk_no].set_screen_doors(m_rom_data, l_chunk_door_data, l_chunk_door_dest_data,
		p_chunk_no == c::CHUNK_IDX_TOWNS ? 0x20 : 0x00);
}

void fe::Game::set_sprites(std::size_t p_chunk_no,
	std::pair<std::size_t, std::size_t> pt_to_sprites) {

	std::size_t l_ptr_to_screens{ get_pointer_address(pt_to_sprites.first + 2 * p_chunk_no, pt_to_sprites.second) };

	for (std::size_t i{ 0 }; i < m_chunks.at(p_chunk_no).m_screens.size(); ++i) {
		std::size_t l_ptr_to_screen{ get_pointer_address(l_ptr_to_screens + 2 * i, pt_to_sprites.second) };

		m_chunks[p_chunk_no].m_screens[i].m_sprite_set =
			extract_sprite_set(m_rom_data, l_ptr_to_screen);
	}
}

void fe::Game::set_intrachunk_scrolling(const fe::Config& p_config, std::size_t p_chunk_no) {
	auto l_otherworld_ptr{ p_config.pointer(c::ID_OTHERWORLD_TRANS_PTR) };
	std::size_t l_ptr_to_screens{ get_pointer_address(l_otherworld_ptr.first + 2 * p_chunk_no, l_otherworld_ptr.second) };

	for (std::size_t i{ 0 }; m_rom_data.at(l_ptr_to_screens + i) != 0xff; i += 5) {
		byte l_screen_id{ m_rom_data.at(l_ptr_to_screens + i) };

		m_chunks.at(p_chunk_no).m_screens.at(l_screen_id).m_intrachunk_scroll =
			fe::IntraChunkScroll(
				m_rom_data.at(l_ptr_to_screens + i + 1),
				m_rom_data.at(l_ptr_to_screens + i + 2),
				m_rom_data.at(l_ptr_to_screens + i + 3),
				m_rom_data.at(l_ptr_to_screens + i + 4)
			);
	}

}

void fe::Game::set_interchunk_scrolling(const fe::Config& p_config, std::size_t p_chunk_no) {
	auto l_sameworld_ptr{ p_config.pointer(c::ID_SAMEWORLD_TRANS_PTR) };
	std::size_t l_ptr_to_screens{ get_pointer_address(l_sameworld_ptr.first + 2 * p_chunk_no, l_sameworld_ptr.second) };

	for (std::size_t i{ 0 }; m_rom_data.at(l_ptr_to_screens + i) != 0xff; i += 4) {
		byte l_screen_id{ m_rom_data.at(l_ptr_to_screens + i) };

		m_chunks.at(p_chunk_no).m_screens.at(l_screen_id).m_interchunk_scroll =
			fe::InterChunkScroll(
				m_rom_data.at(l_ptr_to_screens + i + 1),
				m_rom_data.at(l_ptr_to_screens + i + 2),
				m_rom_data.at(l_ptr_to_screens + i + 3)
			);
	}
}

// find all guru (with spawn point) door entrances
// and update the spawn location with the door data
int fe::Game::calculate_spawn_locations_by_guru(void) {
	int l_cnt{ 0 };

	std::map<byte, byte> script_to_spawn;
	// reverse the map
	for (const auto& kv : m_spawn_to_script_no)
		script_to_spawn[kv.second] = kv.first;

	for (std::size_t c{ 0 }; c < m_chunks.size(); ++c)
		for (std::size_t s{ 0 }; s < m_chunks[c].m_screens.size(); ++s)
			for (const auto& l_door : m_chunks[c].m_screens[s].m_doors) {
				if (l_door.m_door_type == fe::DoorType::Building) {
					const auto& spriteset{ m_npc_bundles.at(l_door.m_npc_bundle) };

					// check if any sprite in this sprite set sets a spawn point
					bool l_sprite_found{ false };
					byte l_spawn_no{ 0 };

					for (const auto& sprite : spriteset.m_sprites)
						if (sprite.m_text_id.has_value() &&
							script_to_spawn.find(sprite.m_text_id.value()) !=
							end(script_to_spawn)) {
							l_sprite_found = true;
							l_spawn_no = script_to_spawn[sprite.m_text_id.value()];
							break;
						}

					// if we match, update the spawn points vector with the chunk no,
					// screen no and door location
					if (l_sprite_found) {
						std::optional<byte> l_stage_id;

						const auto l_stage_idx{ m_stages.get_stage_idx_from_world(c) };

						if (l_stage_idx.has_value())
							l_stage_id = static_cast<byte>(l_stage_idx.value());
						else {
							// try to deduce the stage id if the world to stage is null
							const auto& scr{ m_chunks[c].m_screens };

							// traverse the area left and right until we get an other-world transition
							// then get the exit world from the transition and remap to stage
							// TODO: Do a proper flood-fill search
							std::size_t l_screen_left{ s };
							std::size_t l_screen_right{ s };
							int l_loops{ 0 };

							do {
								++l_loops;
								if (scr[l_screen_left].m_intrachunk_scroll.has_value()) {
									std::size_t l_world_no{
									static_cast<std::size_t>(scr[l_screen_left].m_intrachunk_scroll.value().m_dest_chunk)
									};

									auto l_left_stage{ m_stages.get_stage_idx_from_world(l_world_no) };
									if (l_left_stage.has_value())
										l_stage_id = static_cast<byte>(l_left_stage.value());
									break;
								}
								else if (scr[l_screen_left].m_scroll_right.has_value())
									l_screen_left = scr[l_screen_left].m_scroll_left.value();

							} while (scr[l_screen_left].m_scroll_left.has_value() && l_loops < 50);

							if (!l_stage_id.has_value()) {
								do {
									++l_loops;
									if (scr[l_screen_right].m_intrachunk_scroll.has_value()) {
										std::size_t l_world_no{
										static_cast<std::size_t>(scr[l_screen_right].m_intrachunk_scroll.value().m_dest_chunk)
										};

										auto l_right_stage{ m_stages.get_stage_idx_from_world(l_world_no) };
										if (l_right_stage.has_value())
											l_stage_id = static_cast<byte>(l_right_stage.value());
										break;
									}
									else if (scr[l_screen_right].m_scroll_right.has_value())
										l_screen_right = scr[l_screen_right].m_scroll_right.value();

								} while (scr[l_screen_right].m_scroll_right.has_value() && l_loops < 50);
							}

						}

						if (l_stage_id.has_value()) {
							m_spawn_locations.at(l_spawn_no) = fe::Spawn_location(
								static_cast<byte>(c),
								static_cast<byte>(s),
								l_stage_id.value(),
								l_door.m_coords.first,
								l_door.m_coords.second,
								l_door.m_npc_bundle
							);
							++l_cnt;
						}
					}

				}
			}

	return l_cnt;
}

bool fe::Game::calculate_push_block_parameters(void) {
	bool l_ok{ false };

	for (std::size_t w{ 0 }; w < m_chunks.size(); ++w) {
		const auto& stage{ m_stages.get_stage_idx_from_world(w) };

		// if this world is uniquely mapped to by a stage we can possibly match
		if (stage.has_value()) {
			const auto& world{ m_chunks[w] };

			for (std::size_t s{ 0 }; s < world.m_screens.size(); ++s) {
				const auto& screen{ world.m_screens[s] };

				for (std::size_t y{ 0 }; y < 12; ++y)
					for (std::size_t x{ 0 }; x < 15; ++x) {
						// we have two push-blocks on top of each other here
						if (world.m_metatiles.at(screen.get_mt_at_pos(x, y)).m_block_property == 0x06 &&
							world.m_metatiles.at(screen.get_mt_at_pos(x, y + 1)).m_block_property == 0x06) {

							m_push_block.m_stage = static_cast<byte>(stage.value());
							m_push_block.m_cover_x = static_cast<byte>(x);
							m_push_block.m_cover_y = static_cast<byte>(y);
							m_push_block.m_screen = static_cast<byte>(s);
							m_push_block.m_source_0 = screen.get_mt_at_pos(x + 1, y);
							m_push_block.m_source_1 = screen.get_mt_at_pos(x + 1, y + 1);
							m_push_block.m_target_0 = screen.get_mt_at_pos(x, y);
							m_push_block.m_target_1 = screen.get_mt_at_pos(x, y + 1);
							l_ok = true;
							break;
						}
					}
			}
		}
	}

	return l_ok;
}

// make sure there are no references to any metatiles in p_mt_to_delete
void fe::Game::delete_metatiles(std::size_t p_chunk_no,
	const std::unordered_set<byte>& p_mt_to_delete) {
	auto& l_chunk{ m_chunks.at(p_chunk_no) };
	auto& l_mt{ l_chunk.m_metatiles };

	// Step 1: Build index remapping
	std::vector<int> remap(l_mt.size(), -1);
	int new_index = 0;

	for (size_t old_index{ 0 }; old_index < l_mt.size(); ++old_index) {
		if (p_mt_to_delete.count(static_cast<byte>(old_index)) == 0) {
			remap[old_index] = new_index++;
		}
	}

	// Step 2: Remove metatiles
	std::vector<fe::Metatile> new_metatiles;
	for (size_t i = 0; i < l_mt.size(); ++i) {
		if (remap[i] != -1) {
			new_metatiles.push_back(l_mt[i]);
		}
	}

	l_mt = std::move(new_metatiles);

	// re-index screen tilemaps
	for (auto& scr : l_chunk.m_screens)
		for (auto& row : scr.m_tilemap)
			for (byte& b : row)
				b = static_cast<byte>(remap[b]);

	// re-index mattock animation
	for (byte& b : l_chunk.m_mattock_animation)
		b = static_cast<byte>(remap[b]);

	// if the push-block happens on this world, make sure we re-index the tiles involved
	if (m_push_block.m_stage == static_cast<byte>(p_chunk_no)) {
		m_push_block.m_draw_block = static_cast<byte>(remap[m_push_block.m_draw_block]);
		m_push_block.m_source_0 = static_cast<byte>(remap[m_push_block.m_source_0]);
		m_push_block.m_source_1 = static_cast<byte>(remap[m_push_block.m_source_1]);
		m_push_block.m_target_0 = static_cast<byte>(remap[m_push_block.m_target_0]);
		m_push_block.m_target_1 = static_cast<byte>(remap[m_push_block.m_target_1]);
	}

}

void fe::Game::delete_screens(std::size_t p_chunk_no, const std::unordered_set<byte>& p_scr_to_delete) {
	auto& l_chunk{ m_chunks.at(p_chunk_no) };
	auto& lr_scr{ l_chunk.m_screens };

	// Step 1: Build index remapping
	std::vector<int> remap(lr_scr.size(), -1);
	int new_index = 0;

	for (size_t old_index{ 0 }; old_index < lr_scr.size(); ++old_index) {
		if (p_scr_to_delete.count(static_cast<byte>(old_index)) == 0) {
			remap[old_index] = new_index++;
		}
	}

	// Step 2: Remove screens
	std::vector<fe::Screen> new_screens;
	for (size_t i = 0; i < lr_scr.size(); ++i) {
		if (remap[i] != -1) {
			new_screens.push_back(lr_scr[i]);
		}
	}

	lr_scr = std::move(new_screens);

	// re-index screens

	// game metadata - push blocks
	if (m_stages.m_stages[m_push_block.m_stage].m_world_id == p_chunk_no)
		m_push_block.m_screen = remap[m_push_block.m_screen];

	// spawn locations
	for (std::size_t i{ 0 }; i < 8; ++i)
		if (m_spawn_locations.at(i).m_world == p_chunk_no)
			m_spawn_locations[i].m_screen = remap[m_spawn_locations[i].m_screen];

	// stage screens
	for (auto& l_stage : m_stages.m_stages) {
		if (m_stages.m_stages[l_stage.m_next_stage].m_world_id == p_chunk_no)
			l_stage.m_next_screen = remap[l_stage.m_next_screen];
		if (m_stages.m_stages[l_stage.m_prev_stage].m_world_id == p_chunk_no)
			l_stage.m_prev_screen = remap[l_stage.m_prev_screen];
	}

	// start screen
	if (m_stages.m_stages[0].m_world_id == p_chunk_no)
		m_stages.m_start_screen = remap[m_stages.m_start_screen];

	for (std::size_t c{ 0 }; c < 8; ++c) {
		auto& l_scr{ m_chunks[c].m_screens };
		for (std::size_t s{ 0 }; s < l_scr.size(); ++s) {

			// for current chunk screens check scrolling and sameworld-transitions
			if (c == p_chunk_no) {

				if (l_scr[s].m_scroll_left.has_value())
					l_scr[s].m_scroll_left = remap[l_scr[s].m_scroll_left.value()];
				if (l_scr[s].m_scroll_right.has_value())
					l_scr[s].m_scroll_right = remap[l_scr[s].m_scroll_right.value()];
				if (l_scr[s].m_scroll_up.has_value())
					l_scr[s].m_scroll_up = remap[l_scr[s].m_scroll_up.value()];
				if (l_scr[s].m_scroll_down.has_value())
					l_scr[s].m_scroll_down = remap[l_scr[s].m_scroll_down.value()];

				// re-index same-world door destinations
				for (std::size_t d{ 0 }; d < l_scr[s].m_doors.size(); ++d) {
					if (l_scr[s].m_doors[d].m_door_type == fe::DoorType::SameWorld)
						l_scr[s].m_doors[d].m_dest_screen_id = remap[l_scr[s].m_doors[d].m_dest_screen_id];
				}

				// re-index same-world transitions
				if (l_scr[s].m_interchunk_scroll.has_value()) {
					l_scr[s].m_interchunk_scroll.value().m_dest_screen =
						remap[l_scr[s].m_interchunk_scroll.value().m_dest_screen];
				}

			}

			// re-index same-world transitions
			if (l_scr[s].m_intrachunk_scroll.has_value() &&
				l_scr[s].m_intrachunk_scroll.value().m_dest_chunk == p_chunk_no) {
				l_scr[s].m_intrachunk_scroll.value().m_dest_screen =
					remap[l_scr[s].m_intrachunk_scroll.value().m_dest_screen];
			}
		}

	}
}

std::set<byte> fe::Game::get_referenced_metatiles(std::size_t p_chunk_no) const {
	std::set<byte> l_result;
	const auto& l_chunk{ m_chunks.at(p_chunk_no) };

	// screen tilemaps refer to metatiles of course
	for (const auto& scr : l_chunk.m_screens)
		for (const auto& row : scr.m_tilemap)
			for (byte b : row)
				l_result.insert(b);

	// mattock animations refer to 4 metatiles
	for (byte b : l_chunk.m_mattock_animation)
		l_result.insert(b);

	// the push-block happens on this world, make sure we re-index the tiles involved
	if (m_push_block.m_stage == static_cast<byte>(p_chunk_no)) {
		l_result.insert(m_push_block.m_draw_block);
		l_result.insert(m_push_block.m_source_0);
		l_result.insert(m_push_block.m_source_1);
		l_result.insert(m_push_block.m_target_0);
		l_result.insert(m_push_block.m_target_1);
	}

	return l_result;
}

std::set<byte> fe::Game::get_referenced_screens(std::size_t p_chunk_no) const {
	std::set<byte> l_result;

	// handle metadata

	// push-block
	if (m_push_block.m_stage == p_chunk_no)
		l_result.insert(m_push_block.m_screen);

	// spawn locations
	for (std::size_t i{ 0 }; i < 8; ++i)
		if (m_spawn_locations.at(i).m_world == p_chunk_no)
			l_result.insert(m_spawn_locations[i].m_screen);

	// stage screens
	for (const auto& l_stage : m_stages.m_stages) {
		if (m_stages.m_stages[l_stage.m_next_stage].m_world_id == static_cast<byte>(p_chunk_no))
			l_result.insert(static_cast<byte>(l_stage.m_next_screen));
		if (m_stages.m_stages[l_stage.m_prev_stage].m_world_id == static_cast<byte>(p_chunk_no))
			l_result.insert(static_cast<byte>(l_stage.m_prev_screen));
	}

	// start screen
	if (m_stages.m_stages[0].m_world_id == p_chunk_no)
		l_result.insert(static_cast<byte>(m_stages.m_start_screen));

	for (std::size_t i{ 0 }; i < 8; ++i) {

		const auto& l_scr{ m_chunks[i].m_screens };

		for (std::size_t s{ 0 }; s < l_scr.size(); ++s) {

			if (i == p_chunk_no) {
				// handle screens in the same world

				// scrolling refs
				if (l_scr[s].m_scroll_left.has_value())
					l_result.insert(l_scr[s].m_scroll_left.value());
				if (l_scr[s].m_scroll_right.has_value())
					l_result.insert(l_scr[s].m_scroll_right.value());
				if (l_scr[s].m_scroll_up.has_value())
					l_result.insert(l_scr[s].m_scroll_up.value());
				if (l_scr[s].m_scroll_down.has_value())
					l_result.insert(l_scr[s].m_scroll_down.value());

				// same-world transition ref
				if (l_scr[s].m_interchunk_scroll.has_value())
					l_result.insert(l_scr[s].m_interchunk_scroll.value().m_dest_screen);

				// door refs - only need to check same-world
				// we don't delete building screens, and other-world doors
				// are defined in chunk metadata
				for (std::size_t d{ 0 }; d < l_scr[s].m_doors.size(); ++d)
					if (l_scr[s].m_doors[d].m_door_type == fe::DoorType::SameWorld)
						l_result.insert(l_scr[s].m_doors[d].m_dest_screen_id);
			}

			// other-world transitions - could possibly come from the same world
			// if user wants to waste a byte
			if (l_scr[s].m_intrachunk_scroll.has_value() &&
				l_scr[s].m_intrachunk_scroll.value().m_dest_chunk == i)
				l_result.insert(l_scr[s].m_intrachunk_scroll.value().m_dest_screen);
		}

	}

	return l_result;
}

bool fe::Game::is_metatile_referenced(std::size_t p_chunk_no,
	std::size_t p_metatile_no) const {
	const auto l_refs{ get_referenced_metatiles(p_chunk_no) };

	return l_refs.find(static_cast<byte>(p_metatile_no)) != end(l_refs);
}

bool fe::Game::is_screen_referenced(std::size_t p_chunk_no, std::size_t p_screen_no) const {
	const auto l_refs{ get_referenced_screens(p_chunk_no) };

	return l_refs.find(static_cast<byte>(p_screen_no)) != end(l_refs);
}

std::size_t fe::Game::delete_unreferenced_metatiles(std::size_t p_chunk_no) {
	auto& l_chunk{ m_chunks.at(p_chunk_no) };
	std::unordered_set<byte> mts_to_delete;
	auto l_refs{ get_referenced_metatiles(p_chunk_no) };

	for (std::size_t i{ 0 }; i < l_chunk.m_metatiles.size(); ++i)
		if (l_refs.find(static_cast<byte>(i)) == end(l_refs))
			mts_to_delete.insert(static_cast<byte>(i));

	delete_metatiles(p_chunk_no, mts_to_delete);

	return mts_to_delete.size();
}

std::size_t fe::Game::delete_unreferenced_screens(std::size_t p_chunk_no) {
	auto& l_chunk{ m_chunks.at(p_chunk_no) };
	std::unordered_set<byte> scrs_to_delete;
	auto l_refs{ get_referenced_screens(p_chunk_no) };

	for (std::size_t i{ 0 }; i < l_chunk.m_screens.size(); ++i)
		if (l_refs.find(static_cast<byte>(i)) == end(l_refs))
			scrs_to_delete.insert(static_cast<byte>(i));

	delete_screens(p_chunk_no, scrs_to_delete);

	return scrs_to_delete.size();
}

fe::Sprite_set fe::Game::extract_sprite_set(const std::vector<byte>& p_rom_data, std::size_t p_offset) const {
	fe::Sprite_set l_result;

	std::size_t l_offset{ p_offset };

	// firstly: extract sprite data
	while (m_rom_data.at(l_offset) != 0xff) {
		byte l_id{ m_rom_data.at(l_offset) };
		byte l_y{ static_cast<byte>(m_rom_data.at(l_offset + 1) / 16) };
		byte l_x{ static_cast<byte>(m_rom_data.at(l_offset + 1) % 16) };

		l_result.m_sprites.push_back(fe::Sprite(l_id, l_x, l_y));

		l_offset += 2;
	}

	// secondly: extract sprite text data
	std::size_t l_sprite_no{ 0 };

	while (p_rom_data.at(++l_offset) != 0xff) {

		byte l_text_id{ m_rom_data.at(l_offset) };

		// there is one entry in the original data set
		// that is missing a delimiter after the text bytes so check vector size
		if (l_sprite_no < l_result.size())
			l_result.m_sprites[l_sprite_no++].m_text_id = l_text_id;
	}

	// finally the command byte which is stored with the sprite data for some reason
	if (m_rom_data.at(l_offset + 1) == 0x80)
		l_result.m_command_byte = m_rom_data.at(l_offset + 2);

	return l_result;
}

std::size_t fe::Game::get_default_tileset_no(std::size_t p_chunk_no, std::size_t p_screen_no) const {
	if (p_chunk_no == c::CHUNK_IDX_BUILDINGS)
		return m_building_scenes.at(p_screen_no).m_tileset;
	else
		return m_chunks.at(p_chunk_no).m_scene.m_tileset;
}

std::size_t fe::Game::get_default_palette_no(std::size_t p_chunk_no, std::size_t p_screen_no) const {
	if (p_chunk_no == c::CHUNK_IDX_BUILDINGS)
		return m_building_scenes.at(p_screen_no).m_palette;
	else
		return m_chunks.at(p_chunk_no).m_scene.m_palette;
}

void fe::Game::extract_scenes_if_empty(const fe::Config& p_config) {

	// set building scenes
	while (m_building_scenes.size() < c::WORLD_BUILDINGS_SCREEN_COUNT)
		m_building_scenes.push_back(fe::Scene());

	std::size_t l_bscene_start{ p_config.constant(c::ID_BUILDING_SCENE_OFFSET) };

	for (std::size_t i{ 0 }; i < m_building_scenes.size(); ++i) {
		if (m_building_scenes[i].m_palette == 256)
			m_building_scenes[i].m_palette = m_rom_data.at(l_bscene_start + c::WORLD_BUILDINGS_SCREEN_COUNT + i);
		if (m_building_scenes[i].m_tileset == 256)
			m_building_scenes[i].m_tileset = m_rom_data.at(l_bscene_start + 2 * c::WORLD_BUILDINGS_SCREEN_COUNT + i);
		if (m_building_scenes[i].m_music == 256)
			m_building_scenes[i].m_music = m_rom_data.at(l_bscene_start + i);
		if (m_building_scenes[i].m_x >= 16) {
			byte pos{ m_rom_data.at(l_bscene_start + 3 * c::WORLD_BUILDINGS_SCREEN_COUNT + i) };
			m_building_scenes[i].m_x = pos % 16;
			m_building_scenes[i].m_y = pos / 16;
		}
	}

	// set scene values for each world
	std::size_t l_wscene_start{ p_config.constant(c::ID_WORLD_SCENE_OFFSET) };

	for (std::size_t i{ 0 }; i < m_chunks.size(); ++i) {
		if (m_chunks[i].m_scene.m_tileset == 256)
			m_chunks[i].m_scene.m_tileset = m_rom_data.at(l_wscene_start + i);
		if (m_chunks[i].m_scene.m_palette == 256)
			m_chunks[i].m_scene.m_palette = m_rom_data.at(l_wscene_start + 8 + i);
		if (m_chunks[i].m_scene.m_x >= 16) {
			byte pos{ m_rom_data.at(l_wscene_start + 16 + i) };
			m_chunks[i].m_scene.m_x = pos % 16;
			m_chunks[i].m_scene.m_y = pos / 16;
		}
		if (m_chunks[i].m_scene.m_music == 256)
			m_chunks[i].m_scene.m_music = m_rom_data.at(l_wscene_start + 24 + i);
	}
}

void fe::Game::extract_palette_to_music(const fe::Config& p_config) {
	auto l_slots{ p_config.constant(c::ID_PALETTE_TO_MUSIC_SLOTS) };
	auto l_slot_offset{ p_config.constant(c::ID_PALETTE_TO_MUSIC_OFFSET) };
	std::size_t l_start{ m_pal_to_music.m_slots.size() };

	for (std::size_t i{ l_start }; i < l_slots; ++i)
		m_pal_to_music.m_slots.push_back(fe::PaletteMusicSlot(
			m_rom_data.at(l_slot_offset + i),
			m_rom_data.at(l_slot_offset + l_slots + i)
		));
}

void fe::Game::extract_fog_parameters(const fe::Config& p_config) {
	m_fog.m_world_no = m_rom_data.at(
		p_config.constant(c::ID_FOG_WORLD_OFFSET)
	);
	m_fog.m_palette_no = m_rom_data.at(
		p_config.constant(c::ID_FOG_PALETTE_OFFSET)
	);
}

void fe::Game::extract_hud_attributes(const fe::Config& p_config) {
	std::size_t l_palette_count{ p_config.constant(c::ID_PALETTE_COUNT) };
	std::size_t l_hud_attribute_count{ p_config.constant(c::ID_HUD_ATTRIBUTE_LOOKUP_COUNT) };
	std::size_t l_hud_attribute_offset{ p_config.constant(c::ID_HUD_ATTRIBUTE_LOOKUP_OFFSET) };
	std::size_t l_attr_to_hud_idx_offset{
		p_config.constant(c::ID_PALETTE_OFFSET) +
		16 * l_palette_count
	};

	for (std::size_t i{ 0 };
		m_hud_attributes.m_palette_to_hud_idx.size() < l_palette_count; ++i)
		m_hud_attributes.m_palette_to_hud_idx.push_back(m_rom_data.at(l_attr_to_hud_idx_offset + i));

	for (std::size_t i{ 0 };
		m_hud_attributes.m_hud_attributes.size() < l_hud_attribute_count; ++i)
		m_hud_attributes.m_hud_attributes.push_back(
			fe::PaletteAttribute(m_rom_data.at(l_hud_attribute_offset + i))
		);
}

void fe::Game::initialize_game_gfx_metadata(const fe::Config& p_config) {
	m_game_gfx.clear();

	m_game_gfx.push_back(fe::GameGfxTilemap(
		"Title Screen",
		p_config.constant(c::ID_TITLE_TILEMAP_MT_X),
		p_config.constant(c::ID_TITLE_TILEMAP_MT_Y),
		p_config.constant(c::ID_TITLE_TILEMAP_MT_W),
		p_config.constant(c::ID_TITLE_TILEMAP_MT_H),
		p_config.constant(c::ID_TITLE_TILEMAP_OFFSET),
		p_config.constant(c::ID_TITLE_CHR_OFFSET),
		p_config.constant(c::ID_TITLE_CHR_PPU_INDEX),
		p_config.constant(c::ID_TITLE_CHR_PPU_COUNT),
		true,
		true, true,
		p_config.constant(c::ID_TITLE_ATTRIBUTE_OFFSET),
		p_config.constant(c::ID_TITLE_PALETTE_OFFSET),
		true,
		p_config.constant(c::ID_ALPHABET_CHR_OFFSET),
		p_config.constant(c::ID_NUMERIC_CHR_OFFSET)
	));

	m_game_gfx.push_back(fe::GameGfxTilemap(
		"Intro Animation Screen",
		p_config.constant(c::ID_INTRO_ANIM_TILEMAP_MT_X),
		p_config.constant(c::ID_INTRO_ANIM_TILEMAP_MT_Y),
		p_config.constant(c::ID_INTRO_ANIM_TILEMAP_MT_W),
		p_config.constant(c::ID_INTRO_ANIM_TILEMAP_MT_H),
		p_config.constant(c::ID_INTRO_ANIM_TILEMAP_OFFSET),
		p_config.constant(c::ID_INTRO_ANIM_CHR_OFFSET),
		p_config.constant(c::ID_INTRO_ANIM_CHR_PPU_INDEX),
		p_config.constant(c::ID_INTRO_ANIM_CHR_PPU_COUNT),
		false,
		true, true,
		p_config.constant(c::ID_INTRO_ANIM_ATTRIBUTE_OFFSET),
		p_config.constant(c::ID_INTRO_ANIM_PALETTE_OFFSET)
	));

	m_game_gfx.push_back(fe::GameGfxTilemap(
		"Outro Animation Screen",
		p_config.constant(c::ID_OUTRO_ANIM_TILEMAP_MT_X),
		p_config.constant(c::ID_OUTRO_ANIM_TILEMAP_MT_Y),
		p_config.constant(c::ID_OUTRO_ANIM_TILEMAP_MT_W),
		p_config.constant(c::ID_OUTRO_ANIM_TILEMAP_MT_H),
		p_config.constant(c::ID_OUTRO_ANIM_TILEMAP_OFFSET),
		p_config.constant(c::ID_OUTRO_ANIM_CHR_OFFSET),
		p_config.constant(c::ID_OUTRO_ANIM_CHR_PPU_INDEX),
		p_config.constant(c::ID_OUTRO_ANIM_CHR_PPU_COUNT),
		false,
		true, true,
		p_config.constant(c::ID_OUTRO_ANIM_ATTRIBUTE_OFFSET),
		p_config.constant(c::ID_OUTRO_ANIM_PALETTE_OFFSET)
	));

	m_game_gfx.push_back(fe::GameGfxTilemap(
		"Items (background gfx)",
		p_config.constant(c::ID_ITEM_VSCREEN_TILEMAP_MT_X),
		p_config.constant(c::ID_ITEM_VSCREEN_TILEMAP_MT_Y),
		p_config.constant(c::ID_ITEM_VSCREEN_TILEMAP_MT_W),
		p_config.constant(c::ID_ITEM_VSCREEN_TILEMAP_MT_H),
		p_config.constant(c::ID_ITEM_VSCREEN_TILEMAP_OFFSET),
		p_config.constant(c::ID_ITEM_VSCREEN_CHR_OFFSET),
		p_config.constant(c::ID_ITEM_VSCREEN_CHR_PPU_INDEX),
		p_config.constant(c::ID_ITEM_VSCREEN_CHR_PPU_COUNT),
		false,
		false, false));
}

fe::Fog::Fog(byte p_world_no,
	byte p_palette_no) :
	m_world_no{ p_world_no },
	m_palette_no{ p_palette_no }
{
}

fe::Fog::Fog(void) :
	m_world_no{ 2 },
	m_palette_no{ 10 }
{
}

fe::PaletteAttribute::PaletteAttribute(byte p_tl, byte p_tr,
	byte p_bl, byte p_br) :
	m_tl{ p_tl },
	m_tr{ p_tr },
	m_bl{ p_bl },
	m_br{ p_br }
{
}


fe::PaletteAttribute::PaletteAttribute(byte b) {
	m_tl = (b >> 0) & 0x03; // bits 0–1
	m_tr = (b >> 2) & 0x03; // bits 2–3
	m_bl = (b >> 4) & 0x03; // bits 4–5
	m_br = (b >> 6) & 0x03; // bits 6–7
}

byte fe::PaletteAttribute::to_byte(void) const {
	byte result{ 0 };
	result |= (m_tl & 0x03) << 0; // bits 0–1
	result |= (m_tr & 0x03) << 2; // bits 2–3
	result |= (m_bl & 0x03) << 4; // bits 4–5
	result |= (m_br & 0x03) << 6; // bits 6–7
	return result;
}
