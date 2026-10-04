#include "HackManager.h"
#include "fh_constants.h"
#include "fe/ROM_Manager.h"
#include "common/klib/Asm6502.h"
#include <stdexcept>
#include <utility>

namespace {

	constexpr byte ValueIO{ fh::RAM::ZP_e5 };
	constexpr byte TmpScript{ fh::RAM::ZP_e6 };

	void append_FlagDoorRequirements_LookupTable(klib::Asm6502& code,
		const std::vector<std::pair<byte, std::optional<byte>>>& p_requirements) {
		code.label("@lookup_table");
		for (const auto& [flag, script] : p_requirements) {
			code.db(flag);
			code.db(script.value_or(0xff));
		}
	}

	void append_FlagDoorRequirements_TableLookup(klib::Asm6502& code) {
		code.label("@table_lookup");

		// A = door requirement (9-15)
		// convert requirement to 2-byte table offset: (requirement - 9) * 2
		code.sec();
		code.sbc_imm(0x09);
		code.asl_a();
		code.tax();

		code.lda_abs_x("@lookup_table", 1);
		code.sta_zp(TmpScript);

		code.lda_abs_x("@lookup_table");

		code.rts();
	}

	void append_FlagDoorRequirements_FlagChecker(klib::Asm6502& code) {
		code.label("@flag_check");

		// A = extended flag number
		// returns A = 0 if clear, non-zero if set
		code.tax();
		code.and_imm(0x07);
		code.tay();

		code.txa();
		code.lsr_a();
		code.lsr_a();
		code.lsr_a();
		code.tax();

		code.lda_abs_x(fh::RAM::Flags);
		code.and_abs_y("@bitmask_table");
		code.rts();

		code.label("@bitmask_table");
		code.db(0x01); code.db(0x02); code.db(0x04); code.db(0x08);
		code.db(0x10); code.db(0x20); code.db(0x40); code.db(0x80);
	}

	klib::Asm6502 build_FlagDoorRequirements(const std::vector<std::pair<byte, std::optional<byte>>>& p_requirements) {
		klib::Asm6502 code;

		append_FlagDoorRequirements_LookupTable(code, p_requirements);
		append_FlagDoorRequirements_TableLookup(code);
		append_FlagDoorRequirements_FlagChecker(code);

		code.label("@main");
		code.jsr("@table_lookup");
		code.jsr("@flag_check");
		code.rts();

		return code;
	}
}

word fh::HackManager::install_FlagDoorRequirements(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, const fh::GeneralHack& p_hack) {
	const bool sram{ p_hack.bool_or("sram", false) };

	if (sram && (p_hack.has_param("bank") || p_hack.has_param("addr")))
		throw std::runtime_error("FlagDoorRequirements: sram cannot be combined with bank or addr");

	const byte bank{ p_hack.byte_or("bank", 15) };
	word install_addr{ cpu_addr };

	if (sram) {
		install_addr = sram_hack_addr();
	}
	else if (bank != 15) {
		install_addr = p_hack.has_param("addr") ? p_hack.get_word("addr") :
			fe::ROM_Manager::find_trailing_free_cpu_addr(p_rom, bank, 0xff, 16);
	}

	const auto req_data{ p_hack.split_byte_optional_byte("data") };
	if (req_data.empty() || req_data.size() > 7)
		throw std::runtime_error("FlagDoorRequirements requires data entry count 1-7");

	auto free_bank_code{ build_FlagDoorRequirements(req_data) };

	const word main_entry_addr{ free_bank_code.label_addr("@main", install_addr) };
	if (sram) {
		install_sram_hack(p_rom, free_bank_code);
	}
	else {
		const word next_addr{
			free_bank_code.apply_hack_and_clear_get_next_cpu_addr(
				p_rom, bank, install_addr)
		};

		if (bank == 15)
			cpu_addr = next_addr;
	}

	const word handler_addr{ sram ? sram_hack_addr() : cpu_addr };

	klib::Asm6502 code;

	// install hook
	code.jmp(handler_addr);
	code.apply_hack_and_clear(p_rom, 15, ROM::Game_RunDoorRequirementHandler_BEQ_RTS);

	// new routine
	code.cmp_imm(0x09);
	code.bcs("@extended");
	code.asl_a();
	code.bne("@vanilla");
	code.rts();

	code.label("@vanilla");
	code.jmp(ROM::Game_RunDoorRequirementHandler_TAY);

	code.label("@extended");

	if (bank == 15 || sram) {
		code.jsr(main_entry_addr);
	}
	else {
		// main_entry_addr expects A = door requirement (9-15) so preserve it across the bank switch
		code.sta_zp(ValueIO);

		code.lda_abs(fh::RAM::CurrentROMBank);
		code.pha();

		code.ldx_imm(bank);
		code.jsr(cfg_word(p_config, c::ID_ROM_MMC1_UPDATEROMBANK));

		code.lda_zp(ValueIO);
		code.jsr(main_entry_addr);
		// output: preserve flag result across bank restore
		code.sta_zp(ValueIO);

		code.pla();
		code.tax();
		code.jsr(cfg_word(p_config, c::ID_ROM_MMC1_UPDATEROMBANK));

		code.lda_zp(ValueIO);
	}

	code.beq("@requirement_failed");
	code.jmp(ROM::Game_UnlockDoorWithSoundEffect);

	code.label("@requirement_failed");
	code.lda_zp(TmpScript);
	code.cmp_imm(0xff);
	code.beq("@return");

	// run configured failure iScript
	code.jsr(cfg_word(p_config, c::ID_ROM_VANILLA_FAR_CALL));
	code.db(12);
	code.dw(ROM::IScripts_Begin - 1);

	code.label("@return");
	code.rts();

	if (sram) {
		install_sram_hack(p_rom, code);
		return cpu_addr;
	}

	return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 15, handler_addr);
}
