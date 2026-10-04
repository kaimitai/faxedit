#include "HackManager.h"
#include "fh_constants.h"
#include "fe/Game.h"
#include "fe/ROM_Manager.h"
#include "common/klib/Asm6502.h"
#include <format>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
	constexpr byte DOOR_KEY_REQ_MIN{ 0x01 };
	constexpr byte DOOR_KEY_REQ_MAX{ 0x05 };
	constexpr byte DOOR_FLAG_MAX{ static_cast<byte>(fh::c::FlagsByteCount * 8 - 1) };

	constexpr byte PTR_LO{ fh::RAM::ZP_e2 };
	constexpr byte PTR_HI{ fh::RAM::ZP_e3 };
	constexpr byte VALUE_IO{ fh::RAM::ZP_e5 };

	struct DoorFlag {
		byte screen;
		byte yx;
		byte flag;
	};

	using DoorFlagTable = std::vector<std::vector<DoorFlag>>;

	DoorFlagTable make_door_flag_table(const fe::Game* p_game) {
		if (!p_game)
			throw std::runtime_error("PermaDoors: Door data not available");

		DoorFlagTable table(p_game->m_chunks.size());
		std::size_t flag_count{ 0 };

		for (std::size_t world{ 0 }; world < p_game->m_chunks.size(); ++world) {
			const auto stage{ p_game->m_stages.get_stage_from_world(world) };

			for (std::size_t screen{ 0 };
				screen < p_game->m_chunks[world].m_screens.size(); ++screen) {

				for (const auto& door : p_game->m_chunks[world].m_screens[screen].m_doors) {
					byte requirement{};

					if (door.m_door_type == fe::DoorType::NextWorld ||
						door.m_door_type == fe::DoorType::PrevWorld) {
						if (!stage)
							continue; // world belongs to multiple stages -> ambiguous

						requirement = door.m_door_type == fe::DoorType::NextWorld
							? (*stage)->m_next_requirement
							: (*stage)->m_prev_requirement;
					}
					else {
						requirement = static_cast<byte>(door.m_requirement & 0x0f);
					}

					if (requirement < DOOR_KEY_REQ_MIN || requirement > DOOR_KEY_REQ_MAX)
						continue;

					if (flag_count > DOOR_FLAG_MAX)
						throw std::runtime_error("Too many key-locked doors");

					table[world].push_back({
						.screen = static_cast<byte>(screen),
						.yx = static_cast<byte>(
							(door.m_coords.second << 4) | door.m_coords.first),
						.flag = static_cast<byte>(DOOR_FLAG_MAX - flag_count++)
						});
				}
			}
		}

		return table;
	}

	void append_PermaDoors_LookupTable(klib::Asm6502& code, const DoorFlagTable& table) {
		code.label("@lookup_table");

		// emit world ptrs
		for (std::size_t world{ 0 }; world < table.size(); ++world)
			code.dw(std::format("door_flags_world_{}", world));

		// emit lookup array per world
		for (std::size_t world{ 0 }; world < table.size(); ++world) {
			code.label(std::format("door_flags_world_{}", world));

			for (const auto& door : table[world]) {
				code.db(door.screen);
				code.db(door.yx);
				code.db(door.flag);
			}

			code.db(0xff);
		}
	}

	void append_PermaDoors_TableWalker(klib::Asm6502& code) {
		code.label("@table_walker");
		code.lda_zp(fh::RAM::ZP_CurrentWorld);
		code.asl_a();
		code.tax();

		code.lda_abs_x("@lookup_table");
		code.sta_zp(PTR_LO);
		code.lda_abs_x("@lookup_table", 1);
		code.sta_zp(PTR_HI);

		code.ldy_imm(0x00);
		code.label("@next");
		code.lda_ind_y(PTR_LO);
		code.cmp_imm(0xff);
		code.beq("@not_found");

		code.cmp_zp(fh::RAM::ZP_CurrentScreen);
		code.bne("@skip");

		code.iny();
		code.lda_ind_y(PTR_LO);
		code.cmp_zp(fh::RAM::ZP_DoorBlockPos);
		code.beq("@found");

		// screen matched, but Y is now +1
		code.iny();
		code.iny();
		code.jmp("@next");

		code.label("@skip");
		code.iny();
		code.iny();
		code.iny();
		code.jmp("@next");

		code.label("@found");
		code.iny();
		code.lda_ind_y(PTR_LO);
		// flag no stored in A
		code.rts();

		code.label("@not_found");
		code.lda_imm(0xff);
		// $ff denotes no flag selection
		code.rts();
	}

	void append_PermaDoors_BitmaskTable(klib::Asm6502& code) {
		code.label("@bitmask_table");
		code.db(0x01);
		code.db(0x02);
		code.db(0x04);
		code.db(0x08);
		code.db(0x10);
		code.db(0x20);
		code.db(0x40);
		code.db(0x80);
	}

	// ; A = flag number
	// ; Returns A = 0 if not set / invalid
	// ; Returns A != 0 if set
	void append_PermaDoors_CheckFlag(klib::Asm6502& code) {
		code.label("@check_flag");
		code.cmp_imm(0xff);
		code.beq("@clear");

		code.tax();
		code.lsr_a(3);
		code.tay();

		code.txa();
		code.and_imm(0x07);
		code.tax();

		code.lda_abs_y(fh::RAM::Flags);
		code.and_abs_x("@bitmask_table");
		code.rts();

		code.label("@clear");
		code.lda_imm(0x00);
		code.rts();
	}

	// A = flag number
	void append_PermaDoors_SetFlag(klib::Asm6502& code) {
		code.label("@set_flag");

		code.cmp_imm(0xff);
		code.beq("@done");

		code.tax();
		code.lsr_a(3);
		code.tay();

		code.txa();
		code.and_imm(0x07);
		code.tax();
		code.lda_abs_y(fh::RAM::Flags);
		code.ora_abs_x("@bitmask_table");
		code.sta_abs_y(fh::RAM::Flags);

		code.label("@done");
		code.rts();
	}

	// ; ZP addr VALUE_IO:
	// ; 0 = check
	// ; 1 = set
	// ;
	// ; returns:
	// ; check -> VALUE_IO=0 or VALUE_IO!=0
	// ; set -> ignored
	void append_PermaDoors_Main(klib::Asm6502& code) {
		code.label("@main");

		code.jsr("@table_walker");
		code.ldx_zp(VALUE_IO);
		code.beq("@check");

		code.jsr("@set_flag");
		code.rts();

		code.label("@check");
		code.jsr("@check_flag");
		code.sta_zp(VALUE_IO);
		code.rts();
	}

	std::pair<word, word> install_PermaDoors_FreeBankInstall(std::vector<byte>& p_rom, byte p_bank, word cpu_addr,
		const fe::Game* p_game) {
		const auto l_door_flag_table{ make_door_flag_table(p_game) };

		klib::Asm6502 code;

		append_PermaDoors_LookupTable(code, l_door_flag_table);
		append_PermaDoors_TableWalker(code);
		append_PermaDoors_BitmaskTable(code);
		append_PermaDoors_CheckFlag(code);
		append_PermaDoors_SetFlag(code);
		append_PermaDoors_Main(code);

		const word main_entry_addr{ code.label_addr("@main", cpu_addr) };
		cpu_addr = code.apply_hack_and_clear_get_next_cpu_addr(p_rom, p_bank, cpu_addr);

		return std::make_pair(main_entry_addr, cpu_addr);
	}

}

word fh::HackManager::install_PermaDoors(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, const fh::GeneralHack& p_hack, const fe::Game* p_game) const {
	const byte bank{ p_hack.byte_or("bank", 15) };
	const word install_addr{
		bank == 15 ?
			cpu_addr :
			p_hack.has_param("addr") ?
				p_hack.get_word("addr") :
				fe::ROM_Manager::find_trailing_free_cpu_addr(p_rom, bank, 0xff, 16)
	};

	const auto [main_entry_addr, next_addr] { install_PermaDoors_FreeBankInstall(p_rom, bank, install_addr, p_game) };

	if (bank == 15)
		cpu_addr = next_addr;

	klib::Asm6502 code;

	code.label("@set_flag_trampoline");
	code.lda_imm(0x01);
	code.sta_zp(VALUE_IO);
	code.jsr("@shared_logic");
	code.lda_imm(0xff);
	code.sta_abs(RAM::SelectedItem);
	code.rts();

	code.label("@check_flag_trampoline");
	code.lda_imm(0x00);
	code.sta_zp(VALUE_IO);
	code.jsr("@shared_logic");
	code.lda_zp(VALUE_IO);
	code.beq("@locked");
	// door was unlocked in the past
	code.lda_imm(0x00);
	code.sta_abs(RAM::DoorKeyRequirement);
	code.rts();

	code.label("@locked");
	// door still locked, let vanilla handle it
	code.lda_abs(RAM::DoorKeyRequirement);
	code.rts();

	code.label("@shared_logic");
	if (bank == 15) {
		code.jmp(main_entry_addr);
	}
	else {
		code.lda_abs(fh::RAM::CurrentROMBank);
		code.pha();

		code.ldx_imm(bank);
		code.jsr(fh::HackManager::cfg_word(p_config, fh::c::ID_ROM_MMC1_UPDATEROMBANK));
		code.jsr(main_entry_addr);

		code.pla();
		code.tax();
		code.jsr(fh::HackManager::cfg_word(p_config, fh::c::ID_ROM_MMC1_UPDATEROMBANK));
		code.rts();
	}

	const word set_flag_trampoline{
		static_cast<word>(cpu_addr + code.label_position("@set_flag_trampoline"))
	};
	const word check_flag_trampoline{
	static_cast<word>(cpu_addr + code.label_position("@check_flag_trampoline"))
	};

	const word result{ code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 15, cpu_addr) };

	// install hooks
	code.jsr(check_flag_trampoline);
	code.apply_hack_and_clear(p_rom, 15, ROM::Game_RunDoorRequirementHandler);

	code.jsr(set_flag_trampoline);
	code.nop(2);
	code.apply_hack_and_clear(p_rom, 15, ROM::Game_UnlockDoorWithKey_afterUse);

	// install flag-clearing routine
	install_hack_clear_flag_memory(p_config, p_rom);

	return result;
}
