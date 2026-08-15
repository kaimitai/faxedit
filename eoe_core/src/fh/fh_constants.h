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
		constexpr word Portrait_Pump{ 0x87b0 };
		constexpr word ItemNameDraw{ 0x8c36 };
		constexpr word IconDraw{ 0x8c58 };
		constexpr word OpenWindowDraw{ 0x8ef1 };
		constexpr word WindowClose{ 0x9002 };
		constexpr word Text_ContinueGate{ 0x9956 };
		constexpr word TextGridLay{ 0x9910 };
		constexpr word IScripts_RootPointerLo{ 0x9f6b };
		constexpr word IScripts_RootPointerHi{ 0xa003 };
		constexpr word GameLoop_RunScreenEventHandlers{ 0xef4b };

		// bank 15 addresses
		constexpr word Game_Init_JSR_Game_InitMMCAndBank{ 0xc954 };
		constexpr word Game_Init_JSR_Game_InitScreenAndMusic{ Game_Init_JSR_Game_InitMMCAndBank + 3 };
		constexpr word WaitForInterrupt{ 0xca2e };
		constexpr word Game_InitMMCAndBank{ 0xcbbf };
		constexpr word PPUBuffer_WaitEmpty{ 0xcff4 };
		constexpr word PPUQueueAppendHeader{ 0xcfdc };
		constexpr word Screen_CopyBgPaletteToShadow{ 0xd03b };
		constexpr word PPUBuffer_QueuePaletteUpload{ 0xd090 };
		constexpr word Screen_SetFadePalette{ 0xd0ad };
		constexpr word Sound_PlayEffect{ 0xd0e4 };
		constexpr word Area_SetBlocks{ 0xd7c5 };
		constexpr word Area_ConvertPixelsToBlockPos{ 0xe86c };
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
	}

	namespace RAM {
		constexpr byte ZP_Temp07{ 0x07 };
		constexpr byte ZP_Temp08{ 0x08 };
		constexpr byte ZP_CurrentWorld{ 0x24 };
		constexpr byte ZP_CurrentScreen{ 0x63 };
		constexpr byte ZP_DoorBlockPos{ 0x6a };
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
		constexpr byte ZP_Temp_Int24_L{ 0xec };
		constexpr byte ZP_Temp_Int24_M{ 0xed };
		constexpr byte ZP_Temp_Int24_U{ 0xee };
		constexpr byte ZP_MusicCurrent{ 0xfa };
		constexpr byte ZP_PlayerState{ 0xa4 };
		constexpr byte ZP_PlayerInvincibilityTimer{ 0xad };

		constexpr word CurrentROMBank{ 0x0100 };
		// IScripts_Begin stores the active root index here before opening the
		// textbox. Vanilla root $1f is reserved for the death dialogue.
		constexpr word CurrentIScriptRoot{ 0x0200 };
		constexpr word IScriptTextBoxContext{ 0x0201 };
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
		constexpr word PortraitSavedPalette{ 0x03d3 };
		constexpr word SelectedWeapon{ 0x03bd };
		constexpr word SelectedMagic{ 0x03c0 };

		constexpr word Flags{ 0x0101 };
		constexpr word DoorKeyRequirement{ 0x042b };
		constexpr word QuestFlags{ 0x042d };
		constexpr word CurrentScreen_SpecialEventID{ 0x042e };
		constexpr word CurrentStage{ 0x0435 };
		constexpr word PlayerIsDead{ 0x0438 };
		constexpr word ScreenBuffer{ 0x0600 };
	}

	namespace c {
		constexpr byte FlagsByteCount{ 0x1f };

		constexpr char ID_ROM_ISCRIPTS_LOADBYTE[]{ "rom_iscripts_loadbyte" };
		constexpr char ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE[]{ "rom_iscripts_skipaddrandinvoke" };
		constexpr char ID_ROM_ISCRIPTS_JUMPTONEXTADDR[]{ "rom_iscripts_jumptonextaddr" };
		constexpr char ID_ROM_ISCRIPTS_INVOKENEXTACTION[]{ "rom_iscripts_invokenextaction" };
		constexpr char ID_ROM_ISCRIPTS_UPDATEPORTRAITANIMATION[]{ "rom_iscripts_updateportraitanimation" };
		constexpr char ID_ROM_MMC1_UPDATEROMBANK[]{ "rom_mmc1_updaterombank" };
		constexpr char ID_ROM_PLAYER_UPDATEEXPERIENCE[]{ "rom_player_updateexperience" };
		constexpr char ID_ROM_PLAYER_ISCLIMBING[]{ "rom_player_isclimbing" };

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

		// Used only by AtlasDevShowMessageFromVar. No default is shipped: a
		// project must define both before that opcode can be installed, and
		// Config::constant throws by name if either is missing, so the build
		// fails loudly rather than reading unallocated RAM.
		constexpr char ID_HACK_SCRIPT_VAR_RAM_ADDR[]{ "hack_script_var_ram_addr" };
		constexpr char ID_HACK_SCRIPT_VAR_COUNT[]{ "hack_script_var_count" };

		constexpr char ID_FLAGS_WRAM_TO_SRAM[]{ "flags_wram_to_sram" };

		constexpr char ID_ISCRIPT_RG2_START[]{ "iscript_data_rg2_start" };
		constexpr char ID_COMMAND_BYTE_COUNT_OFFSET[]{ "command_byte_count_offset" };
	}
}

#endif
