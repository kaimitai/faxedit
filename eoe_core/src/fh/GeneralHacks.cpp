#include "HackManager.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"
#include "fh_constants.h"
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
	auto next_cpu_addr{ get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, p_bank, cpu_addr),
		p_bank == 0x0f ? 0x10000 : 0xc000) };

	// install the hook
	code.jsr(cpu_addr);
	code.apply_hack_and_clear(p_rom, p_bank, ROM::GameLoop_CheckPauseGame_JSR_Sprites_FlipRanges);

	return next_cpu_addr;
}

std::size_t fh::HackManager::install_general_hacks(const fe::Config& p_config, std::vector<byte>& p_rom, byte p_bank,
	std::size_t p_cpu_addr_start, std::size_t p_cpu_addr_end, const std::vector<GeneralHackLib>& p_hacks) const {
	std::size_t max_size{ p_cpu_addr_end - p_cpu_addr_start };

	// TODO: Throw if cpu range is invalid
	const word cpu_start{ static_cast<word>(p_cpu_addr_start) };
	word cpu_addr{ cpu_start };

	for (fh::GeneralHackLib hack : p_hacks) {
		switch (hack) {
		case fh::GeneralHackLib::KillSwitch:
			cpu_addr = install_KillSwitch(p_config, p_rom, p_bank, cpu_addr);
			break;

		default:
			break;
		}

		if (static_cast<std::size_t>(cpu_addr) > p_cpu_addr_end)
			throw std::runtime_error(std::format("Hack overflow in bank ${:02x}", p_bank));
	}

	return static_cast<std::size_t>(cpu_addr) - p_cpu_addr_start;
}
