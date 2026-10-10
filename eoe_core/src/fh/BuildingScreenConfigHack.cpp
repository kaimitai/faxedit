#include "HackManager.h"
#include "fh_constants.h"
#include "fe/Game.h"
#include "fe/fe_constants.h"
#include "common/klib/Asm6502.h"
#include <format>
#include <stdexcept>
#include <vector>

namespace {
	constexpr byte SCRATCH_VAR{ fh::RAM::ZP_e5 };
}

word fh::HackManager::install_BuildingScreenConfig(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, const fh::GeneralHack& p_hack, const fe::Game* p_game) {
	// read the references to the tables as words directly from ROM
	const auto bld_scene_palette{ klib::Asm6502::read_word(p_rom, p_config.pointer(fe::c::ID_BLD_SCENE_PALETTE_PTR).first) };
	const auto bld_scene_tileset{ klib::Asm6502::read_word(p_rom, p_config.pointer(fe::c::ID_BLD_SCENE_TILESET_PTR).first) };
	const auto bld_scene_pos{ klib::Asm6502::read_word(p_rom, p_config.pointer(fe::c::ID_BLD_SCENE_POS_PTR).first) };
	const auto bld_scene_music{ klib::Asm6502::read_word(p_rom, p_config.pointer(fe::c::ID_BLD_SCENE_MUSIC_PTR).first) };

	const auto spawns{ p_hack.split_twice_bytes("spawns", 2) };
	const bool sram_install{ p_hack.bool_or("sram", false) };
	const bool install_end_screen{ p_hack.has_param("end_screen") };

	std::vector<byte> directions(p_game ? p_game->m_building_scenes.size() : 10, 0x00);
	const bool install_directions{ p_hack.has_param("right") };
	if (install_directions) {
		const auto right_screens{ p_hack.split_bytes("right") };
		for (byte screen : right_screens)
			if (screen >= directions.size())
				throw std::runtime_error(std::format("BuildingScreenConfig references invalid building screen index {}", screen));
			else
				directions[screen] = 0x40;
	}


	std::vector<byte> spawn_screens(p_game ? p_game->m_spawn_locations.size() : 8, 1);
	for (const auto& spawn : spawns)
		spawn_screens.at(spawn.at(0)) = spawn.at(1);

	klib::Asm6502 code;

	// load position
	code.lda_abs_x(bld_scene_pos);
	code.sta_zp(RAM::ZP_TransitionStartPos);
	// load tileset
	code.lda_abs_x(bld_scene_tileset);
	code.sta_abs(RAM::BuildingTileset);

	code.rts();
	code.label("@spawn_screens");
	for (byte b : spawn_screens)
		code.db(b);

	if (install_directions) {
		// entry path
		code.label("@set_direction");
		code.ldx_zp(RAM::ZP_TransitionScreen);
		code.lda_abs_x("@directions");
		code.jmp("@combine_direction");

		// exit path
		code.label("@exit_direction");
		code.ldx_zp(RAM::ZP_TransitionScreen);
		code.lda_abs_x("@directions");
		code.eor_imm(0x40);

		// shared tail
		code.label("@combine_direction");
		code.sta_zp(SCRATCH_VAR);
		code.lda_zp(RAM::ZP_PlayerState);
		code.and_imm(0xbf);
		code.ora_zp(SCRATCH_VAR);
		code.rts();

		// data table
		code.label("@directions");
		for (byte direction : directions)
			code.db(direction);
	}

	word hack_addr{ cpu_addr };
	word next_cpu_addr{ cpu_addr };

	const auto lookup_table_offset{ code.label_position("@spawn_screens") };
	const auto set_direction_offset{ install_directions ? code.label_position("@set_direction") : 0 };
	const auto exit_direction_offset{ install_directions ? code.label_position("@exit_direction") : 0 };

	if (sram_install)
		hack_addr = install_sram_hack(p_rom, code);
	else
		next_cpu_addr = code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 15, cpu_addr);

	const word lookup_table_addr{ static_cast<word>(lookup_table_offset + hack_addr) };
	const word set_direction_addr{ static_cast<word>(hack_addr + set_direction_offset) };
	const word exit_direction_addr{ static_cast<word>(hack_addr + exit_direction_offset) };

	// hook - do as much work as we can on "this side"
	// assumes X holds spawn point index
	// store building screen index and use this to lookup the scene data
	code.lda_abs_x(lookup_table_addr);
	code.sta_zp(RAM::ZP_TransitionScreen);
	code.tax();

	// load palette index
	code.lda_abs_x(bld_scene_palette);
	code.sta_zp(RAM::ZP_TransitionPalette);
	// load music index
	code.lda_abs_x(bld_scene_music);
	code.sta_abs(RAM::World_DefaultMusic);
	// lookup the other two values in the new routine
	code.jsr(hack_addr);
	code.nop(2);
	code.apply_hack_and_clear(p_rom, 15, ROM::Game_SpawnInTemple_LDA_Palette);

	// direction override hooks
	if (install_directions) {
		// when spawning in church/temple, load the direction from metadata
		code.jsr(set_direction_addr);
		code.nop();
		code.apply_hack_noclear(p_rom, 15, ROM::Game_SpawnInTemple_SetFacing);
		// when entering buildings normally
		code.apply_hack_and_clear(p_rom, 15, ROM::EnterBuilding_SetFacing);
		// when exiting buildings, reverse the direction
		code.jsr(exit_direction_addr);
		code.nop();
		code.apply_hack_and_clear(p_rom, 15, ROM::ExitBuilding_SetFacing);
	}

	if (install_end_screen) {
		const byte endgame_screen{ p_hack.byte_or("end_screen", 0) };

		if (!p_game)
			throw std::runtime_error("BuildingScreenConfig requires game data when using parameter end_screen");
		if (endgame_screen >= p_game->m_building_scenes.size())
			throw std::runtime_error(std::format("BuildingScreenConfig references invalid endgame screen index {}", endgame_screen));

		const auto& scene{ p_game->m_building_scenes.at(endgame_screen) };

		klib::Asm6502::apply_byte(p_rom, endgame_screen, 15, ROM::EndGameScreen);
		klib::Asm6502::apply_byte(p_rom, static_cast<byte>(scene.m_music), 15, ROM::EndGameMusic);
		klib::Asm6502::apply_byte(p_rom, scene.get_pos_as_byte(), 15, ROM::EndGamePosition);
		klib::Asm6502::apply_byte(p_rom, static_cast<byte>(scene.m_palette), 15, ROM::EndGamePalette);
		klib::Asm6502::apply_byte(p_rom, static_cast<byte>(scene.m_tileset), 15, ROM::EndGameTileset);

		// only override facing when direction overrides are enabled
		if (install_directions) {
			const bool face_right{ directions.at(endgame_screen) == 0x40 };
			if (face_right)
				code.ora_imm(0x40);
			else
				code.and_imm(0xbf);
			code.apply_hack_and_clear(p_rom, 15, ROM::EndGame_SetFacing + 2);
		}
	}

	return next_cpu_addr;
}
