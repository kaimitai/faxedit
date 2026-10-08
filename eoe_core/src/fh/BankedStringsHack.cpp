#include "HackManager.h"
#include "fh_constants.h"
#include "fe/ROM_Manager.h"
#include "fi/fi_constants.h"
#include "common/klib/Asm6502.h"
#include <format>
#include <stdexcept>
#include <vector>

namespace {
	constexpr byte TempMessageBank{ fh::RAM::ZP_e5 };

	constexpr word MsgLoadThunk{ fh::ROM::DEADCODE_TextBox_ClosePortrait };
	constexpr word NextCharThunk{ fh::ROM::DEADCODE_TextBox_ClosePortrait + 3 };

	constexpr bool INSTALL_VERIFICATION{ false };

	void init_thunk_bytes(std::vector<byte>& p_rom) {
		const auto msgload_offset{ klib::Asm6502::get_file_offset(12, MsgLoadThunk) };
		const auto nextchar_offset{ klib::Asm6502::get_file_offset(12, NextCharThunk) };

		for (std::size_t i{ 0 }; i < 3; ++i) {
			p_rom.at(msgload_offset + i) = 0xff;
			p_rom.at(nextchar_offset + i) = 0xff;
		}
	}

	void verify_thunk_bytes(const std::vector<byte>& p_rom) {
		const auto msgload_offset{ klib::Asm6502::get_file_offset(12, MsgLoadThunk) };
		const auto nextchar_offset{ klib::Asm6502::get_file_offset(12, NextCharThunk) };

		for (std::size_t i{ 0 }; i < 3; ++i)
			if (p_rom.at(msgload_offset + i) != 0xff || p_rom.at(nextchar_offset + i) != 0xff)
				throw std::runtime_error("BankedStrings thunk area is not uninitialized");
	}

	void copy_string_bank_code(const fe::Config& config, std::vector<byte>& rom) {
		constexpr std::size_t BANK_SIZE{ 0x4000 };
		constexpr std::size_t HEADER_SIZE{ 0x10 };

		const std::size_t string_start{ config.constant(fi::c::ID_STRING_DATA_START) };
		const std::size_t string_end{ config.constant(fi::c::ID_STRING_DATA_END) };
		const std::set<byte> banks{ config.vset_as_set(fi::c::ID_STRING_BANKS) };

		const std::size_t source_bank{ (string_start - HEADER_SIZE) / BANK_SIZE };
		const std::size_t bank_start{ HEADER_SIZE + source_bank * BANK_SIZE };
		const std::size_t string_begin{ string_start - bank_start };
		const std::size_t string_finish{ string_end - bank_start };

		if (string_start < HEADER_SIZE || string_end < string_start ||
			string_finish > BANK_SIZE || bank_start + BANK_SIZE > rom.size())
			throw std::runtime_error("Invalid original string bank range");

		for (const byte bank : banks) {
			if (bank == source_bank)
				throw std::runtime_error("Cannot copy string bank onto itself");

			const std::size_t target{ HEADER_SIZE + std::size_t(bank) * BANK_SIZE };

			if (target + BANK_SIZE > rom.size())
				throw std::runtime_error(std::format("String bank {} exceeds ROM size", bank));

			std::copy_n(rom.begin() + bank_start, string_begin, rom.begin() + target);

			std::copy(rom.begin() + bank_start + string_finish,
				rom.begin() + bank_start + BANK_SIZE,
				rom.begin() + target + string_finish);
		}
	}
}

word fh::HackManager::install_BankedStrings(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, const fh::GeneralHack& p_hack) {
	if constexpr (INSTALL_VERIFICATION)
		verify_thunk_bytes(p_rom);

	const bool sram{ p_hack.bool_or("sram", false) };
	const bool copy_bank{ p_hack.bool_or("copy", false) };

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

	// copy all non-string data from the original string bank to the others
	if (copy_bank)
		copy_string_bank_code(p_config, p_rom);

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

	if constexpr (INSTALL_VERIFICATION)
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

	if constexpr (INSTALL_VERIFICATION)
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

	if constexpr (INSTALL_VERIFICATION)
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

	if constexpr (INSTALL_VERIFICATION)
		init_thunk_bytes(p_rom);

	return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 12, cpu_addr);
}
