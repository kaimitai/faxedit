#ifndef FE_GAME_MANAGER_H
#define FE_GAME_MANAGER_H

#include <vector>
#include "fe/Game.h"
#include "fe/Config.h"
#include "fe/MessageCallback.h"
#include "fh/HackManager.h"

using byte = unsigned char;

namespace pugi {
	class xml_document;
}

namespace fe::game {

	struct LoadedGame {
		Config config;
		Game game;
	};

	struct RomPatchOptions {
		bool world_chr_data{ true };
		bool palettes{ true };
		bool stages{ true };
		bool mattock_animations{ true };
		bool push_blocks{ true };
		bool jump_on_tiles{ true };
		bool scenes{ true };
		bool fog{ true };
		bool bg_gfx{ true };
		bool cinematics{ true };
		bool sprite_gfx{ true };
		bool bank15_data{ true };
		bool sprite_data{ true };
		bool metadata{ true };
		bool tilemaps{ true };

		bool throw_on_cinematic_overflow{ true };
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

	// save game to in-memory xml
	pugi::xml_document save_game_xml(
		const Config& p_config,
		Game& p_game);

	// save game to xml file
	void save_game_xml_to_file(
		const Config& p_config,
		Game& p_game,
		const std::string& p_filepath,
		const MessageCallback& p_message = nullptr);

	// analysis and validation
	void analyze_game_data(const Game& p_game, const Config& p_config, bool p_warn_tilemap_95_pct,
		bool p_warn_00_doors, const MessageCallback& p_message = nullptr);
	void validate_and_repair_spawn_points(fe::Game& p_game, const fe::MessageCallback& p_message = nullptr);
	void validate_and_repair_game(Game& p_game, const MessageCallback& p_message = nullptr);

	// sameworld to stage-door data migration
	void migrate_stage_door_hack_data(Game& p_game);
	void migrate_double_tileset_data(Game& p_game);

	// rom patching - the MacDaddy!
	std::vector<byte> patch_rom(
		const Config& p_config,
		Game& p_game,
		const RomPatchOptions& p_options,
		const MessageCallback& p_message = nullptr);

	void patch_rom_to_file(
		const Config& p_config,
		Game& p_game,
		const std::string& p_filepath,
		const RomPatchOptions& p_options,
		const MessageCallback& p_message = nullptr);

	std::vector<byte> generate_ips(
		const Config& p_config,
		Game& p_game,
		const RomPatchOptions& p_options,
		const MessageCallback& p_message = nullptr);

	void generate_ips_to_file(
		const Config& p_config,
		Game& p_game,
		const std::string& p_filepath,
		const RomPatchOptions& p_options,
		const MessageCallback& p_message = nullptr);
}

#endif
