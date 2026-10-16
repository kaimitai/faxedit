#include "HackManager.h"
#include "fh_constants.h"
#include "common/klib/Asm6502.h"
#include "common/klib/Kfile.h"
#include <format>
#include <stdexcept>

word fh::HackManager::install_ExtraTitleScreenChr(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, const fh::GeneralHack& p_hack) {
	const auto filename{ p_hack.get_string("file") };
	const std::vector<byte> chr_data{ klib::file::read_file_as_bytes(filename) };
	const bool sram{ p_hack.bool_or("sram", false) };

	if (chr_data.empty() || chr_data.size() % 16 != 0 || chr_data.size() > static_cast<std::size_t>(5 * 16))
		throw std::runtime_error(std::format("File '{}' must contain 1-5 chr-tiles", filename));

	const byte chr_count{ static_cast<byte>(chr_data.size() / 16) };

	const auto install_addr{ sram ? sram_hack_addr() : cpu_addr };

	klib::Asm6502 hook;
	hook.jsr(install_addr);
	hook.apply_hack_and_clear(p_rom, 12, cfg_word(p_config, c::ID_STARTSCREEN_DRAW_JSR_CLEARALLTILEMAPS));

	klib::Asm6502 code;
	code.lda_imm_lo("@chr_data");
	code.sta_zp(RAM::ZP_ScriptAddr);
	code.lda_imm_hi("@chr_data");
	code.sta_zp(RAM::ZP_ScriptAddrU);

	// destination = tile $FB: $1000 + ($FB * 16) = $1FB0
	code.lda_imm(0xb0);
	code.sta_zp(RAM::ZP_e8);
	code.lda_imm(0x1f);
	code.sta_zp(RAM::ZP_e9);

	code.ldx_imm(12);
	code.ldy_imm(chr_count);
	code.jsr(cfg_word(p_config, c::ID_PPU_WRITETILESFROMCHRRAM));

	code.jmp(cfg_word(p_config, c::ID_ROM_PPU_CLEARALLTILEMAPS));
	code.label("@chr_data");
	for (byte b : chr_data)
		code.db(b);

	word next_cpu_addr{ cpu_addr };

	if (sram)
		install_sram_hack(p_rom, code);
	else
		next_cpu_addr = code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 12, install_addr);

	return next_cpu_addr;
}
