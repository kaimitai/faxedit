#ifndef FH_HACKMANAGER_H
#define FH_HACKMANAGER_H

#include "common/magic_enum.hpp"
#include "fe/Config.h"
#include "TilemapChanges.h"
#include "GeneralHack.h"
#include <cstddef>
#include <cstdint>
#include <vector>

using byte = unsigned char;
using word = uint16_t;

namespace klib {
	class Asm6502;
}

namespace fe {
	struct Game;
}

namespace fh {

	enum class HackLib {
		SetFlag, ClearFlag, IfFlag, SelectFlag, SetSelectedFlag, IfSelectedFlag, ClearSelectedFlag,
		SetQuestFlag, ClearQuestFlag, IfQuestFlag,
		RunScreenHandler, GetXP, IfWorld, IfScreen, IfStage, Die,
		JSR, Return, ForceDoor, IfYX, IfDoorYX,
		IfAddrEquals, IfAddrBetween, SetAddr,
		AtlasDevSetVar, AtlasDevAddVar, AtlasDevSubVar,
		AtlasDevIfVarEqual, AtlasDevIfVarLess, AtlasDevIfVarGreaterEqual,
		AtlasDevShakeScreen, AtlasDevFadeOut, AtlasDevFadeIn,
		AtlasDevSetMusic, AtlasDevPlaySFX, AtlasDevIfMusic,
		AtlasDevShowSequentialMessages, AtlasDevShowNumberInMessage, AtlasDevShowChoiceToVar, AtlasDevClearPortrait, AtlasDevEntitySayMessage, AtlasDevShowMessageFromVar, AtlasDevHideTextbox, AtlasDevSetPortrait,
		AtlasDevOpenTextbox, AtlasDevCloseDialogue,
		AtlasDevIfEntityCountAtLeast, AtlasDevCountActiveEntities, AtlasDevFindEntity,
		AtlasDevFreezeEntities, AtlasDevResumeEntities, AtlasDevIfBossPresent,
		AtlasDevIfEntityTypePresent, AtlasDevIfEntitySlotActive, AtlasDevIfEntityHidden,
		AtlasDevSetEntityHidden, AtlasDevSetEntityHealth, AtlasDevDamageEntity,
		AtlasDevHealEntity, AtlasDevSetEntityInvincible, AtlasDevFaceEntityToPlayer,
		AtlasDevKnockbackEntity,
		AtlasDevSetEntityBehavior, AtlasDevSetEntitySpeed, AtlasDevSetEntityFacing,
		AtlasDevEntityFieldToVar, AtlasDevDrawVarNumber,
		AtlasDevGetLocationToVars, AtlasDevGetPlayerPositionToVars,
		AtlasDevVarBitOp, AtlasDevVarShift, AtlasDevClampVar, AtlasDevIfVarMask,
		AtlasDevIfPlayerFacing, AtlasDevIfPlayerClimbing, AtlasDevIfPlayerGrounded,
		AtlasDevIfPlayerAttacking, AtlasDevIfPlayerInvincible, AtlasDevIfPlayerDead,
		AtlasDevIfSelectedWeapon, AtlasDevIfSelectedMagic,
		AtlasDevWaitFrames, AtlasDevWaitForButtonPress,
		AtlasDevIfButtonHeld, AtlasDevIfButtonPressed,
		AtlasDevSetFacing, AtlasDevSetPlayerPosition,
		AtlasDevOpenWindow, AtlasDevShowIcon, AtlasDevCloseWindow,
		AtlasDevLayText, AtlasDevOpenWindowAtEntity, AtlasDevRestoreRect,
		AtlasDevShowItemName, AtlasDevShowIconEx, AtlasDevClearText,
		AtlasDevLayTextAt, AtlasDevClearTextLine, AtlasDevLayTextLine, AtlasDevSetHealth, AtlasDevSetMana, AtlasDevFullHeal, AtlasDevFullMana, AtlasDevIfHealthBelow, AtlasDevIfHealthAtLeast, AtlasDevIfManaAtLeast, AtlasDevAddExperience, AtlasDevSetExperience, AtlasDevSetGold, AtlasDevIfGoldAtLeast, AtlasDevIfXPAtLeast, AtlasDevIfItemCount,
		AtlasDevSetPalette, AtlasDevRestorePalette, AtlasDevLoadBgPalette, AtlasDevLoadSpritePalette,
		AtlasDevFlashScreen, AtlasDevSetColorEmphasis, AtlasDevQueuePaletteFlush, AtlasDevWipeScreenStep,
		AtlasDevAnimateTiles, AtlasDevSetTextColor, AtlasDevSetAttrRect, AtlasDevPlaceChrTile,
		AtlasDevDissolveEntity, AtlasDevSetEntityPosition, AtlasDevSetEntityScript,
		AtlasDevSetEntityBScript, AtlasDevIfEntityInRange,
		AtlasDevWarpToDoor, AtlasDevWarpAreaScreenPos,
		AtlasDevSpawnEntity, AtlasDevDropItem, AtlasDevDespawnEntity,
		AtlasDevDespawnAllEntities, AtlasDevSetMetatile, AtlasDevSetScreenEvent,
		AtlasDevApplyEffect, AtlasDevCastSpell, AtlasDevIfMagicActive,
		AtlasDevClearVisibleMagic, AtlasDevSpawnMagicAt,
		AtlasDevCastSpellFromEntity, AtlasDevSetMagicPosition,
		AtlasDevSetMagicFacing,
		AtlasDevArmRole, AtlasDevDayNight,
		// Keep Count last.
		Count
	};

	class HackManager {

		// script action library
		word apply_SetFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
			word cpu_addr, word flag_decode_helper_addr, word bitmask_table_addr) const;
		word apply_ClearFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
			word cpu_addr, word flag_decode_helper_addr, word bitmask_table_addr) const;
		word apply_IfFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
			word cpu_addr, word flag_decode_helper_addr, word bitmask_table_addr) const;
		word apply_SelectFlag(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_SetSelectedFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
			word cpu_addr, word bitmask_table_addr) const;
		word apply_ClearSelectedFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
			word cpu_addr, word bitmask_table_addr) const;
		word apply_IfSelectedFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
			word cpu_addr, word bitmask_table_addr) const;
		word apply_SetQuestFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
			word cpu_addr, word quest_flag_decode_helper_addr, word bitmask_table_addr) const;
		word apply_ClearQuestFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
			word cpu_addr, word quest_flag_decode_helper_addr, word bitmask_table_addr) const;
		word apply_IfQuestFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
			word cpu_addr, word quest_flag_decode_helper_addr, word bitmask_table_addr) const;
		word apply_RunScreenHandler(const fe::Config& p_config, std::vector<byte>& p_rom,
			word cpu_addr) const;
		word apply_GetXP(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_IfWorld(std::vector<byte>& p_rom, word cpu_addr, word helper_if_a_equals_addr) const;
		word apply_IfScreen(std::vector<byte>& p_rom, word cpu_addr, word helper_if_a_equals_addr) const;
		word apply_IfStage(std::vector<byte>& p_rom, word cpu_addr, word helper_if_a_equals_addr) const;
		word apply_Die(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_JSR(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_Return(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_ForceDoor(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_IfYX(std::vector<byte>& p_rom, word cpu_addr,
			word helper_get_player_block_pos_addr, word helper_if_a_equals_addr) const;
		word apply_IfDoorYX(std::vector<byte>& p_rom, word cpu_addr,
			word helper_if_a_equals_addr) const;
		word apply_IfAddrEquals(std::vector<byte>& p_rom, word cpu_addr,
			word helper_load_word_addr, word helper_if_a_equals_addr) const;
		word apply_IfAddrBetween(std::vector<byte>& p_rom, word cpu_addr,
			word helper_load_word_addr, word helper_if_a_between_addr) const;
		word apply_SetAddr(const fe::Config& p_config, std::vector<byte>& p_rom,
			word cpu_addr, word helper_load_word_addr) const;
		word apply_AtlasDevSetVar(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevAddVar(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSubVar(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfVarEqual(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfVarLess(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfVarGreaterEqual(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;

		word apply_AtlasDevShakeScreen(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevFadeOut(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevFadeIn(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetMusic(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevPlaySFX(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfMusic(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;

		word apply_AtlasDevShowSequentialMessages(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevShowNumberInMessage(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevShowChoiceToVar(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevClearPortrait(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevEntitySayMessage(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevShowMessageFromVar(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevHideTextbox(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetPortrait(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevOpenTextbox(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevCloseDialogue(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfEntityCountAtLeast(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevCountActiveEntities(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevFindEntity(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevFreezeEntities(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevResumeEntities(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfBossPresent(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfEntityTypePresent(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfEntitySlotActive(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfEntityHidden(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetEntityHidden(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetEntityHealth(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevDamageEntity(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevHealEntity(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetEntityInvincible(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevFaceEntityToPlayer(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevKnockbackEntity(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetEntityBehavior(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetEntitySpeed(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetEntityFacing(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevEntityFieldToVar(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevDrawVarNumber(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevGetLocationToVars(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevGetPlayerPositionToVars(const fe::Config& p_config, std::vector<byte>& p_rom,
			word cpu_addr, word helper_get_player_block_pos_addr) const;
		word apply_AtlasDevVarBitOp(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevVarShift(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevClampVar(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfVarMask(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfPlayerFacing(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfPlayerClimbing(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfPlayerGrounded(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfPlayerAttacking(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfPlayerInvincible(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfPlayerDead(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfSelectedWeapon(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfSelectedMagic(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevWaitFrames(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevWaitForButtonPress(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfButtonHeld(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfButtonPressed(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetFacing(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetPlayerPosition(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevOpenWindow(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevShowIcon(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevCloseWindow(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevLayText(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevOpenWindowAtEntity(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevRestoreRect(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevShowItemName(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevShowIconEx(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevClearText(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevLayTextAt(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevClearTextLine(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevLayTextLine(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetHealth(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetMana(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevFullHeal(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevFullMana(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfHealthBelow(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfHealthAtLeast(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfManaAtLeast(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevAddExperience(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetExperience(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetGold(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfGoldAtLeast(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfXPAtLeast(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfItemCount(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetPalette(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevRestorePalette(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevLoadBgPalette(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevLoadSpritePalette(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevFlashScreen(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetColorEmphasis(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevQueuePaletteFlush(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevWipeScreenStep(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevAnimateTiles(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetTextColor(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetAttrRect(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevPlaceChrTile(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevWarpToDoor(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevWarpAreaScreenPos(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSpawnEntity(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevDissolveEntity(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetEntityPosition(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetEntityScript(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetEntityBScript(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfEntityInRange(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevDropItem(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevDespawnEntity(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevDespawnAllEntities(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetMetatile(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetScreenEvent(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevApplyEffect(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevCastSpell(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevIfMagicActive(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevClearVisibleMagic(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSpawnMagicAt(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevCastSpellFromEntity(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetMagicPosition(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevSetMagicFacing(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevArmRole(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_AtlasDevDayNight(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		void emit_arm_role_tail(const fe::Config& p_config, klib::Asm6502& p_code) const;

		// shared helpers for the script action library
		word apply_helper_DecodeScriptFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
			word cpu_addr) const;
		word apply_helper_DecodeQuestFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
			word cpu_addr) const;
		word apply_helper_IfAEquals(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_helper_IfABetween(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_helper_GetPlayerBlockPos(std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_helper_LoadWord(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;

		bool requires_any(const std::vector<HackLib>& p_lib, const std::set<HackLib>& p_required) const;

		// other hacks
		word install_hack_clear_flag_memory(const fe::Config& p_config, std::vector<byte>& p_rom) const;
		void install_static_hack_flags_to_sram(const fe::Config& p_config, std::vector<byte>& p_rom) const;

		// tilemap change hacks
		word install_hack_tm_flag_helper(std::vector<byte>& p_rom, byte p_bank, word p_cpu_addr,
			word p_table_addr) const;
		word install_hack_tm_tilemap_changer(const fe::Config& p_config, std::vector<byte>& p_rom, byte p_bank, word p_cpu_addr) const;
		word install_hack_tm_descriptor_handler(std::vector<byte>& p_rom, byte p_bank, word p_cpu_addr,
			word flag_helper_cpu_addr, word tm_changer_cpu_addr) const;
		word install_hack_tm_lookup(std::vector<byte>& p_rom, byte p_bank, word p_cpu_addr,
			word descriptor_handler_cpu_addr, word data_table_start_cpu_addr) const;

		word install_hack_tm_event_handler(const fe::Config& p_config, std::vector<byte>& p_rom,
			byte tm_lookup_bank, word tm_lookup_cpu_addr) const;
		word install_script_variable_reset(const fe::Config& p_config, std::vector<byte>& p_rom,
			word cpu_addr, word end_handler_addr) const;

		// general hack library implementations
		// bank 15 general hacks
		word install_KillSwitch(const fe::Config& p_config, std::vector<byte>& p_rom, byte p_bank, word cpu_addr) const;
		word install_SameWorldTransPal2Mus(const fe::Config& p_config, std::vector<byte>& p_rom, byte p_bank, word cpu_addr,
			bool p_stage_door_hack_installed = true) const;
		// bank 14 general hacks
		word install_FastStart(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr,
			const fh::GeneralHack& p_hack) const;
		word install_QuestFlagItemDrops(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr,
			const fh::GeneralHack& p_hack) const;
		word install_BossLockedItems(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr,
			const fh::GeneralHack& p_hack) const;
		// bank 15
		word install_AtlasDevFrameScheduler(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr,
			const fh::GeneralHack&) const;
		word install_AtlasDevDayNightCycle(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr,
			const fh::GeneralHack&) const;
		// bank 12 general hacks
		word install_FlexibleItems(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr,
			const fh::GeneralHack& p_hack) const;
		word install_FogRules(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr,
			const fh::GeneralHack& p_hack) const;

		// util
		word get_next_cpu_addr(word cpu_addr, std::size_t hack_size, std::size_t max_addr = 0xc000) const;
		static word cfg_word(const fe::Config& p_config, const std::string& p_id);
		static byte cfg_byte(const fe::Config& p_config, const std::string& p_id);
		std::vector<word> read_script_opcode_addrs(const std::vector<byte>& p_rom, std::size_t p_opcode_count) const;
		std::size_t write_script_opcode_table(std::vector<byte>& p_rom, word cpu_addr,
			const std::vector<word>& p_jump_table) const;
		std::vector<word> read_screen_event_handler_addrs(const fe::Config& p_config, const std::vector<byte>& p_rom) const;
		std::size_t detect_screen_event_handler_count(const fe::Config& p_config, const std::vector<byte>& p_rom) const;

	public:
		HackManager(void) = default;

		static void install_hack_sameworld_to_stage_doors(const fe::Config& p_config, std::vector<byte>& p_rom);
		static void install_hack_double_tileset(const fe::Config& p_config, std::vector<byte>& p_rom);
		std::size_t apply_tilemap_change_subsystem(const fe::Config& p_config, std::vector<byte>& p_rom,
			const fh::TilemapChanges& tm_changes) const;
		std::size_t apply_script_library(const fe::Config& p_config, std::vector<byte>& p_rom,
			std::size_t p_file_offset, const std::vector<HackLib>& p_lib, std::size_t p_base_opcode_count) const;
		std::size_t install_general_hacks(const fe::Config& p_config, std::vector<byte>& p_rom, byte p_bank,
			std::size_t p_cpu_addr_start, std::size_t p_cpu_addr_end, const std::vector<GeneralHack>& p_hacks,
			const fe::Game* p_game = nullptr) const;
		static void install_hack_surom_expansion(const fe::Config& p_config, std::vector<byte>& p_rom);
	};

}

namespace magic_enum::customize {

	template<>
	struct enum_range<fh::HackLib> {
		static constexpr int min = 0;
		static constexpr int max = static_cast<int>(fh::HackLib::Count) - 1;
	};

}

#endif
