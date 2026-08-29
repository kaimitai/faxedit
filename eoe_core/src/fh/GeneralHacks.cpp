#include "HackManager.h"
#include "fe/Config.h"
#include "fe/Game.h"
#include "common/klib/Asm6502.h"
#include "fh_constants.h"
#include "fe/fe_constants.h"
#include <format>
#include <stdexcept>

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
	const bool state{ p_hack.bool_or("state", true) };
	const bool selling{ p_hack.bool_or("selling", true) };
	const word price{ p_hack.word_or("price", 100) };

	klib::Asm6502 code;

	// compare against nonexistent world $ff instead of building world $04
	if (buildings)
		klib::Asm6502::apply_byte(p_rom, 0xff, 12, ROM::PlayerMenu_HandleInventoryMenuInput_CMP_WorldNo);

	if (state) {
		code.nop(2);
		code.apply_hack_and_clear(p_rom, 15, ROM::GameLoop_CheckUseCurrentItem_BNE_Return);
	}

	if (!selling)
		return cpu_addr;
	else {
		// install hook
		code.jmp(cpu_addr);
		code.apply_hack_and_clear(p_rom, 12, ROM::ShowSellMenu_JSR_FindSellMenuEntry);

		// preserve the original item ID in X when no sell-table entry is found
		code.nop();
		code.apply_hack_and_clear(p_rom, 12, ROM::FindSellMenuEntry_TAX);

		// the sell-any-item routine itself
		code.jsr(ROM::FindSellMenuEntry);
		code.cmp_imm(0xff);
		code.beq("@shop_entry_missing");
		// item has a normal sell-table entry; continue with vanilla logic
		code.jmp(ROM::ShowSellMenu_LDX_StringCount);

		code.label("@shop_entry_missing");
		code.txa();
		code.ldx_abs(RAM::UIStringCount);
		code.sta_abs_x(RAM::UIDataArray);
		code.lda_imm(price % 256);
		code.sta_abs_x(RAM::ShopItemCostsLo);
		code.lda_imm(price / 256);
		code.jmp(ROM::ShowSellMenu_STA_CostHi);

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

// makes a trampoline for calls to the tileset loader
word fh::HackManager::install_DynamicTilesets(
	const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {

	const byte bank{ p_hack.get_byte("bank") };
	const word addr{ p_hack.get_word("addr") };
	const auto entries{ p_hack.split_twice_bytes("data", 3) };

	// world -> (screen -> tileset)
	std::map<byte, std::map<byte, byte>> assigns;
	for (const auto& entry : entries)
		assigns[entry[0]][entry[1]] = entry[2];

	const word MMC1_UpdateROMBank{
		cfg_word(p_config, c::ID_ROM_MMC1_UPDATEROMBANK)
	};
	const word CurrentROMBank{ RAM::CurrentROMBank };

	klib::Asm6502 code;

	// -------------------------------------------------------------------------
	// remote routine - stage doors and other-world transitions
	// -------------------------------------------------------------------------

	code.lda_zp(RAM::ZP_CurrentWorld);
	code.cmp_imm(0x00);
	code.bne("@done");
	code.lda_zp(RAM::ZP_CurrentScreen);
	code.cmp_imm(0x01);
	code.bne("@done");

	code.lda_imm(0x0f);
	code.sta_zp(RAM::ZP_TilesIndex);

	code.label("@done");
	code.rts();

	const word sameworld_addr{
		code.apply_hack_and_clear_get_next_cpu_addr(p_rom, bank, addr)
	};

	// -------------------------------------------------------------------------
	// remote routine - same-world doors and transitions
	// -------------------------------------------------------------------------

	code.lda_zp(RAM::ZP_CurrentWorld);
	code.cmp_imm(0x00);
	code.bne("@done");
	code.lda_zp(RAM::ZP_TransitionScreen);
	code.cmp_imm(0x01);
	code.bne("@done");

	code.lda_imm(0x0f);
	code.sta_zp(RAM::ZP_TilesIndex);

	code.label("@done");
	code.rts();

	code.apply_hack_and_clear_get_next_cpu_addr(p_rom, bank, sameworld_addr);

	// -------------------------------------------------------------------------
	// bank 15 - stage doors / other-world trampoline
	// -------------------------------------------------------------------------

	code.jsr(cpu_addr);
	code.apply_hack_noclear(
		p_rom, 15, ROM::Game_EnterAreaHandler_JSR_Area_LoadTiles);
	code.apply_hack_noclear(
		p_rom, 15, ROM::Game_LoadCurrentArea_JSR_Area_LoadTiles);
	code.apply_hack_and_clear(
		p_rom, 15, ROM::Game_LoadFirstLevel_JSR_Area_LoadTiles);

	// save currently mapped switchable bank
	code.lda_abs(CurrentROMBank);
	code.pha();

	// switch to DynamicTilesets bank
	code.ldx_imm(bank);
	code.jsr(MMC1_UpdateROMBank);

	// perform lookup
	code.jsr(addr);

	// restore previous bank
	code.pla();
	code.tax();
	code.jsr(MMC1_UpdateROMBank);

	code.jmp(ROM::Area_LoadTiles);

	const word sameworld_cpu_addr{
		code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 15, cpu_addr)
	};

	// -------------------------------------------------------------------------
	// bank 15 - same-world trampoline
	// -------------------------------------------------------------------------

	code.jsr(sameworld_cpu_addr);
	code.apply_hack_and_clear(
		p_rom, 15, ROM::Game_SetupEnterScreen_JSR_Screen_Load);

	// save currently mapped switchable bank
	code.lda_abs(CurrentROMBank);
	code.pha();

	// switch to DynamicTilesets bank
	code.ldx_imm(bank);
	code.jsr(MMC1_UpdateROMBank);

	// perform lookup
	code.jsr(sameworld_addr);

	// restore previous bank
	code.pla();
	code.tax();
	code.jsr(MMC1_UpdateROMBank);

	// If the lookup changed ZP_TilesIndex this reloads the tileset.
	// For the POC we're still doing this unconditionally on this path.
	code.jsr(ROM::Area_LoadTiles);

	code.jmp(ROM::Screen_Load);

	return code.apply_hack_and_clear_get_next_cpu_addr(
		p_rom, 15, sameworld_cpu_addr);
}

std::size_t fh::HackManager::install_general_hacks(const fe::Config& p_config, std::vector<byte>& p_rom, byte p_bank,
	std::size_t p_cpu_addr_start, std::size_t p_cpu_addr_end, const std::vector<GeneralHack>& p_hacks,
	const fe::Game* p_game) const {
	// TODO: Throw if cpu range is invalid but that would be a config error
	const word cpu_start{ static_cast<word>(p_cpu_addr_start) };
	word cpu_addr{ cpu_start };

	for (const auto& hack : p_hacks) {
		switch (hack.get_type()) {
		case fh::GeneralHackLib::KillSwitch:
			cpu_addr = install_KillSwitch(p_config, p_rom, p_bank, cpu_addr);
			break;
		case fh::GeneralHackLib::SameWorldTransPal2Mus:
			cpu_addr = install_SameWorldTransPal2Mus(p_config, p_rom, p_bank, cpu_addr,
				p_game && p_game->m_sw_door_type == fe::SameWorldDoorType::Randumizer_0_30);
			break;
		case fh::GeneralHackLib::FogRules:
			cpu_addr = install_FogRules(p_config, p_rom, cpu_addr, hack);
			break;
		case fh::GeneralHackLib::DynamicTilesets:
			cpu_addr = install_DynamicTilesets(p_config, p_rom, cpu_addr, hack);
			break;
		case fh::GeneralHackLib::FastStart:
			cpu_addr = install_FastStart(p_config, p_rom, cpu_addr, hack);
			break;
		case fh::GeneralHackLib::QuestFlagItemDrops:
			cpu_addr = install_QuestFlagItemDrops(p_config, p_rom, cpu_addr, hack);
			break;
		case fh::GeneralHackLib::FlexibleItems:
			cpu_addr = install_FlexibleItems(p_config, p_rom, cpu_addr, hack);
			break;
		case fh::GeneralHackLib::BossLockedItems:
			cpu_addr = install_BossLockedItems(p_config, p_rom, cpu_addr, hack);
			break;
		default:
			throw std::runtime_error("Unsupported general hack library routine.");
		}

		if (static_cast<std::size_t>(cpu_addr) > p_cpu_addr_end)
			throw std::runtime_error(std::format("Hack overflow in bank ${:02x}", p_bank));
	}

	return static_cast<std::size_t>(cpu_addr) - p_cpu_addr_start;
}
