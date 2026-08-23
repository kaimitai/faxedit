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
	code.jsr(ROM::Game_LoadFirstLevel);
	code.rts();

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
		case fh::GeneralHackLib::FastStart:
			cpu_addr = install_FastStart(p_config, p_rom, cpu_addr, hack);
			break;
		case fh::GeneralHackLib::QuestFlagItemDrops:
			cpu_addr = install_QuestFlagItemDrops(p_config, p_rom, cpu_addr, hack);
			break;
		default:
			throw std::runtime_error("Unsupported general hack library routine.");
		}

		if (static_cast<std::size_t>(cpu_addr) > p_cpu_addr_end)
			throw std::runtime_error(std::format("Hack overflow in bank ${:02x}", p_bank));
	}

	return static_cast<std::size_t>(cpu_addr) - p_cpu_addr_start;
}
