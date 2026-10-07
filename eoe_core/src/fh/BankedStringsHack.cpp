#include "HackManager.h"
#include "fh_constants.h"
#include "fe/ROM_Manager.h"
#include "common/klib/Asm6502.h"
#include <format>
#include <stdexcept>

namespace {
	constexpr byte TempMessageBank{ fh::RAM::ZP_e5 };
}

word fh::HackManager::install_BankedStrings(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, const fh::GeneralHack& p_hack) {
	const bool sram{ p_hack.bool_or("sram", false) };

	const word install_addr{ sram ? sram_hack_addr() : cpu_addr };

	klib::Asm6502 code;

	code.label("Messages_Load-Banked");
	code.sta_abs(RAM::StringID);
	code.lda_abs(RAM::CurrentROMBank);
	code.pha();
	code.ldx_zp(TempMessageBank);
	code.jmp(ROM::Messages_Load_JSR_MMC1_UpdateROMBank);

	code.label("TextBox_ShowNextChar-Banked");
	code.lda_abs(RAM::CurrentROMBank);
	code.pha();
	code.ldx_zp(TempMessageBank);
	code.jmp(ROM::TextBox_ShowNextChar_JSR_MMC1_UpdateROMBank);

	const word messages_load_banked_addr{ code.label_addr("Messages_Load-Banked", install_addr) };
	const word txtbox_shownextchar_addr{ code.label_addr("TextBox_ShowNextChar-Banked", install_addr) };

	// TODO: Fixup script opcode calls to these entrypoints

	if (sram) {
		install_sram_hack(p_rom, code);
		return cpu_addr;
	}

	return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 15, install_addr);

}
