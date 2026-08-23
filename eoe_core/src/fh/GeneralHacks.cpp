#include "HackManager.h"
#include "fe/Config.h"
#include "fe/Game.h"
#include "common/klib/Asm6502.h"
#include "fh_constants.h"
#include "fe/fe_constants.h"
#include <format>
#include <stdexcept>

// kill switch; Pressing Select while the game is paused kills the player when the game is unpaused
word fh::HackManager::install_KillSwitch(const fe::Config& p_config, std::vector<byte>& p_rom, byte p_bank, word cpu_addr) const {
	klib::Asm6502 code;

	// call the routine vanilla would have called if we didn't install the hook
	code.jsr(ROM::Sprites_FlipRanges);
	code.lda_zp(RAM::ZP_Joy1_ChangedButtonMask);
	// test bit 5: select button
	code.and_imm(0b00100000);
	code.beq("@select_not_pressed");
	code.lda_imm(0x01);
	code.sta_abs(RAM::PlayerIsDead);
	code.label("@select_not_pressed");
	code.rts();
	const auto next_cpu_addr{ code.apply_hack_and_clear_get_next_cpu_addr(p_rom, p_bank, cpu_addr) };

	// install the hook
	code.jsr(cpu_addr);
	code.apply_hack_and_clear(p_rom, p_bank, ROM::GameLoop_CheckPauseGame_JSR_Sprites_FlipRanges);

	return next_cpu_addr;
}

word fh::HackManager::install_SameWorldTransPal2Mus(const fe::Config& p_config, std::vector<byte>& p_rom,
	byte p_bank, word cpu_addr, bool p_stage_door_hack_installed) const {
	klib::Asm6502 code;

	// if the stage door hack is not installed, this becomes a static patch
	// where we jump to the door-pal2mus logic directly
	if (!p_stage_door_hack_installed) {
		code.jmp(ROM::Player_CheckHandleEnterDoor_LDX_pal2mus_slots);
		code.apply_hack_and_clear(p_rom, p_bank, ROM::SwTransJmpSetupEnterScreen);
		return cpu_addr;
	}

	// constants from the rom
	const auto pal_ptr{ p_config.pointer(fe::c::ID_PAL2MUS_PALETTE_PTR) };
	const auto mus_ptr{ p_config.pointer(fe::c::ID_PAL2MUS_MUSIC_PTR) };

	byte pal2mus_slot_count{ p_rom.at(p_config.constant(fe::c::ID_PAL2MUS_ENTRY_COUNT_OFFSET)) };
	word pal2mus_pal_table_addr{ static_cast<word>(klib::Asm6502::read_word(p_rom, pal_ptr.first)) };
	word pal2mus_mus_table_addr{ static_cast<word>(klib::Asm6502::read_word(p_rom, mus_ptr.first)) };

	// add new routine which copies the vanilla logic for sw-door pal2mus
	code.ldx_imm(pal2mus_slot_count);
	code.label("@paletteCheckLoop");
	code.lda_zp(RAM::ZP_TransitionPalette);
	code.cmp_abs_x(pal2mus_pal_table_addr);
	code.beq("@setupArea");
	code.dex();
	code.bpl("@paletteCheckLoop");
	code.bmi("@enterScreen");

	code.label("@setupArea");
	code.lda_abs_x(pal2mus_mus_table_addr);
	code.cmp_abs(RAM::World_DefaultMusic);
	code.beq("@enterScreen");
	code.sta_zp(RAM::ZP_MusicCurrent);
	code.sta_abs(RAM::World_DefaultMusic);

	code.label("@enterScreen");
	code.jmp(ROM::Game_SetupEnterScreen);

	// insert the new routine in rom
	const auto next_cpu_addr{ code.apply_hack_and_clear_get_next_cpu_addr(p_rom, p_bank, cpu_addr) };

	// from sw-transitions, jump into our new routine rather than Game_SetupEnterScreen
	code.jmp(cpu_addr);
	code.apply_hack_and_clear(p_rom, p_bank, ROM::SwTransJmpSetupEnterScreen);

	return next_cpu_addr;
}

std::size_t fh::HackManager::install_general_hacks(const fe::Config& p_config, std::vector<byte>& p_rom, byte p_bank,
	std::size_t p_cpu_addr_start, std::size_t p_cpu_addr_end, const std::vector<GeneralHack>& p_hacks,
	const fe::Game* p_game) const {
	// TODO: Throw if cpu range is invalid but that would be a config error
	const word cpu_start{ static_cast<word>(p_cpu_addr_start) };
	word cpu_addr{ cpu_start };

	for (const auto& hack : p_hacks) {
		switch (hack.get_type()) {
		case fh::GeneralHackLib::KillSwitch:
			cpu_addr = install_KillSwitch(p_config, p_rom, p_bank, cpu_addr);
			break;
		case fh::GeneralHackLib::SameWorldTransPal2Mus:
			cpu_addr = install_SameWorldTransPal2Mus(p_config, p_rom, p_bank, cpu_addr,
				p_game && p_game->m_sw_door_type == fe::SameWorldDoorType::Randumizer_0_30);
			break;
		default:
			throw std::runtime_error("Unsupported general hack library routine.");
		}

		if (static_cast<std::size_t>(cpu_addr) > p_cpu_addr_end)
			throw std::runtime_error(std::format("Hack overflow in bank ${:02x}", p_bank));
	}

	return static_cast<std::size_t>(cpu_addr) - p_cpu_addr_start;
}
