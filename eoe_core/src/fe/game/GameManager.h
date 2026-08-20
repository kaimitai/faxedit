#ifndef FE_GAME_MANAGER_H
#define FE_GAME_MANAGER_H

#include <vector>
#include "fe/Game.h"
#include "fe/Config.h"
#include "fe/MessageCallback.h"

using byte = unsigned char;

namespace fe::game {

	struct LoadedGame {
		Config config;
		Game game;
	};

	// completely in-memory
	LoadedGame load_rom(
		const std::vector<byte>& p_rom,
		const Config& p_config,
		const MessageCallback& p_message = nullptr);

	// load/configure config from xml, rom already in memory
	LoadedGame load_rom(
		const std::vector<byte>& p_rom,
		const std::string& p_config_path,
		const std::string& p_config_override_path,
		const std::string& p_region,
		const MessageCallback& p_message = nullptr);

	// load game and config from files
	LoadedGame load_rom(
		const std::string& p_rom_path,
		const std::string& p_config_path,
		const std::string& p_config_override_path,
		const std::string& p_region,
		const MessageCallback& p_message = nullptr);

	// load game from xml in-memory
	fe::Game load_game_xml(
		const fe::Config& p_config,
		const pugi::xml_document& p_doc,
		const std::vector<byte>& p_rom,
		const MessageCallback& p_message = nullptr);

	// game xml from file
	fe::Game load_game_xml_from_file(
		const Config& p_config,
		const std::string& p_filepath,
		const std::vector<byte>& p_rom,
		const MessageCallback& p_message = nullptr);

	// analysis and validation
	void analyze_game_data(const Game& p_game, const Config& p_config, bool p_warn_tilemap_95_pct,
		bool p_warn_00_doors, const MessageCallback& p_message = nullptr);
	void validate_and_repair_spawn_points(fe::Game& p_game, const fe::MessageCallback& p_message = nullptr);
	void validate_and_repair_game(Game& p_game, const MessageCallback& p_message = nullptr);

	// sameworld to stage-door data migration
	void migrate_stage_door_hack_data(Game& p_game);
}

#endif
