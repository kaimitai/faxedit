#include "HackManager.h"
#include "fe/Config.h"
#include "fe/Game.h"
#include "common/klib/Asm6502.h"
#include "common/klib/Kstring.h"
#include "AtlasDevFrameScheduler.h"
#include "fh_constants.h"
#include "fe/fe_constants.h"
#include <algorithm>
#include <cctype>
#include <format>
#include <initializer_list>
#include <map>
#include <stdexcept>
#include <utility>

// kill switch; Pressing Select while the game is paused kills the player when the game is unpaused
word fh::HackManager::install_KillSwitch(const fe::Config& p_config, std::vector<byte>& p_rom, byte p_bank, word cpu_addr) const {
	klib::Asm6502 code;

	// call the routine vanilla would have called if we didn't install the hook
	code.jsr(ROM::Sprites_FlipRanges);
	code.lda_zp(RAM::ZP_Joy1_ChangedButtonMask);
	// test bit 5: select button
	code.and_imm(0b00100000);
	code.beq("@select_not_pressed");
	code.lda_imm(0x01);
	code.sta_abs(RAM::PlayerIsDead);
	code.label("@select_not_pressed");
	code.rts();
	const auto next_cpu_addr{ code.apply_hack_and_clear_get_next_cpu_addr(p_rom, p_bank, cpu_addr) };

	// install the hook
	code.jsr(cpu_addr);
	code.apply_hack_and_clear(p_rom, p_bank, ROM::GameLoop_CheckPauseGame_JSR_Sprites_FlipRanges);

	return next_cpu_addr;
}

word fh::HackManager::install_SameWorldTransPal2Mus(const fe::Config& p_config, std::vector<byte>& p_rom,
	byte p_bank, word cpu_addr, bool p_stage_door_hack_installed) const {
	klib::Asm6502 code;

	// if the stage door hack is not installed, this becomes a static patch
	// where we jump to the door-pal2mus logic directly
	if (!p_stage_door_hack_installed) {
		code.jmp(ROM::Player_CheckHandleEnterDoor_LDX_pal2mus_slots);
		code.apply_hack_and_clear(p_rom, p_bank, ROM::SwTransJmpSetupEnterScreen);
		return cpu_addr;
	}

	// constants from the rom
	const auto pal_ptr{ p_config.pointer(fe::c::ID_PAL2MUS_PALETTE_PTR) };
	const auto mus_ptr{ p_config.pointer(fe::c::ID_PAL2MUS_MUSIC_PTR) };

	byte pal2mus_slot_count{ p_rom.at(p_config.constant(fe::c::ID_PAL2MUS_ENTRY_COUNT_OFFSET)) };
	word pal2mus_pal_table_addr{ static_cast<word>(klib::Asm6502::read_word(p_rom, pal_ptr.first)) };
	word pal2mus_mus_table_addr{ static_cast<word>(klib::Asm6502::read_word(p_rom, mus_ptr.first)) };

	// add new routine which copies the vanilla logic for sw-door pal2mus
	code.ldx_imm(pal2mus_slot_count);
	code.label("@paletteCheckLoop");
	code.lda_zp(RAM::ZP_TransitionPalette);
	code.cmp_abs_x(pal2mus_pal_table_addr);
	code.beq("@setupArea");
	code.dex();
	code.bpl("@paletteCheckLoop");
	code.bmi("@enterScreen");

	code.label("@setupArea");
	code.lda_abs_x(pal2mus_mus_table_addr);
	code.cmp_abs(RAM::World_DefaultMusic);
	code.beq("@enterScreen");
	code.sta_zp(RAM::ZP_MusicCurrent);
	code.sta_abs(RAM::World_DefaultMusic);

	code.label("@enterScreen");
	code.jmp(ROM::Game_SetupEnterScreen);

	// insert the new routine in rom
	const auto next_cpu_addr{ code.apply_hack_and_clear_get_next_cpu_addr(p_rom, p_bank, cpu_addr) };

	// from sw-transitions, jump into our new routine rather than Game_SetupEnterScreen
	code.jmp(cpu_addr);
	code.apply_hack_and_clear(p_rom, p_bank, ROM::SwTransJmpSetupEnterScreen);

	return next_cpu_addr;
}

// dynamically added to bank 14, but with hooks and constants in bank 15
word fh::HackManager::install_FastStart(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const word gold{ p_hack.word_or("gold", 1500) };
	const bool ring_of_elf{ p_hack.bool_or("ring_of_elf", true) };

	klib::Asm6502::apply_byte(p_rom, 80, 15, ROM::Start_Health);
	klib::Asm6502::apply_byte(p_rom, 80, 15, ROM::Start_Mana);

	klib::Asm6502 code;

	// install hook from bank 15
	code.jsr(cpu_addr);
	code.apply_hack_and_clear(p_rom, 15, ROM::Game_Start_JSR_Game_LoadFirstLevel);

	// new routine
	code.lda_imm(gold % 256);
	code.sta_abs(RAM::PlayerGold_L);
	code.lda_imm(gold / 256);
	code.sta_abs(RAM::PlayerGold_M);
	if (ring_of_elf) {
		// start with special items bit 7 set (ring of elf)
		code.lda_imm(0b10000000);
		code.sta_abs(RAM::SpecialItemBitfield);
	}
	code.jmp(ROM::Game_LoadFirstLevel);

	return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 14, cpu_addr);
}

// two item drops depend on quest flags; wyvern mattock and stone dropper wing boots
// this hack will check whether the player has the item in inventory (or equipped)
// param: type=both,wing_boots,mattock
word fh::HackManager::install_QuestFlagItemDrops(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, const fh::GeneralHack& p_hack) const {
	const std::string type{ p_hack.string_or("type", "both") };
	const bool hack_mattock{ type == "mattock" || type == "both" };
	const bool hack_wing_boots{ type == "wing_boots" || type == "both" };

	if (!hack_mattock && !hack_wing_boots)
		throw std::runtime_error(std::format("Invalid QuestFlagItemDrops hack type: {}", type));

	klib::Asm6502 code;
	/*
	A = item ID
	Returns:
		Z = 0 if the player has the item
		Z = 1 if the player does not have the item
	Preserves: X
	Clobbers: A, Y
	Could be made a generic helper if other hacks need this
	*/

	code.label("@check_has_item");
	code.cmp_abs(RAM::SelectedItem);
	code.beq("@found");
	code.ldy_imm(0x00);

	code.label("@loop");
	code.cpy_abs(RAM::NumberOfItems);
	code.beq("@not_found");
	code.cmp_abs_y(RAM::ItemInventory);
	code.beq("@found");
	code.iny();
	code.bne("@loop");

	code.label("@not_found");
	code.lda_imm(0x00);
	code.rts();

	code.label("@found");
	code.lda_imm(0x01);
	code.rts();

	// Item-specific entry points.
	word check_mattock{ 0 };
	if (hack_mattock) {
		check_mattock = static_cast<word>(cpu_addr + code.size());
		code.lda_imm(0x09);
		code.bne("@check_has_item");
	}
	word check_wing_boots{ 0 };
	if (hack_wing_boots) {
		check_wing_boots = static_cast<word>(cpu_addr + code.size());
		code.lda_imm(0x0f);
		code.bne("@check_has_item");
	}

	// install the routine in bank 14
	const word result{ code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 14, cpu_addr) };

	// replace vanilla quest-flag checks with inventory checks.
	if (hack_mattock) {
		code.jsr(check_mattock);
		code.nop(2);
		code.apply_hack_and_clear(p_rom, 14, ROM::SpriteBehavior_MattockDroppedFromRipasheiku_LDA_Quests);

		// do not set quest flag when picking up the item
		code.nop(8);
		code.apply_hack_and_clear(p_rom, 15, ROM::Player_PickUpMattockWithQuest);

		// do not reset the quest flag when player dies
		code.nop(8);
		code.apply_hack_and_clear(p_rom, 15, ROM::Player_Spawn_LDA_Quests);
	}
	if (hack_wing_boots) {
		code.jsr(check_wing_boots);
		code.nop(2);
		code.apply_hack_and_clear(p_rom, 14, ROM::SpriteBehavior_WingBootsDroppedByZorugeriru_LDA_Quests);

		// do not set quest flag when picking up the item
		code.nop(8);
		code.apply_hack_and_clear(p_rom, 15, ROM::Player_PickUpWingBootsWithQuest);
	}

	return result;
}

// make the boss locked items show in the screen regardless of which boss it is
// optionally keep the item hidden until all enemy sprites have been removed
word fh::HackManager::install_BossLockedItems(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const bool enemies{ p_hack.bool_or("enemies", true) };

	klib::Asm6502 code;
	code.jsr(cpu_addr);

	// all boss locked items will no longer check for a given sprite ID in A, but for any
	code.apply_hack_noclear(p_rom, 14, ROM::SpriteBehavior_BattleSuit_CheckForBosses);
	code.apply_hack_noclear(p_rom, 14, ROM::SpriteBehavior_BattleHelmet_CheckForBosses);
	code.apply_hack_noclear(p_rom, 14, ROM::SpriteBehavior_DragonSlayer_CheckForBosses);
	code.apply_hack_noclear(p_rom, 14, ROM::SpriteBehavior_QMattock_CheckForBosses);
	code.apply_hack_noclear(p_rom, 14, ROM::SpriteBehavior_QWingBoots_CheckForBosses);
	code.apply_hack_noclear(p_rom, 14, ROM::SpriteBehavior_BlackOnyx_CheckForBosses);
	code.apply_hack_and_clear(p_rom, 14, ROM::SpriteBehavior_Pendant_CheckForBosses);

	// return C=0 if a boss/enemy is present, C=1 otherwise
	code.txa();
	code.pha();
	code.ldy_imm(0x07);

	code.label("@next_sprite");
	code.lda_abs_y(RAM::EntitySlotActive);
	code.cmp_imm(0xff);
	code.beq("@next_slot");
	code.tax();
	code.lda_abs_x(ROM::SpriteTypeTable);

	// boss always blocks
	code.cmp_imm(0x07); // sprite type 7 - boss
	code.beq("@sprite_blocks_item");

	if (enemies) {
		// enemy blocks when requested
		code.cmp_imm(0x00); // sprite type 0 - enemy
		code.beq("@sprite_blocks_item");
	}

	code.label("@next_slot");
	code.dey();
	code.bpl("@next_sprite");

	code.pla();
	code.tax();
	code.sec();
	code.rts();

	code.label("@sprite_blocks_item");
	code.pla();
	code.tax();
	code.clc();
	code.rts();

	return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 14, cpu_addr);
}

// supports using items inside buildings, disregarding player state flags
// and selling items to shops which do not sell those items for a given price
word fh::HackManager::install_FlexibleItems(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const bool buildings{ p_hack.bool_or("buildings", true) };
	const bool wep_indoors{ p_hack.bool_or("wep_indoors", false) };
	const bool state{ p_hack.bool_or("state", true) };
	const bool selling{ p_hack.bool_or("selling", true) };
	const word price{ p_hack.word_or("price", 100) };

	klib::Asm6502 code;

	// compare against nonexistent world $ff instead of building world $04
	if (buildings)
		klib::Asm6502::apply_byte(p_rom, 0xff, 12, cfg_word(p_config, c::ID_ROM_PLAYERMENU_HANDLEINVENTORYMENUINPUT_CMP_WORLDNO));

	if (state) {
		code.nop(2);
		code.apply_hack_and_clear(p_rom, 15, ROM::GameLoop_CheckUseCurrentItem_BNE_Return);
	}

	if (wep_indoors) {
		code.nop(3);
		code.apply_hack_and_clear(p_rom, 15, ROM::Game_EnterBuilding_STA_ActiveWeapon);
		code.apply_byte(p_rom, 0xff, 15, ROM::Player_SetWeapon_CMP_BuildingsWorldNo);
	}

	if (!selling)
		return cpu_addr;
	else {
		// install hook
		code.jmp(cpu_addr);
		code.apply_hack_and_clear(p_rom, 12, cfg_word(p_config, c::ID_ROM_SHOWSELLMENU_JSR_FINDSELLMENUENTRY));

		// preserve the original item ID in X when no sell-table entry is found
		code.nop();
		code.apply_hack_and_clear(p_rom, 12, cfg_word(p_config, c::ID_ROM_FINDSELLMENUENTRY_TAX));

		// the sell-any-item routine itself
		code.jsr(cfg_word(p_config, c::ID_ROM_FINDSELLMENUENTRY));
		code.cmp_imm(0xff);
		code.beq("@shop_entry_missing");
		// item has a normal sell-table entry; continue with vanilla logic
		code.jmp(cfg_word(p_config, c::ID_ROM_SHOWSELLMENU_LDX_STRINGCOUNT));

		code.label("@shop_entry_missing");
		code.txa();
		code.ldx_abs(cfg_word(p_config, c::ID_RAM_UISTRINGCOUNT));
		code.sta_abs_x(cfg_word(p_config, c::ID_RAM_UIDATAARRAY));
		code.lda_imm(price % 256);
		code.sta_abs_x(cfg_word(p_config, c::ID_RAM_SHOPITEMCOSTSLO));
		code.lda_imm(price / 256);
		code.jmp(cfg_word(p_config, c::ID_ROM_SHOWSELLMENU_STA_COSTHI));

		return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 12, cpu_addr);
	}
}

// routine which enables fogs for arbitrary (world, palette)-combinations
word fh::HackManager::install_FogRules(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	std::map<byte, std::set<std::optional<byte>>> rules;

	const auto rules_raw{ p_hack.split_byte_optional_byte("rules") };

	for (const auto& rule : rules_raw)
		rules[rule.first].insert(rule.second);

	if (rules.empty())
		throw std::runtime_error("FogRules requires at least one rule");

	klib::Asm6502 code;

	// install hook
	code.jsr(cpu_addr);
	code.nop(6);
	code.apply_hack_and_clear(p_rom, 15, ROM::Fog_OnTick_CMP_02);

	// replacement fog predicate - world is in A on entry
	// returns A=0 (Z set) when fog is active, otherwise A=1 (Z clear),
	// allowing the original BNE after return to remain and be used
	std::map<byte, std::string> labels;

	// world dispatch
	for (const auto& [world, palettes] : rules) {
		if (palettes.contains(std::nullopt)) {
			code.cmp_imm(world);
			code.beq("@fog_active");
		}
		else {
			const auto label{ std::format("@world_{:02X}", world) };
			labels.emplace(world, label);
			code.cmp_imm(world);
			code.beq(label);
		}
	}

	// no world match
	code.bne("@fog_inactive");

	// palette checks
	for (const auto& [world, label] : labels) {
		code.label(label);
		code.lda_abs(RAM::ScreenPaletteIndex);

		for (const auto& palette : rules.at(world)) {
			code.cmp_imm(*palette);
			code.beq("@fog_active");
		}

		// no palette match for this world
		code.bne("@fog_inactive");
	}

	code.label("@fog_active");
	code.lda_imm(0x00);
	code.rts();
	code.label("@fog_inactive");
	code.lda_imm(0x01);
	code.rts();

	return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 15, cpu_addr);
}

// makes a trampoline for calls to the tileset loader, with data lookup
word fh::HackManager::install_DynamicTilesets(const fe::Config& p_config,
	std::vector<byte>& p_rom, byte p_bank, word cpu_addr,
	const fh::GeneralHack& p_hack, const fe::Game* p_game) const {

	const std::size_t world_count{ p_game ? p_game->m_chunks.size() : 8 };
	const byte loader_bank{ p_hack.byte_or("bank", p_bank) };
	const bool local_loader{ loader_bank == p_bank };
	const word loader_addr{ local_loader ? cpu_addr : p_hack.get_word("addr") };

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

	klib::Asm6502 code;

	// *************** DATA LOADER - BEGIN *************** //
	// Inputs:
	// Current world  - $e2
	// Current screen - $e3
	// uses ($e4, $e5) for ptr lookup
	// stores tile index override in $95 if a match is found
	// on return; stores 0 (no match) or 1 (match) in $e2
	const byte current_world_and_retval{ RAM::ZP_e2 };
	const byte current_screen{ RAM::ZP_e3 };
	const byte ptr_lo{ RAM::ZP_e4 };
	const byte ptr_hi{ RAM::ZP_e5 };

	// deref world ptr
	code.lda_zp(current_world_and_retval);
	code.asl_a();
	code.tay();

	code.lda_abs_y("@world_ptrs");
	code.sta_zp(ptr_lo);
	code.iny();
	code.lda_abs_y("@world_ptrs");
	code.sta_zp(ptr_hi);
	code.ldy_imm(0x00);

	// walk array (2 bytes per entry) sorted by screen no
	code.label("@loop");
	code.lda_ind_y(ptr_lo);
	code.cmp_imm(0xff);
	code.beq("@not_found");

	code.cmp_zp(current_screen);
	code.beq("@found");
	code.bcs("@not_found"); // if screen no in table > current_screen, stop searching

	code.iny();
	code.iny();
	code.bne("@loop");

	code.label("@not_found");
	code.lda_imm(0x00);
	code.sta_zp(current_world_and_retval);
	code.rts();

	code.label("@found");
	code.iny();
	code.lda_ind_y(ptr_lo);
	code.sta_zp(RAM::ZP_TilesIndex);

	code.lda_imm(0x01);
	code.sta_zp(current_world_and_retval);
	code.rts();

	// *************** DATA LOADER - END, DATA TABLE - BEGIN *************** //
	// emit ptr table
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
	// *************** DATA TABLE - END *************** //
	if (local_loader)
		cpu_addr = code.apply_hack_and_clear_get_next_cpu_addr(p_rom, loader_bank, loader_addr);
	else
		code.apply_hack_and_clear(p_rom, loader_bank, loader_addr);

	// -------------------------------------------------------------------------
	// bank 15 - shared lookup helper
	// Inputs: $e3 = screen (uses current world from $24)
	// Output: $e2 = 0/1
	// -------------------------------------------------------------------------
	const word lookup_cpu_addr{ cpu_addr };

	code.lda_zp(RAM::ZP_CurrentWorld);
	code.sta_zp(current_world_and_retval);
	if (local_loader) {
		code.jsr(loader_addr);
	}
	else {
		// save currently mapped switchable bank
		code.lda_abs(CurrentROMBank);
		code.pha();
		// switch to DynamicTilesets loader bank
		code.ldx_imm(loader_bank);
		code.jsr(MMC1_UpdateROMBank);
		// perform lookup
		code.jsr(loader_addr);
		// restore previous bank
		code.pla();
		code.tax();
		code.jsr(MMC1_UpdateROMBank);
	}
	code.rts();
	cpu_addr = code.apply_hack_and_clear_get_next_cpu_addr(p_rom, p_bank, cpu_addr);

	if (opt_otherworld || opt_stage_doors || opt_start_screen) {
		// -------------------------------------------------------------------------
		// bank 15 - stage doors / other-world transitions / game start trampoline
		// -------------------------------------------------------------------------
		const word trampoline_addr{ cpu_addr };

		code.lda_zp(RAM::ZP_CurrentScreen);
		code.sta_zp(current_screen);
		code.jsr(lookup_cpu_addr); // shared lookup helper
		// vanilla path always loads tiles
		code.jmp(ROM::Area_LoadTiles);
		cpu_addr = code.apply_hack_and_clear_get_next_cpu_addr(p_rom, p_bank, trampoline_addr);

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
		// -------------------------------------------------------------------------
		// bank 15 - same-world trampoline (doors and transitions)
		// -------------------------------------------------------------------------
		const word trampoline_addr{ cpu_addr };

		code.lda_zp(RAM::ZP_TransitionScreen);
		code.sta_zp(current_screen);
		code.jsr(lookup_cpu_addr);
		// only reload tiles if an override was found
		code.lda_zp(current_world_and_retval);
		code.beq("@load_screen");
		code.jsr(ROM::Area_LoadTiles);
		code.label("@load_screen");
		code.jmp(ROM::Screen_Load);
		cpu_addr = code.apply_hack_and_clear_get_next_cpu_addr(p_rom, p_bank, trampoline_addr);

		code.jsr(trampoline_addr);
		code.apply_hack_and_clear(p_rom, 15, ROM::Game_SetupEnterScreen_JSR_Screen_Load);
	}

	if (opt_exit_building) {
		// -------------------------------------------------------------------------
		// bank 15 - exit-building trampoline
		// -------------------------------------------------------------------------
		const word trampoline_addr{ cpu_addr };

		code.lda_abs(RAM::SavedScreen);
		code.sta_zp(current_screen);
		code.jsr(lookup_cpu_addr);
		code.jmp(ROM::Area_LoadTiles);
		cpu_addr = code.apply_hack_and_clear_get_next_cpu_addr(p_rom, p_bank, trampoline_addr);

		code.jsr(trampoline_addr);
		code.apply_hack_and_clear(p_rom, 15, ROM::Game_ExitBuilding_JSR_Area_LoadTiles);
	}

	if (opt_enter_building) {
		// -------------------------------------------------------------------------
		// bank 15 - enter-building trampoline
		// -------------------------------------------------------------------------
		const word trampoline_addr{ cpu_addr };

		code.lda_zp(RAM::ZP_TransitionScreen);
		code.sta_zp(current_screen);
		code.jsr(lookup_cpu_addr);
		code.jmp(ROM::Area_LoadTiles);
		cpu_addr = code.apply_hack_and_clear_get_next_cpu_addr(p_rom, p_bank, trampoline_addr);

		code.jsr(trampoline_addr);
		code.apply_hack_and_clear(p_rom, 15, ROM::Game_EnterBuilding_JSR_Area_LoadTiles);
	}

	return cpu_addr;
}

// adds a configurable item to the inventory when touching poison instead of taking damage
// can be used in conjunction with "use item" override hacks
void fh::HackManager::install_PoisonPickup(const fe::Config& p_config, std::vector<byte>& p_rom,
	const fh::GeneralHack& p_hack) const {
	const byte item{ p_hack.byte_or("item", 0x10) };
	const byte sound{ p_hack.byte_or("sound", 0x08) };
	const bool script{ p_hack.bool_or("script", true) };

	klib::Asm6502 code;

	if (!script) {
		code.nop(8);
		code.apply_hack_and_clear(p_rom, 15, ROM::Player_PickUpPoison);
	}

	// keep the vanilla LDA opcode and replace its immediate operand
	code.db(sound);
	code.jsr(ROM::Sound_PlayEffect);
	code.lda_imm(item);
	code.jsr(ROM::Player_PickUpItem);
	code.rts();

	code.apply_hack_and_clear(p_rom, 15, ROM::Player_PickUpPoison_DamageSoundIndex);
}

word fh::HackManager::install_TextSpeed(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, const fh::GeneralHack& p_hack) const {
	const byte mask{ p_hack.byte_or("mask", 0x00) };

	klib::Asm6502 code;
	code.apply_byte(p_rom, mask, 15, cfg_word(p_config, c::ID_TEXTBOX_SHOW_NEXT_CHAR_IF_READY_TIMER_CONST));

	// hook
	code.jsr(cpu_addr);
	code.nop(2);
	code.apply_hack_and_clear(p_rom, 15, cfg_word(p_config, c::ID_TEXTBOX_SHOW_NEXT_CHAR_LDA_01));

	// new routine
	code.lda_abs(RAM::TextBox_Timer);
	code.and_imm(0x02);
	code.lsr_a();
	code.eor_imm(0x01);
	code.sta_abs(RAM::TextBox_PlayTextSound);
	code.rts();

	return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 15, cpu_addr);
}

void fh::HackManager::install_BugFixes(std::vector<byte>& p_rom) const {
	klib::Asm6502::apply_word(p_rom, RAM::Inventory_ArmorsCount, 15, ROM::Player_PickUpBattleSuit_WeaponsCountBug);
	klib::Asm6502::apply_word(p_rom, RAM::Inventory_WeaponsCount, 15, ROM::Player_PickUpDragonSlayer_ArmoursCountBug);
	constexpr byte OP_BEQ{ 0xf0 };
	klib::Asm6502::apply_byte(p_rom, OP_BEQ, 14, ROM::PendantBugBNE);
}

namespace {
	struct FallProfile { std::vector<byte> curve; byte steer; };
	const std::map<std::string, FallProfile> FALL_PROFILES{
		{ "vanilla", { {}, 0 } },
		{ "arc",     { { 1, 1, 1, 1, 2, 2, 4, 4, 4, 4, 8 }, 1 } },
		{ "zelda2",  { { 1, 2, 3, 4, 5, 6, 7, 8 }, 2 } },
		{ "floaty",  { { 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6 }, 2 } },
		{ "moon",    { { 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 4 }, 2 } },
	};
	constexpr std::size_t FALL_MAX_ENTRIES{ 16 };
	constexpr int FALL_MAX_STEP{ 8 };
	constexpr std::size_t FALL_SIZE_MARK{ 25 };
	constexpr std::size_t FALL_SIZE_STEP{ 27 };
	constexpr std::size_t FALL_SIZE_ARMED{ 20 };
	constexpr std::size_t FALL_SIZE_MARK_G{ 30 };
	constexpr std::size_t FALL_SIZE_STEP_G{ 42 };

	void require_vanilla(const std::vector<byte>& p_rom, word p_cpu, std::initializer_list<byte> p_bytes,
		const char* p_what) {
		const auto off{ klib::Asm6502::get_file_offset(15, p_cpu) };
		std::size_t i{ 0 };
		for (byte b : p_bytes) {
			if (p_rom.at(off + i) != b)
				throw std::runtime_error(std::format(
					"AtlasDevFallControl: {} at ${:04x} is not vanilla (byte {} is {:02x}, expected {:02x})",
					p_what, p_cpu, i, p_rom.at(off + i), b));
			++i;
		}
	}
}

// AtlasDevFallControl: an accelerating fall curve walked by the jump phase
// byte $A6 (idle during free fall), and Left/Right steering in the air.
// Three main line splices, no RAM of its own. With kind=N every stub first
// scans the AtlasDevFrameScheduler slot bytes for N and runs the vanilla
// bytes when no slot holds it, so scripts switch the hack with
// AtlasDevArmRole N; the installer seeds a boot slot unless boot=false.
// The regression test pins the emitted bytes for both shapes.
word fh::HackManager::install_AtlasDevFallControl(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, const fh::GeneralHack& p_hack) const {
	using namespace fh::afs;
	std::string profile{ p_hack.string_or("profile", "arc") };
	std::transform(profile.begin(), profile.end(), profile.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	const auto it{ FALL_PROFILES.find(profile) };
	if (it == FALL_PROFILES.end())
		throw std::runtime_error(std::format("AtlasDevFallControl: unknown profile '{}'", profile));
	std::vector<byte> curve{ it->second.curve };
	byte steer{ it->second.steer };

	if (p_hack.has_param("curve")) {
		curve.clear();
		for (const auto& s : p_hack.split("curve")) {
			const int v{ klib::str::parse_numeric(s) };
			if (v < 0 || v > FALL_MAX_STEP)
				throw std::runtime_error(std::format(
					"AtlasDevFallControl: curve entry {} is outside 0..{} px", v, FALL_MAX_STEP));
			curve.push_back(static_cast<byte>(v));
		}
	}
	if (p_hack.has_param("steer")) {
		steer = p_hack.get_byte("steer");
		if (steer > 2)
			throw std::runtime_error("AtlasDevFallControl: steer must be 0, 1 or 2");
	}
	if (curve.size() > FALL_MAX_ENTRIES)
		throw std::runtime_error(std::format("AtlasDevFallControl: curve has {} entries, at most {}",
			curve.size(), FALL_MAX_ENTRIES));
	if (!curve.empty() && std::all_of(curve.begin(), curve.end(), [](byte b) { return b == 0; }))
		throw std::runtime_error("AtlasDevFallControl: every curve entry is zero, the player would never fall");
	const byte kind{ p_hack.byte_or("kind", 0) };
	const bool boot{ p_hack.bool_or("boot", true) };

	if (curve.empty() && steer == 0)
		return cpu_addr;

	const bool gated{ kind != 0 };
	const word armed_addr{ cpu_addr };
	const word mark_addr{ static_cast<word>(cpu_addr + (gated ? FALL_SIZE_ARMED : 0)) };
	const word step_addr{ static_cast<word>(mark_addr + (gated ? FALL_SIZE_MARK_G : FALL_SIZE_MARK)) };
	const word curve_addr{ static_cast<word>(step_addr + (gated ? FALL_SIZE_STEP_G : FALL_SIZE_STEP)) };
	const word steer_addr{ static_cast<word>(curve.empty()
		? (gated ? cpu_addr + FALL_SIZE_ARMED : cpu_addr)
		: curve_addr + curve.size()) };

	// ownership checks first, so a refused install leaves the ROM byte identical
	std::size_t scheduler{ 0 };
	std::size_t arm_site{ OFF_ARM0 + 3 };
	if (gated) {
		const word base{ find_base(p_rom) };
		if (base == 0)
			throw std::runtime_error("AtlasDevFallControl: kind needs the AtlasDevFrameScheduler hack installed first");
		scheduler = klib::Asm6502::get_file_offset(15, base);
		if (boot) {
			const auto read_operand{ [&p_rom, scheduler](std::size_t site) {
				return static_cast<word>(p_rom[scheduler + site] | (p_rom[scheduler + site + 1] << 8));
			} };
			const word stub_target{ static_cast<word>(base + OFF_STUB) };
			constexpr std::size_t pre_sites[3]{ OFF_PRE0, OFF_PRE1, OFF_PRE2 };
			for (std::size_t i{ 0 }; i < 3; ++i)
				if (p_rom[scheduler + OFF_ARM0 + i] == kind) {
					if (read_operand(pre_sites[i]) != stub_target)
						throw std::runtime_error(std::format(
							"AtlasDevFallControl: scheduler kind {} slot has a PRE claimant", kind));
					arm_site = OFF_ARM0 + i;
					break;
				}
			if (arm_site == OFF_ARM0 + 3)
				for (std::size_t i{ 0 }; i < 3; ++i)
					if (p_rom[scheduler + OFF_ARM0 + i] == 0x00
						&& read_operand(pre_sites[i]) == stub_target) {
						arm_site = OFF_ARM0 + i;
						break;
					}
			if (arm_site == OFF_ARM0 + 3)
				throw std::runtime_error("AtlasDevFallControl: scheduler arm table has no unclaimed slot");
		}
	}
	if (!curve.empty()) {
		require_vanilla(p_rom, ROM::Player_Fall_MarkDescending, { 0xa5, 0xa4, 0x09, 0x04, 0x85, 0xa4 }, "mark site");
		require_vanilla(p_rom, ROM::Player_Fall_Step, { 0xa5, 0xa1, 0x18, 0x69, 0x08, 0x85, 0xa1 }, "step site");
	}
	if (steer != 0)
		require_vanilla(p_rom, ROM::Player_Input_AirborneGate, { 0xa5, 0xa4, 0x29, 0x05, 0xf0, 0x0f }, "airborne gate");

	klib::Asm6502 code;
	if (gated) {
		// armed: Z set when some slot holds our kind; JSR and RTS keep the flags
		code.label("@armed");
		code.lda_abs(RAM_SLOT0); code.cmp_imm(kind); code.beq("@armed_yes");
		code.lda_abs(RAM_SLOT1); code.cmp_imm(kind); code.beq("@armed_yes");
		code.lda_abs(RAM_SLOT2); code.cmp_imm(kind);
		code.label("@armed_yes"); code.rts();
	}
	if (!curve.empty()) {
		const byte last{ static_cast<byte>(curve.size() - 1) };
		// mark: the first fall frame initialises the phase; 32 means an arc ran out mid air
		if (gated) { code.jsr("@armed"); code.bne("@marked"); }
		code.lda_zp(0xa4); code.and_imm(0x04); code.bne("@marked");
		code.ldx_imm(0x00);
		code.lda_zp(0xa6); code.cmp_imm(0x20); code.bne("@reset");
		code.ldx_imm(last);
		code.label("@reset"); code.stx_zp(0xa6);
		code.label("@marked"); code.lda_zp(0xa4); code.ora_imm(0x04); code.sta_zp(0xa4); code.rts();
		// step: clamp the phase to the curve, add the entry, saturate on the last one
		if (gated) { code.jsr("@armed"); code.bne("@vanilla8"); }
		code.ldx_zp(0xa6);
		code.cpx_imm(last); code.bcc("@inrange");
		code.ldx_imm(last); code.stx_zp(0xa6);
		code.label("@inrange");
		code.lda_zp(0xa1); code.clc(); code.adc_abs_x(curve_addr); code.sta_zp(0xa1);
		code.cpx_imm(last); code.bcs("@nostep");
		code.db(0xe6); code.db(0xa6);                       // INC $A6 (no inc_zp in Asm6502)
		code.label("@nostep"); code.jmp(ROM::Player_Fall_AfterStep);
		if (gated) {
			code.label("@vanilla8");                        // the displaced vanilla bytes
			code.lda_zp(0xa1); code.clc(); code.adc_imm(0x08); code.sta_zp(0xa1);
			code.jmp(ROM::Player_Fall_AfterStep);
		}
		for (byte b : curve) code.db(b);
	}
	if (steer == 1 || (gated && steer == 2)) {
		if (gated) { code.jsr("@armed"); code.bne("@vgate"); }
		if (steer == 2) {
			code.label("@normal"); code.jmp(ROM::Player_Input_Normal);
		}
		else {
			code.lda_zp(0xa4); code.and_imm(0x05); code.beq("@normal");
			code.lda_zp(0xa4); code.bmi("@momentum");
			code.lda_zp(0x16); code.and_imm(0x03); code.bne("@normal");
			code.label("@momentum"); code.jmp(ROM::Player_Input_AirborneContinue);
			code.label("@normal"); code.jmp(ROM::Player_Input_Normal);
		}
		if (gated) {
			code.label("@vgate");                           // the vanilla gate, replicated
			code.lda_zp(0xa4); code.and_imm(0x05); code.beq("@normal");
			code.jmp(ROM::Player_Input_AirborneContinue);
		}
	}

	const word next{ code.size() == 0 ? cpu_addr
		: code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 15, cpu_addr) };

	klib::Asm6502 hook;
	if (!curve.empty()) {
		hook.jsr(mark_addr); hook.nop(3);
		hook.apply_hack_and_clear(p_rom, 15, ROM::Player_Fall_MarkDescending);
		hook.jmp(step_addr); hook.nop(4);
		hook.apply_hack_and_clear(p_rom, 15, ROM::Player_Fall_Step);
	}
	if (steer == 1 || (gated && steer == 2)) {
		hook.jmp(steer_addr); hook.nop(3);
		hook.apply_hack_and_clear(p_rom, 15, ROM::Player_Input_AirborneGate);
	}
	else if (steer == 2) {
		hook.jmp(ROM::Player_Input_Normal); hook.nop(3);
		hook.apply_hack_and_clear(p_rom, 15, ROM::Player_Input_AirborneGate);
	}
	if (gated && boot)
		p_rom[scheduler + arm_site] = kind;
	return next;
}

// orchestrator for per-bank hack injection
std::size_t fh::HackManager::install_general_hacks(const fe::Config& p_config, std::vector<byte>& p_rom, byte p_bank,
	std::size_t p_cpu_addr_start, std::size_t p_cpu_addr_end, const std::vector<GeneralHack>& p_hacks,
	const fe::Game* p_game) const {
	if (p_hacks.empty())
		return 0;

	std::size_t crown_count{ 0 };
	bool ladder_exit{ false };
	bool jump_buffer{ false };
	for (const auto& hack : p_hacks) {
		if (hack.get_type() == GeneralHackLib::AtlasDevLadderCrown) {
			++crown_count;
			ladder_exit = ladder_exit || hack.string_or("mode", "crown") != "vanilla";
		}
	}
	if (crown_count > 1)
		throw std::runtime_error("AtlasDevLadderCrown: only one entry is allowed");
	// Crown checks retail continuations before companions install their hooks.
	// Move only Crown; all existing hacks retain their own relative order.
	auto ordered{p_hacks};
	if (ladder_exit) {
		for (const auto& hack : p_hacks)
			if (hack.get_type() == GeneralHackLib::AtlasDevJumpControl)
				jump_buffer = jump_buffer || hack.byte_or("buffer", 5) != 0;
		const auto crown{std::find_if(ordered.begin(), ordered.end(), [](const auto& hack) {
			return hack.get_type() == GeneralHackLib::AtlasDevLadderCrown;
		})};
		std::rotate(ordered.begin(), crown, std::next(crown));
	}

	std::size_t cpu_window_start{ 0 };
	std::size_t cpu_window_end{ 0 };
	switch (p_bank) {
	case 12:
	case 14:
		cpu_window_start = 0x8000;
		cpu_window_end = 0xc000;
		break;
	case 15:
		cpu_window_start = 0xc000;
		cpu_window_end = 0x10000;
		break;
	default:
		throw std::runtime_error(std::format(
			"Unsupported general-hack PRG bank {}", p_bank));
	}

	if (p_cpu_addr_start < cpu_window_start
		|| p_cpu_addr_start > 0xffff
		|| p_cpu_addr_end > cpu_window_end
		|| p_cpu_addr_start > p_cpu_addr_end)
		throw std::runtime_error("Invalid general-hack CPU range");

	// Work transactionally: a rejected installer or a final capacity failure
	// must not leave a partially modified ROM in memory.
	std::vector<byte> patched_rom{ p_rom };
	// TODO: Throw if cpu range is invalid but that would be a config error
	const word cpu_start{ static_cast<word>(p_cpu_addr_start) };
	word cpu_addr{ cpu_start };
	// these native companions scan their prospective bodies before the
	// assembler's bounds check. Crown can leave less than one body available;
	// reject that case before the scan, without changing their legacy path.
	const auto crown_companion_capacity = [&](std::size_t bytes) {
		const auto offset{klib::Asm6502::get_file_offset(p_bank, cpu_addr)};
		if (cpu_addr > p_cpu_addr_end || bytes > p_cpu_addr_end - cpu_addr
			|| offset > patched_rom.size() || bytes > patched_rom.size() - offset)
			throw std::runtime_error("AtlasDevLadderCrown: fixed-bank companion overflow");
	};

	for (const auto& hack : ordered) {
		const word previous_cpu_addr{ cpu_addr };
		switch (hack.get_type()) {
		case fh::GeneralHackLib::KillSwitch:
			cpu_addr = install_KillSwitch(p_config, patched_rom, p_bank, cpu_addr);
			break;
		case fh::GeneralHackLib::SameWorldTransPal2Mus:
			cpu_addr = install_SameWorldTransPal2Mus(p_config, patched_rom, p_bank, cpu_addr,
				p_game && p_game->m_sw_door_type == fe::SameWorldDoorType::Randumizer_0_30);
			break;
		case fh::GeneralHackLib::FogRules:
			cpu_addr = install_FogRules(p_config, patched_rom, cpu_addr, hack);
			break;
		case fh::GeneralHackLib::DynamicTilesets:
			cpu_addr = install_DynamicTilesets(p_config, patched_rom, p_bank, cpu_addr, hack, p_game);
			break;
		case fh::GeneralHackLib::PoisonPickup:
			install_PoisonPickup(p_config, patched_rom, hack);
			break;
		case fh::GeneralHackLib::TextSpeed:
			cpu_addr = install_TextSpeed(p_config, patched_rom, cpu_addr, hack);
			break;
		case fh::GeneralHackLib::SRAM:
			install_SRAM(p_config, patched_rom, hack);
			break;
		case fh::GeneralHackLib::BugFixes:
			install_BugFixes(patched_rom);
			break;
		case fh::GeneralHackLib::FastStart:
			cpu_addr = install_FastStart(p_config, patched_rom, cpu_addr, hack);
			break;
		case fh::GeneralHackLib::QuestFlagItemDrops:
			cpu_addr = install_QuestFlagItemDrops(p_config, patched_rom, cpu_addr, hack);
			break;
		case fh::GeneralHackLib::FlexibleItems:
			cpu_addr = install_FlexibleItems(p_config, patched_rom, cpu_addr, hack);
			break;
		case fh::GeneralHackLib::BossLockedItems:
			cpu_addr = install_BossLockedItems(p_config, patched_rom, cpu_addr, hack);
			break;
		case fh::GeneralHackLib::AtlasDevFrameScheduler:
			if (ladder_exit) crown_companion_capacity(afs::CORE_SIZE);
			cpu_addr = install_AtlasDevFrameScheduler(p_config, patched_rom, cpu_addr, hack);
			break;
		case fh::GeneralHackLib::AtlasDevDayNightCycle:
			cpu_addr = install_AtlasDevDayNightCycle(p_config, patched_rom, cpu_addr, hack);
			break;
		case fh::GeneralHackLib::AtlasDevInfectedTint:
			cpu_addr = install_AtlasDevInfectedTint(p_config, patched_rom, cpu_addr, hack);
			break;
		case fh::GeneralHackLib::AtlasDevTimeOfDay:
			cpu_addr = install_AtlasDevTimeOfDay(p_config, patched_rom, cpu_addr, hack);
			break;
		case fh::GeneralHackLib::AtlasDevJumpControl: {
			if (ladder_exit) {
				const bool coyote{hack.byte_or("coyote", 5) != 0};
				const bool buffer{hack.byte_or("buffer", 5) != 0};
				const bool hop{hack.byte_or("shorthop", 3) != 0};
				const bool air{hack.byte_or("airjumps", 0) != 0};
				const bool switched{hack.byte_or("switchable", 0) != 0};
				const bool trying{coyote || air};
				// Native Jump's fall/init/hop/buffer emitter sizes depend only
				// on these five booleans. Crown's regression pins all 32 shapes.
				std::size_t bytes{};
				if (coyote || buffer || air)
					bytes += 3 + (switched ? 24 : 0) + (coyote ? 18 : 0)
						+ (buffer ? 3 + (trying ? 2 : 0) : 4)
						+ (trying ? 25 + (buffer ? 8 : 0) : 0) + (air ? 17 : 0);
				if (buffer || air)
					bytes += 22 + (switched ? 34 : 0) + (buffer ? 3 + (air ? 2 : 0) : 4)
						+ (air ? 43 : 0) + (buffer ? 15 : 3);
				if (hop) bytes += 25 + (switched ? 24 : 0);
				if (buffer) bytes += 28;
				crown_companion_capacity(bytes);
			}
			cpu_addr = install_AtlasDevJumpControl(p_config, patched_rom, cpu_addr, hack);
			break;
		}
		case fh::GeneralHackLib::AtlasDevLadderControl:
			if (ladder_exit && hack.byte_or("attackflag", 0xff) != 0xff)
				crown_companion_capacity(14);
			cpu_addr = install_AtlasDevLadderControl(p_config, patched_rom, cpu_addr, hack);
			break;
		case fh::GeneralHackLib::AtlasDevFallControl:
			cpu_addr = install_AtlasDevFallControl(p_config, patched_rom, cpu_addr, hack);
			break;
		case fh::GeneralHackLib::AtlasDevLadderCrown:
			if (p_bank != 15)
				throw std::runtime_error("AtlasDevLadderCrown: requires PRG bank 15");
			cpu_addr = install_AtlasDevLadderCrown(p_config, patched_rom, cpu_addr, hack, jump_buffer);
			break;
		default:
			throw std::runtime_error("Unsupported general hack library routine.");
		}

		if (cpu_addr < previous_cpu_addr)
			throw std::runtime_error(std::format(
				"Hack address wrapped in bank ${:02x}", p_bank));
		if (static_cast<std::size_t>(cpu_addr) > p_cpu_addr_end)
			throw std::runtime_error(std::format("Hack overflow in bank ${:02x}", p_bank));
	}

	p_rom = std::move(patched_rom);
	return static_cast<std::size_t>(cpu_addr) - p_cpu_addr_start;
}
