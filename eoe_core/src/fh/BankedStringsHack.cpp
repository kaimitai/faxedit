#include "HackManager.h"
#include "fh_constants.h"
#include "fe/ROM_Manager.h"
#include "common/klib/Asm6502.h"
#include <format>
#include <stdexcept>
#include <vector>

namespace {
	constexpr byte TempMessageBank{ fh::RAM::ZP_e5 };

	constexpr word MsgLoadThunk{ fh::ROM::DEADCODE_TextBox_ClosePortrait };
	constexpr word NextCharThunk{ fh::ROM::DEADCODE_TextBox_ClosePortrait + 3 };

	void init_thunk_bytes(std::vector<byte>& p_rom) {
		for (std::size_t i{ 0 }; i < 3; ++i) {
			p_rom.at(MsgLoadThunk + i) = 0xff;
			p_rom.at(NextCharThunk + i) = 0xff;
		}
	}

	void verify_thunk_bytes(const std::vector<byte>& p_rom) {
		for (std::size_t i{ 0 }; i < 3; ++i)
			if (p_rom.at(MsgLoadThunk + i) != 0xff || p_rom.at(NextCharThunk + i) != 0xff)
				throw std::runtime_error("BankedStrings thunk area is not uninitialized");
	}
}

word fh::HackManager::install_BankedStrings(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, const fh::GeneralHack& p_hack) {
	verify_thunk_bytes(p_rom);

	const bool sram{ p_hack.bool_or("sram", false) };

	const word install_addr{ sram ? sram_hack_addr() : cpu_addr };

	klib::Asm6502 code;

	code.label("Messages_Load-Banked");
	code.sta_abs(RAM::StringID);
	code.lda_abs(RAM::CurrentROMBank);
	code.pha();
	code.ldx_zp(TempMessageBank);
	code.jmp(cfg_word(p_config, c::ID_ROM_MESSAGES_LOAD_JSR_MMC1_UPDATEROMBANK));

	code.label("TextBox_ShowNextChar-Banked");
	code.lda_abs(RAM::CurrentROMBank);
	code.pha();
	code.ldx_zp(TempMessageBank);
	code.jmp(cfg_word(p_config, c::ID_ROM_TEXTBOX_SHOWNEXTCHAR_JSR_MMC1_UPDATEROMBANK));

	const word messages_load_banked_addr{ code.label_addr("Messages_Load-Banked", install_addr) };
	const word txtbox_shownextchar_addr{ code.label_addr("TextBox_ShowNextChar-Banked", install_addr) };

	// update thunks
	klib::Asm6502 thunk_code;
	thunk_code.jmp(messages_load_banked_addr);
	thunk_code.jmp(txtbox_shownextchar_addr);
	thunk_code.apply_hack_and_clear(p_rom, 12, MsgLoadThunk);

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
	code.jsr(MsgLoadThunk);

	code.label("@write_loop");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_UPDATEPORTRAITANIMATION));
	code.jsr(NextCharThunk);

	code.jsr(cfg_word(p_config, c::ID_ROM_TEXT_CONTINUEGATE));
	code.bcc("@not_dismissed");
	code.jmp(ROM::IScripts_MessageFinish);

	code.label("@not_dismissed");
	code.bne("@write_loop");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	init_thunk_bytes(p_rom);
	return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 12, cpu_addr);
}

word fh::HackManager::apply_MsgNoskipEx(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	// string bank
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(TempMessageBank);

	// string index
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.jsr(MsgLoadThunk);

	code.label("@loop");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_UPDATEPORTRAITANIMATION));
	code.jsr(NextCharThunk);

	code.jsr(cfg_word(p_config, c::ID_ROM_TEXT_CHECK_CONTINUEGATE));
	code.bcc("@loop");

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	init_thunk_bytes(p_rom);
	return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 12, cpu_addr);
}

word fh::HackManager::apply_MsgPromptEx(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	// string bank
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(TempMessageBank);

	// string index
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.jsr(MsgLoadThunk);

	code.label("@message_loop");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_UPDATEPORTRAITANIMATION));
	code.jsr(NextCharThunk);

	code.jsr(cfg_word(p_config, c::ID_ROM_TEXT_QUESTION_CONTINUEGATE));

	code.bcc("@not_dismissed");
	code.jmp(ROM::IScripts_MessageFinish);

	code.label("@not_dismissed");
	code.bne("@message_loop");

	// vanilla US/EU behavior; JP omits this call
	code.jsr(ROM::IScripts_PositionAndFillPlaceholderText);

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	init_thunk_bytes(p_rom);
	return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 12, cpu_addr);
}

word fh::HackManager::apply_IfMsgPromptEx(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	// string bank
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(TempMessageBank);

	// string index
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.jsr(MsgLoadThunk);

	code.label("@message_loop");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_UPDATEPORTRAITANIMATION));
	code.jsr(NextCharThunk);

	code.jsr(cfg_word(p_config, c::ID_ROM_TEXT_QUESTION_CONTINUEGATE));

	code.bcc("@not_dismissed");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	code.label("@not_dismissed");
	code.bne("@message_loop");

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	init_thunk_bytes(p_rom);
	return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 12, cpu_addr);
}
