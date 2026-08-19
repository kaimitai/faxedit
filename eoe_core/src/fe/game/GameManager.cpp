#include "GameManager.h"
#include "fe/ROM_Manager.h"
#include "fe/fe_constants.h"
#include "fe/sprite/fe_sprite_constants.h"
#include "common/klib/Kfile.h"
#include <format>

fe::game::LoadedGame fe::game::load_rom(
	const std::vector<byte>& p_rom,
	const Config& p_config,
	const MessageCallback& p_message) {

	ROM_Manager rom_manager;
	Game game{ p_config, p_rom };
	game.m_sprite_gfx_manager.load_rom(p_config, game.m_rom_data, rom_manager);
	game.generate_tilesets(p_config);
	validate_and_repair_game(game, p_message);

	return { p_config, std::move(game) };
}

fe::game::LoadedGame fe::game::load_rom(
	const std::vector<byte>& p_rom,
	const std::string& p_config_path,
	const std::string& p_config_override_path,
	const std::string& p_region,
	const MessageCallback& p_message) {
	Config config(p_config_path, p_config_override_path, p_rom, p_region);

	if (p_region.empty())
		send_message(p_message, { std::format("ROM region detected: '{}'", config.get_region()) });
	else
		send_message(p_message, { std::format("Region specified as '{}'", p_region) });

	return load_rom(p_rom, config, p_message);
}

fe::game::LoadedGame fe::game::load_rom(
	const std::string& p_rom_path,
	const std::string& p_config_path,
	const std::string& p_config_override_path,
	const std::string& p_region,
	const MessageCallback& p_message) {
	send_message(p_message, { std::format("Attempting to load file {}", p_rom_path) });
	return load_rom(klib::file::read_file_as_bytes(p_rom_path),
		p_config_path, p_config_override_path,
		p_region, p_message);
}

// analysis and validation
void fe::game::analyze_game_data(const Game& p_game, const Config& p_config, bool p_warn_tilemap_95_pct,
	bool p_warn_00_doors, const MessageCallback& p_message) {
	fe::ROM_Manager rom_mgr;
	const auto lc_sprite_types{ p_game.extract_sprite_types(p_config) };

	send_message(p_message, { "Starting integrity analysis", fe::MsgType::Info });

	// check tilemap sizes
	constexpr std::size_t BANK_SIZE{ 0x4000 };
	const auto tilemap_sizes{ rom_mgr.get_world_tilemap_sizes(p_game) };

	for (std::size_t w{ 0 }; w < tilemap_sizes.size(); ++w) {
		const auto l_wtm_size{ tilemap_sizes[w] };

		if (l_wtm_size > BANK_SIZE) {
			send_message(p_message,
				{ std::format("World {} tilemap consumes {}/{} bytes ({:.2f}% of one bank)",
					w, l_wtm_size, BANK_SIZE,
					100.0 * static_cast<double>(l_wtm_size) / static_cast<double>(BANK_SIZE)),
				fe::MsgType::Error });
		}
		else if (p_warn_tilemap_95_pct &&
			l_wtm_size * 100 >= BANK_SIZE * 95) {
			send_message(p_message,
				{ std::format("World {} tilemap consumes {}/{} bytes ({:.2f}% of one bank)",
					w, l_wtm_size, BANK_SIZE,
					100.0 * static_cast<double>(l_wtm_size) / static_cast<double>(BANK_SIZE)),
				fe::MsgType::Warning });
		}
	}

	for (std::size_t c{ 0 }; c < p_game.m_chunks.size(); ++c) {
		const auto l_ref_scr{ p_game.get_referenced_screens(c) };
		for (std::size_t s{ 0 }; s < p_game.m_chunks[c].m_screens.size(); ++s) {
			const auto& scr{ p_game.m_chunks[c].m_screens[s] };

			// check if screen is referenced
			if (c != c::CHUNK_IDX_BUILDINGS && l_ref_scr.find(static_cast<byte>(s)) == end(l_ref_scr))
				send_message(p_message, { std::format("World {}, Screen {} has no references", c, s), fe::MsgType::Error });

			// check that defined other-world transitions can be used
			if (scr.m_intrachunk_scroll.has_value()) {
				bool l_ow_block{ false };
				for (const auto& row : scr.m_tilemap)
					for (byte b : row) {
						byte l_block_prop{ p_game.m_chunks[c].m_metatiles.at(b).m_block_property };
						if (l_block_prop == c::BLOCK_PROPERTY_OW_FOREGROUND ||
							l_block_prop == c::BLOCK_PROPERTY_OW_RETURN) {
							l_ow_block = true;
							break;
						}
					}

				if (!l_ow_block)
					send_message(p_message, { std::format("World {}, Screen {}: Other-world transition is defined, but no metatile with property ow-transition is used",
						c, s), fe::MsgType::Error });
			}

			// check that defined same-world transitions can be used
			if (scr.m_interchunk_scroll.has_value()) {
				bool l_sw_block{ false };
				for (const auto& row : scr.m_tilemap)
					for (byte b : row) {
						byte l_block_prop{ p_game.m_chunks[c].m_metatiles.at(b).m_block_property };
						if (l_block_prop == c::BLOCK_PROPERTY_SW_LADDER ||
							l_block_prop == c::BLOCK_PROPERTY_SW_FOREGROUND) {
							l_sw_block = true;
							break;
						}
					}

				if (!l_sw_block)
					send_message(p_message, { std::format("World {}, Screen {}: Same-world transition is defined, but no metatile with property sw-transition is used",
						c, s), fe::MsgType::Error });
			}

			// check that doors are correctly placed
			std::set<std::pair<byte, byte>> unique_door_pos;
			std::size_t doorcnt{ scr.m_doors.size() };

			for (std::size_t d{ 0 }; d < doorcnt; ++d) {
				const auto& door{ scr.m_doors[d] };

				if (p_game.m_chunks[c].m_metatiles.at(
					scr.get_mt_at_pos(door.m_coords.first,
						door.m_coords.second)).m_block_property
					!= 0x03) {
					send_message(p_message, { std::format("World {}, Screen {}, Door ({},{}): Not placed on door-type metatile",
						c, s, door.m_coords.first, door.m_coords.second), fe::MsgType::Error });
				}

				// no need to check dest coords for doors to building - they come from the scene data
				if (p_warn_00_doors &&
					door.m_door_type != fe::DoorType::Building &&
					door.m_dest_coords.first == 0 &&
					door.m_dest_coords.second == 0)
					send_message(p_message, { std::format("World {}, Screen {}, Door ({},{}): Destination coords are (0, 0) - was this intentional?",
						c, s, door.m_coords.first, door.m_coords.second), fe::MsgType::Warning });

				unique_door_pos.insert(door.m_coords);
			}

			if (unique_door_pos.size() != doorcnt)
				send_message(p_message, { std::format("World {}, Screen {}: Several doors defined at the same position", c, s), fe::MsgType::Error });

			// validate sprite counts and total ppu tile counts
			// also check that only NPCs have scripts attached
			const auto& sprites{ scr.m_sprite_set.m_sprites };
			std::size_t total_ppu_tile_count{ 0 };
			for (std::size_t spr{ 0 }; spr < sprites.size(); ++spr) {
				total_ppu_tile_count += p_game.m_sprite_gfx_manager.get_sprite_chr_bank_size(sprites[spr].m_id);

				if (sprites[spr].m_text_id && sprites[spr].m_id < lc_sprite_types.size()) {
					const auto sprite_cat{ lc_sprite_types[sprites[spr].m_id] };
					if (sprite_cat != fe::SpriteType::NPC &&
						sprite_cat != fe::SpriteType::GameTrigger)
						send_message(p_message,
							{ std::format("World {}, Screen {}, Sprite {}: Script attached, but sprite category is not compatible",
								c, s, spr),
							fe::MsgType::Error });
				}

			}
			if (total_ppu_tile_count > c::PPU_DYNAMIC_TILE_COUNT)
				send_message(p_message,
					{ std::format("World {}, Screen {}: Sprites use {} dynamic sprite chr-tiles, but the maximum is {}",
						c, s, total_ppu_tile_count, c::PPU_DYNAMIC_TILE_COUNT),
					fe::MsgType::Error }
				);
		}
	}

	// check that metatile usage for buildings don't go across tilesets
	std::map<byte, std::set<std::size_t>> l_mt_usage; // <metatile no> -> set <tileset no>
	const auto& buildingscreens{ p_game.m_chunks.at(c::CHUNK_IDX_BUILDINGS).m_screens };

	for (std::size_t i{ 0 }; i < buildingscreens.size(); ++i) {
		std::size_t tileset_no{ p_game.get_default_tileset_no(c::CHUNK_IDX_BUILDINGS, i) };

		for (std::size_t y{ 0 }; y < 13; ++y)
			for (std::size_t x{ 0 }; x < 16; ++x)
				l_mt_usage[buildingscreens.at(i).get_mt_at_pos(x, y)].insert(tileset_no);
	}

	for (const auto& kv : l_mt_usage) {
		if (kv.second.size() != 1) {
			send_message(p_message, { std::format("Metatile {} in the buildings world used across tilesets", kv.first), fe::MsgType::Error });
		}
	}

	send_message(p_message, { "Integrity analysis completed", fe::MsgType::Success });
}

void fe::game::validate_and_repair_spawn_points(Game& p_game, const MessageCallback& p_message) {
	for (auto& spawn : p_game.m_spawn_locations) {

		if (spawn.m_world >= p_game.m_chunks.size()) {
			send_message(p_message,
				{ std::format("Invalid spawn world {}: set to 0", spawn.m_world), fe::MsgType::Error }
			);
			spawn.m_world = 0;
		}

		const auto& world = p_game.m_chunks[spawn.m_world];

		if (spawn.m_screen >= world.m_screens.size()) {
			send_message(p_message,
				{ std::format("Invalid spawn screen {} in world {}: set to 0",
					spawn.m_screen, spawn.m_world), fe::MsgType::Error }
			);
			spawn.m_screen = 0;
		}
	}
}

void fe::game::validate_and_repair_game(fe::Game& p_game, const MessageCallback& p_message) {

	const auto validate_screen_connection = [&p_message](std::optional<byte>& conn, std::size_t world,
		std::size_t screen, std::size_t screen_count) -> void {
			if (conn && (static_cast<std::size_t>(conn.value()) >= screen_count)) {
				conn.reset();
				send_message(p_message,
					{ std::format("Invalid connection reference on World {}, Screen {}: connection disabled", world, screen), fe::MsgType::Error }
				);
			}
		};

	const auto validate_door_dest_palette = [&p_message, &p_game](fe::Door& door, std::size_t world, std::size_t screen) -> void {
		if (door.m_dest_palette_id >= p_game.m_palettes.size()) {
			door.m_dest_palette_id = 0;
			send_message(p_message,
				{ std::format("Invalid destination palette for door on World {}, Screen {}, (x, y)=({}, {}): was set to 0", world, screen,
					door.m_coords.first, door.m_coords.second), fe::MsgType::Error }
			);
		}
		};

	const auto validate_door_dest_screen = [&p_message, &p_game](fe::Door& door, std::size_t world, std::size_t screen) -> void {
		if (door.m_dest_screen_id >= p_game.m_chunks.at(c::CHUNK_IDX_BUILDINGS).m_screens.size()) {
			door.m_dest_screen_id = 0;
			send_message(p_message,
				{ std::format("Invalid destination screen for door on World {}, Screen {}, (x, y)=({}, {}): was set to 0", world, screen,
					door.m_coords.first, door.m_coords.second), fe::MsgType::Error }
			);
		}
		};

	// check worlds
	for (std::size_t w{ 0 }; w < p_game.m_chunks.size(); ++w) {
		auto& world{ p_game.m_chunks[w] };

		// check all screen data for world
		std::size_t screen_count{ world.m_screens.size() };
		for (std::size_t s{ 0 }; s < screen_count; ++s) {
			auto& screen{ world.m_screens[s] };

			// validate connections
			validate_screen_connection(screen.m_scroll_left, w, s, screen_count);
			validate_screen_connection(screen.m_scroll_right, w, s, screen_count);
			validate_screen_connection(screen.m_scroll_up, w, s, screen_count);
			validate_screen_connection(screen.m_scroll_down, w, s, screen_count);

			// validate doors
			for (std::size_t d{ 0 }; d < screen.m_doors.size(); ++d) {
				auto& door{ screen.m_doors[d] };
				if (door.m_door_type == fe::DoorType::SameWorld)
					validate_door_dest_palette(door, w, s);
				else if (door.m_door_type == fe::DoorType::Building)
					validate_door_dest_screen(door, w, s);
			}

			// validate tilemap
			for (std::size_t y{ 0 }; y < screen.m_tilemap.size(); ++y)
				for (std::size_t x{ 0 }; x < screen.m_tilemap[y].size(); ++x)
					if (static_cast<std::size_t>(screen.get_mt_at_pos(x, y)) >= world.m_metatiles.size()) {
						send_message(p_message,
							{ std::format("Invalid metatile reference on World {}, Screen {}, x {}, y {}: {} was set to 0", w, s, x, y, screen.m_tilemap[y][x]), fe::MsgType::Error }
						);
						screen.m_tilemap[y][x] = 0;
					}
		}

		// check scene for world
		if (world.m_scene.m_palette >= p_game.m_palettes.size()) {
			send_message(p_message, { std::format("Invalid palette reference on World {}: {} was set to 0", w, world.m_scene.m_palette), fe::MsgType::Error });
			world.m_scene.m_palette = 0;
		}
	}

	// check building scenes
	for (std::size_t bscene{ 0 }; bscene < p_game.m_building_scenes.size(); ++bscene) {
		auto& bScene{ p_game.m_building_scenes[bscene] };

		if (bScene.m_palette >= p_game.m_palettes.size()) {
			send_message(p_message, { std::format("Invalid palette reference on scene for building screen {}: {} was set to 0", bscene, bScene.m_palette), fe::MsgType::Error });
			bScene.m_palette = 0;
		}
	}

	// stages
	auto& stages = p_game.m_stages.m_stages;

	for (std::size_t i = 0; i < stages.size(); ++i) {
		auto& stage = stages[i];

		if (stage.m_next_stage >= stages.size()) {
			send_message(p_message,
				{ std::format("Invalid next_stage index {} in stage {}: set to 0",
					stage.m_next_stage, i), fe::MsgType::Error }
			);
			stage.m_next_stage = 0;
		}

		if (stage.m_prev_stage >= stages.size()) {
			send_message(p_message,
				{ std::format("Invalid prev_stage index {} in stage {}: set to 0",
					stage.m_prev_stage, i), fe::MsgType::Error }
			);
			stage.m_prev_stage = 0;
		}

		// validate next screen
		const auto& next = stages[stage.m_next_stage];
		if (next.m_world_id < p_game.m_chunks.size()) {
			const auto& world = p_game.m_chunks[next.m_world_id];
			if (stage.m_next_screen >= world.m_screens.size()) {
				send_message(p_message,
					{ std::format("Invalid next_screen {} in stage {}: set to 0",
						stage.m_next_screen, i), fe::MsgType::Error }
				);
				stage.m_next_screen = 0;
			}
		}

		// validate prev screen
		const auto& prev = stages[stage.m_prev_stage];
		if (prev.m_world_id < p_game.m_chunks.size()) {
			const auto& world = p_game.m_chunks[prev.m_world_id];
			if (stage.m_prev_screen >= world.m_screens.size()) {
				send_message(p_message,
					{ std::format("Invalid prev_screen {} in stage {}: set to 0",
						stage.m_prev_screen, i), fe::MsgType::Error }
				);
				stage.m_prev_screen = 0;
			}
		}
	}

	// start screen
	if (!p_game.m_stages.m_stages.empty()) {
		const auto& start = p_game.m_stages.m_stages[0];

		if (start.m_world_id < p_game.m_chunks.size()) {
			const auto& world = p_game.m_chunks[start.m_world_id];

			if (p_game.m_stages.m_start_screen >= world.m_screens.size()) {
				send_message(p_message,
					{ std::format("Invalid start screen {}: set to 0",
						p_game.m_stages.m_start_screen), fe::MsgType::Error }
				);
				p_game.m_stages.m_start_screen = 0;
			}
		}
	}

	// push-block
	if (p_game.m_push_block.m_stage >= p_game.m_stages.m_stages.size()) {
		send_message(p_message, { "Invalid push-block stage: set to 0", fe::MsgType::Error });
		p_game.m_push_block.m_stage = 0;
	}

	const auto pb_world =
		p_game.m_stages.m_stages[p_game.m_push_block.m_stage].m_world_id;

	if (pb_world < p_game.m_chunks.size()) {
		const auto& world = p_game.m_chunks[pb_world];

		auto clamp_mt = [&](byte& mt) {
			if (mt >= world.m_metatiles.size()) {
				send_message(p_message, { "Invalid push-block metatile: set to 0", fe::MsgType::Error });
				mt = 0;
			}
			};

		clamp_mt(p_game.m_push_block.m_draw_block);
		clamp_mt(p_game.m_push_block.m_source_0);
		clamp_mt(p_game.m_push_block.m_source_1);
		clamp_mt(p_game.m_push_block.m_target_0);
		clamp_mt(p_game.m_push_block.m_target_1);

		if (p_game.m_push_block.m_screen >= world.m_screens.size()) {
			send_message(p_message, { "Invalid push-block screen: set to 0", fe::MsgType::Error });
			p_game.m_push_block.m_screen = 0;
		}
	}

	validate_and_repair_spawn_points(p_game, p_message);
}
