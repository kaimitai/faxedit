#ifndef FH_CONSTANTS_H
#define FH_CONSTANTS_H

#include <cstddef>

using byte = unsigned char;
using word = uint16_t;

namespace fh {

	namespace ROM {
		// bank 12 addresses
		constexpr word IScripts_JumpTable_Ref_U{ 0x8273 }; // us, jp
		constexpr word IScripts_JumpTable_Ref_L{ 0x8277 }; // us, jp
		constexpr word TextBox_OpenForNPC{ 0x81e2 };
		constexpr word TextBox_Close{ 0x81fb };
		constexpr word TextBox_OpenForPortrait{ 0x821f };
		constexpr word TextBox_ClearForPortraitAndText{ 0x822b };
		constexpr word IScripts_MessageFinish{ 0x82b4 };
		constexpr word Menu_WaitInput{ 0x84ed };
		constexpr word ShowSellMenu_JSR_FindSellMenuEntry{ 0x8691 };
		constexpr word ShowSellMenu_LDX_StringCount{ 0x8696 };
		constexpr word ShowSellMenu_STA_CostHi{ 0x86a9 };
		constexpr word FindSellMenuEntry{ 0x8704 };
		constexpr word FindSellMenuEntry_TAX{ 0x8716 };
		constexpr word Portrait_Pump{ 0x87b0 };
		constexpr word PlayerMenu_HandleInventoryMenuInput_CMP_WorldNo{ 0x8b87 };
		constexpr word ItemNameDraw{ 0x8c36 };
		constexpr word IconDraw{ 0x8c58 };
		constexpr word OpenWindowDraw{ 0x8ef1 };
		constexpr word WindowClose{ 0x9002 };
		constexpr word Text_ContinueGate{ 0x9956 };
		constexpr word TextGridLay{ 0x9910 };
		constexpr word IScripts_RootPointerLo{ 0x9f6b };
		constexpr word IScripts_RootPointerHi{ 0xa003 };
		constexpr word GameLoop_RunScreenEventHandlers{ 0xef4b };
		constexpr word TextBox_RectRecompute{ 0x81c0 };

		// bank 14 addresses
		constexpr word SpriteBehavior_MattockDroppedFromRipasheiku_LDA_Quests{ 0xa3f4 };
		constexpr word SpriteBehavior_WingBootsDroppedByZorugeriru_LDA_Quests{ 0xa418 };

		// bank 15 addresses
		constexpr word GameLoop_CheckUseCurrentItem_BNE_Return{ 0xc47c };
		constexpr word Player_PickUpWingBootsWithQuest{ 0xc6d0 };
		constexpr word Player_PickUpMattockWithQuest{ 0xc74a };
		constexpr word Game_Init_JSR_Game_InitMMCAndBank{ 0xc954 };
		constexpr word Game_Init_JSR_Game_InitScreenAndMusic{ Game_Init_JSR_Game_InitMMCAndBank + 3 };
		constexpr word WaitForInterrupt{ 0xca2e };
		constexpr word Game_InitMMCAndBank{ 0xcbbf };
		// Waits until the PPU queue has room for up to $24 bytes.
		constexpr word PPUBuffer_WaitForCapacity{ 0xcfca };
		constexpr word PPUBuffer_WaitEmpty{ 0xcff4 };
		constexpr word Sprites_FlipRanges{ 0xcba8 };
		constexpr word MMC1_UpdateROMBank_SerialWrite{ 0xcc21 };
		constexpr word MMC1_EnsurePRG_fastPath{ 0xccd2 };
		constexpr word Area_LoadTiles{ 0xceb8 };
		constexpr word Area_LoadTiles_LDX_Bank{ 0xcee1 };
		constexpr word PPUQueueAppendHeader{ 0xcfdc };
		constexpr word Screen_CopyBgPaletteToShadow{ 0xd03b };
		// Stages the selected sprite palette and stores its index at $03d4.
		constexpr word Screen_CopySpritePaletteToShadow{ 0xd062 };
		constexpr word PPUBuffer_QueuePaletteUpload{ 0xd090 };
		constexpr word Screen_SetFadePalette{ 0xd0ad };
		constexpr word Sound_PlayEffect{ 0xd0e4 };
		constexpr word Area_SetBlocks{ 0xd7c5 };
		constexpr word Area_SetBlockAtPosition{ 0xd7b0 };
		constexpr word EntityAllocate{ 0xc205 };
		constexpr word EntityChrPass{ 0xc28d };
		constexpr word Game_SetupAndLoadOutsideArea{ 0xdadc };
		constexpr word Player_Spawn_LDA_Quests{ 0xdb12 };
		constexpr word Game_Start_JSR_Game_LoadFirstLevel{ 0xdb2c };
		constexpr word Start_Mana{ 0xdb30 };
		constexpr word Screen_Load{ 0xdd46 };
		constexpr word Game_LoadFirstLevel{ 0xdea7 };
		constexpr word Start_Health{ 0xdeaf };
		constexpr word Game_LoadCurrentArea_LoadPalette{ 0xdf1d };
		constexpr word Game_LoadCurrentArea_LDX_Stage{ 0xdf22 };
		constexpr word GameLoop_CheckPauseGame_JSR_Sprites_FlipRanges{ 0xe039 };
		constexpr word Player_CheckHandleEnterDoor_LDX_pal2mus_slots{ 0xe54a };
		constexpr word Player_CheckHandleEnterDoor_enterScreen{ 0xe565 };
		constexpr word Player_EnterDoorToOutside_JMP_SetupArea{ 0xe5d7 };
		constexpr word Area_SetStateFromDoorDestination_STA_DoorReq{ 0xe84c };
		constexpr word Area_ConvertPixelsToBlockPos{ 0xe86c };
		constexpr word SwTransJmpSetupEnterScreen{ 0xea2c };
		constexpr word GameLoop_RunScreenEventHandlers_CMP_06{ 0xef55 };
		constexpr word GameLoop_RunScreenEventHandlers_LDA_EventTable{ 0xef5a };
		constexpr word Portrait_LoadTiles{ 0xf24d };
		constexpr word Portrait_Clear{ 0xf281 };
		constexpr word Messages_Load{ 0xf3f5 };
		constexpr word Text_ShowNextChar{ 0xf466 };
		constexpr word TextGridRowQueue{ 0xf5d9 };
		constexpr word PPUAddressFromPos{ 0xf804 };
		constexpr word PPUAdvanceRow{ 0xf826 };
		constexpr word PPUQueuePayload{ 0xf845 };
		// One vanilla caller; inputs $ea/$eb tile position, $ec/$ed/$ee value,
		// Y digit count 1..7.  Emits through the buffered PPU queue.
		constexpr word Number_DrawAtPos{ 0xfa03 };
		constexpr word UI_DrawPlayerHPValue{ 0xfa75 };
		constexpr word Player_SetMP{ 0xfa85 };
		constexpr word Hud_DrawGold{ 0xf9e7 };
		constexpr word SpecialItemIdTable{ 0x9add };
		constexpr word SpecialItemMaskTable{ 0x8d52 };

		constexpr byte TransitionBank{ 0x0e };
		constexpr word EntityFacePlayerX{ 0x867b };
		constexpr word EntityWeaponDeathTail{ 0x889c };
		constexpr word Door_Dispatch{ 0xe533 };
		constexpr word Game_SetupEnterScreen{ 0xdaa0 };
		constexpr word Game_SetupNewArea{ 0xdacd };
		constexpr word Screen_PaletteFadeOut{ 0xda2f };
		constexpr word Door_MatchAtBlockPos{ 0xe7f5 };
		constexpr word Game_MainLoop{ 0xdb45 };
		constexpr word RegionAreaTable{ 0xdafe };
		constexpr word AreaPaletteTable{ 0xdf4c };
	}

	namespace RAM {
		constexpr byte ZP_Temp07{ 0x07 };
		constexpr byte ZP_Temp08{ 0x08 };
		// PPUMASK shadow copied to $2001 by the NMI.
		constexpr byte ZP_PPUMaskShadow{ 0x0b };
		// Horizontal nametable page; bit 0 selects $23c0/$27c0 attributes.
		constexpr byte ZP_CameraNametableParity{ 0x0d };
		constexpr byte ZP_Joy1_ChangedButtonMask{ 0x19 };
		// Producer cursor of the $0500 PPU command ring.  Producers seed X
		// from it, write payload bytes, and publish with one STX; the NMI
		// consumer compares it against the read cursor $1f.
		constexpr byte ZP_PPUBufferWriteCursor{ 0x20 };
		constexpr byte ZP_CurrentWorld{ 0x24 };
		constexpr byte ZP_CurrentScreen{ 0x63 };
		constexpr byte ZP_DoorBlockPos{ 0x6a };
		constexpr byte ZP_TilesIndex = 0x95;
		constexpr byte ZP_PlayerPosX = 0x9e;
		constexpr byte ZP_PlayerPosY = 0xa1;
		constexpr byte ZP_PlayerPosArgX = 0xb5;
		constexpr byte ZP_PlayerPosArgY = 0xb6;
		constexpr byte ZP_ScriptAddr{ 0xdb };
		constexpr byte ZP_ScriptAddrU{ 0xdc };
		constexpr byte ZP_ScriptOffset{ 0xdd };
		constexpr byte ZP_e2{ 0xe2 };
		constexpr byte ZP_e3{ 0xe3 };
		constexpr byte ZP_e4{ 0xe4 };
		constexpr byte ZP_e5{ 0xe5 };
		constexpr byte ZP_e6{ 0xe6 };
		constexpr byte ZP_e7{ 0xe7 };
		// $e8/$e9 are PPUQueueAppendHeader's PPU address input (lo/hi);
		// $ea/$eb are PPUAddressFromPos' tile position input (x/y).
		constexpr byte ZP_e8{ 0xe8 };
		constexpr byte ZP_e9{ 0xe9 };
		constexpr byte ZP_ea{ 0xea };
		constexpr byte ZP_eb{ 0xeb };
		constexpr byte ZP_Temp_Int24_L{ 0xec };
		constexpr byte ZP_Temp_Int24_M{ 0xed };
		constexpr byte ZP_Temp_Int24_U{ 0xee };
		constexpr byte ZP_ef{ 0xef };
		constexpr byte ZP_MusicCurrent{ 0xfa };
		constexpr byte ZP_PlayerState{ 0xa4 };
		constexpr byte ZP_PlayerInvincibilityTimer{ 0xad };

		constexpr word CurrentROMBank{ 0x0100 };
		// IScripts_Begin stores the active root index here before opening the
		// textbox. Vanilla root $1f is reserved for the death dialogue.
		constexpr word CurrentIScriptRoot{ 0x0200 };
		constexpr word IScriptTextBoxContext{ 0x0201 };
		constexpr word UIStringCount{ 0x021f };
		constexpr word UIDataArray{ 0x0220 };
		constexpr word ShopItemCostsLo{ 0x0228 };
		// The eight-slot entity arrays.  Every one of these is the game's own
		// RAM rather than space a hack must reserve: counting absolute
		// references in the vanilla PRG gives 72 for $02cc, 141 for $02dc and
		// 162 for $02e4, against zero for any address a hack invented.
		constexpr word EntitySlotActive{ 0x02cc };   // identity; bit 7 set = free
		constexpr word EntityOpsMode{ 0x02d4 };      // behaviour id, or $ff for BScript
		constexpr word EntityFlags{ 0x02dc };        // bit 0 facing, bit 4 hidden
		constexpr word EntityPhase{ 0x02e4 };
		constexpr word EntitySpeedFraction{ 0x02ec };
		constexpr word EntitySpeedWhole{ 0x02f4 };
		constexpr word EntityDropIdentity{ 0x02fc };
		constexpr word EntityMagicState{ 0x0334 };
		constexpr word EntityMagicCounter{ 0x033c };
		constexpr word CurrentEntitySlot{ 0x0378 };
		constexpr word EntityHealth{ 0x0344 };
		constexpr word EntityHitStun{ 0x034c };      // vanilla's own i-frame counter
		constexpr word EntityProgramLo{ 0x0354 };
		constexpr word EntityProgramHi{ 0x035c };
		constexpr word EntityScriptRoot{ 0x036c };
		constexpr word EntityUpdateFreeze{ 0x0426 }; // nonzero pauses every slot
		constexpr byte ZP_EntityX{ 0xba };
		constexpr byte ZP_EntityY{ 0xc2 };
		constexpr byte ZP_PlayerX{ 0x9e };
		constexpr byte ZP_PlayerY{ 0xa1 };
		constexpr word ItemInventory{ 0x03ad };
		constexpr word SelectedItem{ 0x03c1 };
		constexpr word NumberOfItems{ 0x03c6 };
		constexpr word World_DefaultMusic{ 0x03d1 };
		constexpr word PortraitSavedPalette{ 0x03d3 };
		constexpr word SelectedWeapon{ 0x03bd };
		constexpr word SelectedMagic{ 0x03c0 };
		constexpr word VisibleMagicState{ 0x02b3 };
		constexpr word VisibleMagicFlags{ 0x02b4 };
		constexpr word VisibleMagicXFraction{ 0x02b5 };
		constexpr word VisibleMagicX{ 0x02b6 };
		constexpr word VisibleMagicYFraction{ 0x02b7 };
		constexpr word VisibleMagicY{ 0x02b8 };
		constexpr word VisibleMagicCounter{ 0x02b9 };
		constexpr word VisibleMagicPhase{ 0x02ba };

		constexpr word Flags{ 0x0101 };
		constexpr word DoorKeyRequirement{ 0x042b };
		constexpr word QuestFlags{ 0x042d };
		constexpr word CurrentScreen_SpecialEventID{ 0x042e };
		constexpr word CurrentStage{ 0x0435 };
		constexpr word PlayerIsDead{ 0x0438 };
		constexpr word ScreenBuffer{ 0x0600 };
		constexpr word PlayerHPFraction{ 0x0432 };
		constexpr word PlayerHP{ 0x0431 };
		constexpr word PlayerGold_L{ 0x0392 };
		constexpr word PlayerGold_M{ 0x0393 };
		constexpr word PlayerGold_U{ 0x0394 };
		constexpr word InventoryArrays{ 0x039d };
		constexpr word InventoryCounts{ 0x03c2 };
		constexpr word SpecialItemBitfield{ 0x042c };
		constexpr word PlayerMana{ 0x039a };
		constexpr word PlayerXP_L{ 0x0390 };
		constexpr word PlayerXP_U{ 0x0391 };
		constexpr word PlayerTitle{ 0x0437 };
		constexpr word PendingTitle{ 0x04ed };
		// The 32-byte staged palette ($0293-$02b2, background then sprite
		// half) that queue command $00 copies to PPU palette RAM whole.
		constexpr word PaletteShadow{ 0x0293 };
		// The sub-palette the engine caches for textbox glyphs (single
		// vanilla writer, $d048); glyphs render in that sub's colour 3.
		constexpr word TextBoxSubPalette{ 0x038d };
		// Selector for the ROM background palette set; area transitions
		// overwrite it, RestorePalette re-stages from it.
		constexpr word ScreenPaletteIndex{ 0x03d0 };
		// PPU command ring; write cursor $20, read cursor $1f.
		constexpr word PPUBufferRing{ 0x0500 };
		constexpr byte ZP_DoorMatchFlag{ 0xb7 };
		constexpr byte ZP_TransitionScreen{ 0x64 };
		constexpr byte ZP_TransitionPalette{ 0x65 };
		constexpr byte ZP_TransitionStartPos{ 0x6c };
		constexpr word LiveWeapon{ 0x03c8 };
		// The four one-byte timed-effect counters, in this order: ointment,
		// glove, wing boots, hour glass.  Bit 7 set is the engine's own
		// "inactive" encoding - $c6af initialises all four to $ff - and the
		// effect tick decrements them one step per 64 frames while it is
		// clear.  Region-invariant: all four retail images reference
		// $0427-$042a the same number of times.
		constexpr word TimedEffectTimers{ 0x0427 };

		// RAM used by the sameworld to stage-door hack
		constexpr word Hack_StageChangePending{ 0x1fff };
		constexpr word Hack_PendingStage{ 0x1ffe };
	}

	namespace c {
		constexpr byte FlagsByteCount{ 0x1f };

		constexpr char ID_ROM_ISCRIPTS_BEGIN[]{ "rom_iscripts_begin" };
		constexpr char ID_ROM_ISCRIPTS_LOADBYTE[]{ "rom_iscripts_loadbyte" };
		constexpr char ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE[]{ "rom_iscripts_skipaddrandinvoke" };
		constexpr char ID_ROM_ISCRIPTS_JUMPTONEXTADDR[]{ "rom_iscripts_jumptonextaddr" };
		constexpr char ID_ROM_ISCRIPTS_INVOKENEXTACTION[]{ "rom_iscripts_invokenextaction" };
		constexpr char ID_ROM_ISCRIPTS_UPDATEPORTRAITANIMATION[]{ "rom_iscripts_updateportraitanimation" };
		constexpr char ID_ROM_MMC1_UPDATEROMBANK[]{ "rom_mmc1_updaterombank" };
		constexpr char ID_ROM_PLAYER_UPDATEEXPERIENCE[]{ "rom_player_updateexperience" };
		constexpr char ID_ROM_PLAYER_RANKREFRESH[]{ "rom_player_rankrefresh" };
		constexpr char ID_ROM_PLAYER_EXPHUDREDRAW[]{ "rom_player_exphudredraw" };
		constexpr char ID_ROM_PLAYER_ISCLIMBING[]{ "rom_player_isclimbing" };
		constexpr char ID_ROM_PPU_ADDRESS_FROM_POS[]{ "rom_ppu_address_from_pos" };
		constexpr char ID_ROM_PPU_QUEUE_PAYLOAD[]{ "rom_ppu_queue_payload" };
		constexpr char ID_ROM_WINDOW_CLOSE[]{ "rom_window_close" };
		constexpr char ID_ROM_VANILLA_FAR_CALL[]{ "rom_vanilla_far_call" };
		constexpr char ID_ROM_HUD_DRAW_TIMER[]{ "rom_hud_draw_timer" };

		constexpr char ID_HACK_CLEAR_PERSISTENT_FLAGS[]{ "hack_clear_persistent_flags" };
		constexpr char ID_TM_CHANGE_BANK[]{ "hack_tm_change_bank" };
		constexpr char ID_TM_CHANGE_CPU_ADDR[]{ "hack_tm_change_cpu_addr" };
		constexpr char ID_TM_CHANGE_HANDLER_CPU_ADDR[]{ "hack_tm_handler_cpu_addr" };
		constexpr char ID_TM_CHANGE_HANDLER_TABLE_CPU_ADDR[]{ "hack_tm_handler_table_cpu_addr" };
		constexpr char ID_TM_CHANGE_HANDLER_WAIT_FRAMES[]{ "hack_tm_change_wait_frames" };
		constexpr char ID_TM_CHANGE_HANDLER_SOUND_EFFECT[]{ "hack_tm_change_sound_effect" };
		constexpr char ID_TM_CHANGE_HANDLER_IDX[]{ "hack_tm_change_handler_index" };

		constexpr char ID_HACK_SCRIPT_JSR_RAM_ADDR_LO[]{ "hack_script_jsr_ram_addr_lo" };
		constexpr char ID_HACK_SCRIPT_JSR_RAM_ADDR_HI[]{ "hack_script_jsr_ram_addr_hi" };
		constexpr char ID_HACK_SCRIPT_SELECTED_FLAG_RAM_ADDR[]{ "hack_script_selected_flag_ram_addr" };

		// sameworld to stage-door hack injection point IDs
		constexpr char ID_HACK_HANDLE_PALETTE_ADDR[]{ "hack_handle_palette_addr" };
		constexpr char ID_HACK_SET_PENDING_STAGE_ADDR[]{ "hack_set_pending_stage_addr" };
		constexpr char ID_HACK_DECODE_REQ_ADDR[]{ "hack_decode_req_addr" };
		constexpr char ID_HACK_LOAD_WORLD_ADDR[]{ "hack_load_world_addr" };

		// double tileset hack
		constexpr char ID_HACK_DOUBLE_TILESET_ADDR[]{ "hack_double_tileset_addr" };

		// Transient iScript registers.
		constexpr char ID_HACK_SCRIPT_VAR_RAM_ADDR[]{ "hack_script_var_ram_addr" };
		constexpr char ID_HACK_SCRIPT_VAR_COUNT[]{ "hack_script_var_count" };

		constexpr char ID_FLAGS_WRAM_TO_SRAM[]{ "flags_wram_to_sram" };

		constexpr char ID_ISCRIPT_RG2_START[]{ "iscript_data_rg2_start" };
		constexpr char ID_COMMAND_BYTE_COUNT_OFFSET[]{ "command_byte_count_offset" };
	}
}

#endif
