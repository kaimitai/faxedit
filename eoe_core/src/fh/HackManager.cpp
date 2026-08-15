#include "HackManager.h"
#include "fh_constants.h"
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

// DO NOT USE YET.  This opcode is published for review, not for shipping
// scripts.  It needs the script-variable feature (hack_script_var_ram_addr /
// hack_script_var_count), which is not upstream, so Config::constant throws
// by name and the build fails rather than the ROM misbehaving.  It has had no
// hardware validation beyond a purpose-built fixture that defined that RAM.
word fh::HackManager::apply_AtlasDevShowNumberInMessage(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;
	// Renders a script register, so this needs the script-variable feature
	// exactly as AtlasDevShowChoiceToVar and AtlasDevShowMessageFromVar do.
	// Config::constant throws by name if a project has not defined the base,
	// which is what stops this reading unallocated RAM.
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

// DO NOT USE YET.  This opcode is published for review, not for shipping
// scripts.  It needs the script-variable feature (hack_script_var_ram_addr /
// hack_script_var_count), which is not upstream, so Config::constant throws
// by name and the build fails rather than the ROM misbehaving.  It has had no
// hardware validation beyond a purpose-built fixture that defined that RAM.
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
	// The chosen index is stored in a script register, so this opcode needs
	// the script-variable feature just as AtlasDevShowMessageFromVar does.
	// Reading the base through the config rather than hardcoding $03B5 means
	// Config::constant throws by name when a project has not defined it,
	// instead of silently writing to unallocated RAM.
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

// DO NOT USE YET.  This opcode is published for review, not for shipping
// scripts.  It needs the script-variable feature (hack_script_var_ram_addr /
// hack_script_var_count), which is not upstream, so Config::constant throws
// by name and the build fails rather than the ROM misbehaving.  It has had no
// hardware validation beyond a purpose-built fixture that defined that RAM.
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

// DO NOT USE YET.  This opcode is published for review, not for shipping
// scripts.  It needs the script-variable feature (hack_script_var_ram_addr /
// hack_script_var_count), which is not upstream, so Config::constant throws
// by name and the build fails rather than the ROM misbehaving.  It has had no
// hardware validation beyond a purpose-built fixture that defined that RAM.
//
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

// DO NOT USE YET.  This opcode is published for review, not for shipping
// scripts.  It needs the script-variable feature (hack_script_var_ram_addr /
// hack_script_var_count), which is not upstream, so Config::constant throws
// by name and the build fails rather than the ROM misbehaving.  It has had no
// hardware validation beyond a purpose-built fixture that defined that RAM.
//
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

// DO NOT USE YET.  This opcode is published for review, not for shipping
// scripts.  It needs the script-variable feature (hack_script_var_ram_addr /
// hack_script_var_count), which is not upstream, so Config::constant throws
// by name and the build fails rather than the ROM misbehaving.  It has had no
// hardware validation beyond a purpose-built fixture that defined that RAM.
//
// AtlasDevCountActiveEntities Register
//
// Stores how many of the eight entity slots are live, 0..8, into a script
// register.  AtlasDevIfEntityCountAtLeast answers the common form of this
// question as a branch and needs no register at all; reach for this one only
// when the number itself is wanted, to print or to do arithmetic on.
word fh::HackManager::apply_AtlasDevCountActiveEntities(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	// Read the register file through the configuration rather than hardcoding
	// an address, exactly as the packet 6 register opcodes do.  A project that
	// has not defined the feature fails the build by name here, instead of
	// shipping a ROM that writes RAM nobody allocated.
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

// DO NOT USE YET.  This opcode is published for review, not for shipping
// scripts.  It needs the script-variable feature (hack_script_var_ram_addr /
// hack_script_var_count), which is not upstream, so Config::constant throws
// by name and the build fails rather than the ROM misbehaving.  It has had no
// hardware validation beyond a purpose-built fixture that defined that RAM.
//
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
	const std::set<HackLib> BLOCK_POS_REQUIRED{ HackLib::IfYX };
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
