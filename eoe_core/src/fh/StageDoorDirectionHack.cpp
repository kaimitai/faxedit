#include "HackManager.h"
#include "fh_constants.h"
#include "fe/Game.h"
#include "fe/ROM_Manager.h"
#include "fe/fe_constants.h"
#include "common/klib/Asm6502.h"
#include <format>
#include <map>
#include <set>
#include <vector>

namespace {

	using ScreenPairs = std::vector<std::vector<byte>>;

	constexpr byte PTR_LO{ fh::RAM::ZP_e2 };
	constexpr byte PTR_HI{ fh::RAM::ZP_e3 };
	constexpr byte VALUE_IO{ fh::RAM::ZP_e5 };

	std::map<std::size_t, std::set<byte>> gen_lookup(const ScreenPairs& p_data,
		std::size_t world_count) {
		std::map<std::size_t, std::set<byte>> result;

		for (const auto& elm : p_data) {
			const byte world{ elm.at(0) };
			const byte screen{ elm.at(1) };
			if (world < world_count && screen != 0xff)
				result[world].insert(screen);
		}

		return result;
	}

	void append_StageDoorDirection_LookupTable(klib::Asm6502& code,
		const ScreenPairs& p_left, const ScreenPairs& p_right, std::size_t world_count) {
		auto left{ gen_lookup(p_left, world_count) };
		auto right{ gen_lookup(p_right, world_count) };

		code.label("@lookup_table");
		for (std::size_t world{ 0 }; world < world_count; ++world) {
			code.dw(left[world].empty() ? "@empty" : std::format("world_{}_left", world));
			code.dw(right[world].empty() ? "@empty" : std::format("world_{}_right", world));
		}

		for (std::size_t world{ 0 }; world < world_count; ++world) {
			if (!left[world].empty()) {
				code.label(std::format("world_{}_left", world));
				for (byte screen : left[world])
					code.db(screen);
				code.db(0xff);
			}

			if (!right[world].empty()) {
				code.label(std::format("world_{}_right", world));
				for (byte screen : right[world])
					code.db(screen);
				code.db(0xff);
			}
		}

		code.label("@empty");
		code.db(0xff);
	}

	void append_StageDoorDirection_Main(klib::Asm6502& code) {
		code.label("@main");

		// preserve facing direction while clearing other player state bits
		code.lda_zp(fh::RAM::ZP_PlayerState);
		code.and_imm(0x40);
		code.sta_zp(fh::RAM::ZP_PlayerState);

		// each world has two 16-bit pointers; left and right
		code.lda_zp(fh::RAM::ZP_CurrentWorld);
		code.asl_a();
		code.asl_a();
		code.tax();

		// load the left-facing screen list
		code.lda_abs_x("@lookup_table");
		code.sta_zp(PTR_LO);
		code.lda_abs_x("@lookup_table", 1);
		code.sta_zp(PTR_HI);

		// force left if the transition screen is found
		code.lda_imm(0x00);
		code.jsr("@search");
		code.bcs("@done");

		// otherwise search the right-facing screen list
		code.lda_abs_x("@lookup_table", 2);
		code.sta_zp(PTR_LO);
		code.lda_abs_x("@lookup_table", 3);
		code.sta_zp(PTR_HI);

		code.lda_imm(0x40);
		code.jsr("@search");

		code.label("@done");
		code.rts();

		// A = desired direction ($00 left, $40 right)
		// C = 1 if found, 0 otherwise
		// updates PlayerState only on a match
		code.label("@search");
		code.sta_zp(VALUE_IO);
		code.ldy_imm(0x00);

		// scan screen indices until a match or $ff terminator
		code.label("@next");
		code.lda_ind_y(PTR_LO);
		code.cmp_imm(0xff);
		code.beq("@missing");

		code.cmp_zp(fh::RAM::ZP_TransitionScreen);
		code.beq("@found");

		code.iny();
		code.bne("@next");

		code.label("@missing");
		code.clc();
		code.rts();

		// apply the override and signal success
		code.label("@found");
		code.lda_zp(VALUE_IO);
		code.sta_zp(fh::RAM::ZP_PlayerState);
		code.sec();
		code.rts();
	}

	klib::Asm6502 make_StageDoorDirection(const ScreenPairs& p_left, const ScreenPairs& p_right,
		std::size_t world_count) {
		klib::Asm6502 code;

		if (p_left.empty() && p_right.empty()) {
			code.label("@main");
			code.lda_zp(fh::RAM::ZP_PlayerState);
			code.and_imm(0b01000000);
			code.sta_zp(fh::RAM::ZP_PlayerState);
			code.rts();
		}
		else {
			append_StageDoorDirection_LookupTable(
				code, p_left, p_right, world_count);
			append_StageDoorDirection_Main(code);
		}

		return code;
	}

}

word fh::HackManager::install_StageDoorDirection(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, const fh::GeneralHack& p_hack, const fe::Game* p_game) {
	const bool start_left{ p_hack.bool_or("start_left", false) };
	const bool sram{ p_hack.bool_or("sram", false) };

	if (sram && (p_hack.has_param("bank") || p_hack.has_param("addr")))
		throw std::runtime_error(
			"StageDoorDirection cannot specify bank/addr with sram=true");

	// start screen direction hack
	{
		klib::Asm6502 code;

		const word install_addr{ sram ? sram_hack_addr() : cpu_addr };

		code.sta_abs(RAM::PlayerXP_U);
		if (!start_left)
			code.lda_imm(0x40);
		code.sta_zp(RAM::ZP_PlayerState);
		code.rts();

		if (sram)
			install_sram_hack(p_rom, code);
		else
			cpu_addr = code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 15, cpu_addr);

		code.jsr(install_addr);
		code.apply_hack_and_clear(p_rom, 15, ROM::Game_Start_ClearXP_Hi);
	}

	// stage door direction hack
	{
		const auto left{ p_hack.has_param("left") ? p_hack.split_twice_bytes("left", 2) : ScreenPairs{} };
		const auto right{ p_hack.has_param("right") ? p_hack.split_twice_bytes("right", 2) : ScreenPairs{} };

		const byte bank{ p_hack.byte_or("bank", 15) };

		const word install_addr{
			bank == 15 ?
				cpu_addr :
				p_hack.has_param("addr") ?
					p_hack.get_word("addr") :
					fe::ROM_Manager::find_trailing_free_cpu_addr(
						p_rom, bank, 0xff, 16)
		};

		auto code{ make_StageDoorDirection(left, right, p_game ? p_game->m_chunks.size() : 8) };

		word main_entry_addr{};

		if (sram) {
			const std::size_t main_offset{ code.label_position("@main") };
			const word runtime_addr{ install_sram_hack(p_rom, code) };
			main_entry_addr = static_cast<word>(runtime_addr + main_offset);
		}
		else {
			main_entry_addr = code.label_addr("@main", install_addr);
			const word next_addr{ code.apply_hack_and_clear_get_next_cpu_addr(p_rom, bank, install_addr) };
			if (bank == 15)
				cpu_addr = next_addr;
		}

		// bank 15 and SRAM can call directly, while other banks use the vanilla farcall mechanism
		word hook_addr{ main_entry_addr };

		if (!sram && bank != 15) {
			klib::Asm6502 trampoline;

			trampoline.jsr(cfg_word(p_config, c::ID_ROM_VANILLA_FAR_CALL));
			trampoline.db(bank);
			trampoline.dw(main_entry_addr - 1);
			trampoline.rts();

			hook_addr = cpu_addr;
			cpu_addr = trampoline.apply_hack_and_clear_get_next_cpu_addr(p_rom, 15, cpu_addr);
		}

		// Replace LDA #$40 / STA PlayerState at $E0BC
		code.jsr(hook_addr);
		code.nop();
		code.apply_hack_and_clear(p_rom, 15, ROM::Player_SetInitialState_SetFlags);
	}

	return cpu_addr;
}
