#include "GameManager.h"
#include "fe/ROM_Manager.h"
#include "fe/fe_constants.h"
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
