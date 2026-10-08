#include "HackManager.h"
#include "fh_constants.h"
#include "fe/ROM_Manager.h"
#include "common/klib/Asm6502.h"
#include <format>
#include <stdexcept>
#include <vector>

namespace {
	constexpr byte TempMessageBank{ fh::RAM::ZP_e5 };

	std::vector<word> msg_load_fixups, txtbox_nextchar_fixups;
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

	// fixup script opcode calls to these entrypoints, then clear the vectors
	for (auto addr : msg_load_fixups)
		klib::Asm6502::apply_word(p_rom, messages_load_banked_addr, 12, addr);
	for (auto addr : txtbox_nextchar_fixups)
		klib::Asm6502::apply_word(p_rom, txtbox_shownextchar_addr, 12, addr);
	msg_load_fixups.clear();
	txtbox_nextchar_fixups.clear();

	if (sram) {
		install_sram_hack(p_rom, code);
		return cpu_addr;
	}

	return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 15, install_addr);
}

word fh::HackManager::apply_MsgEx(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	// string bank
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(TempMessageBank);

	// string index
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));

	code.label("@msg-load");
	code.jsr(0xffff); // will be fixed up later

	code.label("@write_loop");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_UPDATEPORTRAITANIMATION));

	code.label("@shownextchar-load");
	code.jsr(0xffff); // will be fixed up later

	code.jsr(cfg_word(p_config, c::ID_ROM_TEXT_CONTINUEGATE));
	code.bcc("@not_dismissed");
	code.jmp(ROM::IScripts_MessageFinish);

	code.label("@not_dismissed");
	code.bne("@write_loop");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	msg_load_fixups.push_back(code.label_addr("@msg-load", cpu_addr) + 1);
	txtbox_nextchar_fixups.push_back(code.label_addr("@shownextchar-load", cpu_addr) + 1);

	return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 12, cpu_addr);
}

word fh::HackManager::apply_MsgNoskipEx(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	// string bank
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(TempMessageBank);

	// string index
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));

	code.label("@msg-load");
	code.jsr(0xffff); // will be fixed up later

	code.label("@loop");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_UPDATEPORTRAITANIMATION));

	code.label("@shownextchar-load");
	code.jsr(0xffff); // will be fixed up later

	code.jsr(cfg_word(p_config, c::ID_ROM_TEXT_CHECK_CONTINUEGATE));
	code.bcc("@loop");

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	msg_load_fixups.push_back(code.label_addr("@msg-load", cpu_addr) + 1);
	txtbox_nextchar_fixups.push_back(code.label_addr("@shownextchar-load", cpu_addr) + 1);

	return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 12, cpu_addr);
}

word fh::HackManager::apply_MsgPromptEx(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 12, cpu_addr);
}

word fh::HackManager::apply_IfMsgPromptEx(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 12, cpu_addr);
}
