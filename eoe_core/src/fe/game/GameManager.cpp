#include "GameManager.h"
#include "fe/ROM_Manager.h"
#include "fe/fe_constants.h"
#include "fh/fh_constants.h"
#include "fe/sprite/fe_sprite_constants.h"
#include "common/klib/Kfile.h"
#include "fe/xml/Xml_helper.h"
#include "fh/HackManager.h"
#include "common/klib/IPS_Patch.h"
#include <format>

fe::game::LoadedGame fe::game::load_rom(
	const std::vector<byte>& p_rom,
	const Config& p_config,
	const MessageCallback& p_message) {

	ROM_Manager rom_manager;
	Game game{ p_config, p_rom };

	if (p_config.boolean_or(c::ID_RANDOMIZER_DOORS, false))
		game.m_sw_door_type = fe::SameWorldDoorType::Randumizer_0_30;

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

// load game from xml in-memory
fe::Game fe::game::load_game_xml(
	const fe::Config& p_config,
	const pugi::xml_document& p_doc,
	const std::vector<byte>& p_rom,
	const MessageCallback& p_message) {
	fe::Game game{ xml::load_game_xml(p_doc) };

	game.m_rom_data = p_rom;

	validate_and_repair_game(game, p_message);
	game.extract_scenes_if_empty(p_config);
	game.extract_palette_to_music(p_config);
	game.extract_hud_attributes(p_config);
	game.generate_tilesets(p_config);
	game.m_gfx_manager.initialize(p_config, game.m_rom_data);

	ROM_Manager rom_manager;
	game.m_sprite_gfx_manager.load_rom(p_config, game.m_rom_data, rom_manager);

	game.cinematic.parse_rom(p_config, game.m_rom_data);

	if (game.m_sw_door_type == SameWorldDoorType::Randumizer_0_30)
		send_message(p_message, { "Loaded XML uses the sameworld-door to stage-door hack", MsgType::Info });

	return game;
}

fe::Game fe::game::load_game_xml_from_file(
	const Config& p_config,
	const std::string& p_filepath,
	const std::vector<byte>& p_rom,
	const MessageCallback& p_message) {
	send_message(p_message, { "Attempting to load xml " + p_filepath, MsgType::Info });
	return load_game_xml(p_config, xml::load_xml_file(p_filepath), p_rom, p_message);
}

pugi::xml_document fe::game::save_game_xml(
	const Config& p_config,
	Game& p_game) {
	// gfx context is authoritative for palettes which share ROM storage
	p_game.sync_palettes(p_game.get_shared_palettes(p_config));
	return xml::save_game_xml(p_game);
}

// save game to xml file
void fe::game::save_game_xml_to_file(
	const Config& p_config,
	Game& p_game,
	const std::string& p_filepath,
	const MessageCallback& p_message) {
	xml::save_xml_file(save_game_xml(p_config, p_game), p_filepath);
	send_message(p_message, { "xml file written to " + p_filepath, MsgType::Success });
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

// sameworld to stage-door data migration
void fe::game::migrate_stage_door_hack_data(Game& p_game) {
	// ensure the data has not already been migrated
	if (p_game.m_sw_door_type == fe::SameWorldDoorType::Randumizer_0_30)
		throw std::runtime_error("Stage-door data already migrated.");

	// check if the data can actually be migrated
	// copy the world -> stages lookup map so we can use [] to populate missing entries
	auto world2stages{ p_game.m_stages.m_world_to_stage };

	for (std::size_t w{ 0 }; w < p_game.m_chunks.size(); ++w) {
		const auto& stages = world2stages[w];

		for (const auto& scr : p_game.m_chunks[w].m_screens)
			for (const auto& door : scr.m_doors)
				if (door.m_door_type == fe::DoorType::SameWorld &&
					stages.size() != 1)
					throw std::runtime_error(
						std::format("World {} is referenced by {} stage(s). Expected exactly one.",
							w, stages.size()));
	}

	// validation passed - migrate the data
	for (std::size_t w{ 0 }; w < p_game.m_chunks.size(); ++w)
		for (auto& scr : p_game.m_chunks[w].m_screens)
			for (auto& door : scr.m_doors)
				if (door.m_door_type == fe::DoorType::SameWorld) {
					byte dest_stage{ static_cast<byte>(world2stages[w][0]) };
					door.m_requirement = static_cast<byte>((dest_stage << 4) | (door.m_requirement & 0x0f));
				}

	p_game.m_sw_door_type = fe::SameWorldDoorType::Randumizer_0_30;
}

/***** ROM PATCHING - BEGIN ****/

namespace {
	// helper which reports on generic patching attempts
	bool check_patched_size(const std::string& p_data_type, std::size_t p_patch_data_size, std::size_t p_max_data_size,
		const fe::MessageCallback& p_message) {
		bool l_ok{ p_patch_data_size <= p_max_data_size };

		send_message(p_message, { std::format("Patching {} {}: Used {} of {} available bytes ({:.2f}%)",
			p_data_type,
			(l_ok ? "succeeded" : "failed"),
			p_patch_data_size, p_max_data_size,
			100.0f * (static_cast<float>(p_patch_data_size) / static_cast<float>(p_max_data_size))), l_ok ? fe::MsgType::Success : fe::MsgType::Error });

		return l_ok;
	}

	void report_sprite_gfx_patch(const fe::SpriteGfxPatchResult& result, const fe::MessageCallback& p_message) {

		const auto bank_header = [](int p_bank_no, std::optional<std::size_t> p_bank_used) -> std::string {
			if (!p_bank_used) {
				return std::format("Bank {} fail: ", p_bank_no);
			}
			else {
				std::size_t bank_used{ p_bank_used.value() };

				return std::format("Bank {}: {}/{} bytes ({:.2f}%): ", p_bank_no, bank_used, 0x4000,
					(100.0f * bank_used) / 0x4000);
			}
			};

		const auto bank_item = [](const std::string& p_item_name, std::optional<std::size_t> p_value,
			bool p_add_comma = true) -> std::string {
				std::string l_result{ p_item_name };

				if (p_value)
					l_result += std::format("={}", p_value.value());
				else
					l_result += "=null";

				if (p_add_comma)
					l_result += ", ";

				return l_result;
			};

		std::string bank6res{ bank_header(6, result.bank6_used) };
		std::string bank7res{ bank_header(7, result.bank7_used) };
		std::string bank8res{ bank_header(8, result.bank8_used) };

		bank6res += bank_item("npc_chr", result.bank6_used);
		bank6res += bank_item("sprite_cutoff", result.bank6_sprite_cutoff, false);

		bank7res += bank_item("npc_chr", result.bank7_sprite_chr_used);
		bank7res += bank_item("npc_frame", result.bank7_npc_anim_frame_used);
		bank7res += bank_item("player_frame", result.bank7_player_anim_frame_used);
		bank7res += bank_item("portrait_frame", result.bank7_portrait_anim_frame_used, false);

		bank8res += bank_item("player_list", result.bank8_player_load_lists);
		bank8res += bank_item("wep_list", result.bank8_weapons_load_lists);
		bank8res += bank_item("player_chr", result.bank8_player_chr);
		bank8res += bank_item("wep_chr", result.bank8_weapons_chr);
		bank8res += bank_item("common_chr", result.bank8_common_chr);
		bank8res += bank_item("shield_chr", result.bank8_shield_chr);
		bank8res += bank_item("shield_list", result.bank8_shield_load_lists);
		bank8res += bank_item("portrait_list", result.bank8_portrait_load_lists);
		bank8res += bank_item("portrait_chr", result.bank8_portrait_chr, false);

		send_message(p_message, { bank6res, result.bank6_used ? fe::MsgType::Info : fe::MsgType::Error });
		send_message(p_message, { bank7res, result.bank7_used ? fe::MsgType::Info : fe::MsgType::Error });
		send_message(p_message, { bank8res, result.bank8_used ? fe::MsgType::Info : fe::MsgType::Error });
	}
}

std::vector<byte> fe::game::patch_rom(
	const Config& p_config,
	Game& p_game,
	const RomPatchOptions& p_options,
	const MessageCallback& p_message) {
	bool l_good{ true };

	const auto l_world_labels{ p_config.bmap(c::ID_WORLD_LABELS) };

	const auto world_name = [&l_world_labels](std::size_t p_world) -> std::string {
		auto it{ l_world_labels.find(static_cast<byte>(p_world)) };

		return it != l_world_labels.end() ? it->second : "Unknown";
		};

	// the randumizer can inject a hack surrounding pal2mus so avoid patching garbage data
	// also, we will not install the sameworld to stage-door hack since the randomizer patches that itself
	// although it probably wouldn't have broken anything with our default hack injection points
	bool is_randumizer{ p_config.boolean_or(c::ID_DISABLE_PAL2MUS, false) };
	std::size_t l_dyndata_bytes{ 0 };
	fe::ROM_Manager rom_manager;

	// gfx context is authoritative for palettes which share ROM storage
	p_game.sync_palettes(p_game.get_shared_palettes(p_config));

	auto x_rom{ p_game.m_rom_data };

	// ensure the door hack is applied if it is supposed to be
	// skip it for randomizer ROMs as they keep the hack in different locations
	if (p_game.m_sw_door_type == fe::SameWorldDoorType::Randumizer_0_30 && !is_randumizer) {
		fh::HackManager::install_hack_sameworld_to_stage_doors(p_config, x_rom);
	}

	// world tileset chr
	if (p_options.world_chr_data) {
		rom_manager.encode_chr_data(p_config, p_game, x_rom);
		send_message(p_message, { "Patched world tileset chr data", fe::MsgType::Success });
	}

	// world palettes
	if (p_options.palettes) {
		rom_manager.encode_palette_data(p_config, p_game, x_rom);
		send_message(p_message, { "Patched world palettes", fe::MsgType::Success });
	}

	// stage definitions
	if (p_options.stages) {
		rom_manager.encode_stage_data(p_config, p_game, x_rom);
		send_message(p_message, { "Patched stage definitions", fe::MsgType::Success });
	}

	// mattock animations
	if (p_options.mattock_animations) {
		rom_manager.encode_mattock_animations(p_config, p_game, x_rom);
		send_message(p_message, { "Patched mattock animations", fe::MsgType::Success });
	}

	// push-block definition
	if (p_options.push_blocks) {
		rom_manager.encode_push_block(p_config, p_game, x_rom);
		send_message(p_message, { "Patched push-block definition", fe::MsgType::Success });
	}

	// jump-on tiles
	if (p_options.jump_on_tiles) {
		rom_manager.encode_jump_on_tiles(p_config, p_game, x_rom);
		send_message(p_message, { "Patched jump-on tiles", fe::MsgType::Success });
	}

	// world scene data
	if (p_options.scenes) {
		rom_manager.encode_scene_data(p_config, p_game, x_rom);
		send_message(p_message, { "Patched world scenes", fe::MsgType::Success });
	}

	// fog definition
	if (p_options.fog) {
		rom_manager.encode_fog_data(p_config, p_game, x_rom);
		send_message(p_message, { "Patched fog definition", fe::MsgType::Success });
	}

	// background graphics
	if (p_options.bg_gfx) {
		p_game.m_gfx_manager.patch_rom(x_rom);
		send_message(p_message, { "Patched background gfx", fe::MsgType::Success });
	}

	std::pair<std::size_t, std::size_t> l_bret(0, 0);

	// cinematics
	if (p_options.cinematics) {
		auto cinema_res{ p_game.cinematic.patch_rom(p_config, x_rom) };
		std::size_t used_space{ cinema_res.data_section_end - cinema_res.data_section_start };
		std::size_t free_space_end{ p_options.throw_on_cinematic_overflow ?
			p_config.constant(c::ID_ISCRIPT_DATA_RG2_START) :
		p_config.constant(c::ID_ISCRIPT_DATA_RG2_END) };

		bool cinematic_patched_ok{ check_patched_size("Cinematic Data", used_space,
			free_space_end - cinema_res.data_section_start, p_message) };

		l_good &= cinematic_patched_ok;

		if (!cinematic_patched_ok && p_options.throw_on_cinematic_overflow)
			send_message(p_message, {
				std::format(
					"Cinematic data overflow: constant '{}' must be set to at least 0x{:05x} (see the documentation)",
					c::ID_ISCRIPT_DATA_RG2_START,
					cinema_res.data_section_end),
				fe::MsgType::Error });
	}

	// sprite gfx
	if (p_options.sprite_gfx) {
		auto spritegfxres{ p_game.m_sprite_gfx_manager.patch_rom(p_config, x_rom, rom_manager) };
		l_dyndata_bytes += spritegfxres.bank6_used.value_or(0);
		l_dyndata_bytes += spritegfxres.bank7_used.value_or(0);
		l_dyndata_bytes += spritegfxres.bank8_used.value_or(0);
		l_good &= spritegfxres.success;
		report_sprite_gfx_patch(spritegfxres, p_message);
		if (spritegfxres.success)
			send_message(p_message, { "Sprite Gfx data patched!", fe::MsgType::Success });
		else
			send_message(p_message, { "Could not patch Sprite Gfx data", fe::MsgType::Error });
	}

	// bank 15 - coupled dynamic data
	if (p_options.bank15_data) {
		const auto bank15_res{ rom_manager.encode_bank_15_data(p_config, p_game, x_rom, !is_randumizer) };

		l_good &= check_patched_size("Bank 15 Data (transitions, palette-to-music, spawns, building scenes)",
			bank15_res.used_bytes, bank15_res.available_bytes, p_message);
		l_dyndata_bytes += bank15_res.used_bytes;

		if (!p_options.general_hacks.empty()) {
			fh::HackManager hack_mgr;
			const auto bank15_hack_available_size{ bank15_res.free_range_cpu_end - bank15_res.free_range_cpu_start };
			const std::size_t bank15_hack_size{ hack_mgr.install_general_hacks(p_config, x_rom, 15,
				bank15_res.free_range_cpu_start, bank15_res.free_range_cpu_end, p_options.general_hacks) };
			send_message(p_message, { std::format("Installed general hacks in bank 15 ({}/{} bytes)",
				bank15_hack_size, bank15_hack_available_size) });
			l_dyndata_bytes += bank15_hack_size;
		}
	}

	// sprite metadata
	if (p_options.sprite_data) {
		l_bret = rom_manager.encode_sprite_data(p_config, p_game, x_rom);
		l_good &= check_patched_size("Sprite Data", l_bret.first, l_bret.second, p_message);
		l_dyndata_bytes += l_bret.first;
	}

	// world metadata
	if (p_options.metadata) {
		l_bret = rom_manager.encode_metadata(p_config, p_game, x_rom);
		l_good &= check_patched_size("Worlds Metadata", l_bret.first, l_bret.second, p_message);
		l_dyndata_bytes += l_bret.first;
	}

	// screen tilemaps
	if (p_options.tilemaps) {
		auto l_tm_result{ rom_manager.encode_game_tilemaps(p_config, x_rom,
			p_game) };
		l_good &= l_tm_result.m_result;

		std::size_t l_max_tm_byte_size{ p_config.constant(c::ID_WORLD_TILEMAP_MAX_SIZE) };

		if (l_tm_result.m_result) {

			for (const auto& kv : l_tm_result.m_assignments) {
				std::size_t l_bank_byte_size{ 0 };
				std::string l_bank_output;

				for (std::size_t w : kv.second) {
					std::size_t l_byte_size{ l_tm_result.m_sizes[w] };
					l_bank_byte_size += l_byte_size;
					l_dyndata_bytes += l_byte_size;
					l_bank_output += std::format("({} {} bytes) ",
						world_name(w), l_byte_size);
				}

				send_message(p_message, { std::format("Bank {}: {}- total bytes: {}/{} ({:.2f}%)",
					kv.first, l_bank_output, l_bank_byte_size, l_max_tm_byte_size,
					100.0f * static_cast<float>(l_bank_byte_size) / static_cast<float>(l_max_tm_byte_size)),
					fe::MsgType::Info });
			}

			send_message(p_message, { "Tilemaps patched!", fe::MsgType::Success });
		}
		else {
			send_message(p_message, { std::format("Could not pack all world tilemaps across the banks, each of byte size {}",
				l_max_tm_byte_size), fe::MsgType::Error });
			for (std::size_t i{ 0 }; i < 8; ++i) {
				send_message(p_message, { std::format("Byte size for {}: {}",
					world_name(i),
					l_tm_result.m_sizes[i]), fe::MsgType::Info });
			}
		}
	}

	if (p_options.apply_sw_pal2mus_hack) {
		fh::HackManager hack_manager;
		hack_manager.install_SameWorldTransPal2Mus(p_config, x_rom, 15,
			static_cast<word>(p_config.constant_or(fh::c::ID_HACK_SW_TRANS_PAL2MUS_ADDR, 0xc033)));
		send_message(p_message, { "Enabled palette to music functionality for sameworld-transitions", fe::MsgType::Info });
	}

	// bank duplication - region-specific config and not a setting
	// must be done after all other patching has completed
	if (p_config.boolean_or(c::ID_DUPLICATE_STATIC_BANK, false)) {
		rom_manager.duplicate_static_bank(x_rom);
		send_message(p_message, { "Duplicated bank 15 into bank 31", fe::MsgType::Info });
	}

	if (l_good) {
		send_message(p_message, { std::format("ROM data patched ({} dynamic bytes)",
			l_dyndata_bytes), fe::MsgType::Success });
		return x_rom;
	}
	else {
		throw std::runtime_error("Could not patch ROM data");
	}
}

void fe::game::patch_rom_to_file(
	const Config& p_config,
	Game& p_game,
	const std::string& p_filepath,
	const RomPatchOptions& p_options,
	const MessageCallback& p_message) {
	klib::file::write_bytes_to_file(patch_rom(p_config, p_game, p_options, p_message), p_filepath);
	send_message(p_message, { "ROM file written to " + p_filepath, MsgType::Success });
}

std::vector<byte> fe::game::generate_ips(
	const Config& p_config,
	Game& p_game,
	const RomPatchOptions& p_options,
	const MessageCallback& p_message) {
	return klib::ips::generate_patch(p_game.m_rom_data, patch_rom(p_config, p_game, p_options, p_message));
}

void fe::game::generate_ips_to_file(
	const Config& p_config,
	Game& p_game,
	const std::string& p_filepath,
	const RomPatchOptions& p_options,
	const MessageCallback& p_message) {
	const auto ips{ generate_ips(p_config, p_game, p_options, p_message) };
	klib::file::write_bytes_to_file(ips, p_filepath);
	send_message(p_message, { std::format("ips patch written to {} ({} bytes)",	p_filepath, ips.size()), MsgType::Success });
}

/***** ROM PATCHING - END ******/
