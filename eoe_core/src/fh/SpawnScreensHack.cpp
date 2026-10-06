#include "HackManager.h"
#include "fh_constants.h"
#include "fe/Game.h"
#include "fe/fe_constants.h"
#include "common/klib/Asm6502.h"

word fh::HackManager::install_SpawnScreens(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, const fh::GeneralHack& p_hack, const fe::Game* p_game = nullptr) {
	// read the references to the tables as words directly from ROM
	const auto bld_scene_palette{ klib::Asm6502::read_word(p_rom, p_config.pointer(fe::c::ID_BLD_SCENE_PALETTE_PTR).first) };
	const auto bld_scene_tileset{ klib::Asm6502::read_word(p_rom, p_config.pointer(fe::c::ID_BLD_SCENE_TILESET_PTR).first) };
	const auto bld_scene_pos{ klib::Asm6502::read_word(p_rom, p_config.pointer(fe::c::ID_BLD_SCENE_POS_PTR).first) };
	const auto bld_scene_music{ klib::Asm6502::read_word(p_rom, p_config.pointer(fe::c::ID_BLD_SCENE_MUSIC_PTR).first) };

	const auto data{ p_hack.split_twice_bytes("data", 2) };

	std::vector<byte> spawn_screens(p_game ? p_game->m_spawn_locations.size() : 8, 1);
	for (const auto& data_pair : data)
		spawn_screens.at(data_pair.at(0)) = data_pair.at(1);

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

	const auto lookup_table_addr{ code.label_addr("@spawn_screens", cpu_addr) };
	const auto next_addr{ code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 15, cpu_addr) };

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
	code.jsr(cpu_addr);
	code.nop(2);
	code.apply_hack_and_clear(p_rom, 15, ROM::Game_SpawnInTemple_LDA_Palette);

	return next_addr;
}
