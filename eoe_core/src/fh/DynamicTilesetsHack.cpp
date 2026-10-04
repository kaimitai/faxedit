#include "HackManager.h"
#include "fh_constants.h"
#include "fe/ROM_Manager.h"
#include "common/klib/Asm6502.h"
#include <format>

namespace {

	constexpr byte CurrentWorldAndRetval{ fh::RAM::ZP_e2 };
	constexpr byte CurrentScreen{ fh::RAM::ZP_e3 };
	constexpr byte PtrLo{ fh::RAM::ZP_e4 };
	constexpr byte PtrHi{ fh::RAM::ZP_e5 };

	// Inputs:
	// Current world  - $e2
	// Current screen - $e3
	// uses ($e4, $e5) for ptr lookup
	// stores tile index override in $95 if a match is found
	// on return; stores 0 (no match) or 1 (match) in $e2
	void append_DynamicTilesets_Loader(klib::Asm6502& code) {
		code.label("@loader");

		// deref world ptr
		code.lda_zp(CurrentWorldAndRetval);
		code.asl_a();
		code.tay();

		code.lda_abs_y("@world_ptrs");
		code.sta_zp(PtrLo);
		code.iny();
		code.lda_abs_y("@world_ptrs");
		code.sta_zp(PtrHi);
		code.ldy_imm(0x00);

		// walk array (2 bytes per entry) sorted by screen no
		code.label("@loop");
		code.lda_ind_y(PtrLo);
		code.cmp_imm(0xff);
		code.beq("@not_found");

		code.cmp_zp(CurrentScreen);
		code.beq("@found");
		code.bcs("@not_found"); // if screen no in table > current_screen, stop searching

		code.iny();
		code.iny();
		code.bne("@loop");

		code.label("@not_found");
		code.lda_imm(0x00);
		code.sta_zp(CurrentWorldAndRetval);
		code.rts();

		code.label("@found");
		code.iny();
		code.lda_ind_y(PtrLo);
		code.sta_zp(fh::RAM::ZP_TilesIndex);

		code.lda_imm(0x01);
		code.sta_zp(CurrentWorldAndRetval);
		code.rts();
	}

	void append_DynamicTilesets_Data(klib::Asm6502& code,
		const std::map<byte, std::map<byte, byte>>& assigns, std::size_t world_count) {
		code.label("@world_ptrs");

		for (std::size_t world{ 0 }; world < world_count; ++world) {
			const byte w{ static_cast<byte>(world) };
			if (assigns.contains(w))
				code.dw(std::format("@world{}", world));
			else
				code.dw("@empty");
		}

		// screen to tileset data for each world (sorted by screen index)
		for (auto it{ assigns.begin() }; it != assigns.end(); ++it) {
			const auto& [world, assignment] { *it };

			code.label(std::format("@world{}", world));

			for (const auto& [screen, tileset] : assignment) {
				code.db(screen);
				code.db(tileset);
			}

			// @empty immediately follows the final world's data so let's save a byte :)
			if (std::next(it) != assigns.end())
				code.db(0xff);
		}
		// shared terminator
		code.label("@empty");
		code.db(0xff);
	}

	klib::Asm6502 build_DynamicTilesets_Loader(const std::map<byte, std::map<byte, byte>>& assigns,
		std::size_t world_count) {
		klib::Asm6502 code;
		append_DynamicTilesets_Loader(code);
		append_DynamicTilesets_Data(code, assigns, world_count);
		return code;
	}

	// -------------------------------------------------------------------------
	// Inputs: $e3 = screen (uses current world from $24)
	// Output: $e2 = 0/1
	// -------------------------------------------------------------------------
	void append_DynamicTilesets_Lookup(klib::Asm6502& code, bool local_loader,
		byte loader_bank, word loader_entry_addr, word mmc1_update_rom_bank) {
		code.label("@lookup");

		code.lda_zp(fh::RAM::ZP_CurrentWorld);
		code.sta_zp(CurrentWorldAndRetval);
		if (local_loader) {
			code.jsr(loader_entry_addr);
		}
		else {
			// save currently mapped switchable bank
			code.lda_abs(fh::RAM::CurrentROMBank);
			code.pha();
			// switch to DynamicTilesets loader bank
			code.ldx_imm(loader_bank);
			code.jsr(mmc1_update_rom_bank);
			// perform lookup
			code.jsr(loader_entry_addr);
			// restore previous bank
			code.pla();
			code.tax();
			code.jsr(mmc1_update_rom_bank);
		}
		code.rts();
	}

	// -------------------------------------------------------------
	// stage doors / other-world transitions / game start trampoline
	// -------------------------------------------------------------
	void append_DynamicTilesets_AreaLoadTrampoline(klib::Asm6502& code, word lookup_addr) {
		code.label("@area_load_trampoline");

		code.lda_zp(fh::RAM::ZP_CurrentScreen);
		code.sta_zp(CurrentScreen);
		code.jsr(lookup_addr); // shared lookup helper
		// vanilla path always loads tiles
		code.jmp(fh::ROM::Area_LoadTiles);
	}

	// ----------------------------------------------
	// same-world trampoline (doors and transitions)
	// ----------------------------------------------
	void append_DynamicTilesets_SameWorldTrampoline(klib::Asm6502& code, word lookup_addr) {
		code.label("@sameworld_trampoline");

		code.lda_zp(fh::RAM::ZP_TransitionScreen);
		code.sta_zp(CurrentScreen);
		code.jsr(lookup_addr);
		// only reload tiles if an override was found
		code.lda_zp(CurrentWorldAndRetval);
		code.beq("@load_screen");
		code.jsr(fh::ROM::Area_LoadTiles);
		code.label("@load_screen");
		code.jmp(fh::ROM::Screen_Load);
	}

	// -------------------------------------------
	// exit-building trampoline
	// -------------------------------------------
	void append_DynamicTilesets_ExitBuildingTrampoline(klib::Asm6502& code, word lookup_addr) {
		code.label("@exit_building_trampoline");

		code.lda_abs(fh::RAM::SavedScreen);
		code.sta_zp(CurrentScreen);
		code.jsr(lookup_addr);
		code.jmp(fh::ROM::Area_LoadTiles);
	}

	// --------------------------------------
	// enter-building trampoline
	// --------------------------------------
	void append_DynamicTilesets_EnterBuildingTrampoline(klib::Asm6502& code, word lookup_addr) {
		code.label("@enter_building_trampoline");

		code.lda_zp(fh::RAM::ZP_TransitionScreen);
		code.sta_zp(CurrentScreen);
		code.jsr(lookup_addr);
		code.jmp(fh::ROM::Area_LoadTiles);
	}

}

// makes a trampoline for calls to the tileset loader, with data lookup
word fh::HackManager::install_DynamicTilesets(const fe::Config& p_config,
	std::vector<byte>& p_rom, byte p_bank, word cpu_addr,
	const fh::GeneralHack& p_hack, const fe::Game* p_game) const {

	const std::size_t world_count{ p_game ? p_game->m_chunks.size() : 8 };
	const byte loader_bank{ p_hack.byte_or("bank", p_bank) };
	const bool local_loader{ loader_bank == p_bank };
	const word loader_addr{
	local_loader ?
		cpu_addr : p_hack.has_param("addr") ?
			p_hack.get_word("addr") :
			fe::ROM_Manager::find_trailing_free_cpu_addr(p_rom, loader_bank, 0xff, 16)
	};

	const bool opt_enter_building{ p_hack.bool_or("enter_building", false) };
	const bool opt_exit_building{ p_hack.bool_or("exit_building", true) };
	const bool opt_sameworld{ p_hack.bool_or("sameworld", true) };
	const bool opt_start_screen{ p_hack.bool_or("start_screen", true) };
	const bool opt_otherworld{ p_hack.bool_or("otherworld", true) };
	const bool opt_stage_doors{ p_hack.bool_or("stage_doors", true) };

	const auto entries{ p_hack.split_twice_bytes("data", 3) };

	// world -> (screen -> tileset)
	std::map<byte, std::map<byte, byte>> assigns;
	for (const auto& entry : entries)
		assigns[entry[0]][entry[1]] = entry[2];

	const word MMC1_UpdateROMBank{ cfg_word(p_config, c::ID_ROM_MMC1_UPDATEROMBANK) };
	const word CurrentROMBank{ RAM::CurrentROMBank };

	auto code{ build_DynamicTilesets_Loader(assigns, world_count) };
	const word loader_entry_addr{ code.label_addr("@loader", loader_addr) };

	if (local_loader)
		cpu_addr = code.apply_hack_and_clear_get_next_cpu_addr(p_rom, loader_bank, loader_entry_addr);
	else
		code.apply_hack_and_clear(p_rom, loader_bank, loader_entry_addr);

	append_DynamicTilesets_Lookup(code, local_loader, loader_bank, loader_entry_addr, MMC1_UpdateROMBank);
	const word lookup_cpu_addr{ code.label_addr("@lookup", cpu_addr) };

	cpu_addr = code.apply_hack_and_clear_get_next_cpu_addr(p_rom, p_bank, cpu_addr);

	if (opt_otherworld || opt_stage_doors || opt_start_screen) {
		append_DynamicTilesets_AreaLoadTrampoline(code, lookup_cpu_addr);

		const word trampoline_addr{ code.label_addr("@area_load_trampoline", cpu_addr) };
		cpu_addr = code.apply_hack_and_clear_get_next_cpu_addr(p_rom, p_bank, cpu_addr);

		if (opt_otherworld) {
			code.jsr(trampoline_addr);
			code.apply_hack_and_clear(p_rom, 15, ROM::Game_EnterAreaHandler_JSR_Area_LoadTiles);
		}
		if (opt_stage_doors) {
			code.jsr(trampoline_addr);
			code.apply_hack_and_clear(p_rom, 15, ROM::Game_LoadCurrentArea_JSR_Area_LoadTiles);
		}
		if (opt_start_screen) {
			code.jsr(trampoline_addr);
			code.apply_hack_and_clear(p_rom, 15, ROM::Game_LoadFirstLevel_JSR_Area_LoadTiles);
		}
	}

	if (opt_sameworld) {
		append_DynamicTilesets_SameWorldTrampoline(code, lookup_cpu_addr);

		const word trampoline_addr{ code.label_addr("@sameworld_trampoline", cpu_addr) };
		cpu_addr = code.apply_hack_and_clear_get_next_cpu_addr(p_rom, p_bank, trampoline_addr);

		code.jsr(trampoline_addr);
		code.apply_hack_and_clear(p_rom, 15, ROM::Game_SetupEnterScreen_JSR_Screen_Load);
	}

	if (opt_exit_building) {
		append_DynamicTilesets_ExitBuildingTrampoline(code, lookup_cpu_addr);

		const word trampoline_addr{ code.label_addr("@exit_building_trampoline", cpu_addr) };
		cpu_addr = code.apply_hack_and_clear_get_next_cpu_addr(p_rom, p_bank, cpu_addr);

		code.jsr(trampoline_addr);
		code.apply_hack_and_clear(p_rom, 15, ROM::Game_ExitBuilding_JSR_Area_LoadTiles);
	}

	if (opt_enter_building) {
		append_DynamicTilesets_EnterBuildingTrampoline(code, lookup_cpu_addr);

		const word trampoline_addr{ code.label_addr("@enter_building_trampoline", cpu_addr) };
		cpu_addr = code.apply_hack_and_clear_get_next_cpu_addr(p_rom, p_bank, cpu_addr);

		code.jsr(trampoline_addr);
		code.apply_hack_and_clear(p_rom, 15, ROM::Game_EnterBuilding_JSR_Area_LoadTiles);
	}

	return cpu_addr;
}
