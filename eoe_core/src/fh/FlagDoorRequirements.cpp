#include "HackManager.h"
#include "fh_constants.h"
#include "common/klib/Asm6502.h"
#include <stdexcept>

namespace {

	constexpr byte TmpScript{ fh::RAM::ZP_e2 };

	word install_FlagDoorRequirements_LookupTable(std::vector<byte>& p_rom, byte p_bank, word cpu_addr,
		const std::vector<std::pair<byte, std::optional<byte>>>& p_requirements) {
		klib::Asm6502 code;

		for (const auto& [flag, script] : p_requirements) {
			code.db(flag);
			code.db(script.value_or(0xff));
		}

		return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, p_bank, cpu_addr);
	}

	word install_FlagDoorRequirements_TableLookup(std::vector<byte>& p_rom, byte p_bank, word cpu_addr,
		word p_table_addr) {
		klib::Asm6502 code;

		// A = door requirement (9-15)
		// convert requirement to 2-byte table offset: (requirement - 9) * 2
		code.sec();
		code.sbc_imm(0x09);
		code.asl_a();
		code.tax();

		code.lda_abs_x(p_table_addr + 1);
		code.sta_zp(TmpScript);

		code.lda_abs_x(p_table_addr);

		code.rts();

		return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, p_bank, cpu_addr);
	}

	word install_FlagDoorRequirements_FlagChecker(std::vector<byte>& p_rom, byte p_bank, word cpu_addr) {
		klib::Asm6502 code;

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

		return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, p_bank, cpu_addr);
	}

}

word fh::HackManager::install_FlagDoorRequirements(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, const fh::GeneralHack& p_hack) const {
	constexpr byte BANK{ 15 };

	const auto req_data{ p_hack.split_byte_optional_byte("data") };
	if (req_data.empty() || req_data.size() > 7)
		throw std::runtime_error("FlagDoorRequirements requires data entry count 1-7");

	const word lookup_table_addr{ cpu_addr };
	cpu_addr = install_FlagDoorRequirements_LookupTable(p_rom, BANK, lookup_table_addr, req_data);

	const word table_lookup_addr{ cpu_addr };
	cpu_addr = install_FlagDoorRequirements_TableLookup(p_rom, BANK, table_lookup_addr, lookup_table_addr);

	const word flag_check_addr{ cpu_addr };
	cpu_addr = install_FlagDoorRequirements_FlagChecker(p_rom, BANK, flag_check_addr);

	const word handler_addr{ cpu_addr };

	klib::Asm6502 code;

	// install hook
	code.jmp(handler_addr);
	code.apply_hack_and_clear(p_rom, BANK, ROM::Game_RunDoorRequirementHandler_BEQ_RTS);

	// new routine
	code.cmp_imm(0x09);
	code.bcs("@extended");
	code.asl_a();
	code.bne("@vanilla");
	code.rts();

	code.label("@vanilla");
	code.jmp(ROM::Game_RunDoorRequirementHandler_TAY);

	code.label("@extended");
	code.jsr(table_lookup_addr);
	code.jsr(flag_check_addr);
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

	return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, BANK, handler_addr);
}
