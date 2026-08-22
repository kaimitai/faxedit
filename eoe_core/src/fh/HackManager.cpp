#include "HackManager.h"
#include "fh_constants.h"
#include "fe/fe_constants.h"
#include "common/klib/Asm6502.h"
#include "common/klib/Kstring.h"
#include <algorithm>
#include <cassert>
#include <set>
#include <stdexcept>

// shared helpers for script library hacks

// reads the next script operand as a flag number, stores the byte number (relative to start of flags block)
// in X, and the bit number (0-7) in that byte in Y
word fh::HackManager::apply_helper_DecodeScriptFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = flag no

	code.pha(); // save original flag
	code.lsr_a(3); // A = flag no / 8
	code.tax(); // X = byte index

	code.pla();
	code.and_imm(0x07); // A = bit number
	code.tay();

	code.rts();

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// reads the next script operand as a quest flag number (0-7)
// and stores the corresponding bit number in Y
word fh::HackManager::apply_helper_DecodeQuestFlag(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = quest flag no
	code.and_imm(0x07); // A = bit number
	code.tay();

	code.rts();

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// if A equals the operand, jump - otherwise continue script execution
word fh::HackManager::apply_helper_IfAEquals(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr) const {
	klib::Asm6502 code;

	code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = operand (comparison value)
	code.sta_zp(RAM::ZP_Temp07);
	code.pla();
	code.cmp_zp(RAM::ZP_Temp07);
	code.beq("@equal");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	code.label("@equal");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// if min <= A <= max, jump - otherwise continue script execution
word fh::HackManager::apply_helper_IfABetween(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr) const {
	klib::Asm6502 code;

	// preserve value to test
	code.pha();

	// load minimum
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(RAM::ZP_e2);

	// load maximum
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(RAM::ZP_e3);

	// restore value to test
	code.pla();

	// if A < min -> fail
	code.cmp_zp(RAM::ZP_e2);
	code.bcc("@fail");

	// if A <= max -> success
	code.cmp_zp(RAM::ZP_e3);
	code.beq("@success");
	code.bcc("@success");

	code.label("@fail");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	code.label("@success");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// Converts the player's current pixel position to a block position.
// Returns: X = (Y_block << 4) | X_block
word fh::HackManager::apply_helper_GetPlayerBlockPos(std::vector<byte>& p_rom,
	word cpu_addr) const {
	klib::Asm6502 code;

	code.lda_zp(RAM::ZP_PlayerPosX);
	code.clc();
	code.adc_imm(0x07);                 // normalize to player center
	code.sta_zp(RAM::ZP_PlayerPosArgX);

	code.lda_zp(RAM::ZP_PlayerPosY);
	code.sta_zp(RAM::ZP_PlayerPosArgY);

	code.jsr(ROM::Area_ConvertPixelsToBlockPos);

	code.rts();

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// helper which reads the next script operand as a 16-bit cpu address and stores it in ($e2,$e3)
word fh::HackManager::apply_helper_LoadWord(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr) const {
	klib::Asm6502 code;

	// lo byte
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(RAM::ZP_e2);

	// hi byte
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(RAM::ZP_e3);

	code.rts();

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// script library hacks
word fh::HackManager::apply_SetFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, word flag_decode_helper_addr, word bitmask_table_addr) const {
	klib::Asm6502 code;

	code.jsr(flag_decode_helper_addr);

	code.lda_abs_x(RAM::Flags);
	code.ora_abs_y(bitmask_table_addr);
	code.sta_abs_x(RAM::Flags);

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_ClearFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, word flag_decode_helper_addr, word bitmask_table_addr) const {
	klib::Asm6502 code;

	code.jsr(flag_decode_helper_addr);

	code.lda_abs_x(RAM::Flags);
	code.sta_zp(RAM::ZP_Temp07);

	code.lda_abs_y(bitmask_table_addr);
	code.eor_imm(0xff); // invert mask

	code.and_zp(RAM::ZP_Temp07);
	code.sta_abs_x(RAM::Flags);

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_IfFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, word flag_decode_helper_addr, word bitmask_table_addr) const {
	klib::Asm6502 code;

	code.jsr(flag_decode_helper_addr);

	code.lda_abs_x(RAM::Flags);
	code.and_abs_y(bitmask_table_addr);
	code.bne("@set");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	code.label("@set");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_SelectFlag(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	const word SelectedFlagRamAddr{ cfg_word(p_config, c::ID_HACK_SCRIPT_SELECTED_FLAG_RAM_ADDR) };

	// A = flag number
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	// remember the selected flag number
	code.sta_mem(SelectedFlagRamAddr);
	// continue executing the script
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_SetSelectedFlag(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr, word bitmask_table_addr) const {
	klib::Asm6502 code;

	const word SelectedFlagRamAddr{ cfg_word(p_config, c::ID_HACK_SCRIPT_SELECTED_FLAG_RAM_ADDR) };

	// A = selected flag number
	code.lda_mem(SelectedFlagRamAddr);

	// if no flag has been selected, do nothing
	code.cmp_imm(0xff);
	code.beq("@done");

	// decode flag number -> X = byte index, Y = bit index
	code.pha();
	code.lsr_a(3);
	code.tax();

	code.pla();
	code.and_imm(0x07);
	code.tay();

	// set the bit
	code.lda_abs_x(RAM::Flags);
	code.ora_abs_y(bitmask_table_addr);
	code.sta_abs_x(RAM::Flags);

	code.label("@done");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_ClearSelectedFlag(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr, word bitmask_table_addr) const {
	klib::Asm6502 code;

	const word SelectedFlagRamAddr{ cfg_word(p_config, c::ID_HACK_SCRIPT_SELECTED_FLAG_RAM_ADDR) };

	// A = selected flag number
	code.lda_mem(SelectedFlagRamAddr);

	// if no flag has been selected, do nothing
	code.cmp_imm(0xff);
	code.beq("@done");

	// decode flag number -> X = byte index, Y = bit index
	code.pha();
	code.lsr_a(3);
	code.tax();

	code.pla();
	code.and_imm(0x07);
	code.tay();

	// clear the bit
	code.lda_abs_x(RAM::Flags);
	code.sta_zp(RAM::ZP_Temp07);

	code.lda_abs_y(bitmask_table_addr);
	code.eor_imm(0xff);

	code.and_zp(RAM::ZP_Temp07);
	code.sta_abs_x(RAM::Flags);

	code.label("@done");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_IfSelectedFlag(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr, word bitmask_table_addr) const {
	klib::Asm6502 code;

	const word SelectedFlagRamAddr{ cfg_word(p_config, c::ID_HACK_SCRIPT_SELECTED_FLAG_RAM_ADDR) };

	// A = selected flag number
	code.lda_mem(SelectedFlagRamAddr);

	// if no flag has been selected, treat it as clear
	code.cmp_imm(0xff);
	code.beq("@clear");

	// decode flag number -> X = byte index, Y = bit index
	code.pha();
	code.lsr_a(3);
	code.tax();

	code.pla();
	code.and_imm(0x07);
	code.tay();

	// test the bit
	code.lda_abs_x(RAM::Flags);
	code.and_abs_y(bitmask_table_addr);
	code.bne("@set");

	code.label("@clear");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	code.label("@set");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_SetQuestFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, word quest_flag_decode_helper_addr, word bitmask_table_addr) const {
	klib::Asm6502 code;

	code.jsr(quest_flag_decode_helper_addr);

	code.lda_abs(RAM::QuestFlags);
	code.ora_abs_y(bitmask_table_addr);
	code.sta_abs(RAM::QuestFlags);

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_ClearQuestFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, word quest_flag_decode_helper_addr, word bitmask_table_addr) const {
	klib::Asm6502 code;

	code.jsr(quest_flag_decode_helper_addr);

	code.lda_abs(RAM::QuestFlags);
	code.sta_zp(RAM::ZP_Temp07);

	code.lda_abs_y(bitmask_table_addr);
	code.eor_imm(0xff); // invert mask

	code.and_zp(RAM::ZP_Temp07);
	code.sta_abs(RAM::QuestFlags);

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_IfQuestFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, word quest_flag_decode_helper_addr, word bitmask_table_addr) const {
	klib::Asm6502 code;

	code.jsr(quest_flag_decode_helper_addr);

	code.lda_abs(RAM::QuestFlags);
	code.and_abs_y(bitmask_table_addr);
	code.bne("@set");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	code.label("@set");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// runs custom screen handler - data-driven tilemap changes (by default handler index 3)
// we can certainly assume that RAM::CurrentScreen_SpecialEventID is 0xff when this is invoked
// so we are not storing and restoring it
word fh::HackManager::apply_RunScreenHandler(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr) const {
	klib::Asm6502 code;

	code.lda_imm(cfg_byte(p_config, c::ID_TM_CHANGE_HANDLER_IDX));
	code.sta_abs(RAM::CurrentScreen_SpecialEventID);

	code.jsr(ROM::GameLoop_RunScreenEventHandlers);

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_GetXP(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = xp lo byte
	code.sta_zp(RAM::ZP_Temp_Int24_L);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = xp hi byte
	code.sta_zp(RAM::ZP_Temp_Int24_M);
	code.jsr(cfg_word(p_config, c::ID_ROM_PLAYER_UPDATEEXPERIENCE));

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_IfWorld(std::vector<byte>& p_rom, word cpu_addr, word helper_if_a_equals_addr) const {
	klib::Asm6502 code;

	code.lda_zp(RAM::ZP_CurrentWorld);
	code.jmp(helper_if_a_equals_addr);

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_IfScreen(std::vector<byte>& p_rom, word cpu_addr, word helper_if_a_equals_addr) const {
	klib::Asm6502 code;

	code.lda_zp(RAM::ZP_CurrentScreen);
	code.jmp(helper_if_a_equals_addr);

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_IfStage(std::vector<byte>& p_rom, word cpu_addr, word helper_if_a_equals_addr) const {
	klib::Asm6502 code;

	code.lda_abs(RAM::CurrentStage);
	code.jmp(helper_if_a_equals_addr);

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_Die(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.lda_imm(0x01);
	code.sta_abs(RAM::PlayerIsDead);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_JSR(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	const word ScriptReturn_Lo_ram_addr{ cfg_word(p_config, c::ID_HACK_SCRIPT_JSR_RAM_ADDR_LO) };
	const word ScriptReturn_Hi_ram_addr{ cfg_word(p_config, c::ID_HACK_SCRIPT_JSR_RAM_ADDR_HI) };

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = target lo
	code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = target hi
	code.pha();

	// compute return address = base + offset
	code.clc();
	code.lda_zp(RAM::ZP_ScriptAddr);
	code.adc_zp(RAM::ZP_ScriptOffset);
	code.sta_mem(ScriptReturn_Lo_ram_addr);

	code.lda_zp(RAM::ZP_ScriptAddrU);
	code.adc_imm(0x00);
	code.sta_mem(ScriptReturn_Hi_ram_addr);

	// restore target
	code.pla();
	code.sta_zp(RAM::ZP_ScriptAddrU);
	code.pla();
	code.sta_zp(RAM::ZP_ScriptAddr);
	code.lda_imm(0x00);
	code.sta_zp(RAM::ZP_ScriptOffset);

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_Return(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	const word ScriptReturn_Lo_ram_addr{ cfg_word(p_config, c::ID_HACK_SCRIPT_JSR_RAM_ADDR_LO) };
	const word ScriptReturn_Hi_ram_addr{ cfg_word(p_config, c::ID_HACK_SCRIPT_JSR_RAM_ADDR_HI) };

	// retrieve lo addr
	code.lda_mem(ScriptReturn_Lo_ram_addr);
	code.sta_zp(RAM::ZP_ScriptAddr);

	// retrieve hi addr
	code.lda_mem(ScriptReturn_Hi_ram_addr);
	code.sta_zp(RAM::ZP_ScriptAddrU);

	// set offset to 0 - as the JSR normalized the target address
	code.lda_imm(0x00);
	code.sta_zp(RAM::ZP_ScriptOffset);

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_ForceDoor(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	// the requirement check has already failed and invoked this script
	// override that result so the caller proceeds through the door afterall
	code.lda_imm(0x00);
	code.sta_abs(RAM::DoorKeyRequirement);

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_IfYX(std::vector<byte>& p_rom, word cpu_addr,
	word helper_get_player_block_pos_addr, word helper_if_a_equals_addr) const {
	klib::Asm6502 code;

	code.jsr(helper_get_player_block_pos_addr); // X = packed YX
	code.txa();                                 // A = packed YX
	code.jmp(helper_if_a_equals_addr);          // compare with script operand

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_IfDoorYX(std::vector<byte>& p_rom, word cpu_addr,
	word helper_if_a_equals_addr) const {
	klib::Asm6502 code;

	code.lda_zp(RAM::ZP_DoorBlockPos);
	code.jmp(helper_if_a_equals_addr);

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_IfAddrEquals(std::vector<byte>& p_rom, word cpu_addr,
	word helper_load_word_addr, word helper_if_a_equals_addr) const {
	klib::Asm6502 code;

	code.jsr(helper_load_word_addr);
	code.ldy_imm(0x00);
	code.lda_ind_y(RAM::ZP_e2);
	code.jmp(helper_if_a_equals_addr);

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_IfAddrBetween(std::vector<byte>& p_rom, word cpu_addr,
	word helper_load_word_addr, word helper_if_a_between_addr) const {
	klib::Asm6502 code;

	code.jsr(helper_load_word_addr);
	code.ldy_imm(0x00);
	code.lda_ind_y(RAM::ZP_e2);
	code.jmp(helper_if_a_between_addr);

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_SetAddr(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, word helper_load_word_addr) const {
	klib::Asm6502 code;

	// ($e2,$e3) = destination address
	code.jsr(helper_load_word_addr);

	// value operand
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));

	code.ldy_imm(0x00);
	code.sta_ind_y(RAM::ZP_e2);

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevSetVar(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	const word Vars{ cfg_word(p_config, c::ID_HACK_SCRIPT_VAR_RAM_ADDR) };
	const byte Count{ cfg_byte(p_config, c::ID_HACK_SCRIPT_VAR_COUNT) };

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.tsx();
	code.lda_abs_x(0x0102); code.cmp_imm(Count); code.bcs("@done");
	code.tay(); code.lda_abs_x(0x0101); code.db(0x99); code.dw(Vars);
	code.label("@done"); code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevAddVar(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	const word Vars{ cfg_word(p_config, c::ID_HACK_SCRIPT_VAR_RAM_ADDR) };
	const byte Count{ cfg_byte(p_config, c::ID_HACK_SCRIPT_VAR_COUNT) };

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.tsx();
	code.lda_abs_x(0x0102); code.cmp_imm(Count); code.bcs("@done");
	code.tay(); code.lda_abs_x(0x0101); code.clc();
	code.db(0x79); code.dw(Vars); // ADC Vars,Y
	code.db(0x99); code.dw(Vars); // STA Vars,Y
	code.label("@done"); code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevSubVar(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	const word Vars{ cfg_word(p_config, c::ID_HACK_SCRIPT_VAR_RAM_ADDR) };
	const byte Count{ cfg_byte(p_config, c::ID_HACK_SCRIPT_VAR_COUNT) };

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.tsx();
	code.lda_abs_x(0x0102); code.cmp_imm(Count); code.bcs("@done");
	code.tay(); code.lda_abs_y(Vars); code.sec();
	code.db(0xfd); code.dw(0x0101); // SBC $0101,X
	code.db(0x99); code.dw(Vars);   // STA Vars,Y
	code.label("@done"); code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevIfVarEqual(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	const word Vars{ cfg_word(p_config, c::ID_HACK_SCRIPT_VAR_RAM_ADDR) };
	const byte Count{ cfg_byte(p_config, c::ID_HACK_SCRIPT_VAR_COUNT) };

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.tsx();
	code.lda_abs_x(0x0102); code.cmp_imm(Count); code.bcs("@false");
	code.tay(); code.lda_abs_y(Vars); code.cmp_abs_x(0x0101); code.bne("@false");
	code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));
	code.label("@false"); code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevIfVarLess(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	const word Vars{ cfg_word(p_config, c::ID_HACK_SCRIPT_VAR_RAM_ADDR) };
	const byte Count{ cfg_byte(p_config, c::ID_HACK_SCRIPT_VAR_COUNT) };

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.tsx();
	code.lda_abs_x(0x0102); code.cmp_imm(Count); code.bcs("@false");
	code.tay(); code.lda_abs_y(Vars); code.cmp_abs_x(0x0101); code.bcs("@false");
	code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));
	code.label("@false"); code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevIfVarGreaterEqual(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	const word Vars{ cfg_word(p_config, c::ID_HACK_SCRIPT_VAR_RAM_ADDR) };
	const byte Count{ cfg_byte(p_config, c::ID_HACK_SCRIPT_VAR_COUNT) };

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.tsx();
	code.lda_abs_x(0x0102); code.cmp_imm(Count); code.bcs("@false");
	code.tay(); code.lda_abs_y(Vars); code.cmp_abs_x(0x0101); code.bcc("@false");
	code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));
	code.label("@false"); code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

namespace {

	// The AtlasDev visual effects declare their signature by name in the
	// configuration's iscript_opcode_impls table, so the emitter asks the
	// configuration how many operands the project wants rather than baking a
	// single arity into the code.  One Byte operand emits the original
	// one-operand handler; the tuned form emits the configurable one.
	std::size_t atlasdev_operand_count(const fe::Config& p_config, const std::string& p_impl) {
		const auto impls{ klib::str::to_lowercase_string_map(
			p_config.str_map("iscript_opcode_impls")) };
		const auto it{ impls.find(klib::str::to_lower(p_impl)) };
		if (it == end(impls))
			throw std::runtime_error("No signature declared for implementation " + p_impl);

		std::size_t result{ 0 };
		for (const auto& part : klib::str::split_string(it->second, ',')) {
			const auto entry{ klib::str::to_lower(klib::str::trim(part)) };
			if (entry.starts_with("args="))
				result = klib::str::split_string(entry.substr(5), '+').size();
			else if (entry.starts_with("argtype=") && entry != "argtype=none")
				result = 1;
		}

		return result;
	}

	std::size_t atlasdev_arity(const fe::Config& p_config, const std::string& p_impl,
		std::size_t p_tuned) {
		const auto result{ atlasdev_operand_count(p_config, p_impl) };
		if (result != 1 && result != p_tuned)
			throw std::runtime_error(p_impl +
				" must declare exactly one Byte operand (legacy form) or its full tuned signature");
		return result;
	}

}

// AtlasDevShakeScreen Frames [Amplitude Period]
//
// Frames    total NMI frames the effect blocks for; 0 is a no-op.
// Amplitude offset magnitude in pixels added to and subtracted from the entry
//           scroll byte $0C.  1..8 read as a shake; 0 holds still; values
//           above ~16 read as a jump cut rather than a shake.  The legacy
//           one-operand form is Amplitude 2.
// Period    NMI frames between direction flips.  1 is the legacy per-frame
//           alternation; 4..8 read as a sway.  0 means "never flip inside a
//           255-frame effect" (the countdown wraps), i.e. a static offset.
//
// Presets, each one checked on hardware:
//   SHAKE_RUMBLE    60 1 1     low buzz, minimum visible amplitude
//   SHAKE_CLASSIC   60 2 1     same motion as the one-operand form
//   SHAKE_HEAVY     60 4 1     heavy impact
//   SHAKE_SLOW_SWAY 60 3 6     slow sway; reads as swimming, not impact
//   SHAKE_QUAKE     90 8 2     long, wide earthquake
//
// The entry value of $0C is restored exactly on completion in both forms.
// Interiors whose non-visible nametable page holds a stale screen strobe that
// page on the inward phase; that is kept deliberately, it gives a second
// effect for free, so treat it as documented behaviour rather than a defect.
word fh::HackManager::apply_AtlasDevShakeScreen(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	const auto operands{ atlasdev_arity(p_config, "AtlasDevShakeScreen", 3) };

	if (operands == 1) {
		code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // total frames
		code.cmp_imm(0x00);                                       // LoadByte returns Z from STY
		code.beq("@done");
		code.pha();                                                // stack: count
		code.lda_mem(0x000c); code.pha();                         // stack: base,count
		code.label("@frame");
		code.tsx(); code.lda_abs_x(0x0102); code.and_imm(0x01); // TSX; count parity
		code.beq("@left");
		code.lda_abs_x(0x0101); code.clc(); code.adc_imm(0x02);
		code.sta_mem(0x000c); code.lda_imm(0x01); code.bne("@wait");
		code.label("@left");
		code.lda_abs_x(0x0101); code.sec(); code.sbc_imm(0x02); // SBC #2
		code.sta_mem(0x000c);
		code.label("@wait");
		code.jsr(ROM::WaitForInterrupt);                           // NMI owns $2005
		code.tsx(); code.dec_abs_x(0x0102);             // TSX; DEC count,X
		code.bne("@frame");
		code.lda_abs_x(0x0101); code.sta_mem(0x000c);              // exact restoration
		code.pla(); code.pla();
		code.label("@done");
		code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
		return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
	}

	// Every operand must be consumed before any early exit, otherwise the
	// script cursor would resume inside this instruction's operand bytes.
	// Stack after the six pushes, with X from TSX:
	//   phase=$0101,X dir=$0102,X base=$0103,X
	//   period=$0104,X amplitude=$0105,X frames=$0106,X
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha(); // frames
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha(); // amplitude
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha(); // period
	code.tsx(); code.lda_abs_x(0x0103);                    // TSX; A = frames
	code.bne("@run");
	code.pla(); code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	code.label("@run");
	code.lda_mem(0x000c); code.pha();                          // entry scroll
	code.lda_imm(0x01); code.pha();                            // outward phase first
	code.tsx(); code.lda_abs_x(0x0103); code.pha();         // phase = period
	code.label("@frame");
	code.tsx(); code.lda_abs_x(0x0102);                     // TSX; direction
	code.beq("@inward");
	code.lda_abs_x(0x0103); code.clc(); code.adc_abs_x(0x0105); // base + amplitude
	code.clc(); code.bcc("@store");
	code.label("@inward");
	code.lda_abs_x(0x0103); code.sec(); code.sbc_abs_x(0x0105); // base - amplitude
	code.label("@store");
	code.sta_mem(0x000c);                                      // NMI owns $2005
	code.jsr(ROM::WaitForInterrupt);
	code.tsx(); code.dec_abs_x(0x0101);             // TSX; DEC phase,X
	code.bne("@tick");
	code.lda_abs_x(0x0104); code.sta_abs_x(0x0101);            // reload phase
	code.lda_abs_x(0x0102); code.eor_imm(0x01); code.sta_abs_x(0x0102);
	code.label("@tick");
	code.dec_abs_x(0x0106);                            // DEC frames,X
	code.bne("@frame");
	code.lda_abs_x(0x0103); code.sta_mem(0x000c);              // exact restoration
	code.pla(); code.pla(); code.pla(); code.pla(); code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevFadeOut Frames [Depth] / AtlasDevFadeIn Frames [Depth]
//
// Frames  total NMI frames; 0 publishes the terminal state immediately.
// Depth   how many of the four vanilla fade stages to traverse, 1..4.
//         4 is the legacy full fade to black.  1..3 stop at a partial
//         darkness that is exactly reversible, because vanilla $D0AD always
//         recomputes each stage from the selected source palette at $03D0
//         rather than from the live palette: stage k is an absolute image,
//         not an accumulated subtraction.
//
// Presets, each one checked on hardware:
//   FADE_DIM   45 1     barely dim; keeps the room readable
//   FADE_DUSK  45 2     dusk
//   FADE_GLOOM 45 3     deep gloom, shapes still visible
//   FADE_BLACK 60 4     full fade to black
//
// Both handlers distribute Depth stages over Frames NMI frames with an
// integer error accumulator, so short durations skip shades rather than
// silently lengthening the effect.  The accumulator is byte-wide and its
// carry is preserved, which is what keeps durations $FD..$FF exact.
//
// Sprites are deliberately untouched: these handlers drive the vanilla
// background-palette path, exactly as vanilla's own Screen_FadeToBlack does
// at all five of its call sites.  See the docs for why no Sprites operand
// ships.
//
// Stack after the pushes, with X from TSX:
//   stage=$0101,X error=$0102,X total=$0103,X depth=$0104,X remaining=$0105,X
word fh::HackManager::apply_AtlasDevFadeOut(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	const auto operands{ atlasdev_arity(p_config, "AtlasDevFadeOut", 2) };

	if (operands == 1) {
		code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
		code.cmp_imm(0x00); // LoadByte's final STY means its incoming Z is not A's Z
		code.bne("@timed");
		code.lda_imm(0x03); code.sta_mem(0x0430);
		code.jsr(ROM::PPUBuffer_WaitEmpty); code.lda_mem(0x03d0);
		code.jsr(ROM::Screen_SetFadePalette);
		code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
		code.label("@timed");
		code.pha(); code.pha(); code.lda_imm(0); code.pha(); code.pha();
		code.label("@frame");
		code.tsx(); code.lda_abs_x(0x0102); code.clc(); code.adc_imm(4);
		code.bcs("@subtract"); // preserve the ninth bit for totals $FD..$FF
		code.label("@advance");
		code.cmp_abs_x(0x0103); code.bcc("@calculated"); code.sec();
		code.label("@subtract");
		code.sbc_abs_x(0x0103); code.sta_abs_x(0x0102);
		code.lda_abs_x(0x0101); code.clc(); code.adc_imm(1); code.sta_abs_x(0x0101);
		code.lda_abs_x(0x0102); code.bcc("@advance");
		code.label("@calculated");
		code.sta_abs_x(0x0102); code.lda_abs_x(0x0101); code.beq("@wait");
		code.sec(); code.sbc_imm(1); code.sta_mem(0x0430);
		code.jsr(ROM::PPUBuffer_WaitEmpty); code.lda_mem(0x03d0);
		code.jsr(ROM::Screen_SetFadePalette);
		code.label("@wait");
		code.jsr(ROM::WaitForInterrupt); code.tsx(); code.dec_abs_x(0x0104);
		code.bne("@frame");
		code.pla(); code.pla(); code.pla(); code.pla();
		code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
		return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
	}

	// Stack after the pushes, with X from TSX:
	//   stage=$0101,X error=$0102,X total=$0103,X depth=$0104,X remaining=$0105,X
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha(); // frames
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));            // depth
	// Vanilla's own fade guards the four-entry delta table at $D0E0 the same
	// way ($DA42); an unclamped stage index would read code bytes as deltas.
	code.cmp_imm(5); code.bcc("@ceiling"); code.lda_imm(4);
	code.label("@ceiling");
	code.cmp_imm(1); code.bcs("@floor"); code.lda_imm(1);
	code.label("@floor");
	code.pha();
	code.tsx(); code.lda_abs_x(0x0102);                    // TSX; A = frames
	code.bne("@timed");
	code.lda_abs_x(0x0101); code.sec(); code.sbc_imm(1); // terminal stage
	code.sta_mem(0x0430);
	code.jsr(ROM::PPUBuffer_WaitEmpty); code.lda_mem(0x03d0);
	code.jsr(ROM::Screen_SetFadePalette);
	code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	code.label("@timed");
	code.pha(); code.lda_imm(0); code.pha(); code.pha();       // total, error, stage
	code.label("@frame");
	code.tsx(); code.lda_abs_x(0x0102); code.clc(); code.adc_abs_x(0x0104);
	code.bcs("@subtract"); // preserve the ninth bit for totals $FD..$FF
	code.label("@advance");
	code.cmp_abs_x(0x0103); code.bcc("@calculated"); code.sec();
	code.label("@subtract");
	code.sbc_abs_x(0x0103); code.sta_abs_x(0x0102);
	code.lda_abs_x(0x0101); code.clc(); code.adc_imm(1); code.sta_abs_x(0x0101);
	code.lda_abs_x(0x0102); code.bcc("@advance");
	code.label("@calculated");
	code.sta_abs_x(0x0102); code.lda_abs_x(0x0101); code.beq("@wait");
	code.sec(); code.sbc_imm(1); code.sta_mem(0x0430);
	code.jsr(ROM::PPUBuffer_WaitEmpty); code.lda_mem(0x03d0);
	code.jsr(ROM::Screen_SetFadePalette);
	code.label("@wait");
	code.jsr(ROM::WaitForInterrupt); code.tsx(); code.dec_abs_x(0x0105);
	code.bne("@frame");
	code.pla(); code.pla(); code.pla(); code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevFadeIn(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	const auto operands{ atlasdev_arity(p_config, "AtlasDevFadeIn", 2) };

	if (operands == 1) {
		code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
		code.cmp_imm(0x00); // LoadByte's final STY means its incoming Z is not A's Z
		code.bne("@timed");
		// The full restore replicates what Screen_LoadBackgroundPalette ($D00D)
		// does in the unmodified game: A = the area's palette id from $03D0,
		// copy its bank-11 entry into the $0293 shadow, queue the upload.
		// $D00D itself must not be called: the extended-flags boot hook
		// repurposes $D005-$D012 as its init stub whenever flag opcodes are
		// installed, so in those builds a call lands mid-hook on a JSR to
		// Game_InitMMCAndBank and resets the game.  $D03B and $D090 are live
		// vanilla routines with callers of their own, including a bank-12
		// caller at $9F2C, so this path is safe from the switched window.
		code.jsr(ROM::PPUBuffer_WaitEmpty); code.lda_mem(0x03d0);
		code.jsr(ROM::Screen_CopyBgPaletteToShadow);
		code.jsr(ROM::PPUBuffer_QueuePaletteUpload);
		code.lda_imm(0xff); code.sta_mem(0x0430);
		code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
		code.label("@timed");
		code.pha(); code.pha(); code.lda_imm(0); code.pha(); code.pha();
		code.label("@frame");
		code.tsx(); code.lda_abs_x(0x0102); code.clc(); code.adc_imm(4);
		code.bcs("@subtract"); // preserve the ninth bit for totals $FD..$FF
		code.label("@advance");
		code.cmp_abs_x(0x0103); code.bcc("@calculated"); code.sec();
		code.label("@subtract");
		code.sbc_abs_x(0x0103); code.sta_abs_x(0x0102);
		code.lda_abs_x(0x0101); code.clc(); code.adc_imm(1); code.sta_abs_x(0x0101);
		code.lda_abs_x(0x0102); code.bcc("@advance");
		code.label("@calculated");
		code.sta_abs_x(0x0102); code.lda_abs_x(0x0101); code.beq("@wait");
		code.cmp_imm(4); code.bcs("@full");
		code.eor_imm(3); code.sta_mem(0x0430); // 1->2, 2->1, 3->0
		code.jsr(ROM::PPUBuffer_WaitEmpty); code.lda_mem(0x03d0);
		code.jsr(ROM::Screen_SetFadePalette);
		code.lda_imm(1); code.bne("@wait");
		code.label("@full");
		code.jsr(ROM::PPUBuffer_WaitEmpty); code.lda_mem(0x03d0); // as the zero path
		code.jsr(ROM::Screen_CopyBgPaletteToShadow);
		code.jsr(ROM::PPUBuffer_QueuePaletteUpload);
		code.lda_imm(0xff); code.sta_mem(0x0430);
		code.label("@wait");
		code.jsr(ROM::WaitForInterrupt); code.tsx(); code.dec_abs_x(0x0104);
		code.bne("@frame");
		code.pla(); code.pla(); code.pla(); code.pla();
		code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
		return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
	}

	// Stack after the pushes, with X from TSX:
	//   stage=$0101,X error=$0102,X total=$0103,X depth=$0104,X remaining=$0105,X
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha(); // frames
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));            // depth
	code.cmp_imm(5); code.bcc("@ceiling"); code.lda_imm(4);   // guard $D0E0's four entries
	code.label("@ceiling");
	code.cmp_imm(1); code.bcs("@floor"); code.lda_imm(1);
	code.label("@floor");
	code.pha();
	code.tsx(); code.lda_abs_x(0x0102);                    // TSX; A = frames
	code.bne("@timed");
	code.jsr(ROM::PPUBuffer_WaitEmpty); code.lda_mem(0x03d0); // see the one-operand form
	code.jsr(ROM::Screen_CopyBgPaletteToShadow);
	code.jsr(ROM::PPUBuffer_QueuePaletteUpload);
	code.lda_imm(0xff); code.sta_mem(0x0430);
	code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	code.label("@timed");
	code.pha(); code.lda_imm(0); code.pha(); code.pha();       // total, error, stage
	code.label("@frame");
	code.tsx(); code.lda_abs_x(0x0102); code.clc(); code.adc_abs_x(0x0104);
	code.bcs("@subtract"); // preserve the ninth bit for totals $FD..$FF
	code.label("@advance");
	code.cmp_abs_x(0x0103); code.bcc("@calculated"); code.sec();
	code.label("@subtract");
	code.sbc_abs_x(0x0103); code.sta_abs_x(0x0102);
	code.lda_abs_x(0x0101); code.clc(); code.adc_imm(1); code.sta_abs_x(0x0101);
	code.lda_abs_x(0x0102); code.bcc("@advance");
	code.label("@calculated");
	code.sta_abs_x(0x0102); code.lda_abs_x(0x0101); code.beq("@wait");
	code.cmp_abs_x(0x0104); code.bcs("@full");   // stage >= depth: full restore
	code.lda_abs_x(0x0104); code.sec(); code.sbc_abs_x(0x0101); // depth-stage
	code.sec(); code.sbc_imm(1);        // ... minus one
	code.sta_mem(0x0430);
	code.jsr(ROM::PPUBuffer_WaitEmpty); code.lda_mem(0x03d0);
	code.jsr(ROM::Screen_SetFadePalette);
	code.lda_imm(1); code.bne("@wait");
	code.label("@full");
	code.jsr(ROM::PPUBuffer_WaitEmpty); code.lda_mem(0x03d0); // see the one-operand form
	code.jsr(ROM::Screen_CopyBgPaletteToShadow);
	code.jsr(ROM::PPUBuffer_QueuePaletteUpload);
	code.lda_imm(0xff); code.sta_mem(0x0430);
	code.label("@wait");
	code.jsr(ROM::WaitForInterrupt); code.tsx(); code.dec_abs_x(0x0105);
	code.bne("@frame");
	code.pla(); code.pla(); code.pla(); code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevSetMusic State: 0 stops the music, 1..16 select a song.
// Values above 16 are a deliberate no-op so a script driven by a variable
// cannot reach an undefined song.
word fh::HackManager::apply_AtlasDevSetMusic(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = requested state
	code.cmp_imm(0x11);                                       // 0=stop, 1..16=song
	code.bcs("@done");                                        // 17..255: safe no-op
	code.sta_zp(RAM::ZP_MusicCurrent);

	code.label("@done");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// --- AtlasDev dialogue opcodes ---------------------------------------
// AtlasDevShowMessageFromVar requires the script-variable feature and will
// fail the build with a named missing-constant error without it; see the
// note in docs/advanced_doc.md before enabling that one.

word fh::HackManager::apply_AtlasDevShowSequentialMessages(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	// Four message-id operands; 0 marks an unused slot.  Ids are
	// 1-based indexes into the bank-13 string table -- raw ids bypass
	// the compiler's string relocation, so scripts should reference
	// [reserved_strings] indexes or table positions they control.
	// Messages_Load is fully reentrant (the textbox frame and grid are
	// laid once at script open), so each id is loaded and pumped with
	// the vanilla Msg pattern; the one-A-press gate between messages is
	// the engine's own continue test.  B skips the remaining messages
	// (operands still consumed -- the stream never desyncs) and the
	// script continues; it does not end the script.
	code.lda_imm(0x04);
	code.db(0x48); // PHA -- remaining-slots counter on the stack
	code.label("@next");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // id
	code.beq("@advance"); // 0: unused slot
	code.jsr(ROM::Messages_Load);
	code.label("@pump");
	code.jsr(ROM::Portrait_Pump);
	code.jsr(ROM::Text_ShowNextChar);
	code.jsr(ROM::Text_ContinueGate);
	code.bcs("@dismiss"); // B pressed
	code.bne("@pump");
	code.label("@advance");
	code.db(0x68); // PLA
	code.db(0x38); // SEC
	code.db(0xe9); code.db(0x01); // SBC #$01
	code.beq("@done");
	code.db(0x48); // PHA
	code.bne("@next"); // A nonzero here: branch always
	code.label("@dismiss");
	code.db(0x68); // PLA
	code.db(0x38); // SEC
	code.db(0xe9); code.db(0x01); // SBC #$01
	code.beq("@done");
	code.tax();
	code.label("@drain");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // consume
	code.db(0xca); // DEX
	code.bne("@drain");
	code.label("@done");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevPlaySFX Id: a public sound effect, $00..$1c. Higher values are
// ignored because the effect routine indexes its table unchecked. Whether
// the music keeps playing under it depends on the effect, not this opcode.
word fh::HackManager::apply_AtlasDevPlaySFX(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = public SFX ID
	code.cmp_imm(0x1d);                                       // IDs $00..$1c only
	code.bcs("@done");
	code.jsr(ROM::Sound_PlayEffect);

	code.label("@done");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevShowNumberInMessage Message Register
word fh::HackManager::apply_AtlasDevShowNumberInMessage(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	const word Vars{ cfg_word(p_config, c::ID_HACK_SCRIPT_VAR_RAM_ADDR) };
	// True in-string substitution would need a new control byte in the
	// fixed-bank text interpreter (a CMP chain with no free byte) --
	// not pure, rejected.  Instead: reveal the whole message WITHOUT
	// the A-wait, then render the register as three digits at the text
	// cursor (box origin + 2 + column $0216 / line $0217) with the
	// same converter the shipped DrawVarNumber uses, then run the
	// engine's own A-wait.  Digit tiles $30-$39 are HUD-resident, so
	// the number needs no grid tiles.  Message id 0 is a no-op (both
	// operands still consumed).  B dismisses as vanilla Msg does.
	// The converter's 24-bit input ($ec/$ed/$ee) CANNOT be filled before the
	// message is revealed: the reveal clobbers all three.  Portrait_Pump
	// writes $ed and compares $ec, and the tile helpers at $f860-$f866 write
	// $ec, $ed and $ee outright.  Filling them first drew whatever the reveal
	// happened to leave behind.  So keep both operands on the stack across
	// the loop -- the same technique AtlasDevShowSequentialMessages uses for
	// its counter -- and load the register only just before the draw.
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // message id
	code.db(0x48); // PHA                          stack: [id]
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // register
	// Bound the index by the configured count, not by a hardcoded eight.
	// Reading the base from hack_script_var_ram_addr while masking the index
	// with AND #$07 means a project that declares four registers still lets
	// register 7 read past the end of its own file.
	code.cmp_imm(cfg_byte(p_config, c::ID_HACK_SCRIPT_VAR_COUNT));
	code.bcc("@reg_ok");
	code.lda_imm(0x00);
	code.label("@reg_ok");
	code.db(0x48); // PHA                          stack: [id, reg]
	code.db(0xba); // TSX -- read the id back without disturbing either entry
	code.lda_abs_x(0x0102); // A = message id
	code.beq("@drop");
	code.jsr(ROM::Messages_Load);
	code.label("@pump"); // reveal fully, no A-wait yet
	code.jsr(ROM::Portrait_Pump);
	code.jsr(ROM::Text_ShowNextChar);
	code.db(0xad); code.db(0x13); code.db(0x02); // LDA $0213 stream active?
	code.bne("@pump");
	code.db(0xad); code.db(0x08); code.db(0x02); // LDA $0208 box x
	code.db(0x18); // CLC
	code.db(0x69); code.db(0x02); // ADC #$02
	code.db(0x6d); code.db(0x16); code.db(0x02); // ADC $0216 cursor column
	code.sta_zp(0xea);
	code.db(0xad); code.db(0x09); code.db(0x02); // LDA $0209 box y
	code.db(0x18); // CLC
	code.db(0x69); code.db(0x02); // ADC #$02
	code.db(0x6d); code.db(0x17); code.db(0x02); // ADC $0217 cursor line
	code.sta_zp(0xeb);
	// Only now is it safe to fill the converter's input.
	code.db(0x68); // PLA -- register index      stack: [id]
	code.tax();
	code.lda_abs_x(Vars);
	code.sta_zp(0xec); // 24-bit value for the converter, low byte
	code.lda_imm(0x00);
	code.sta_zp(0xed);
	code.sta_zp(0xee);
	code.db(0xa0); code.db(0x03); // LDY #$03 -- three digits (0-255)
	code.jsr(ROM::Number_DrawAtPos);
	code.db(0x68); // PLA -- discard the message id   stack: []
	code.label("@wait"); // now the engine's own continue gate
	code.jsr(ROM::Portrait_Pump);
	code.jsr(ROM::Text_ContinueGate);
	code.bcs("@done");
	code.bne("@wait");
	code.jmp("@done"); // stack is already balanced on this path

	code.label("@drop"); // message id 0: both operands consumed, nothing drawn
	code.db(0x68); // PLA -- register
	code.db(0x68); // PLA -- message id

	code.label("@done");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevShowChoiceToVar Count Register
word fh::HackManager::apply_AtlasDevShowChoiceToVar(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	// Runs the vanilla menu selection loop ($84ED: portrait tick,
	// Menu_UpdateAndDraw, joypad) over Count rows, then stores the
	// chosen index in a script register -- $FF when the player
	// cancels with B.  The shop's purchase path is never entered:
	// only the cursor state ($021E current, $021F count) is used, and
	// the script draws its own choice text.  Count is clamped to 1..8.
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // count
	code.cmp_imm(0x01);
	code.db(0xb0); code.db(0x02); // BCS @count_min
	code.lda_imm(0x01);
	code.cmp_imm(0x09);
	code.bcc("@count_ok");
	code.lda_imm(0x08);
	code.label("@count_ok");
	code.sta_abs(0x021f);
	code.lda_imm(0x00);
	code.sta_abs(0x021e); // cursor starts on the first row
	const word Vars{ cfg_word(p_config, c::ID_HACK_SCRIPT_VAR_RAM_ADDR) };
	const byte VarCount{ cfg_byte(p_config, c::ID_HACK_SCRIPT_VAR_COUNT) };
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // register
	code.cmp_imm(VarCount); code.bcc("@reg_ok");
	code.lda_imm(0x00);                       // out of range selects register 0
	code.label("@reg_ok");
	code.sta_zp(0xee);
	code.jsr(ROM::Menu_WaitInput);
	code.bcc("@confirmed"); // carry clear = A pressed
	code.lda_imm(0xff);     // carry set = B pressed, report cancel
	code.bne("@store");
	code.label("@confirmed");
	code.lda_abs(0x021e);
	code.label("@store");
	code.db(0xa6); code.db(0xee); // LDX $EE
	code.sta_abs_x(Vars);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevClearPortrait(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	// The vanilla portrait teardown ($F281): id := $FF, image cleared,
	// area palette restored.  Dialogue/window state untouched, so a
	// script can drop the portrait and keep talking.
	code.jsr(ROM::Portrait_Clear);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevEntitySayMessage(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	// Consume only Slot.  The Message index is left as the very next script
	// byte; vanilla ShowMessage's own LoadByte call consumes it below, so
	// $DB/$DC/$DD only ever advance through IScripts_LoadByte itself and are
	// never separately touched by this handler.
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Slot
	code.cmp_imm(0x08);
	code.bcs("@skip");                                        // slot >= 8: invalid
	code.tax();
	code.lda_abs_x(RAM::EntitySlotActive);                    // $02CC+slot
	code.bmi("@skip");                                        // inactive slot
	code.lda_abs_x(RAM::EntityScriptRoot);                    // $036C+slot
	code.cmp_imm(0xff);
	code.beq("@skip");                                        // no script assigned
	code.tax();                                                // X = root index 0..151
	code.lda_abs_x(ROM::IScripts_RootPointerLo);
	code.sta_zp(0xea);
	code.lda_abs_x(ROM::IScripts_RootPointerHi);
	code.sta_zp(0xeb);
	code.ldy_imm(0x00);
	code.lda_ind_y(0xea);              // A = that entity's raw context byte

	// From here down this is FaxIScripts' own already-proven, owner-passed
	// SetPortrait state machine (tools/faxiscripts_setportrait.py), operating
	// on a derived byte instead of a fresh LoadByte operand, with every exit
	// retargeted from InvokeNextAction to vanilla ShowMessage so the Message
	// index that follows Slot in the script is consumed and rendered next.
	code.cmp_imm(0x00);
	code.beq("@valid");
	code.cmp_imm(0x80);
	code.bcc("@skip");
	code.cmp_imm(0x8b);
	code.bcs("@skip");

	code.label("@valid");
	code.cmp_abs(RAM::IScriptTextBoxContext);
	code.beq("@skip");
	code.pha();
	code.lda_abs(RAM::IScriptTextBoxContext);
	code.bmi("@from_portrait");

	code.pla();
	code.beq("@store_generic");
	code.pha();
	code.jsr(ROM::TextBox_Close);
	code.pla();
	code.sta_abs(RAM::IScriptTextBoxContext);
	code.and_imm(0x7f);
	code.jsr(ROM::Portrait_LoadTiles);
	code.jsr(ROM::TextBox_OpenForPortrait);
	code.jsr(ROM::TextBox_OpenForNPC);
	code.jmp("@show");

	code.label("@store_generic");
	code.sta_abs(RAM::IScriptTextBoxContext);
	code.jmp("@show");

	code.label("@from_portrait");
	code.pla();
	code.sta_abs(RAM::IScriptTextBoxContext);
	code.beq("@to_generic");
	code.lda_abs(RAM::PortraitSavedPalette);
	code.pha();
	code.lda_abs(RAM::IScriptTextBoxContext);
	code.and_imm(0x7f);
	code.jsr(ROM::Portrait_LoadTiles);
	code.pla();
	code.sta_abs(RAM::PortraitSavedPalette);
	code.jmp("@show");

	code.label("@to_generic");
	code.jsr(ROM::Portrait_Clear);
	code.jsr(ROM::TextBox_ClearForPortraitAndText);
	code.jsr(ROM::TextBox_OpenForNPC);

	code.label("@skip");
	code.jmp("@show");

	// Inlined copy of the vanilla message loop that used to live at $82D9.
	// That entry stub has no callers anywhere in the ROM, which makes it the
	// kind of region a modding framework legitimately reclaims; FaxIScripts
	// already does exactly that with $D005-$D012 for its extended-flags boot
	// stub.  Everything the stub actually ran is live and still called here:
	// $F3F5, $87B0, $F466, $9956, and the shared close tail at $82B4.
	code.label("@show");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.jsr(ROM::Messages_Load);
	code.label("@show_char");
	code.jsr(ROM::Portrait_Pump);
	code.jsr(ROM::Text_ShowNextChar);
	code.jsr(ROM::Text_ContinueGate);
	code.bcs("@show_close");
	code.bne("@show_char");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	code.label("@show_close");
	code.jmp(ROM::IScripts_MessageFinish);

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevShowMessageFromVar Register
word fh::HackManager::apply_AtlasDevShowMessageFromVar(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	const word Vars{ cfg_word(p_config, c::ID_HACK_SCRIPT_VAR_RAM_ADDR) };
	const byte Count{ cfg_byte(p_config, c::ID_HACK_SCRIPT_VAR_COUNT) };
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = register
	code.cmp_imm(Count); code.bcs("@invalid");
	code.tay(); code.lda_abs_y(Vars); // A = message ID
	code.cmp_imm(1); code.bcc("@invalid");
	code.cmp_imm(194); code.bcs("@invalid");
	// Inline the vanilla message loop rather than tail-entering $82DC, three
	// bytes into the entry stub at $82D9.  That stub has no callers anywhere
	// in the ROM, so it is reclaimable space, and entering it mid-routine
	// would land mid-instruction if it ever moved.  The routines below are
	// all live code the game calls itself.  A is already the message ID, so
	// the stub's own LoadByte is exactly what gets skipped.
	code.jsr(ROM::Messages_Load);
	code.label("@show_char");
	code.jsr(ROM::Portrait_Pump);
	code.jsr(ROM::Text_ShowNextChar);
	code.jsr(ROM::Text_ContinueGate);
	code.bcs("@show_close");
	code.bne("@show_char");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	code.label("@show_close");
	code.jmp(ROM::IScripts_MessageFinish);
	code.label("@invalid");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevHideTextbox(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	// $81FB is the generic vanilla End-action close path:
	// JSR $81C0 (select the dialogue rectangle), JMP $9002 (restore it).
	code.jsr(ROM::TextBox_Close);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevIfMusic Song Label: jumps when Song is the track selected.
// Music_Current holds the requested ID before the NMI picks it up and the
// same ID with bit 7 set afterwards, so both forms are compared.
word fh::HackManager::apply_AtlasDevIfMusic(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = requested song
	code.cmp_imm(0x11);                                       // valid: 0..16
	code.bcs("@not_equal");                                   // 17..255: false

	code.cmp_zp(RAM::ZP_MusicCurrent);                        // pending form
	code.beq("@equal");
	code.ora_imm(0x80);
	code.cmp_zp(RAM::ZP_MusicCurrent);                        // NMI-promoted form
	code.beq("@equal");

	code.label("@not_equal");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	code.label("@equal");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevOpenTextbox(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	// The counterpart to AtlasDevHideTextbox.  $81E2 is the vanilla open the
	// game itself uses for an NPC conversation, called from three places: it
	// selects the dialogue rectangle, lays the frame, and points the text
	// cursor ($ea/$eb) at the rectangle origin plus two.  Text tiles only
	// exist while a box is open, so this is what makes text drawn after a
	// close visible again without forcing a portrait.
	code.jsr(ROM::TextBox_OpenForNPC);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevCloseDialogue(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	// The portrait counterpart to AtlasDevHideTextbox, which restores only
	// whichever rectangle $81C0 has selected.  A portrait conversation
	// occupies the larger portrait-and-text rectangle, so closing it needs
	// $822B, which sets that rectangle explicitly before restoring it.
	//
	// Drop the context first so the portrait tick cannot repopulate portrait
	// OAM from an NMI landing in the middle of the teardown, and so any later
	// message in the same script is handled as a plain one.
	code.lda_imm(0x00);
	code.sta_abs(RAM::IScriptTextBoxContext);
	code.jsr(ROM::Portrait_Clear);
	code.jsr(ROM::TextBox_ClearForPortraitAndText);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevIfEntityCountAtLeast Count Label
//
// Branches when at least Count of the eight entity slots are live.  This is
// the shape wave gating actually wants -- "once two of the three guards are
// down, open the door" -- and it answers it without a script variable, so it
// needs no RAM the project has not already allocated.
//
// $02CC..$02D3 is the game's own eight-slot table; bit 7 set means the slot is
// free.  The engine frees a slot itself once an entity's position byte reaches
// $F0, so an entity that walks off stops counting exactly as a killed one does.
//
// Count 0 always branches.  Count above 8 never does, because eight slots
// cannot hold nine entities.  Both fall out of the countdown below rather than
// needing a range check.
word fh::HackManager::apply_AtlasDevIfEntityCountAtLeast(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	// Count down from the operand rather than up from zero, so the threshold
	// lives in Y and the accumulator stays free for the table read.  A running
	// total would need somewhere to keep the operand across the loop, and the
	// obvious zero-page scratch ($ec..$ee) is the number converter's input --
	// borrowing it is what broke ShowNumberInMessage.
	//
	// The TAY has to come before the zero test.  LoadByte returns the operand
	// in A but leaves N and Z describing its own script-pointer increment
	// (its last flag-setting instruction is the INY), so branching straight
	// off the JSR tests the pointer, not the operand.  TAY re-derives the
	// flags from A.
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Count
	code.tay();                                               // Y = slots still needed
	code.beq("@taken");                                       // at least zero: always

	code.ldx_imm(0x07);
	code.label("@slot");
	code.lda_abs_x(RAM::EntitySlotActive);
	code.bmi("@skip");                                        // bit 7 set = free slot
	code.db(0x88);                                            // DEY -- one more live one
	code.beq("@taken");                                       // seen enough; stop early
	code.label("@skip");
	code.dex();
	code.bpl("@slot");                                        // X walks 7..0 and no lower

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	code.label("@taken");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// The entity opcodes below share one operand convention, and it is worth
// stating once.  A slot operand outside 0..7 is REJECTED, never folded: the
// handler leaves the game untouched and continues.  Masking the operand with
// AND #$07 would be shorter, but it makes "slot 8" act on slot 0, which is
// worse than doing nothing because it silently moves the wrong actor.
//
// AtlasDevFreezeEntities
//
// $0426 is a plain flag, not a timer: the per-slot update loop skips every
// entity while it is nonzero, and vanilla brackets one of its own calls with
// 1 then 0.  Nothing else clears it during play, so a script that freezes
// MUST resume; ending the script while frozen leaves the game frozen.
word fh::HackManager::apply_AtlasDevFreezeEntities(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.lda_imm(0x01);
	code.sta_abs(RAM::EntityUpdateFreeze);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevResumeEntities -- the other half of AtlasDevFreezeEntities.
word fh::HackManager::apply_AtlasDevResumeEntities(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.lda_imm(0x00);
	code.sta_abs(RAM::EntityUpdateFreeze);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevIfBossPresent Label
//
// Branches when any slot holds a boss.  The boss set is category 7 of the
// bank-14 sprite category table -- exactly {$11,$12} and [$2D..$33] -- which
// cannot be read from here, because bank 14 is unmapped while bank-12 code
// runs and the table has no fixed-bank copy.  The constants are therefore
// inlined.  A free slot holds $ff and an inactive one has bit 7 set, so
// neither can match any of these identities.
word fh::HackManager::apply_AtlasDevIfBossPresent(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.ldy_imm(0x07);
	code.label("@slot");
	code.lda_abs_y(RAM::EntitySlotActive);
	code.cmp_imm(0x11);
	code.beq("@present");
	code.cmp_imm(0x12);
	code.beq("@present");
	code.cmp_imm(0x2d);
	code.bcc("@next");
	code.cmp_imm(0x34);
	code.bcc("@present");
	code.label("@next");
	code.db(0x88);              // DEY
	code.bpl("@slot");

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));
	code.label("@present");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevIfEntityTypePresent Type Label
//
// Branches when any slot holds the given entity identity.  Identities run to
// $64, and the guard matters: an unclamped $ff would match every EMPTY slot
// and report the room as full of whatever was asked for.
word fh::HackManager::apply_AtlasDevIfEntityTypePresent(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Type
	code.cmp_imm(0x65);
	code.bcs("@absent");
	code.ldy_imm(0x07);
	code.label("@slot");
	code.cmp_abs_y(RAM::EntitySlotActive);
	code.beq("@present");
	code.db(0x88);              // DEY
	code.bpl("@slot");

	code.label("@absent");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));
	code.label("@present");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevIfEntitySlotActive Slot Label
//
// Branches when the slot holds a live entity.  There is deliberately no
// negated form: invert the branch target instead.
word fh::HackManager::apply_AtlasDevIfEntitySlotActive(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Slot
	code.cmp_imm(0x08);
	code.bcs("@inactive");      // never read past $02d3
	code.tax();
	code.lda_abs_x(RAM::EntitySlotActive);
	code.bmi("@inactive");      // bit 7 set = free
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	code.label("@inactive");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevIfEntityHidden Slot Label
//
// Reads the same flag bit AtlasDevSetEntityHidden writes and the engine's own
// sword-hit test consults.  An invalid slot is not hidden, matching
// AtlasDevIfEntitySlotActive's treatment of one.
word fh::HackManager::apply_AtlasDevIfEntityHidden(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Slot
	code.cmp_imm(0x08);
	code.bcs("@visible");
	code.tax();
	code.lda_abs_x(RAM::EntityFlags);
	code.and_imm(0x10);
	code.beq("@visible");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	code.label("@visible");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevSetEntityHidden Slot Hidden
//
// Bit 4 of the per-slot flag byte is the engine's own hidden test.  Zero
// shows, nonzero hides; the entity's behaviour keeps running either way, so
// this hides an actor without removing it.
word fh::HackManager::apply_AtlasDevSetEntityHidden(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Slot
	code.cmp_imm(0x08);
	code.bcs("@drop");
	code.tax();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Hidden
	code.cmp_imm(0x00);         // LoadByte's own flags describe its pointer
	code.beq("@show");
	code.lda_abs_x(RAM::EntityFlags);
	code.ora_imm(0x10);
	code.sta_abs_x(RAM::EntityFlags);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	code.label("@show");
	code.lda_abs_x(RAM::EntityFlags);
	code.and_imm(0xef);
	code.sta_abs_x(RAM::EntityFlags);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	// An invalid slot still has to consume its second operand, or the
	// interpreter would read the Hidden byte as the next opcode.
	code.label("@drop");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevSetEntityHealth Slot Health
//
// $0344,X is the live HP the damage code decrements.  Death fires on
// subtract-borrow rather than on zero, so Health 0 means "dies to the next
// hit", not "dead".
word fh::HackManager::apply_AtlasDevSetEntityHealth(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Slot
	code.cmp_imm(0x08);
	code.bcs("@drop");
	code.tax();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Health
	code.sta_abs_x(RAM::EntityHealth);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	code.label("@drop");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevSetEntityInvincible Slot Frames
//
// $034C,X is the byte vanilla's own weapon-hit handler sets to 8 on every
// successful hit and the death path clears.  The per-slot dispatch loop skips
// the entire collision-response block while it is nonzero, then decrements it,
// so this reuses the existing i-frame window rather than adding a hook.
// Frames 0 clears it immediately, matching the death path.
word fh::HackManager::apply_AtlasDevSetEntityInvincible(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Slot
	code.cmp_imm(0x08);
	code.bcs("@drop");
	code.tax();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Frames
	code.sta_abs_x(RAM::EntityHitStun);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	code.label("@drop");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevSetEntityBehavior Slot Behaviour
//
// Selects one of the engine's own behaviours and clears the behaviour-ready
// flag so its initializer runs on the next tick.  Bit 7 is masked off because
// it means "run BScript ops" rather than "dispatch a behaviour".  Behaviour 6
// is refused: the dispatcher special-cases it into an unrelated jump.
word fh::HackManager::apply_AtlasDevSetEntityBehavior(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Slot
	code.cmp_imm(0x08);
	code.bcs("@drop");
	code.tax();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Behaviour
	code.and_imm(0x7f);
	code.cmp_imm(0x06);
	code.beq("@done");
	code.sta_abs_x(RAM::EntityOpsMode);
	code.lda_abs_x(RAM::EntityFlags);
	code.and_imm(0xbf);         // clear ready, so the initializer runs
	code.sta_abs_x(RAM::EntityFlags);
	code.label("@done");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	code.label("@drop");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevSetEntitySpeed Slot Fraction Whole
//
// Walker behaviours cache their speed per slot and copy it into the per-tick
// delta every frame, so writing the cache changes how fast the entity walks.
// This reaches the walker pair only (behaviours 0 and 4); flyers keep their
// velocity elsewhere and are unaffected.  A behaviour restart re-reads the ROM
// operand and overwrites this.
word fh::HackManager::apply_AtlasDevSetEntitySpeed(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Slot
	code.cmp_imm(0x08);
	code.bcs("@drop");
	code.tax();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Fraction
	code.sta_abs_x(RAM::EntitySpeedFraction);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Whole
	code.sta_abs_x(RAM::EntitySpeedWhole);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	code.label("@drop");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevSetEntityFacing Slot Direction
//
// Bit 0 of the per-slot flag byte is the engine's own facing bit.  Direction 0
// faces left, anything else faces right.  An inactive slot is left alone.
word fh::HackManager::apply_AtlasDevSetEntityFacing(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Slot
	code.cmp_imm(0x08);
	code.bcs("@drop");
	code.tax();
	code.lda_abs_x(RAM::EntitySlotActive);
	code.bmi("@drop");          // free slot: consume the operand and do nothing
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Direction
	code.cmp_imm(0x00);         // LoadByte's own flags describe its pointer
	code.beq("@left");
	code.lda_abs_x(RAM::EntityFlags);
	code.ora_imm(0x01);
	code.sta_abs_x(RAM::EntityFlags);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	code.label("@left");
	code.lda_abs_x(RAM::EntityFlags);
	code.and_imm(0xfe);
	code.sta_abs_x(RAM::EntityFlags);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	code.label("@drop");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevEntityFieldToVar Slot Field Register
//
// Reads one per-slot byte into a script register.  Both entity array groups
// have stride 8, so the address is base + field*8 + slot with no table:
// fields 0-5 from $02CC (identity, ops mode, flags, phase, speed fraction,
// speed whole) and fields 6-11 from $0344 (HP, hit-stun, program lo, program
// hi, $0364, dialogue entrypoint).  A field of 12 or above folds to 0, which
// reads the identity; that is harmless because this opcode only ever reads.
// An out-of-range slot is rejected, and still consumes both remaining
// operands so the interpreter cannot mistake one for the next opcode.
word fh::HackManager::apply_AtlasDevEntityFieldToVar(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	const word Vars{ cfg_word(p_config, c::ID_HACK_SCRIPT_VAR_RAM_ADDR) };
	const byte VarCount{ cfg_byte(p_config, c::ID_HACK_SCRIPT_VAR_COUNT) };

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Slot
	code.cmp_imm(0x08);
	code.bcs("@drop_two");
	code.pha();                 // stack: [slot] -- no zero page is borrowed

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Field
	code.cmp_imm(0x0c);
	code.bcc("@field_ok");
	code.lda_imm(0x00);
	code.label("@field_ok");
	code.cmp_imm(0x06);
	code.bcc("@low_group");

	code.sbc_imm(0x06);         // carry is set by the CMP
	code.asl_a(); code.asl_a(); code.asl_a();
	code.tsx();
	code.clc();
	code.adc_abs_x(0x0101);     // + slot, read from its stack cell
	code.tax();
	code.lda_abs_x(RAM::EntityHealth);   // $0344 group
	code.jmp("@store");

	code.label("@low_group");
	code.asl_a(); code.asl_a(); code.asl_a();
	code.tsx();
	code.clc();
	code.adc_abs_x(0x0101);
	code.tax();
	code.lda_abs_x(RAM::EntitySlotActive); // $02CC group

	code.label("@store");
	code.pha();                 // stack: [slot, value] -- LoadByte destroys Y
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Register
	code.cmp_imm(VarCount);
	code.bcc("@reg_ok");
	code.lda_imm(0x00);
	code.label("@reg_ok");
	code.tax();
	code.pla();                 // A = value
	code.sta_abs_x(Vars);
	code.pla();                 // discard the slot; stack balanced
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	code.label("@drop_two");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevDrawVarNumber Register X Y Digits
//
// Draws a script register as a zero-padded decimal at a raw tile position,
// through $FA03, the routine the HUD itself uses.  Digit tiles are resident
// in gameplay CHR, so nothing needs uploading, and the write goes through the
// buffered PPU queue.  This is the readable alternative to
// AtlasDevShowNumberInMessage: the digits land wherever the script says,
// never over the dialogue text, and a later draw at the same position simply
// replaces the earlier number.  The tiles persist like any background tiles
// until the screen is redrawn.
//
// Digits clamps to 1..7, $FA03's own legal range.  X and Y are raw tile
// coordinates; numbers are not boxes, so there is no evenness constraint.
word fh::HackManager::apply_AtlasDevDrawVarNumber(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	const word Vars{ cfg_word(p_config, c::ID_HACK_SCRIPT_VAR_RAM_ADDR) };
	const byte VarCount{ cfg_byte(p_config, c::ID_HACK_SCRIPT_VAR_COUNT) };

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Register
	code.cmp_imm(VarCount);
	code.bcc("@reg_ok");
	code.lda_imm(0x00);
	code.label("@reg_ok");
	code.tax();
	code.lda_abs_x(Vars);
	// $ec/$ed/$ee are the converter's own 24-bit input, $ea/$eb its position
	// input; filling them here is the intended calling convention, not a
	// borrow, and nothing runs between the fill and the JSR.
	code.sta_zp(0xec);
	code.lda_imm(0x00);
	code.sta_zp(0xed);
	code.sta_zp(0xee);

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = X tile
	code.sta_zp(0xea);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Y tile
	code.sta_zp(0xeb);

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Digits
	code.cmp_imm(0x01);
	code.bcs("@lo_ok");
	code.lda_imm(0x01);
	code.label("@lo_ok");
	code.cmp_imm(0x08);
	code.bcc("@hi_ok");
	code.lda_imm(0x07);
	code.label("@hi_ok");
	code.tay();
	code.jsr(ROM::Number_DrawAtPos);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevGetLocationToVars WorldRegister ScreenRegister
word fh::HackManager::apply_AtlasDevGetLocationToVars(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	const word Vars{ cfg_word(p_config, c::ID_HACK_SCRIPT_VAR_RAM_ADDR) };
	const byte VarCount{ cfg_byte(p_config, c::ID_HACK_SCRIPT_VAR_COUNT) };

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.tsx(); // $0102,X = world register; $0101,X = screen register
	code.lda_abs_x(0x0102); code.cmp_imm(VarCount); code.bcs("@done");
	code.lda_abs_x(0x0101); code.cmp_imm(VarCount); code.bcs("@done");
	code.lda_abs_x(0x0102); code.cmp_abs_x(0x0101); code.beq("@done");

	code.lda_abs_x(0x0102); code.tay();
	code.lda_zp(RAM::ZP_CurrentWorld); code.db(0x99); code.dw(Vars);
	code.lda_abs_x(0x0101); code.tay();
	code.lda_zp(RAM::ZP_CurrentScreen); code.db(0x99); code.dw(Vars);

	code.label("@done"); code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevGetPlayerPositionToVars XRegister YRegister
word fh::HackManager::apply_AtlasDevGetPlayerPositionToVars(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr,
	word helper_get_player_block_pos_addr) const {
	klib::Asm6502 code;
	const word Vars{ cfg_word(p_config, c::ID_HACK_SCRIPT_VAR_RAM_ADDR) };
	const byte VarCount{ cfg_byte(p_config, c::ID_HACK_SCRIPT_VAR_COUNT) };

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.tsx(); // $0102,X = X register; $0101,X = Y register
	code.lda_abs_x(0x0102); code.cmp_imm(VarCount); code.bcs("@done");
	code.lda_abs_x(0x0101); code.cmp_imm(VarCount); code.bcs("@done");
	code.lda_abs_x(0x0102); code.cmp_abs_x(0x0101); code.beq("@done");

	// Use the same player-center conversion as IfYX.
	code.jsr(helper_get_player_block_pos_addr);
	code.txa(); code.pha(); // packed block position: YYYYXXXX
	code.tsx();             // $0103,X = X register; $0102,X = Y register

	code.db(0xbc); code.dw(0x0103); // LDY $0103,X
	code.lda_abs_x(0x0101); code.and_imm(0x0f);
	code.db(0x99); code.dw(Vars);

	code.db(0xbc); code.dw(0x0102); // LDY $0102,X
	code.lda_abs_x(0x0101); code.lsr_a(4);
	code.db(0x99); code.dw(Vars);
	code.pla();

	code.label("@done"); code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevVarBitOp Register Operation Value
// Operation: 0 = AND, 1 = OR, 2 = XOR.
word fh::HackManager::apply_AtlasDevVarBitOp(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	const word Vars{ cfg_word(p_config, c::ID_HACK_SCRIPT_VAR_RAM_ADDR) };
	const byte VarCount{ cfg_byte(p_config, c::ID_HACK_SCRIPT_VAR_COUNT) };

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.tsx(); // value, operation, register at $0101..$0103,X
	code.lda_abs_x(0x0103); code.cmp_imm(VarCount); code.bcs("@done");
	code.tay();
	code.lda_abs_x(0x0102); code.beq("@and");
	code.cmp_imm(0x01); code.beq("@or");
	code.cmp_imm(0x02); code.bne("@done");

	code.lda_abs_y(Vars); code.db(0x5d); code.dw(0x0101); // EOR $0101,X
	code.jmp("@store");
	code.label("@and");
	code.lda_abs_y(Vars); code.db(0x3d); code.dw(0x0101); // AND $0101,X
	code.jmp("@store");
	code.label("@or");
	code.lda_abs_y(Vars); code.db(0x1d); code.dw(0x0101); // ORA $0101,X
	code.label("@store"); code.db(0x99); code.dw(Vars);

	code.label("@done"); code.pla(); code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevVarShift Register Direction Count
// Direction: 0 = left, 1 = right. Counts above seven produce zero.
word fh::HackManager::apply_AtlasDevVarShift(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	const word Vars{ cfg_word(p_config, c::ID_HACK_SCRIPT_VAR_RAM_ADDR) };
	const byte VarCount{ cfg_byte(p_config, c::ID_HACK_SCRIPT_VAR_COUNT) };

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.tsx(); // count, direction, register at $0101..$0103,X
	code.lda_abs_x(0x0103); code.cmp_imm(VarCount); code.bcs("@done");
	code.tay();
	code.lda_abs_x(0x0102); code.beq("@left");
	code.cmp_imm(0x01); code.bne("@done");

	code.lda_abs_x(0x0101); code.cmp_imm(0x08); code.bcs("@zero");
	code.tax(); code.lda_abs_y(Vars); code.cpx_imm(0x00); code.beq("@store");
	code.label("@right_loop"); code.lsr_a(); code.dex(); code.bne("@right_loop");
	code.jmp("@store");

	code.label("@left");
	code.lda_abs_x(0x0101); code.cmp_imm(0x08); code.bcs("@zero");
	code.tax(); code.lda_abs_y(Vars); code.cpx_imm(0x00); code.beq("@store");
	code.label("@left_loop"); code.asl_a(); code.dex(); code.bne("@left_loop");
	code.jmp("@store");

	code.label("@zero"); code.lda_imm(0x00);
	code.label("@store"); code.db(0x99); code.dw(Vars);
	code.label("@done"); code.pla(); code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevClampVar Register Minimum Maximum
word fh::HackManager::apply_AtlasDevClampVar(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	const word Vars{ cfg_word(p_config, c::ID_HACK_SCRIPT_VAR_RAM_ADDR) };
	const byte VarCount{ cfg_byte(p_config, c::ID_HACK_SCRIPT_VAR_COUNT) };

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.tsx(); // maximum, minimum, register at $0101..$0103,X
	code.lda_abs_x(0x0103); code.cmp_imm(VarCount); code.bcs("@done");
	code.tay();
	code.lda_abs_x(0x0102); code.cmp_abs_x(0x0101);
	code.beq("@bounds_ok"); code.bcc("@bounds_ok"); code.jmp("@done");

	code.label("@bounds_ok"); code.lda_abs_y(Vars);
	code.cmp_abs_x(0x0102); code.bcc("@minimum");
	code.cmp_abs_x(0x0101); code.beq("@store"); code.bcc("@store");
	code.lda_abs_x(0x0101); code.jmp("@store");
	code.label("@minimum"); code.lda_abs_x(0x0102);
	code.label("@store"); code.db(0x99); code.dw(Vars);

	code.label("@done"); code.pla(); code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevIfVarMask Register Mask Expected Label
word fh::HackManager::apply_AtlasDevIfVarMask(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	const word Vars{ cfg_word(p_config, c::ID_HACK_SCRIPT_VAR_RAM_ADDR) };
	const byte VarCount{ cfg_byte(p_config, c::ID_HACK_SCRIPT_VAR_COUNT) };

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.pha();
	code.tsx(); // expected, mask, register at $0101..$0103,X
	code.lda_abs_x(0x0103); code.cmp_imm(VarCount); code.bcs("@false");
	code.tay();

	// Expected may only contain bits selected by Mask.
	code.lda_abs_x(0x0102); code.eor_imm(0xff);
	code.db(0x3d); code.dw(0x0101); // AND $0101,X
	code.bne("@false");
	code.lda_abs_y(Vars); code.db(0x3d); code.dw(0x0102); // AND $0102,X
	code.cmp_abs_x(0x0101); code.bne("@false");

	code.pla(); code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));
	code.label("@false"); code.pla(); code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));
	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevIfPlayerFacing Direction Label: jumps when the player faces the
// requested way; even values mean left, odd mean right. $A4 bit 6 is the
// engine's own facing bit, clear for left and set for right, so the test
// reads the same state the sprite flipper does.
word fh::HackManager::apply_AtlasDevIfPlayerFacing(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.and_imm(0x01);
	code.beq("@want_left");
	code.lda_zp(RAM::ZP_PlayerState);
	code.and_imm(0x40);
	code.bne("@true");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	code.label("@want_left");
	code.lda_zp(RAM::ZP_PlayerState);
	code.and_imm(0x40);
	code.beq("@true");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	code.label("@true");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevIfPlayerClimbing Label: jumps while the player is actively on a
// ladder. Goes through the same fixed-bank predicate the movement code
// itself uses; carry comes back set only during an actual climb.
word fh::HackManager::apply_AtlasDevIfPlayerClimbing(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_PLAYER_ISCLIMBING));
	code.bcs("@true");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));
	code.label("@true");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevIfPlayerGrounded Label: jumps when the player stands on ground.
// Grounded here means none of the engine's jumping, falling or climbing
// state bits are set, so a jump's landing frame answers true.
word fh::HackManager::apply_AtlasDevIfPlayerGrounded(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.lda_zp(RAM::ZP_PlayerState);
	code.and_imm(0x15);
	code.beq("@true");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));
	code.label("@true");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevIfPlayerAttacking Label: $A4 bit 7 is a sword swing in progress.
word fh::HackManager::apply_AtlasDevIfPlayerAttacking(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.lda_zp(RAM::ZP_PlayerState);
	code.bmi("@true");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));
	code.label("@true");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevIfPlayerInvincible Label: the nonzero hurt timer blocks damage.
word fh::HackManager::apply_AtlasDevIfPlayerInvincible(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.lda_zp(RAM::ZP_PlayerInvincibilityTimer);
	code.bne("@true");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));
	code.label("@true");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevIfPlayerDead Label: jumps inside the death dialogue. HP cannot
// answer this; the death path clears its one-shot latch before the script
// starts, and fatal fixed-point damage can leave a nonzero fractional HP
// byte, so zero-versus-nonzero lies in both directions. What is truthful:
// the death call enters IScripts_Begin with $ff, which maps to reserved
// root $1f and is stored at $0200 for the dialogue. Testing that root
// needs no hook and cannot false-positive in ordinary play.
word fh::HackManager::apply_AtlasDevIfPlayerDead(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.lda_abs(RAM::CurrentIScriptRoot);
	code.cmp_imm(0x1f);
	code.beq("@true");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));
	code.label("@true");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevIfSelectedWeapon Weapon Label
word fh::HackManager::apply_AtlasDevIfSelectedWeapon(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.cmp_abs(RAM::SelectedWeapon);
	code.beq("@true");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));
	code.label("@true");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevIfSelectedMagic Magic Label
word fh::HackManager::apply_AtlasDevIfSelectedMagic(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.cmp_abs(RAM::SelectedMagic);
	code.beq("@true");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));
	code.label("@true");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevCountActiveEntities Register
//
// Stores how many of the eight entity slots are live, 0..8, into a script
// register.  AtlasDevIfEntityCountAtLeast answers the common form of this
// question as a branch and needs no register at all; reach for this one only
// when the number itself is wanted, to print or to do arithmetic on.
word fh::HackManager::apply_AtlasDevCountActiveEntities(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	const word Vars{ cfg_word(p_config, c::ID_HACK_SCRIPT_VAR_RAM_ADDR) };
	const byte VarCount{ cfg_byte(p_config, c::ID_HACK_SCRIPT_VAR_COUNT) };

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Register
	code.cmp_imm(VarCount);
	code.bcc("@reg_ok");
	code.lda_imm(0x00);         // out of range selects register 0, as elsewhere
	code.label("@reg_ok");
	code.pha();                 // stack: [register]

	// Three values are live at once -- the running count, the slot index and
	// the byte just read -- and there are only three registers, so the slot
	// byte is read into Y with LDY abs,X rather than into A.  That keeps the
	// count in A across the whole loop and borrows no zero page.
	code.lda_imm(0x00);         // A = running count
	code.ldx_imm(0x07);
	code.label("@slot");
	code.db(0xbc); code.dw(RAM::EntitySlotActive); // LDY $02CC,X, N from the slot
	code.bmi("@skip");                             // bit 7 set = free slot
	code.clc();
	code.adc_imm(0x01);
	code.label("@skip");
	code.dex();
	code.bpl("@slot");          // X walks 7..0 and no lower

	code.tay();                 // park the count; Y is free again here
	code.pla();                 // A = register, stack balanced
	code.tax();
	code.tya();                 // A = count
	code.sta_abs_x(Vars);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevFindEntity Identity Register
//
// Stores the lowest slot holding the requested entity identity, or $FF when
// no slot does.  The result is a slot index, so it pairs with the opcodes
// that take one.
word fh::HackManager::apply_AtlasDevFindEntity(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	const word Vars{ cfg_word(p_config, c::ID_HACK_SCRIPT_VAR_RAM_ADDR) };
	const byte VarCount{ cfg_byte(p_config, c::ID_HACK_SCRIPT_VAR_COUNT) };

	// The identity has to survive the second operand fetch, and LoadByte
	// clobbers Y as well as A, so the stack is the only place for it.
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Identity
	code.pha();                                               // stack: [identity]

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = Register
	code.cmp_imm(VarCount);
	code.bcc("@reg_ok");
	code.lda_imm(0x00);
	code.label("@reg_ok");
	code.tay();                 // Y = register, held across the search

	code.pla();                 // A = identity, stack balanced from here on
	// Bit 7 set in a slot is the engine's free marker, so no live entity ever
	// carries an identity of $80 or above.  Searching for one could only ever
	// match an empty slot, which would report a free slot as a find; answer
	// "absent" instead, without reading the table.
	code.bmi("@absent");

	code.ldx_imm(0x00);
	code.label("@loop");
	code.cmp_abs_x(RAM::EntitySlotActive);
	code.beq("@found");
	code.inx();
	code.cpx_imm(0x08);
	code.bcc("@loop");          // slots 0..7 and no further

	code.label("@absent");
	code.lda_imm(0xff);
	code.bne("@store");         // $ff is nonzero, so this always branches

	code.label("@found");
	code.txa();                 // the slot index is the answer

	code.label("@store");
	code.db(0x99); code.dw(Vars); // STA Vars,Y
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevSetPortrait(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = raw textbox context
	code.cmp_imm(0x00);                                       // GENERIC is exactly $00
	code.beq("@valid");
	code.cmp_imm(0x80);
	code.bcc("@continue");                                  // $01..$7f: invalid, no-op
	code.cmp_imm(0x8b);
	code.bcs("@continue");                                  // $8b..$ff: invalid, no-op

	code.label("@valid");
	code.cmp_abs(RAM::IScriptTextBoxContext);
	code.beq("@continue");                                  // exact same context: no-op
	code.pha();                                                // preserve validated new context
	code.lda_abs(RAM::IScriptTextBoxContext);
	code.bmi("@from_portrait");

	// Generic -> generic only changes the raw context.  Generic -> portrait
	// restores the old rectangle before building the portrait and text frames.
	code.pla();
	code.beq("@store_generic");
	code.pha();
	code.jsr(ROM::TextBox_Close);
	code.pla();
	code.sta_abs(RAM::IScriptTextBoxContext);
	code.and_imm(0x7f);
	code.jsr(ROM::Portrait_LoadTiles);
	code.jsr(ROM::TextBox_OpenForPortrait);
	code.jsr(ROM::TextBox_OpenForNPC);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	code.label("@store_generic");
	code.sta_abs(RAM::IScriptTextBoxContext);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	code.label("@from_portrait");
	code.pla();
	code.sta_abs(RAM::IScriptTextBoxContext);                  // NMI sees new context first
	code.beq("@to_generic");

	// Portrait -> portrait: $F24D normally saves the *current* portrait
	// palette (2) over $03D3.  Preserve the original pre-portrait value so a
	// later End or portrait -> generic transition restores gameplay correctly.
	code.lda_abs(RAM::PortraitSavedPalette);
	code.pha();
	code.lda_abs(RAM::IScriptTextBoxContext);
	code.and_imm(0x7f);
	code.jsr(ROM::Portrait_LoadTiles);
	code.pla();
	code.sta_abs(RAM::PortraitSavedPalette);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	code.label("@to_generic");
	code.jsr(ROM::Portrait_Clear);
	code.jsr(ROM::TextBox_ClearForPortraitAndText);
	code.jsr(ROM::TextBox_OpenForNPC);

	code.label("@continue");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevWaitFrames(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.tax();
	code.beq("@done");
	code.label("@loop");
	code.jsr(ROM::WaitForInterrupt);
	code.dex();
	code.bne("@loop");
	code.label("@done");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevWaitForButtonPress(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.pha();
	code.label("@release");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_UPDATEPORTRAITANIMATION));
	code.pla(); code.pha(); code.and_zp(0x16); code.bne("@release");
	code.label("@wait");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_UPDATEPORTRAITANIMATION));
	code.pla(); code.pha(); code.and_zp(0x19); code.beq("@wait");
	code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevIfButtonHeld(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.and_zp(0x16); code.beq("@false");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));
	code.label("@false");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));
	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevIfButtonPressed(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.and_zp(0x19); code.beq("@false");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));
	code.label("@false");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));
	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevSetFacing(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.cmp_imm(0x00); code.bne("@face_right");
	code.lda_zp(0xa4); code.and_imm(0xbf); code.sta_zp(0xa4);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	code.label("@face_right");
	code.lda_zp(0xa4); code.ora_imm(0x40); code.sta_zp(0xa4);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevSetPlayerPosition(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.pha();
	code.lda_zp(0x0e); code.bne("@reject");
	code.db(0xba);
	code.lda_abs_x(0x0101); code.and_imm(0xf0); code.cmp_imm(0xb0); code.bcs("@reject");
	code.lda_abs_x(0x0101); code.tax();
	code.db(0xbc); code.dw(0x0600); code.jsr(0xe8c6); code.bne("@reject");
	code.txa(); code.clc(); code.adc_imm(0x10); code.tax();
	code.db(0xbc); code.dw(0x0600); code.jsr(0xe8c6); code.bne("@reject");
	code.txa(); code.clc(); code.adc_imm(0x10); code.tax();
	code.db(0xbc); code.dw(0x0600); code.jsr(0xe8c6); code.cmp_imm(0x01); code.bne("@reject");
	code.pla(); code.tax();
	code.and_imm(0xf0); code.sta_zp(0xa1); code.sta_zp(0xb3);
	code.txa(); code.asl_a(); code.asl_a(); code.asl_a(); code.asl_a();
	code.sta_zp(0x9e); code.sta_zp(0xb2);
	code.lda_zp(0xa4); code.and_imm(0x40); code.sta_zp(0xa4);
	code.lda_zp(0xa5); code.and_imm(0x80); code.sta_zp(0xa5);
	code.lda_imm(0x00);
	code.sta_zp(0x9d); code.sta_zp(0x9f); code.sta_zp(0xa0);
	code.sta_zp(0xa2); code.sta_zp(0xa3); code.sta_zp(0xa6);
	code.sta_zp(0xa9); code.sta_zp(0xaa); code.sta_zp(0xac);
	code.sta_zp(0xae); code.sta_zp(0xb1); code.sta_zp(0xb4);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	code.label("@reject");
	code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevOpenWindow(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.cmp_imm(0x1d); code.bcc("@x_ok"); code.lda_imm(0x1c);
	code.label("@x_ok"); code.sta_abs(0x0208);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.cmp_imm(0x1b); code.bcc("@y_ok"); code.lda_imm(0x1a);
	code.label("@y_ok"); code.sta_abs(0x0209);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.cmp_imm(0x21); code.bcc("@width_ok"); code.lda_imm(0x20);
	code.label("@width_ok"); code.sta_abs(0x020a);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.cmp_imm(0x1f); code.bcc("@height_ok"); code.lda_imm(0x1e);
	code.label("@height_ok"); code.sta_abs(0x020b);
	code.jsr(ROM::OpenWindowDraw);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevShowIcon(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.cmp_imm(0x14); code.bcc("@icon_ok"); code.lda_imm(0x13);
	code.label("@icon_ok"); code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(0xee); code.lda_abs(0x0208); code.clc(); code.adc_zp(0xee); code.sta_zp(0xea);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(0xef); code.lda_abs(0x0209); code.clc(); code.adc_zp(0xef); code.sta_zp(0xeb);
	code.pla(); code.tax(); code.jsr(ROM::IconDraw);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevCloseWindow(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	code.jsr(ROM::WindowClose);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevLayText(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	code.lda_abs(0x0208); code.clc(); code.adc_imm(0x02); code.sta_zp(0xea);
	code.lda_abs(0x0209); code.clc(); code.adc_imm(0x02); code.sta_zp(0xeb);
	code.jsr(ROM::TextGridLay);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevOpenWindowAtEntity(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.cmp_imm(0x08); code.bcs("@player"); code.tax();
	code.db(0xb5); code.db(0xba); code.sta_zp(0xe8);
	code.db(0xb5); code.db(0xc2); code.sta_zp(0xe9); code.bcc("@join");
	code.label("@player");
	code.lda_zp(0x9e); code.sta_zp(0xe8); code.lda_zp(0xa1); code.sta_zp(0xe9);
	code.label("@join");
	code.lda_zp(0xe8); code.lsr_a(3); code.sta_zp(0xee);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.clc(); code.adc_zp(0xee); code.and_imm(0xfe); code.sta_abs(0x0208);
	code.lda_zp(0xe9); code.lsr_a(3); code.sta_zp(0xef);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.clc(); code.adc_zp(0xef); code.and_imm(0xfe); code.sta_abs(0x0209);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.and_imm(0xfe); code.sta_abs(0x020a);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.and_imm(0xfe); code.sta_abs(0x020b);
	code.jsr(ROM::OpenWindowDraw);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevRestoreRect(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.and_imm(0xfe); code.sta_abs(0x0208);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.and_imm(0xfe); code.sta_abs(0x0209);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.and_imm(0xfe); code.sta_abs(0x020a);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.and_imm(0xfe); code.sta_abs(0x020b);
	code.jsr(ROM::WindowClose);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevShowItemName(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.cmp_imm(0xa0); code.bcs("@invalid"); code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.sta_zp(0xea);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.sta_zp(0xeb);
	code.pla(); code.jsr(ROM::ItemNameDraw);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	code.label("@invalid");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevShowIconEx(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.cmp_imm(0x14); code.bcc("@shape_ok"); code.lda_imm(0x13);
	code.label("@shape_ok"); code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.and_imm(0x1f); code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(0xee); code.lda_abs(0x0208); code.clc(); code.adc_zp(0xee); code.sta_zp(0xea);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(0xef); code.lda_abs(0x0209); code.clc(); code.adc_zp(0xef); code.sta_zp(0xeb);
	code.pla(); code.sta_zp(0xee); code.pla(); code.tax(); code.lda_zp(0xee);
	code.jsr(ROM::IconDraw);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevClearText(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	code.lda_abs(0x0208); code.clc(); code.adc_imm(0x02); code.sta_zp(0xea);
	code.lda_abs(0x0209); code.clc(); code.adc_imm(0x02); code.sta_zp(0xeb);
	code.jsr(ROM::PPUAddressFromPos);
	code.lda_imm(0x04); code.sta_zp(0xef);
	code.label("@row");
	code.lda_imm(0x10); code.jsr(ROM::PPUQueueAppendHeader);
	code.lda_imm(0x10); code.sta_zp(0xec);
	code.label("@byte");
	code.lda_imm(0x00); code.jsr(ROM::PPUQueuePayload);
	code.db(0xc6); code.db(0xec); code.bne("@byte");
	code.db(0x86); code.db(0x20); code.jsr(ROM::PPUAdvanceRow);
	code.db(0xc6); code.db(0xef); code.bne("@row");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevLayTextAt(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.sta_zp(0xea);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.sta_zp(0xeb);
	code.jsr(ROM::TextGridLay);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevClearTextLine(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.and_imm(0x03); code.clc(); code.adc_imm(0x02);
	code.db(0x6d); code.dw(0x0209); code.sta_zp(0xeb);
	code.lda_abs(0x0208); code.clc(); code.adc_imm(0x02); code.sta_zp(0xea);
	code.jsr(ROM::PPUAddressFromPos);
	code.lda_imm(0x10); code.jsr(ROM::PPUQueueAppendHeader);
	code.lda_imm(0x10); code.sta_zp(0xec);
	code.label("@byte");
	code.lda_imm(0x00); code.jsr(ROM::PPUQueuePayload);
	code.db(0xc6); code.db(0xec); code.bne("@byte");
	code.db(0x86); code.db(0x20);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevLayTextLine(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.sta_zp(0xea);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.sta_zp(0xeb);
	code.jsr(ROM::PPUAddressFromPos);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); code.tay();
	code.jsr(ROM::TextGridRowQueue);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevSetHealth Value: sets the player's health directly. The operand
// is held across a clear of the fractional HP byte, then handed to the
// fixed-bank tail Player_AddHP itself jumps to, which clamps $51..$ff to
// the $50 maximum, writes integer HP and redraws the Power bar. Setting
// zero empties the bar but does not itself start the death sequence; only
// the damage path checks for death.
word fh::HackManager::apply_AtlasDevSetHealth(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = health value
	code.pha();                              // keep it across the fraction clear
	code.lda_imm(0x00);
	code.sta_abs(RAM::PlayerHPFraction);     // fractional HP := 0
	code.pla();
	code.jsr(ROM::UI_DrawPlayerHPValue);     // clamp to $50, write HP, redraw bar
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevSetMana Amount: sets current MP through the fixed-bank setter,
// the entry vanilla Player_AddMP itself tails into.  $00..$50 land
// exactly; anything higher clamps to the fixed $50 maximum, so a script
// cannot overfill the bar.  The setter stores MP $039a and redraws the
// HUD magic bar in the same call, so bar and value can never disagree.
word fh::HackManager::apply_AtlasDevSetMana(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = requested MP
	code.jsr(ROM::Player_SetMP);                              // clamp, store, redraw bar
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevFullHeal
//
// HP is unsigned 8.8 fixed point at $0431:$0432, and every bounded vanilla
// path -- AddHP's clamp, the HUD setter itself, and the complete Elixir
// refill loop -- embeds full as a fixed $50:$00. None of them reads rank,
// armor, shield, or any other equipment; there is no variable maximum in
// the game. The handler clears the fractional byte, then passes $50 through
// the fixed-bank HUD setter, which stores both HP copies and redraws the
// power bar, so the refill is visible the same frame with no mapper access.
word fh::HackManager::apply_AtlasDevFullHeal(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.lda_imm(0x00);
	code.sta_abs(RAM::PlayerHPFraction);
	code.lda_imm(0x50);
	code.jsr(ROM::UI_DrawPlayerHPValue);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevFullMana
//
// MP is one byte at $039a with a fixed $50 maximum: AddMP, the fixed-bank
// setter, and the Elixir refill loop all embed the literal cap, and none of
// them reads rank or equipment, so full is the same value for every player.
// Passing $50 through Player_SetMP stores the byte and redraws the magic bar
// in one call -- the same entrypoint vanilla AddMP tail-jumps into, so the
// HUD can never disagree with the stored value.
word fh::HackManager::apply_AtlasDevFullMana(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.lda_imm(0x50);
	code.jsr(ROM::Player_SetMP);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevIfHealthBelow Threshold Label: jumps when the player's health is
// strictly below the threshold. HP is the fixed-point pair $0431:$0432 with
// the whole points in $0431; only that byte is compared, so health equal to
// the threshold does not jump even while the fraction is nonzero -- the
// fraction can only place health further above the threshold, never below it.
// Uses nothing beyond A and the flags; the operand loader's JSR is balanced.
word fh::HackManager::apply_AtlasDevIfHealthBelow(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = threshold
	code.cmp_abs(RAM::PlayerHP); // threshold - HP whole byte
	code.bcc("@false"); // threshold < HP
	code.beq("@false"); // threshold == HP
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));
	code.label("@false");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevIfHealthAtLeast Threshold Label: jumps when the player's integer
// HP is at least the threshold. HP is the engine's fixed-point pair --
// integer byte at $0431, fraction at $0432 -- and only the integer byte the
// HUD bar draws is compared, so fractional chip damage never changes the
// answer by itself. The operand arrives in A, so carry-clear means the
// threshold sits below the live HP and BEQ catches the boundary; both
// routes take the jump. The engine clamps HP at $50, so thresholds above
// 80 can never pass.
word fh::HackManager::apply_AtlasDevIfHealthAtLeast(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.cmp_abs(RAM::PlayerHP);
	code.bcc("@true");
	code.beq("@true");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));
	code.label("@true");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevIfManaAtLeast Threshold Label: jumps when the player's magic
// points are greater than or equal to the operand. MP is the single
// unsigned byte at $039a -- no fixed-point fraction like HP -- and the
// game's own add and spend paths cap it at $50, so thresholds above
// that simply never pass. The loader leaves the threshold in A and one
// CMP answers the whole question: carry clear means the threshold is
// below MP, zero means equal, and either takes the jump. Uses only A
// and the flags; no scratch RAM.
word fh::HackManager::apply_AtlasDevIfManaAtLeast(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = threshold
	code.cmp_abs(RAM::PlayerMana);                            // threshold - MP
	code.bcc("@true");                                        // threshold < MP
	code.beq("@true");                                        // threshold == MP
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	code.label("@true");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevAddExperience Amount: adds a 16-bit amount of experience through
// the enemy-kill award path. The vanilla adder saturates, runs the
// promotion check and redraws the HUD digits, and the promotion check has
// exactly one caller in the ROM, so nothing caches experience behind our
// back. Rank can advance at most once per call, same as for a kill.
word fh::HackManager::apply_AtlasDevAddExperience(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = amount lo
	code.sta_zp(RAM::ZP_Temp_Int24_L);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = amount hi
	code.sta_zp(RAM::ZP_Temp_Int24_M);
	code.jsr(cfg_word(p_config, c::ID_ROM_PLAYER_UPDATEEXPERIENCE));

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevSetGold Lo Mid Hi
//
// Writes an exact gold amount.  Gold is a 24-bit little-endian counter at
// $0392..$0394; the three operands land there directly, low byte first, so
// any value 0..16777215 is expressible.  After the stores the handler runs
// $f9e7, the vanilla routine that converts the counter to decimal and
// redraws the seven-digit HUD field -- the same tail every vanilla gold
// change goes through -- so the display can never disagree with the stored
// value.  The routine is resident in the fixed bank, callable from the
// bank-12 handler without a bank switch, exactly like WaitForInterrupt and
// Number_DrawAtPos.
word fh::HackManager::apply_AtlasDevSetGold(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = gold lo
	code.sta_abs(RAM::PlayerGold_L);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = gold mid
	code.sta_abs(RAM::PlayerGold_M);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = gold hi
	code.sta_abs(RAM::PlayerGold_U);

	code.jsr(ROM::Hud_DrawGold);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevIfGoldAtLeast ThresholdLo ThresholdMid ThresholdHi Label
//
// Jumps when the player's gold is at least the given 24-bit threshold.
// $0392..$0394 is the game's own little-endian gold counter -- vanilla's
// add and subtract both propagate carry from low through middle to high --
// and the three operands arrive in that same low-first order, so the low
// and middle bytes are parked on the stack while the high byte is fetched.
//
// The comparison is lexicographic from the high byte down.  At each
// significance the operand byte is compared against the counter byte:
// operand below means the counter already exceeds the threshold there
// (taken), operand above means it can never reach it (not taken), and a
// tie defers one byte down.  A tie at every level is gold equal to the
// threshold, which is taken -- that is what makes the test at-least
// rather than strictly-greater.  Every exit has to drop whatever operand
// bytes are still stacked; the digit in each label is exactly that count.
word fh::HackManager::apply_AtlasDevIfGoldAtLeast(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // threshold low
	code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // threshold middle
	code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // threshold high, in A

	code.cmp_abs(RAM::PlayerGold_U);
	code.bcc("@true2");                 // counter's high byte is larger: taken
	code.bne("@false2");                // smaller: not taken

	code.pla();                         // high bytes tie; the middle decides
	code.cmp_abs(RAM::PlayerGold_M);
	code.bcc("@true1");
	code.bne("@false1");

	code.pla();                         // middle ties too; the low byte decides,
	code.cmp_abs(RAM::PlayerGold_L);          // and equality all the way down means
	code.bcc("@true0");                 // gold equals the threshold, which an
	code.beq("@true0");                 // at-least test takes
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	code.label("@true2");               // two operand bytes still stacked
	code.pla();
	code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	code.label("@false2");
	code.pla();
	code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	code.label("@true1");               // one still stacked
	code.pla();
	code.label("@true0");               // stack already clean
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	code.label("@false1");
	code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevIfXPAtLeast Value Label: jumps when the player's experience is at
// least Value. Experience is the little-endian 16-bit counter at $0390/$0391,
// the same pair the engine's own experience add carries through. The Short
// operand arrives low byte first, so the low byte parks on the stack while
// the high bytes decide: a threshold high byte below the player's answers
// true and one above answers false, both without reading the low byte at
// all; only a tie falls through to the low compare, where below-or-equal
// means the threshold is reached. Every exit pops the parked byte, so the
// stack balances on all paths. The label digit is the operand byte index
// under comparison.
word fh::HackManager::apply_AtlasDevIfXPAtLeast(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // threshold lo
	code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // threshold hi
	code.cmp_abs(RAM::PlayerXP_U);
	code.bcc("@true1");
	code.bne("@false1");

	code.pla();
	code.cmp_abs(RAM::PlayerXP_L);
	code.bcc("@true0");
	code.beq("@true0");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	code.label("@true1");
	code.pla();
	code.label("@true0");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	code.label("@false1");
	code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevIfItemCount Item Count Label
//
// jumps when the player's exact ownership count for Item is at least Count.
// "count" is the vanilla definition, read from the same data the fixed-bank
// inventory routines Player_LacksItem ($8CF7) and Player_RemoveItem ($9A6A)
// use: matching entries in the item's category carried array (five arrays
// packed at $039D, 4+4+4+4+8 category-local slots, live length per category
// at $03C2..$03C6), plus one when that category's selected/equipped register
// ($03BD..$03C1) holds the same category-local id.  the eight special items
// ($80/$81/$82/$83/$92/$8A/$93/$94) are single bits in $042C, so their count
// is exactly zero or one.  an id outside the five defined classes ($00-$03
// weapons, $20-$23 armor, $40-$43 shields, $60-$64 magic, $80-$95 items) is
// always false, Count zero included, and is rejected before any table is
// indexed.  only the live prefix of each array is scanned; stale bytes past
// the count never contribute.
//
// the handler is deliberately scratch-free: every intermediate -- the operand
// pair, the category-local id, the running count, the slots-remaining counter
// and the slot cursor -- lives on the hardware stack, reached through
// TSX + LDA/CMP/DEC/INC $01xx,X, so no zero-page byte is claimed and an NMI at
// any instruction boundary (the vanilla handler preserves A/X/Y and everything
// below its own frame) cannot corrupt the state.  both operands are consumed
// up front; each of the two tails drops them before taking its continuation.
word fh::HackManager::apply_AtlasDevIfItemCount(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // item
	code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // count
	code.pha();

	// classify the item id straight off the stack: nine compare/branch pairs
	// carve the five defined classes out of the byte range, everything else
	// falls to @invalid.  stack offsets: $0102,X = item, $0101,X = count.
	code.tsx();
	code.lda_abs_x(0x0102);
	code.cmp_imm(0x04);
	code.bcc("@ordinary");        // $00-$03 weapons
	code.cmp_imm(0x20);
	code.bcc("@invalid");
	code.cmp_imm(0x24);
	code.bcc("@ordinary");        // $20-$23 armor
	code.cmp_imm(0x40);
	code.bcc("@invalid");
	code.cmp_imm(0x44);
	code.bcc("@ordinary");        // $40-$43 shields
	code.cmp_imm(0x60);
	code.bcc("@invalid");
	code.cmp_imm(0x65);
	code.bcc("@ordinary");        // $60-$64 magic
	code.cmp_imm(0x80);
	code.bcc("@invalid");
	code.cmp_imm(0x96);
	code.bcc("@special_scan");    // $80-$95 items, the eight specials among them

	code.label("@invalid");       // undefined id: unconditionally false
	code.pla();
	code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	// the special ids are not contiguous, so probe the exact eight-entry
	// table the vanilla remove path carries; X holds the row on a hit and
	// selects the matching mask over in @special_hit.  a miss falls through
	// into @ordinary, where the id counts as a plain carried item.
	code.label("@special_scan");
	code.ldx_imm(0x07);
	code.label("@special_probe");
	code.cmp_abs_x(ROM::SpecialItemIdTable);
	code.beq("@special_hit");
	code.dex();
	code.bpl("@special_probe");

	// ordinary item: category = id >> 5, local id = id & $1f.  seed the
	// running count from the selected/equipped register (the same $03BD,Y
	// test Player_LacksItem makes -- equipment does not sit in the carried
	// array), then walk the live prefix.
	code.label("@ordinary");
	code.pha();
	code.lsr_a(5);
	code.tay();                   // Y = category
	code.pla();
	code.and_imm(0x1f);
	code.pha();                   // stack: [item count local]
	code.lda_abs_y(RAM::SelectedWeapon); // base of the five-register file
	code.tsx();
	code.cmp_abs_x(0x0101);       // selected register == local id?
	code.bne("@not_selected");
	code.lda_imm(0x01);
	code.bne("@seeded");
	code.label("@not_selected");
	code.lda_imm(0x00);
	code.label("@seeded");
	code.pha();                   // running count, 0 or 1
	code.lda_abs_y(RAM::InventoryCounts);
	code.pha();                   // slots remaining in the live prefix
	code.tya();
	code.asl_a();
	code.asl_a();
	code.tax();                   // X = category * 4 = first slot index

	// per-slot loop.  the register juggling is the price of scratch-freedom:
	// X serves both as the slot cursor and as the stack index, so the cursor
	// is parked on the stack around every stack-relative access.  relative to
	// the parked cursor: $0102,X = remaining, $0104,X = local id; after the
	// two match-path pushes, $0104,X lands on the running count instead.
	code.label("@slot");
	code.txa();
	code.pha();                   // park the cursor
	code.tsx();
	code.lda_abs_x(0x0102);       // slots remaining
	code.beq("@scanned");
	code.dec_abs_x(0x0102);
	code.pla();
	code.tax();
	code.txa();
	code.pha();
	code.tsx();
	code.lda_abs_x(0x0104);       // category-local id
	code.tay();
	code.pla();
	code.tax();
	code.tya();
	code.cmp_abs_x(RAM::InventoryArrays); // carried slot == local id?
	code.bne("@miss");
	code.pha();
	code.txa();
	code.pha();
	code.tsx();
	code.db(0xfe); code.dw(0x0104); // INC $0104,X -- running count (no inc_abs_x mnemonic)
	code.pla();
	code.tax();
	code.pla();
	code.label("@miss");
	code.inx();
	code.bne("@slot");            // always taken; the cursor never wraps

	// unwind: drop cursor and remaining, keep the count through X while the
	// local id comes off, then compare against the Count operand still on
	// the stack.  CMP leaves carry set exactly when count >= Count.
	code.label("@scanned");
	code.pla();
	code.tax();
	code.pla();
	code.pla();
	code.tax();                   // X = running count
	code.pla();
	code.txa();
	code.tsx();
	code.cmp_abs_x(0x0101);
	code.bcs("@true");

	code.label("@false");
	code.pla();
	code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));
	code.label("@true");
	code.pla();
	code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	// special item: one bit of the bitfield through the mask row matching the
	// id row found above; the resulting 0-or-1 count meets the same >=
	// comparison and shares the two tails.
	code.label("@special_hit");
	code.lda_abs(RAM::SpecialItemBitfield);
	code.db(0x3d); code.dw(ROM::SpecialItemMaskTable); // AND $8D52,X (no and_abs_x mnemonic)
	code.beq("@special_zero");
	code.lda_imm(0x01);
	code.bne("@special_compare");
	code.label("@special_zero");
	code.lda_imm(0x00);
	code.label("@special_compare");
	code.tsx();
	code.cmp_abs_x(0x0101);
	code.bcs("@true");
	code.bcc("@false");

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevSetPalette Sub C0 C1 C2 C3: queues four colours at
// $3f00 + Sub*4. Attribute cells using that sub-palette recolour together.
// The queue cursor is published after all four payload bytes are written.
word fh::HackManager::apply_AtlasDevSetPalette(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = sub 0-3
	code.and_imm(0x03);
	code.asl_a();
	code.asl_a();                           // sub * 4
	code.sta_zp(RAM::ZP_e8);                // PPU address = $3f00 + sub*4
	code.lda_imm(0x3f);
	code.sta_zp(RAM::ZP_e9);
	code.lda_imm(0x04);
	code.jsr(ROM::PPUQueueAppendHeader);    // four-byte raw packet
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // colour 0
	code.jsr(cfg_word(p_config, c::ID_ROM_PPU_QUEUE_PAYLOAD));
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // colour 1
	code.jsr(cfg_word(p_config, c::ID_ROM_PPU_QUEUE_PAYLOAD));
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // colour 2
	code.jsr(cfg_word(p_config, c::ID_ROM_PPU_QUEUE_PAYLOAD));
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // colour 3
	code.jsr(cfg_word(p_config, c::ID_ROM_PPU_QUEUE_PAYLOAD));
	code.db(0x86); code.db(RAM::ZP_PPUBufferWriteCursor); // STX $20 -- publish (no stx_zp mnemonic)
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevRestorePalette: drains the queue, stages the area's background
// palette selected by $03d0, and queues the 32-byte palette upload.
word fh::HackManager::apply_AtlasDevRestorePalette(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(ROM::PPUBuffer_WaitEmpty);
	code.lda_abs(RAM::ScreenPaletteIndex);
	code.jsr(ROM::Screen_CopyBgPaletteToShadow);
	code.jsr(ROM::PPUBuffer_QueuePaletteUpload);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevLoadBgPalette Index: stores the selector at $03d0, drains the
// queue, stages the background palette, and queues its upload.
word fh::HackManager::apply_AtlasDevLoadBgPalette(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = palette set
	code.sta_abs(RAM::ScreenPaletteIndex);
	code.jsr(ROM::PPUBuffer_WaitEmpty);
	code.lda_abs(RAM::ScreenPaletteIndex);
	code.jsr(ROM::Screen_CopyBgPaletteToShadow);
	code.jsr(ROM::PPUBuffer_QueuePaletteUpload);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevLoadSpritePalette Index: drains the queue, stages the selected
// sprite palette, and queues the complete palette shadow.
word fh::HackManager::apply_AtlasDevLoadSpritePalette(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = palette set
	code.pha();
	code.jsr(ROM::PPUBuffer_WaitEmpty);
	code.pla();
	code.jsr(ROM::Screen_CopySpritePaletteToShadow);
	code.jsr(ROM::PPUBuffer_QueuePaletteUpload);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevFlashScreen Frames: sets the PPUMASK greyscale bit in $0b for
// the requested frames, then restores the previous value. Zero is a no-op.
word fh::HackManager::apply_AtlasDevFlashScreen(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = frames
	code.tax();
	code.beq("@done");
	code.lda_zp(RAM::ZP_PPUMaskShadow);
	code.pha();                              // saved pre-flash shadow
	code.ora_imm(0x01);                      // greyscale on
	code.sta_zp(RAM::ZP_PPUMaskShadow);
	code.label("@frame");
	code.jsr(ROM::WaitForInterrupt);
	code.dex();
	code.bne("@frame");
	code.pla();
	code.sta_zp(RAM::ZP_PPUMaskShadow);      // restore the previous mask
	code.label("@done");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevSetColorEmphasis Emphasis: replaces PPUMASK bits 5..7 in the
// $0b shadow while preserving bits 0..4. The NMI copies $0b to $2001.
word fh::HackManager::apply_AtlasDevSetColorEmphasis(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = emphasis byte
	code.db(0x45); code.db(RAM::ZP_PPUMaskShadow); // EOR $0b (no eor_zp mnemonic)
	code.and_imm(0xe0);
	code.db(0x45); code.db(RAM::ZP_PPUMaskShadow); // EOR $0b
	code.sta_zp(RAM::ZP_PPUMaskShadow);            // publish once; NMI does the rest
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevQueuePaletteFlush: queues command $00, which copies the staged
// palette at $0293-$02b2 to PPU palette RAM.
word fh::HackManager::apply_AtlasDevQueuePaletteFlush(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(ROM::PPUBuffer_WaitForCapacity);
	code.jsr(ROM::PPUBuffer_QueuePaletteUpload);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevWipeScreenStep Phase AddrHi AddrLo: queues command $fa as
// [$fa, Phase, AddrHi, AddrLo]. It erodes one 16-byte CHR tile, with Phase
// selecting the columns cleared by this step.
word fh::HackManager::apply_AtlasDevWipeScreenStep(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(ROM::PPUBuffer_WaitForCapacity);
	code.db(0xa6); code.db(RAM::ZP_PPUBufferWriteCursor); // LDX $20 (no ldx_zp mnemonic)
	code.lda_imm(0xfa);                      // queue command: column erase
	code.jsr(cfg_word(p_config, c::ID_ROM_PPU_QUEUE_PAYLOAD));
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // phase 0-7
	code.jsr(cfg_word(p_config, c::ID_ROM_PPU_QUEUE_PAYLOAD));
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // PPU address hi
	code.jsr(cfg_word(p_config, c::ID_ROM_PPU_QUEUE_PAYLOAD));
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // PPU address lo
	code.jsr(cfg_word(p_config, c::ID_ROM_PPU_QUEUE_PAYLOAD));
	code.db(0x86); code.db(RAM::ZP_PPUBufferWriteCursor); // STX $20 -- publish
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevAnimateTiles FirstTile Count: queues one command $fc rotation
// for each consecutive PT1 tile. Zero is a no-op and Count clamps to 8.
// Eight calls return a tile to its original pixels.
word fh::HackManager::apply_AtlasDevAnimateTiles(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = first tile
	code.sta_zp(RAM::ZP_Temp_Int24_U);       // running tile id
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = count
	code.beq("@done");
	code.cmp_imm(0x09);
	code.bcc("@count_ok");
	code.lda_imm(0x08);                      // clamp to 8 tiles
	code.label("@count_ok");
	code.sta_zp(RAM::ZP_ef);
	code.jsr(ROM::PPUBuffer_WaitForCapacity);
	code.db(0xa6); code.db(RAM::ZP_PPUBufferWriteCursor); // LDX $20 (no ldx_zp mnemonic)
	code.label("@tile");
	code.lda_imm(0xfc);                      // queue command: rotate tiles
	code.sta_abs_x(RAM::PPUBufferRing);
	code.inx();
	code.lda_zp(RAM::ZP_Temp_Int24_U);
	code.lsr_a(4);
	code.clc();
	code.adc_imm(0x10);                      // pattern table 1
	code.sta_abs_x(RAM::PPUBufferRing);      // PPU address hi
	code.inx();
	code.lda_zp(RAM::ZP_Temp_Int24_U);
	code.asl_a();
	code.asl_a();
	code.asl_a();
	code.asl_a();
	code.sta_abs_x(RAM::PPUBufferRing);      // PPU address lo
	code.inx();
	code.db(0xe6); code.db(RAM::ZP_Temp_Int24_U); // INC $ee (no inc_zp mnemonic)
	code.dec_zp(RAM::ZP_ef);
	code.bne("@tile");
	code.db(0x86); code.db(RAM::ZP_PPUBufferWriteCursor); // STX $20 -- publish
	code.label("@done");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevSetTextColor Colour: changes colour 3 of the textbox sub-palette
// selected by $038d. The same colour is used by the HUD. $ff reloads the
// room's background palette. Some textbox-close paths also reload it.
word fh::HackManager::apply_AtlasDevSetTextColor(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = colour, $ff restores
	code.cmp_imm(0xff);
	code.beq("@restore");
	code.pha();
	code.jsr(ROM::PPUBuffer_WaitEmpty);
	code.lda_abs(RAM::TextBoxSubPalette);
	code.and_imm(0x03);
	code.asl_a();
	code.asl_a();
	code.adc_imm(0x03);                      // sub*4 + 3, carry clear (see above)
	code.tax();
	code.pla();
	code.sta_abs_x(RAM::PaletteShadow);      // the staged glyph-colour byte
	code.jsr(ROM::PPUBuffer_QueuePaletteUpload);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	code.label("@restore");
	code.jsr(ROM::PPUBuffer_WaitEmpty);
	code.lda_abs(RAM::ScreenPaletteIndex);
	code.jsr(ROM::Screen_CopyBgPaletteToShadow);
	code.jsr(ROM::PPUBuffer_QueuePaletteUpload);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevSetAttrRect AttrX AttrY AttrW AttrH Sub: assigns one sub-palette
// to a rectangle of 32x32-pixel attribute cells. The value is copied into
// all four quadrants of each attribute byte. Zero sizes are no-ops and the
// rectangle clips at the 8x8 table edge.
word fh::HackManager::apply_AtlasDevSetAttrRect(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // attr x
	code.and_imm(0x07);
	code.sta_zp(RAM::ZP_ea);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // attr y
	code.and_imm(0x07);
	code.sta_zp(RAM::ZP_eb);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // attr width
	code.sta_zp(RAM::ZP_Temp_Int24_L);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // attr height
	code.sta_zp(RAM::ZP_Temp_Int24_M);

	// sub -> all four quadrants: v = s | s<<2, then v |= v<<4
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // sub 0-3
	code.and_imm(0x03);
	code.sta_zp(RAM::ZP_Temp_Int24_U);
	code.asl_a();
	code.asl_a();
	code.db(0x05); code.db(RAM::ZP_Temp_Int24_U); // ORA $ee (no ora_zp mnemonic)
	code.sta_zp(RAM::ZP_Temp_Int24_U);
	code.asl_a();
	code.asl_a();
	code.asl_a();
	code.asl_a();
	code.db(0x05); code.db(RAM::ZP_Temp_Int24_U); // ORA $ee
	code.sta_zp(RAM::ZP_Temp_Int24_U);            // finished attribute byte

	// Clip width to 8-x.  All operands have already been consumed, so a
	// zero dimension can continue at @done without desynchronizing the script.
	code.lda_imm(0x08);
	code.sec();
	code.db(0xe5); code.db(RAM::ZP_ea);      // SBC $ea (no sbc_zp mnemonic)
	code.cmp_zp(RAM::ZP_Temp_Int24_L);
	code.bcs("@width_ok");
	code.sta_zp(RAM::ZP_Temp_Int24_L);
	code.label("@width_ok");
	code.lda_zp(RAM::ZP_Temp_Int24_L);
	code.beq("@done");

	// Clip height to 8-y.
	code.lda_imm(0x08);
	code.sec();
	code.db(0xe5); code.db(RAM::ZP_eb);      // SBC $eb (no sbc_zp mnemonic)
	code.cmp_zp(RAM::ZP_Temp_Int24_M);
	code.bcs("@height_ok");
	code.sta_zp(RAM::ZP_Temp_Int24_M);
	code.label("@height_ok");
	code.lda_zp(RAM::ZP_Temp_Int24_M);
	code.beq("@done");

	code.lda_zp(RAM::ZP_CameraNametableParity);
	code.and_imm(0x01);
	code.asl_a();
	code.asl_a();
	code.ora_imm(0x23);
	code.sta_zp(RAM::ZP_e9);                 // attribute base hi: $23 or $27
	code.lda_zp(RAM::ZP_eb);
	code.asl_a();
	code.asl_a();
	code.asl_a();                            // row * 8
	code.clc();
	code.adc_zp(RAM::ZP_ea);                 // + column
	code.clc();
	code.adc_imm(0xc0);                      // + $c0: the attribute table
	code.sta_zp(RAM::ZP_e8);

	code.label("@row");
	code.lda_zp(RAM::ZP_Temp_Int24_L);
	code.jsr(ROM::PPUQueueAppendHeader);     // one packet of Width bytes
	code.lda_zp(RAM::ZP_Temp_Int24_L);
	code.sta_zp(RAM::ZP_ef);                 // per-row counter, in memory
	code.label("@byte");
	code.lda_zp(RAM::ZP_Temp_Int24_U);
	code.jsr(cfg_word(p_config, c::ID_ROM_PPU_QUEUE_PAYLOAD));
	code.dec_zp(RAM::ZP_ef);
	code.bne("@byte");
	code.db(0x86); code.db(RAM::ZP_PPUBufferWriteCursor); // STX $20 -- publish the row
	code.lda_zp(RAM::ZP_e8);
	code.clc();
	code.adc_imm(0x08);                      // next attribute row
	code.sta_zp(RAM::ZP_e8);
	code.dec_zp(RAM::ZP_Temp_Int24_M);
	code.bne("@row");
	code.label("@done");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// AtlasDevPlaceChrTile Tile X Y: queues one background tile. X is limited
// to 0..31 and Y to 0..29 so the write stays outside the attribute table.
word fh::HackManager::apply_AtlasDevPlaceChrTile(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // tile
	code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // x
	code.and_imm(0x1f);
	code.sta_zp(RAM::ZP_ea);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // y
	code.cmp_imm(0x1e);
	code.bcc("@y_ok");
	code.lda_imm(0x1d);
	code.label("@y_ok");
	code.sta_zp(RAM::ZP_eb);
	code.jsr(cfg_word(p_config, c::ID_ROM_PPU_ADDRESS_FROM_POS));
	code.lda_imm(0x01);
	code.jsr(ROM::PPUQueueAppendHeader);
	code.pla();
	code.jsr(cfg_word(p_config, c::ID_ROM_PPU_QUEUE_PAYLOAD));
	code.db(0x86); code.db(RAM::ZP_PPUBufferWriteCursor); // STX $20 -- publish
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// Finds the door at packed YX and runs the normal door transition.
// Missing doors continue the script. Region exits keep their normal lock.
word fh::HackManager::apply_AtlasDevWarpToDoor(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = packed door pos
	code.tax();
	code.lda_imm(0x00);
	code.sta_zp(RAM::ZP_DoorMatchFlag);
	code.jsr(ROM::Door_MatchAtBlockPos);
	code.lda_zp(RAM::ZP_DoorMatchFlag);
	code.bne("@found");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	code.label("@found");
	code.jsr(ROM::TextBox_RectRecompute);
	code.jsr(cfg_word(p_config, c::ID_ROM_WINDOW_CLOSE));
	code.lda_imm(0x00);
	code.sta_abs(RAM::DoorKeyRequirement);
	code.jsr(cfg_word(p_config, c::ID_ROM_VANILLA_FAR_CALL));
	code.db(ROM::TransitionBank);
	code.dw(ROM::Door_Dispatch - 1);
	code.jmp(ROM::Game_MainLoop);

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// Runs the normal same-area or cross-area transition.
// Rejects area 4 and values above 7. Screen IDs are not checked.
word fh::HackManager::apply_AtlasDevWarpAreaScreenPos(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = target area
	code.sta_zp(0xee);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = target screen
	code.sta_zp(0xef);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = packed start pos
	code.pha();

	code.lda_zp(0xee);
	code.cmp_imm(0x04);
	code.beq("@refused");
	code.cmp_imm(0x08);
	code.bcc("@loadable");
	code.label("@refused");
	code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	code.label("@loadable");
	code.lda_zp(RAM::ZP_CurrentWorld);
	code.cmp_imm(0x04);
	code.bne("@weapon_live");
	code.lda_abs(RAM::SelectedWeapon);
	code.sta_abs(RAM::LiveWeapon);
	code.label("@weapon_live");

	code.jsr(ROM::TextBox_RectRecompute);
	code.jsr(cfg_word(p_config, c::ID_ROM_WINDOW_CLOSE));

	code.pla();
	code.sta_zp(RAM::ZP_TransitionStartPos);
	code.lda_zp(0xef);
	code.sta_zp(RAM::ZP_TransitionScreen);
	code.db(0xa6); code.db(0xee); // LDX $EE
	code.lda_abs_x(ROM::AreaPaletteTable);
	code.sta_zp(RAM::ZP_TransitionPalette);
	code.lda_zp(0xee);
	code.cmp_zp(RAM::ZP_CurrentWorld);
	code.beq("@same_area");

	code.sta_zp(RAM::ZP_CurrentWorld);
	code.ldx_imm(0x05);
	code.label("@region_scan");
	code.lda_abs_x(ROM::RegionAreaTable);
	code.cmp_zp(RAM::ZP_CurrentWorld);
	code.beq("@region_set");
	code.dex();
	code.bpl("@region_scan");
	code.bne("@region_done");
	code.label("@region_set");
	code.db(0x8e); code.dw(RAM::CurrentStage); // STX $0435
	code.label("@region_done");
	code.jsr(ROM::Screen_PaletteFadeOut);
	code.jsr(cfg_word(p_config, c::ID_ROM_VANILLA_FAR_CALL));
	code.db(ROM::TransitionBank);
	code.dw(ROM::Game_SetupNewArea - 1);
	code.jmp(ROM::Game_MainLoop);

	code.label("@same_area");
	code.jsr(ROM::Screen_PaletteFadeOut);
	code.jsr(cfg_word(p_config, c::ID_ROM_VANILLA_FAR_CALL));
	code.db(ROM::TransitionBank);
	code.dw(ROM::Game_SetupEnterScreen - 1);
	code.jmp(ROM::Game_MainLoop);

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevSpawnEntity(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.cmp_imm(0x65);
	code.bcc("@type_ok");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	code.label("@type_ok");
	code.sta_abs(0x038b);
	code.ldx_imm(0x07);
	code.label("@scan");
	code.lda_abs_x(RAM::EntitySlotActive);
	code.cmp_imm(0xff);
	code.beq("@slot");
	code.dex();
	code.bpl("@scan");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	code.label("@slot");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.pha();
	code.and_imm(0xf0);
	code.sta_abs(0x038a);
	code.pla();
	code.asl_a(); code.asl_a(); code.asl_a(); code.asl_a();
	code.sta_abs(0x0389);
	code.jsr(cfg_word(p_config, c::ID_ROM_VANILLA_FAR_CALL));
	code.db(ROM::TransitionBank);
	code.dw(ROM::EntityAllocate - 1);
	code.db(0xae); code.dw(0x0378); // LDX allocated slot
	code.lda_imm(0x00);
	code.sta_abs_x(RAM::EntityHitStun);
	code.lda_imm(0xff);
	code.sta_abs_x(RAM::EntityScriptRoot);
	code.jsr(ROM::EntityChrPass);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevDropItem(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.cmp_imm(0x65);
	code.bcc("@type_ok");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	code.label("@type_ok");
	code.sta_abs(0x038b);
	code.ldx_imm(0x07);
	code.label("@scan");
	code.lda_abs_x(RAM::EntitySlotActive);
	code.cmp_imm(0xff);
	code.beq("@slot");
	code.dex();
	code.bpl("@scan");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	code.label("@slot");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.pha();
	code.and_imm(0xf0);
	code.sta_abs(0x038a);
	code.pla();
	code.asl_a(); code.asl_a(); code.asl_a(); code.asl_a();
	code.sta_abs(0x0389);
	code.jsr(cfg_word(p_config, c::ID_ROM_VANILLA_FAR_CALL));
	code.db(ROM::TransitionBank);
	code.dw(ROM::EntityAllocate - 1);
	code.db(0xae); code.dw(0x0378); // LDX allocated slot
	code.lda_imm(0x00);
	code.sta_abs_x(RAM::EntityHitStun);
	code.pla();
	code.sta_abs_x(RAM::EntityScriptRoot);
	code.jsr(ROM::EntityChrPass);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevDespawnEntity(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.cmp_imm(0x08);
	code.bcs("@done");
	code.tax();
	code.lda_imm(0xff);
	code.sta_abs_x(RAM::EntitySlotActive);
	code.label("@done");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevDespawnAllEntities(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.ldx_imm(0x07);
	code.lda_imm(0xff);
	code.label("@loop");
	code.sta_abs_x(RAM::EntitySlotActive);
	code.dex();
	code.bpl("@loop");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevSetMetatile(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	const word max_tile_table{ static_cast<word>(cpu_addr + 53) };

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.pha();
	code.db(0xba); // TSX
	code.lda_abs_x(0x0102);
	code.and_imm(0xf0);
	code.cmp_imm(0xd0);
	code.bcs("@reject");
	code.lda_abs_x(0x0101);
	code.db(0xa6); code.db(RAM::ZP_CurrentWorld); // LDX current area
	code.cpx_imm(0x08);
	code.bcs("@reject");
	code.cmp_abs_x(max_tile_table);
	code.bcc("@apply");
	code.beq("@apply");
	code.label("@reject");
	code.pla(); code.pla();
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	code.label("@apply");
	code.pla(); code.sta_abs(0x03ce);
	code.pla(); code.sta_abs(0x03cf);
	code.jsr(ROM::Area_SetBlockAtPosition);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	for (const byte maximum : { 0x7f, 0x88, 0x87, 0x6f, 0xff, 0x5d, 0x62, 0x3f })
		code.db(maximum);

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevDissolveEntity(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(RAM::ZP_e3);
	code.pla();
	code.cmp_imm(0x08);
	code.bcs("@done");
	code.tax();
	code.lda_abs_x(RAM::EntitySlotActive);
	code.bmi("@done");
	code.lda_zp(RAM::ZP_e3);
	code.and_imm(0x01);
	code.beq("@keep_drop");
	code.lda_imm(0x13);
	code.bne("@store_drop");
	code.label("@keep_drop");
	code.lda_abs_x(RAM::EntitySlotActive);
	code.label("@store_drop");
	code.sta_abs_x(RAM::EntityDropIdentity);
	code.lda_zp(RAM::ZP_e3);
	code.and_imm(0x02);
	code.beq("@normal");
	code.lda_imm(0x64);
	code.sta_abs_x(RAM::EntitySlotActive);
	code.lda_imm(0x64);
	code.sta_abs_x(RAM::EntityProgramLo);
	code.bne("@program_hi");
	code.label("@normal");
	code.lda_imm(0x13);
	code.sta_abs_x(RAM::EntitySlotActive);
	code.lda_imm(0x5c);
	code.sta_abs_x(RAM::EntityProgramLo);
	code.label("@program_hi");
	code.lda_imm(0xaf);
	code.sta_abs_x(RAM::EntityProgramHi);
	code.lda_imm(0xff);
	code.sta_abs_x(RAM::EntityOpsMode);
	code.sta_abs_x(RAM::EntityMagicState);
	code.lda_imm(0x00);
	code.sta_abs_x(RAM::EntitySpeedFraction);
	code.sta_abs_x(RAM::EntityHitStun);
	code.lda_abs_x(RAM::EntityFlags);
	code.and_imm(0xbf);
	code.sta_abs_x(RAM::EntityFlags);
	code.lda_imm(0x03);
	code.jsr(ROM::Sound_PlayEffect);
	code.label("@done");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevSetEntityPosition(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(RAM::ZP_e2);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(RAM::ZP_e3);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(RAM::ZP_e4);
	code.lda_zp(RAM::ZP_e2);
	code.cmp_imm(0x08);
	code.bcs("@done");
	code.tax();
	code.lda_zp(RAM::ZP_e3);
	code.db(0x95); code.db(RAM::ZP_EntityX); // STA entity X,X
	code.lda_zp(RAM::ZP_e4);
	code.db(0x95); code.db(RAM::ZP_EntityY); // STA entity Y,X
	code.label("@done");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevSetEntityScript(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(RAM::ZP_e2);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(RAM::ZP_e3);
	code.lda_zp(RAM::ZP_e2);
	code.cmp_imm(0x08);
	code.bcs("@done");
	code.tax();
	code.lda_zp(RAM::ZP_e3);
	code.sta_abs_x(RAM::EntityScriptRoot);
	code.label("@done");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevSetEntityBScript(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(RAM::ZP_e2);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(RAM::ZP_e3);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(RAM::ZP_e4);
	code.lda_zp(RAM::ZP_e2);
	code.cmp_imm(0x08);
	code.bcs("@done");
	code.tax();
	code.lda_zp(RAM::ZP_e3);
	code.sta_abs_x(RAM::EntityProgramLo);
	code.lda_zp(RAM::ZP_e4);
	code.sta_abs_x(RAM::EntityProgramHi);
	code.lda_imm(0xff);
	code.sta_abs_x(RAM::EntityOpsMode);
	code.lda_abs_x(RAM::EntityFlags);
	code.and_imm(0xbf);
	code.sta_abs_x(RAM::EntityFlags);
	code.lda_imm(0x00);
	code.sta_abs_x(RAM::EntityPhase);
	code.label("@done");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevIfEntityInRange(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(RAM::ZP_e2);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.sta_zp(RAM::ZP_e3);
	code.cmp_imm(0x10);
	code.bcs("@false");
	code.lda_zp(RAM::ZP_e2);
	code.cmp_imm(0x08);
	code.bcs("@false");
	code.tax();
	code.lda_abs_x(RAM::EntitySlotActive);
	code.bmi("@false");
	code.db(0xb5); code.db(RAM::ZP_EntityX); // LDA entity X,X
	code.and_imm(0xf0);
	code.sta_zp(RAM::ZP_e4);
	code.lda_zp(RAM::ZP_PlayerX);
	code.and_imm(0xf0);
	code.sec();
	code.db(0xe5); code.db(RAM::ZP_e4); // SBC entity X block
	code.bcs("@x_abs");
	code.eor_imm(0xff);
	code.adc_imm(0x01);
	code.label("@x_abs");
	code.lsr_a(4);
	code.cmp_zp(RAM::ZP_e3);
	code.bcc("@check_y");
	code.beq("@check_y");
	code.bcs("@false");
	code.label("@check_y");
	code.db(0xb5); code.db(RAM::ZP_EntityY); // LDA entity Y,X
	code.and_imm(0xf0);
	code.sta_zp(RAM::ZP_e4);
	code.lda_zp(RAM::ZP_PlayerY);
	code.and_imm(0xf0);
	code.sec();
	code.db(0xe5); code.db(RAM::ZP_e4); // SBC entity Y block
	code.bcs("@y_abs");
	code.eor_imm(0xff);
	code.adc_imm(0x01);
	code.label("@y_abs");
	code.lsr_a(4);
	code.cmp_zp(RAM::ZP_e3);
	code.bcc("@true");
	code.beq("@true");
	code.label("@false");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));
	code.label("@true");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// Accepts events 0-2 and $FF. Invalid values leave the current event unchanged.
word fh::HackManager::apply_AtlasDevSetScreenEvent(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.cmp_imm(0x03);
	code.bcc("@apply");
	code.cmp_imm(0xff);
	code.bne("@done");
	code.label("@apply");
	code.sta_abs(RAM::CurrentScreen_SpecialEventID);
	code.label("@done");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}


// AtlasDevApplyEffect Effect Duration: starts one of the four vanilla timed
// effects (0 ointment, 1 glove, 2 wing boots, 3 hour glass). Writing the
// counter is the whole implementation. The engine's effect tick owns the
// flight bit, the cadence and expiry. Effect masks to 0-3, Duration to
// $00-$7f because bit 7 is the inactive encoding. No item is consumed.
word fh::HackManager::apply_AtlasDevApplyEffect(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // effect
	code.and_imm(0x03);
	code.sta_zp(RAM::ZP_Temp_Int24_U);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // duration
	code.cmp_imm(0x80);
	code.bcc("@duration_ok");
	code.lda_imm(0x7f);
	code.label("@duration_ok");
	code.db(0xa6); code.db(RAM::ZP_Temp_Int24_U); // LDX effect
	code.sta_abs_x(RAM::TimedEffectTimers);
	code.cpx_imm(0x02);
	code.bne("@no_hud");
	code.jsr(cfg_word(p_config, c::ID_ROM_HUD_DRAW_TIMER));
	code.label("@no_hud");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevCastSpell(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	code.cmp_imm(0x05);
	code.bcc("@cast");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
	code.label("@cast");
	code.tay();
	code.sta_abs(RAM::VisibleMagicState);
	code.lda_zp(RAM::ZP_PlayerState);
	code.and_imm(0x40);
	code.sta_abs(RAM::VisibleMagicFlags);
	code.lda_imm(0x00);
	code.sta_abs(RAM::VisibleMagicXFraction);
	code.sta_abs(RAM::VisibleMagicYFraction);
	code.sta_abs(RAM::VisibleMagicCounter);
	code.sta_abs(RAM::VisibleMagicPhase);
	code.lda_zp(RAM::ZP_PlayerPosX);
	code.sta_abs(RAM::VisibleMagicX);
	code.lda_zp(RAM::ZP_PlayerPosY);
	code.cpy_imm(0x01);
	code.beq("@store_y");
	code.cpy_imm(0x02);
	code.beq("@store_y");
	code.clc();
	code.adc_imm(0x08);
	code.label("@store_y");
	code.sta_abs(RAM::VisibleMagicY);
	code.cpy_imm(0x04);
	code.bne("@done");
	code.lda_abs(RAM::VisibleMagicFlags);
	code.ora_imm(0x80);
	code.sta_abs(RAM::VisibleMagicFlags);
	code.lda_imm(0x21);
	code.sta_abs(RAM::VisibleMagicCounter);
	code.label("@done");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevIfMagicActive(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.lda_abs(RAM::VisibleMagicState);
	code.bpl("@active");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));
	code.label("@active");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_AtlasDevClearVisibleMagic(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.lda_imm(0xff);
	code.sta_abs(RAM::VisibleMagicState);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::install_script_variable_reset(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr, word end_handler_addr) const {
	klib::Asm6502 code;
	const word Begin{ cfg_word(p_config, c::ID_ROM_ISCRIPTS_BEGIN) };
	const word Vars{ cfg_word(p_config, c::ID_HACK_SCRIPT_VAR_RAM_ADDR) };
	const byte Count{ cfg_byte(p_config, c::ID_HACK_SCRIPT_VAR_COUNT) };
	if (Count == 0 || Count > 0x80)
		throw std::runtime_error("Script variable count must be between 1 and 128.");

	// Clear every register, then reproduce the six displaced Begin bytes.
	code.pha();
	code.lda_imm(0x00);
	code.ldx_imm(Count - 1);
	code.label("@clear_begin");
	code.sta_abs_x(Vars);
	code.dex();
	code.bpl("@clear_begin");
	code.pla();
	code.cmp_imm(0xff);
	code.bne("@continue");
	code.lda_imm(0x1f);
	code.label("@continue");
	code.jmp(Begin + 6);
	word next{ get_next_cpu_addr(cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr)) };

	code.jmp(cpu_addr);
	code.nop(3);
	code.apply_hack_and_clear(p_rom, 12, Begin);

	// End is the other lifetime boundary.
	const word end_reset{ next };
	code.lda_imm(0x00);
	code.ldx_imm(Count - 1);
	code.label("@clear_end");
	code.sta_abs_x(Vars);
	code.dex();
	code.bpl("@clear_end");
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_UPDATEPORTRAITANIMATION));
	code.lda_abs(RAM::IScriptTextBoxContext);
	code.jmp(end_handler_addr + 6);
	next = get_next_cpu_addr(next,
		code.apply_hack_and_clear(p_rom, 12, next));

	code.jmp(end_reset);
	code.nop(3);
	code.apply_hack_and_clear(p_rom, 12, end_handler_addr);
	return next;
}

// main orchestrator - injects the script routines specified by users through the configuration xml
// and extends the scripting language itself
std::size_t fh::HackManager::apply_script_library(const fe::Config& p_config, std::vector<byte>& p_rom,
	std::size_t p_file_offset, const std::vector<HackLib>& p_lib, std::size_t p_base_opcode_count) const {

	const std::set<HackLib> FLAG_REQUIRED{ HackLib::SetFlag, HackLib::ClearFlag, HackLib::IfFlag,
	HackLib::SelectFlag, HackLib::SetSelectedFlag, HackLib::ClearSelectedFlag, HackLib::IfSelectedFlag };
	const std::set<HackLib> QUEST_FLAG_REQUIRED{ HackLib::SetQuestFlag, HackLib::ClearQuestFlag, HackLib::IfQuestFlag };
	const std::set<HackLib> COMPARE_EQUALS_REQUIRED{ HackLib::IfWorld, HackLib::IfScreen, HackLib::IfStage, HackLib::IfYX, HackLib::IfDoorYX,
	HackLib::IfAddrEquals };
	const std::set<HackLib> COMPARE_BETWEEN_REQUIRED{ HackLib::IfAddrBetween };
	const std::set<HackLib> LOAD_WORD_REQUIRED{ HackLib::IfAddrEquals, HackLib::IfAddrBetween, HackLib::SetAddr };
	const std::set<HackLib> BLOCK_POS_REQUIRED{
		HackLib::IfYX, HackLib::AtlasDevGetPlayerPositionToVars };
	const std::set<HackLib> SCRIPT_VARIABLE_REQUIRED{
		HackLib::AtlasDevSetVar, HackLib::AtlasDevAddVar, HackLib::AtlasDevSubVar,
		HackLib::AtlasDevIfVarEqual, HackLib::AtlasDevIfVarLess,
		HackLib::AtlasDevIfVarGreaterEqual,
		HackLib::AtlasDevShowNumberInMessage, HackLib::AtlasDevShowChoiceToVar,
		HackLib::AtlasDevShowMessageFromVar, HackLib::AtlasDevCountActiveEntities,
		HackLib::AtlasDevFindEntity, HackLib::AtlasDevEntityFieldToVar,
		HackLib::AtlasDevDrawVarNumber, HackLib::AtlasDevGetLocationToVars,
		HackLib::AtlasDevGetPlayerPositionToVars, HackLib::AtlasDevVarBitOp,
		HackLib::AtlasDevVarShift, HackLib::AtlasDevClampVar,
		HackLib::AtlasDevIfVarMask };
	// flag functions need access to the bitmask lookup table
	std::set<HackLib> BITMASK_TABLE_REQUIRED{ FLAG_REQUIRED };
	BITMASK_TABLE_REQUIRED.insert(begin(QUEST_FLAG_REQUIRED), end(QUEST_FLAG_REQUIRED));

	std::vector<word> script_impl_addresses{ read_script_opcode_addrs(p_rom, p_base_opcode_count) };

	const auto rom_addr_start{ klib::Asm6502::get_rom_address(p_file_offset) };
	assert(rom_addr_start.Bank == 12);

	word cpu_addr{ rom_addr_start.CpuAddr };
	// cpu addresses of optional helpers
	std::optional<word> bitmask_table_addr,
		flag_decode_helper_addr,
		quest_flag_decode_helper_addr,
		compare_equals_helper_addr,
		compare_between_helper_addr,
		load_word_helper_addr,
		block_pos_helper_addr;

	// check if the bitmask lookup table needs to be installed
	if (requires_any(p_lib, BITMASK_TABLE_REQUIRED)) {
		const std::vector<byte> BITMASK_TABLE{ 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
		bitmask_table_addr = cpu_addr;
		cpu_addr = get_next_cpu_addr(bitmask_table_addr.value(), klib::Asm6502::apply_bytes(p_rom, BITMASK_TABLE, 12, bitmask_table_addr.value()));
	}

	// check if the flag-check helper must be installed
	if (requires_any(p_lib, FLAG_REQUIRED)) {
		// install hack which clears flag-memory since vanilla does not
		install_hack_clear_flag_memory(p_config, p_rom);
		// install wram to sram for flag data for en-transl and derivatives
		if (p_config.boolean_or(c::ID_FLAGS_WRAM_TO_SRAM, false))
			install_static_hack_flags_to_sram(p_config, p_rom);
		// install the persistent flag decoder helper
		flag_decode_helper_addr = cpu_addr;
		cpu_addr = apply_helper_DecodeScriptFlag(p_config, p_rom, flag_decode_helper_addr.value());
	}

	// check if the quest flag-check helper must be installed
	if (requires_any(p_lib, QUEST_FLAG_REQUIRED)) {
		quest_flag_decode_helper_addr = cpu_addr;
		cpu_addr = apply_helper_DecodeQuestFlag(p_config, p_rom, cpu_addr);
	}

	// install the generic A == operand helper
	if (requires_any(p_lib, COMPARE_EQUALS_REQUIRED)) {
		compare_equals_helper_addr = cpu_addr;
		cpu_addr = apply_helper_IfAEquals(p_config, p_rom, cpu_addr);
	}

	// install the generic min <= A <= max helper
	if (requires_any(p_lib, COMPARE_BETWEEN_REQUIRED)) {
		compare_between_helper_addr = cpu_addr;
		cpu_addr = apply_helper_IfABetween(p_config, p_rom, cpu_addr);
	}

	// check if the load word into (lo, hi) = ($e2, $e3) helper must be installed
	if (requires_any(p_lib, LOAD_WORD_REQUIRED)) {
		load_word_helper_addr = cpu_addr;
		cpu_addr = apply_helper_LoadWord(p_config, p_rom, cpu_addr);
	}

	// check if the block normalizer helper must be installed
	if (requires_any(p_lib, BLOCK_POS_REQUIRED)) {
		block_pos_helper_addr = cpu_addr;
		cpu_addr = apply_helper_GetPlayerBlockPos(p_rom, cpu_addr);
	}

	if (requires_any(p_lib, SCRIPT_VARIABLE_REQUIRED))
		cpu_addr = install_script_variable_reset(p_config, p_rom, cpu_addr,
			script_impl_addresses.at(0) + 1);

	for (HackLib llib : p_lib) {
		script_impl_addresses.push_back(cpu_addr - 1);

		switch (llib) {

		case HackLib::SetFlag: {
			cpu_addr = apply_SetFlag(p_config, p_rom, cpu_addr, flag_decode_helper_addr.value(), bitmask_table_addr.value());
			break;
		}
		case HackLib::ClearFlag: {
			cpu_addr = apply_ClearFlag(p_config, p_rom, cpu_addr, flag_decode_helper_addr.value(), bitmask_table_addr.value());
			break;
		}
		case HackLib::IfFlag: {
			cpu_addr = apply_IfFlag(p_config, p_rom, cpu_addr, flag_decode_helper_addr.value(), bitmask_table_addr.value());
			break;
		}
		case HackLib::SelectFlag: {
			cpu_addr = apply_SelectFlag(p_config, p_rom, cpu_addr);
			break;
		}
		case HackLib::SetSelectedFlag: {
			cpu_addr = apply_SetSelectedFlag(p_config, p_rom, cpu_addr, bitmask_table_addr.value());
			break;
		}
		case HackLib::ClearSelectedFlag: {
			cpu_addr = apply_ClearSelectedFlag(p_config, p_rom, cpu_addr, bitmask_table_addr.value());
			break;
		}
		case HackLib::IfSelectedFlag: {
			cpu_addr = apply_IfSelectedFlag(p_config, p_rom, cpu_addr, bitmask_table_addr.value());
			break;
		}
		case HackLib::SetQuestFlag: {
			cpu_addr = apply_SetQuestFlag(p_config, p_rom, cpu_addr, quest_flag_decode_helper_addr.value(), bitmask_table_addr.value());
			break;
		}
		case HackLib::ClearQuestFlag: {
			cpu_addr = apply_ClearQuestFlag(p_config, p_rom, cpu_addr, quest_flag_decode_helper_addr.value(), bitmask_table_addr.value());
			break;
		}
		case HackLib::IfQuestFlag: {
			cpu_addr = apply_IfQuestFlag(p_config, p_rom, cpu_addr, quest_flag_decode_helper_addr.value(), bitmask_table_addr.value());
			break;
		}
		case HackLib::RunScreenHandler: {
			cpu_addr = apply_RunScreenHandler(p_config, p_rom, cpu_addr);
			break;
		}
		case HackLib::GetXP: {
			cpu_addr = apply_GetXP(p_config, p_rom, cpu_addr);
			break;
		}
		case HackLib::IfWorld: {
			cpu_addr = apply_IfWorld(p_rom, cpu_addr, compare_equals_helper_addr.value());
			break;
		}
		case HackLib::IfScreen: {
			cpu_addr = apply_IfScreen(p_rom, cpu_addr, compare_equals_helper_addr.value());
			break;
		}
		case HackLib::IfStage: {
			cpu_addr = apply_IfStage(p_rom, cpu_addr, compare_equals_helper_addr.value());
			break;
		}
		case HackLib::Die: {
			cpu_addr = apply_Die(p_config, p_rom, cpu_addr);
			break;
		}
		case HackLib::JSR: {
			cpu_addr = apply_JSR(p_config, p_rom, cpu_addr);
			break;
		}
		case HackLib::Return: {
			cpu_addr = apply_Return(p_config, p_rom, cpu_addr);
			break;
		}
		case HackLib::ForceDoor: {
			cpu_addr = apply_ForceDoor(p_config, p_rom, cpu_addr);
			break;
		}
		case HackLib::IfYX: {
			cpu_addr = apply_IfYX(p_rom, cpu_addr, block_pos_helper_addr.value(), compare_equals_helper_addr.value());
			break;
		}
		case HackLib::IfDoorYX: {
			cpu_addr = apply_IfDoorYX(p_rom, cpu_addr, compare_equals_helper_addr.value());
			break;
		}
		case HackLib::IfAddrEquals: {
			cpu_addr = apply_IfAddrEquals(p_rom, cpu_addr, load_word_helper_addr.value(), compare_equals_helper_addr.value());
			break;
		}
		case HackLib::IfAddrBetween: {
			cpu_addr = apply_IfAddrBetween(p_rom, cpu_addr, load_word_helper_addr.value(), compare_between_helper_addr.value());
			break;
		}
		case HackLib::SetAddr:
			cpu_addr = apply_SetAddr(p_config, p_rom, cpu_addr, load_word_helper_addr.value());
			break;

		case HackLib::AtlasDevSetVar:
			cpu_addr = apply_AtlasDevSetVar(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevAddVar:
			cpu_addr = apply_AtlasDevAddVar(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevSubVar:
			cpu_addr = apply_AtlasDevSubVar(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevIfVarEqual:
			cpu_addr = apply_AtlasDevIfVarEqual(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevIfVarLess:
			cpu_addr = apply_AtlasDevIfVarLess(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevIfVarGreaterEqual:
			cpu_addr = apply_AtlasDevIfVarGreaterEqual(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevShakeScreen: {
			cpu_addr = apply_AtlasDevShakeScreen(p_config, p_rom, cpu_addr);
			break;
		}
		case HackLib::AtlasDevFadeOut: {
			cpu_addr = apply_AtlasDevFadeOut(p_config, p_rom, cpu_addr);
			break;
		}
		case HackLib::AtlasDevFadeIn: {
			cpu_addr = apply_AtlasDevFadeIn(p_config, p_rom, cpu_addr);
			break;
		}
		case HackLib::AtlasDevSetMusic:
			cpu_addr = apply_AtlasDevSetMusic(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevPlaySFX:
			cpu_addr = apply_AtlasDevPlaySFX(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevIfMusic:
			cpu_addr = apply_AtlasDevIfMusic(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevShowSequentialMessages:
			cpu_addr = apply_AtlasDevShowSequentialMessages(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevShowNumberInMessage:
			cpu_addr = apply_AtlasDevShowNumberInMessage(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevShowChoiceToVar:
			cpu_addr = apply_AtlasDevShowChoiceToVar(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevClearPortrait:
			cpu_addr = apply_AtlasDevClearPortrait(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevEntitySayMessage:
			cpu_addr = apply_AtlasDevEntitySayMessage(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevShowMessageFromVar:
			cpu_addr = apply_AtlasDevShowMessageFromVar(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevHideTextbox:
			cpu_addr = apply_AtlasDevHideTextbox(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevSetPortrait:
			cpu_addr = apply_AtlasDevSetPortrait(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevOpenTextbox:
			cpu_addr = apply_AtlasDevOpenTextbox(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevCloseDialogue:
			cpu_addr = apply_AtlasDevCloseDialogue(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevIfEntityCountAtLeast:
			cpu_addr = apply_AtlasDevIfEntityCountAtLeast(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevCountActiveEntities:
			cpu_addr = apply_AtlasDevCountActiveEntities(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevFindEntity:
			cpu_addr = apply_AtlasDevFindEntity(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevFreezeEntities:
			cpu_addr = apply_AtlasDevFreezeEntities(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevResumeEntities:
			cpu_addr = apply_AtlasDevResumeEntities(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevIfBossPresent:
			cpu_addr = apply_AtlasDevIfBossPresent(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevIfEntityTypePresent:
			cpu_addr = apply_AtlasDevIfEntityTypePresent(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevIfEntitySlotActive:
			cpu_addr = apply_AtlasDevIfEntitySlotActive(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevIfEntityHidden:
			cpu_addr = apply_AtlasDevIfEntityHidden(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevSetEntityHidden:
			cpu_addr = apply_AtlasDevSetEntityHidden(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevSetEntityHealth:
			cpu_addr = apply_AtlasDevSetEntityHealth(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevSetEntityInvincible:
			cpu_addr = apply_AtlasDevSetEntityInvincible(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevSetEntityBehavior:
			cpu_addr = apply_AtlasDevSetEntityBehavior(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevSetEntitySpeed:
			cpu_addr = apply_AtlasDevSetEntitySpeed(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevSetEntityFacing:
			cpu_addr = apply_AtlasDevSetEntityFacing(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevEntityFieldToVar:
			cpu_addr = apply_AtlasDevEntityFieldToVar(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevDrawVarNumber:
			cpu_addr = apply_AtlasDevDrawVarNumber(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevGetLocationToVars:
			cpu_addr = apply_AtlasDevGetLocationToVars(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevGetPlayerPositionToVars:
			cpu_addr = apply_AtlasDevGetPlayerPositionToVars(p_config, p_rom, cpu_addr,
				block_pos_helper_addr.value());
			break;

		case HackLib::AtlasDevVarBitOp:
			cpu_addr = apply_AtlasDevVarBitOp(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevVarShift:
			cpu_addr = apply_AtlasDevVarShift(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevClampVar:
			cpu_addr = apply_AtlasDevClampVar(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevIfVarMask:
			cpu_addr = apply_AtlasDevIfVarMask(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevIfPlayerFacing:
			cpu_addr = apply_AtlasDevIfPlayerFacing(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevIfPlayerClimbing:
			cpu_addr = apply_AtlasDevIfPlayerClimbing(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevIfPlayerGrounded:
			cpu_addr = apply_AtlasDevIfPlayerGrounded(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevIfPlayerAttacking:
			cpu_addr = apply_AtlasDevIfPlayerAttacking(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevIfPlayerInvincible:
			cpu_addr = apply_AtlasDevIfPlayerInvincible(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevIfPlayerDead:
			cpu_addr = apply_AtlasDevIfPlayerDead(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevIfSelectedWeapon:
			cpu_addr = apply_AtlasDevIfSelectedWeapon(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevIfSelectedMagic:
			cpu_addr = apply_AtlasDevIfSelectedMagic(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevWaitFrames:
			cpu_addr = apply_AtlasDevWaitFrames(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevWaitForButtonPress:
			cpu_addr = apply_AtlasDevWaitForButtonPress(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevIfButtonHeld:
			cpu_addr = apply_AtlasDevIfButtonHeld(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevIfButtonPressed:
			cpu_addr = apply_AtlasDevIfButtonPressed(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevSetFacing:
			cpu_addr = apply_AtlasDevSetFacing(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevSetPlayerPosition:
			cpu_addr = apply_AtlasDevSetPlayerPosition(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevOpenWindow:
			cpu_addr = apply_AtlasDevOpenWindow(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevShowIcon:
			cpu_addr = apply_AtlasDevShowIcon(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevCloseWindow:
			cpu_addr = apply_AtlasDevCloseWindow(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevLayText:
			cpu_addr = apply_AtlasDevLayText(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevOpenWindowAtEntity:
			cpu_addr = apply_AtlasDevOpenWindowAtEntity(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevRestoreRect:
			cpu_addr = apply_AtlasDevRestoreRect(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevShowItemName:
			cpu_addr = apply_AtlasDevShowItemName(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevShowIconEx:
			cpu_addr = apply_AtlasDevShowIconEx(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevClearText:
			cpu_addr = apply_AtlasDevClearText(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevLayTextAt:
			cpu_addr = apply_AtlasDevLayTextAt(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevClearTextLine:
			cpu_addr = apply_AtlasDevClearTextLine(p_config, p_rom, cpu_addr);
			break;

		case HackLib::AtlasDevLayTextLine:
			cpu_addr = apply_AtlasDevLayTextLine(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevSetHealth:
			cpu_addr = apply_AtlasDevSetHealth(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevSetMana:
			cpu_addr = apply_AtlasDevSetMana(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevFullHeal:
			cpu_addr = apply_AtlasDevFullHeal(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevFullMana:
			cpu_addr = apply_AtlasDevFullMana(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevIfHealthBelow:
			cpu_addr = apply_AtlasDevIfHealthBelow(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevIfHealthAtLeast:
			cpu_addr = apply_AtlasDevIfHealthAtLeast(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevIfManaAtLeast:
			cpu_addr = apply_AtlasDevIfManaAtLeast(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevAddExperience:
			cpu_addr = apply_AtlasDevAddExperience(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevSetGold:
			cpu_addr = apply_AtlasDevSetGold(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevIfGoldAtLeast:
			cpu_addr = apply_AtlasDevIfGoldAtLeast(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevIfXPAtLeast:
			cpu_addr = apply_AtlasDevIfXPAtLeast(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevIfItemCount:
			cpu_addr = apply_AtlasDevIfItemCount(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevSetPalette:
			cpu_addr = apply_AtlasDevSetPalette(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevRestorePalette:
			cpu_addr = apply_AtlasDevRestorePalette(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevLoadBgPalette:
			cpu_addr = apply_AtlasDevLoadBgPalette(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevLoadSpritePalette:
			cpu_addr = apply_AtlasDevLoadSpritePalette(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevFlashScreen:
			cpu_addr = apply_AtlasDevFlashScreen(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevSetColorEmphasis:
			cpu_addr = apply_AtlasDevSetColorEmphasis(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevQueuePaletteFlush:
			cpu_addr = apply_AtlasDevQueuePaletteFlush(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevWipeScreenStep:
			cpu_addr = apply_AtlasDevWipeScreenStep(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevAnimateTiles:
			cpu_addr = apply_AtlasDevAnimateTiles(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevSetTextColor:
			cpu_addr = apply_AtlasDevSetTextColor(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevSetAttrRect:
			cpu_addr = apply_AtlasDevSetAttrRect(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevPlaceChrTile:
			cpu_addr = apply_AtlasDevPlaceChrTile(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevWarpToDoor:
			cpu_addr = apply_AtlasDevWarpToDoor(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevWarpAreaScreenPos:
			cpu_addr = apply_AtlasDevWarpAreaScreenPos(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevSpawnEntity:
			cpu_addr = apply_AtlasDevSpawnEntity(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevDropItem:
			cpu_addr = apply_AtlasDevDropItem(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevDespawnEntity:
			cpu_addr = apply_AtlasDevDespawnEntity(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevDespawnAllEntities:
			cpu_addr = apply_AtlasDevDespawnAllEntities(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevSetMetatile:
			cpu_addr = apply_AtlasDevSetMetatile(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevDissolveEntity:
			cpu_addr = apply_AtlasDevDissolveEntity(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevSetEntityPosition:
			cpu_addr = apply_AtlasDevSetEntityPosition(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevSetEntityScript:
			cpu_addr = apply_AtlasDevSetEntityScript(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevSetEntityBScript:
			cpu_addr = apply_AtlasDevSetEntityBScript(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevIfEntityInRange:
			cpu_addr = apply_AtlasDevIfEntityInRange(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevSetScreenEvent:
			cpu_addr = apply_AtlasDevSetScreenEvent(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevApplyEffect:
			cpu_addr = apply_AtlasDevApplyEffect(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevCastSpell:
			cpu_addr = apply_AtlasDevCastSpell(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevIfMagicActive:
			cpu_addr = apply_AtlasDevIfMagicActive(p_config, p_rom, cpu_addr);
			break;
		case HackLib::AtlasDevClearVisibleMagic:
			cpu_addr = apply_AtlasDevClearVisibleMagic(p_config, p_rom, cpu_addr);
			break;

		case HackLib::Count:
		default:
			throw std::runtime_error("Unsupported script library routine.");
		}
	}

	// recreate the script jump table and update references
	cpu_addr += static_cast<word>(write_script_opcode_table(p_rom, cpu_addr, script_impl_addresses));

	return klib::Asm6502::get_file_offset(rom_addr_start.Bank, cpu_addr);
}

// given the extended library, check if any helpers are required
bool fh::HackManager::requires_any(const std::vector<HackLib>& p_lib, const std::set<HackLib>& p_required) const {
	for (HackLib llib : p_required)
		if (std::find(begin(p_lib), end(p_lib), llib) != end(p_lib))
			return true;

	return false;
}

// tilemap change subsystem
std::size_t fh::HackManager::apply_tilemap_change_subsystem(const fe::Config& p_config, std::vector<byte>& p_rom,
	const fh::TilemapChanges& tm_changes) const {

	static const std::vector<byte> BITMASK_TABLE_DATA{ 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
	const word cpu_addr{ cfg_word(p_config, c::ID_TM_CHANGE_CPU_ADDR) };
	const byte data_bank{ static_cast<byte>(p_config.constant(c::ID_TM_CHANGE_BANK)) };

	const word data_table_addr{ cpu_addr };
	const word bitmask_table_addr{ get_next_cpu_addr(data_table_addr,
		klib::Asm6502::apply_bytes(p_rom, tm_changes.to_bytes(data_table_addr), data_bank, data_table_addr)) };
	const word flag_helper_addr{ get_next_cpu_addr(bitmask_table_addr,
		klib::Asm6502::apply_bytes(p_rom, BITMASK_TABLE_DATA, data_bank, bitmask_table_addr)) };
	const word tilemap_changer_addr{ install_hack_tm_flag_helper(p_rom, data_bank, flag_helper_addr, bitmask_table_addr) };
	const word descriptor_handler_addr{ install_hack_tm_tilemap_changer(p_config, p_rom, data_bank, tilemap_changer_addr) };
	const word tm_lookup_addr{ install_hack_tm_descriptor_handler(p_rom, data_bank, descriptor_handler_addr,
		flag_helper_addr, tilemap_changer_addr) };
	const word tm_subsystem_end{ install_hack_tm_lookup(p_rom, data_bank, tm_lookup_addr, descriptor_handler_addr,
			static_cast<word>(data_table_addr + fh::TilemapChanges::EOE_TILEMAP_CHANGE_HEADER.size())) };

	// install screen event handler
	const word tm_event_handler_end{ install_hack_tm_event_handler(p_config, p_rom,
		data_bank, tm_lookup_addr) };

	return static_cast<std::size_t>(tm_subsystem_end - cpu_addr);
}

word fh::HackManager::install_hack_tm_event_handler(const fe::Config& p_config, std::vector<byte>& p_rom,
	byte tm_lookup_bank, word tm_lookup_cpu_addr) const {
	// the handler index for our custom handler
	const byte custom_handler_index{ cfg_byte(p_config, c::ID_TM_CHANGE_HANDLER_IDX) };

	// read the jump table of the three canonical event handlers
	std::vector<word> event_handlers{ read_screen_event_handler_addrs(p_config, p_rom) };

	// we can only append immediately, or overwrite
	if (custom_handler_index > event_handlers.size())
		throw std::runtime_error("Tilemap change handler index exceeds the number of installed screen event handlers.");

	klib::Asm6502 code;

	const word Hack_ScreenEventHandler{ cfg_word(p_config, c::ID_TM_CHANGE_HANDLER_CPU_ADDR) };
	const word Hack_ScreenEventHandlerTable{ cfg_word(p_config, c::ID_TM_CHANGE_HANDLER_TABLE_CPU_ADDR) };
	const word MMC1_UpdateROMBank{ cfg_word(p_config, c::ID_ROM_MMC1_UPDATEROMBANK) };

	// Save the currently mapped switchable bank.
	code.lda_abs(RAM::CurrentROMBank);
	code.pha();
	// switch to the tilemap changes data bank
	code.ldx_imm(tm_lookup_bank);
	code.jsr(MMC1_UpdateROMBank);
	// call the actual tilemap change logic in the other bank
	code.jsr(tm_lookup_cpu_addr);
	// restore the bank that was mapped before this handler ran.
	code.pla();
	code.tax();
	code.jsr(MMC1_UpdateROMBank);

	code.label("@done");
	// Mark the screen event as complete.
	code.lda_imm(0xff);
	code.sta_abs(RAM::CurrentScreen_SpecialEventID);
	code.rts();

	const word result{
		get_next_cpu_addr(Hack_ScreenEventHandler, code.apply_hack_and_clear(p_rom, 15, Hack_ScreenEventHandler),
			0x10000) };

	// remake the event handler table by copying the original addresses
	// and appending / overwriting the custom handler
	// the dispatcher enters handlers using RTS, so entries are address - 1
	if (custom_handler_index == event_handlers.size())
		event_handlers.push_back(Hack_ScreenEventHandler - 1);
	else
		event_handlers[custom_handler_index] = Hack_ScreenEventHandler - 1;

	for (word handler : event_handlers)
		code.dw(handler);

	code.apply_hack_and_clear(p_rom, 15, Hack_ScreenEventHandlerTable);

	// static change: update the valid event-table byte count
	code.cmp_imm(static_cast<byte>(event_handlers.size() * 2));
	code.apply_hack_and_clear(p_rom, 15, ROM::GameLoop_RunScreenEventHandlers_CMP_06);
	// static change: dispatcher expects the high byte first, then the low byte.
	code.lda_abs_y(Hack_ScreenEventHandlerTable + 1);
	code.pha();
	code.lda_abs_y(Hack_ScreenEventHandlerTable);
	code.pha();
	code.apply_hack_and_clear(p_rom, 15, ROM::GameLoop_RunScreenEventHandlers_LDA_EventTable);

	return result;
}

word fh::HackManager::install_hack_tm_lookup(std::vector<byte>& p_rom, byte p_bank, word p_cpu_addr,
	word descriptor_handler_cpu_addr, word data_table_start_cpu_addr) const {
	klib::Asm6502 code;

	code.lda_zp(RAM::ZP_CurrentWorld);
	code.asl_a();
	code.tax();
	code.lda_abs_x(data_table_start_cpu_addr);
	code.sta_zp(RAM::ZP_e4);
	code.lda_abs_x(data_table_start_cpu_addr + 1);
	code.sta_zp(RAM::ZP_e5);
	code.jmp(descriptor_handler_cpu_addr);
	return get_next_cpu_addr(
		p_cpu_addr,
		code.apply_hack_and_clear(p_rom, p_bank, p_cpu_addr));
}

word fh::HackManager::install_hack_tm_descriptor_handler(std::vector<byte>& p_rom, byte p_bank, word p_cpu_addr,
	word flag_helper_cpu_addr, word tm_changer_cpu_addr) const {
	klib::Asm6502 code;

	code.ldy_imm(0x00);
	code.label("@loop");
	code.lda_ind_y(RAM::ZP_e4);
	code.cmp_imm(0xff);
	code.beq("@done");

	code.cmp_zp(RAM::ZP_CurrentScreen);
	code.beq("@found_screen");

	code.iny();

	code.label("@flag_clear");
	code.iny();
	code.iny();
	code.iny();

	code.bne("@loop");

	code.label("@found_screen");
	code.iny(); // y -> flag
	code.lda_ind_y(RAM::ZP_e4);
	code.jsr(flag_helper_cpu_addr);
	code.bcc("@flag_clear");

	code.iny(); // y -> ptr lo

	code.lda_ind_y(RAM::ZP_e4);
	code.sta_zp(RAM::ZP_e2);

	code.iny(); // y -> ptr hi

	code.lda_ind_y(RAM::ZP_e4);
	code.sta_zp(RAM::ZP_e3);

	code.jmp(tm_changer_cpu_addr);

	code.label("@done");
	code.lda_imm(0x00);
	code.rts();

	return get_next_cpu_addr(
		p_cpu_addr,
		code.apply_hack_and_clear(p_rom, p_bank, p_cpu_addr));
}

word fh::HackManager::install_hack_tm_tilemap_changer(const fe::Config& p_config, std::vector<byte>& p_rom, byte p_bank, word p_cpu_addr) const {
	const byte waitframes{ cfg_byte(p_config, c::ID_TM_CHANGE_HANDLER_WAIT_FRAMES) };
	const byte sound_effect{ cfg_byte(p_config, c::ID_TM_CHANGE_HANDLER_SOUND_EFFECT) };

	klib::Asm6502 code;

	code.ldy_imm(0x00);
	code.lda_ind_y(RAM::ZP_e2);
	code.sta_zp(RAM::ZP_Temp08);

	code.ldy_imm(0x01);

	code.label("@loop");

	// wait for a given number of interrupts
	for (std::size_t i{ 0 }; i < waitframes; ++i)
		code.jsr(ROM::WaitForInterrupt);

	// optionally play a sound effect
	if (sound_effect != 0xff) {
		code.lda_imm(sound_effect);
		code.jsr(ROM::Sound_PlayEffect);
	}

	code.lda_ind_y(RAM::ZP_e2);
	code.tax();
	code.iny();
	code.lda_ind_y(RAM::ZP_e2);
	code.iny();

	code.sta_abs_x(RAM::ScreenBuffer);

	code.sty_zp(RAM::ZP_Temp07);
	code.jsr(ROM::Area_SetBlocks);
	code.ldy_zp(RAM::ZP_Temp07);

	code.dec_zp(RAM::ZP_Temp08);
	code.bne("@loop");

	code.rts();

	return get_next_cpu_addr(
		p_cpu_addr,
		code.apply_hack_and_clear(p_rom, p_bank, p_cpu_addr));
}

word fh::HackManager::install_hack_tm_flag_helper(std::vector<byte>& p_rom, byte p_bank, word p_cpu_addr,
	word p_table_addr) const {
	klib::Asm6502 code;

	code.pha();
	code.lsr_a(3);
	code.tax();
	code.lda_abs_x(RAM::Flags);
	code.sta_zp(RAM::ZP_Temp08);
	code.pla();
	code.and_imm(0x07);
	code.tax();
	code.lda_abs_x(p_table_addr);
	code.and_zp(RAM::ZP_Temp08);
	code.beq("@clear");
	code.sec();
	code.rts();

	code.label("@clear");
	code.clc();
	code.rts();

	return get_next_cpu_addr(
		p_cpu_addr,
		code.apply_hack_and_clear(p_rom, p_bank, p_cpu_addr));
}

// other hacks
word fh::HackManager::install_hack_clear_flag_memory(const fe::Config& p_config, std::vector<byte>& p_rom) const {
	const word Hack_ClearPersistentFlags{ cfg_word(p_config, c::ID_HACK_CLEAR_PERSISTENT_FLAGS) };

	klib::Asm6502 code;

	// Helper at $D005.
	code.ldx_imm(c::FlagsByteCount);      // #$1F

	code.label("@loop");
	code.sta_abs_x(RAM::Flags - 1);     // STA $0100,X
	code.dex();
	code.bne("@loop");

	code.jsr(ROM::Game_InitMMCAndBank);
	code.jmp(ROM::Game_Init_JSR_Game_InitScreenAndMusic);
	word result{ static_cast<word>(Hack_ClearPersistentFlags + code.apply_hack_and_clear(p_rom, 15, Hack_ClearPersistentFlags)) };

	// Replace JSR Game_InitMMCAndBank
	code.jmp(Hack_ClearPersistentFlags);
	code.apply_hack_and_clear(p_rom, 15, ROM::Game_Init_JSR_Game_InitMMCAndBank);

	return result;
}

// installs a series of hacks, static and dynamic, which allow the requirement byte in
// sameworld doors to encode a destination stage as well
// door data must be migrated elsewhere by setting requirement = (destination_stage) << 4 + requirement
void fh::HackManager::install_hack_sameworld_to_stage_doors(const fe::Config& p_config, std::vector<byte>& p_rom) {
	// new routine addresses

	// randumizer addresses for reference
	// const word Hack_ClearPendingStageAndLoadWorld{ 0xfda0 };
	// const word Hack_SetPendingStage{ 0xfe00 };
	// const word Hack_HandlePalette{ 0xfe20 };
	// const word Hack_ExtractStageAndRequirement{ 0xfe40 };
	// (we do not add a separate function for palette to music like the
	// randomizer as we already handle the map dynamically in the frontend)

	// new routine addresses to avoid claiming free space
	// default: use the sound effect priority table from index 1
	const word Hack_HandlePalette{ static_cast<word>(p_config.constant_or(c::ID_HACK_HANDLE_PALETTE_ADDR, 0xf389)) };
	// default: use the normally unreachable debug code, lay the functions out contiguously there
	const word Hack_SetPendingStage{ static_cast<word>(p_config.constant_or(c::ID_HACK_SET_PENDING_STAGE_ADDR, 0xdf99)) };
	const word Hack_ExtractStageAndRequirement{ static_cast<word>(p_config.constant_or(c::ID_HACK_DECODE_REQ_ADDR, 0xdfa8)) };
	const word Hack_ClearPendingStageAndLoadWorld{ static_cast<word>(p_config.constant_or(c::ID_HACK_LOAD_WORLD_ADDR, 0xdfb7)) };

	klib::Asm6502 code{};

	// new routine for setting pending stage: Hack_SetPendingStage
	code.lda_imm(0x01);
	code.sta_abs(RAM::Hack_StageChangePending);
	code.lda_abs(RAM::Hack_PendingStage);
	code.sta_abs(RAM::CurrentStage);
	code.jsr(ROM::Game_SetupAndLoadOutsideArea);
	code.rts();
	code.apply_hack_and_clear(p_rom, 15, Hack_SetPendingStage);

	// update the sameworld-door logic to jump into our new routine instead of vanilla
	code.jmp(Hack_SetPendingStage);
	code.apply_hack_and_clear(p_rom, 15, ROM::Player_CheckHandleEnterDoor_enterScreen);

	// new routine for handling hack door palette: Hack_HandlePalette
	code.lda_imm(0x00);
	code.jsr(ROM::Screen_CopySpritePaletteToShadow);
	code.lda_abs(RAM::Hack_StageChangePending);
	code.cmp_imm(0x01);
	code.beq("@stage_pending");
	// hack stage change not pending, use vanilla palette logic
	code.jmp(ROM::Game_LoadCurrentArea_LDX_Stage);
	// hack stage change pending - clear the flag and load screen
	code.label("@stage_pending");
	code.lda_imm(0x00);
	code.sta_abs(RAM::Hack_StageChangePending);
	code.jmp(ROM::Screen_Load);
	code.apply_hack_and_clear(p_rom, 15, Hack_HandlePalette);

	// update the stage palette logic to jump into our palette handler
	code.jmp(Hack_HandlePalette);
	code.nop();
	code.nop();
	code.apply_hack_and_clear(p_rom, 15, ROM::Game_LoadCurrentArea_LoadPalette);

	// extract stage and actual door requirement from hack-door data: Hack_ExtractStageAndRequirement
	code.tay();
	// get stage from the requirement byte (hi nibble)
	code.lsr_a();
	code.lsr_a();
	code.lsr_a();
	code.lsr_a();
	code.sta_abs(RAM::Hack_PendingStage);
	code.tya();
	// get actual requirement (lo nibble)
	code.and_imm(0x0f);
	code.sta_abs(RAM::DoorKeyRequirement);
	code.rts();
	code.apply_hack_and_clear(p_rom, 15, Hack_ExtractStageAndRequirement);

	// instead of storing A in door requirement ram directly, jump to the new routine
	code.jsr(Hack_ExtractStageAndRequirement);
	code.apply_hack_and_clear(p_rom, 15, ROM::Area_SetStateFromDoorDestination_STA_DoorReq);

	// clear pending hack stage change flag and load world
	code.lda_imm(0x00);
	code.sta_abs(RAM::Hack_StageChangePending);
	code.jmp(ROM::Game_SetupAndLoadOutsideArea);
	code.apply_hack_and_clear(p_rom, 15, Hack_ClearPendingStageAndLoadWorld);

	// hook vanilla code into our new routine
	code.jmp(Hack_ClearPendingStageAndLoadWorld);
	code.apply_hack_and_clear(p_rom, 15, ROM::Player_EnterDoorToOutside_JMP_SetupArea);
}

// this code ensures the flag RAM is stored and restored via SRAM for the translation hack 'en-transl' and derivatives
void fh::HackManager::install_static_hack_flags_to_sram(const fe::Config& p_config, std::vector<byte>& p_rom) const {
	const word HackStaticExtraSave{ 0x90c0 };
	const word HackStaticExtraLoad{ 0x90dd };

	const word SRAM_Save_Hook = 0x9cdf;
	const word SRAM_Load_Hook = 0x9121;
	const word SRAM_Save_Continue = 0x9ce3;

	const word SRAM_FlagsBlock_Start{ 0x6171 };

	klib::Asm6502 code;

	// *** extra save routine ***
	code.ldx_imm(c::FlagsByteCount);
	code.label("@next");
	// save $0101-$011f -> $6171-$618f
	code.lda_abs_x(RAM::Flags - 1);
	code.sta_abs_x(SRAM_FlagsBlock_Start - 1);
	code.dex();
	code.bne("@next");

	code.lda_imm(0x14);
	code.sta_zp(0xe9);
	code.jmp(SRAM_Save_Continue);

	code.apply_hack_and_clear(p_rom, 12, HackStaticExtraSave);

	// *** extra load routine ***
	// If the original loop wasn't finished, continue it.
	code.bne("@continue");

	code.ldx_imm(c::FlagsByteCount);
	// load $6171-$618f -> $0101-$011f
	code.label("@next");
	code.lda_abs_x(SRAM_FlagsBlock_Start - 1);
	code.sta_abs_x(RAM::Flags - 1);
	code.dex();
	code.bne("@next");

	code.rts();
	code.label("@continue");
	code.jmp(0x910d);

	code.apply_hack_and_clear(p_rom, 12, HackStaticExtraLoad);

	// install save hook
	code.jmp(HackStaticExtraSave);
	code.apply_hack_and_clear(p_rom, 12, SRAM_Save_Hook);

	// install load hook
	code.jmp(HackStaticExtraLoad);
	code.apply_hack_and_clear(p_rom, 12, SRAM_Load_Hook);
}

word fh::HackManager::cfg_word(const fe::Config& p_config, const std::string& p_id) const {
	return static_cast<word>(p_config.constant(p_id));
}

byte fh::HackManager::cfg_byte(const fe::Config& p_config, const std::string& p_id) const {
	return static_cast<byte>(p_config.constant(p_id));
}

word fh::HackManager::get_next_cpu_addr(word cpu_addr, std::size_t hack_size, std::size_t max_addr) const {
	auto next_addr{ cpu_addr + hack_size };
	if (next_addr > max_addr)
		throw std::runtime_error("Hack overflow");
	return static_cast<word>(next_addr);
}

std::vector<word> fh::HackManager::read_script_opcode_addrs(const std::vector<byte>& p_rom, std::size_t p_opcode_count) const {
	std::vector<word> result;

	word ptrs_hi{ klib::Asm6502::read_word(p_rom, 12, ROM::IScripts_JumpTable_Ref_U) };
	word ptrs_lo{ klib::Asm6502::read_word(p_rom, 12, ROM::IScripts_JumpTable_Ref_L) };

	std::size_t offset_hi{ klib::Asm6502::get_file_offset(12, ptrs_hi) };
	std::size_t offset_lo{ klib::Asm6502::get_file_offset(12, ptrs_lo) };

	for (std::size_t i{ 0 }; i < p_opcode_count; ++i)
		result.push_back(static_cast<word>(p_rom.at(offset_lo + i) | p_rom.at(offset_hi + i) << 8));

	return result;
}

std::size_t fh::HackManager::write_script_opcode_table(std::vector<byte>& p_rom, word table_cpu_addr,
	const std::vector<word>& p_jump_table) const {

	word ref_lo{ table_cpu_addr };
	word ref_hi{ static_cast<word>(table_cpu_addr + p_jump_table.size()) };

	klib::Asm6502::apply_word(p_rom, ref_hi, 12, ROM::IScripts_JumpTable_Ref_U);
	klib::Asm6502::apply_word(p_rom, ref_lo, 12, ROM::IScripts_JumpTable_Ref_L);

	return klib::Asm6502::apply_words_as_split_table(p_rom, p_jump_table, 12, table_cpu_addr);
}

std::vector<word> fh::HackManager::read_screen_event_handler_addrs(const fe::Config& p_config,
	const std::vector<byte>& p_rom) const {
	const word dispatcher_addr{ ROM::GameLoop_RunScreenEventHandlers_LDA_EventTable };

	// TODO: sanity check that the two LDA operands differ by exactly one
	const word table_ref{ klib::Asm6502::read_word(p_rom, 15, static_cast<word>(dispatcher_addr + 5)) };

	std::vector<word> result;

	for (std::size_t i{ 0 }; i < detect_screen_event_handler_count(p_config, p_rom); ++i)
		result.push_back(klib::Asm6502::read_word(p_rom, 15, static_cast<word>(table_ref + 2 * i)));

	return result;
}

std::size_t fh::HackManager::detect_screen_event_handler_count(const fe::Config& p_config,
	const std::vector<byte>& p_rom) const {
	return p_rom.at(p_config.constant(c::ID_COMMAND_BYTE_COUNT_OFFSET)) / 2;
}
