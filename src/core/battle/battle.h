#pragma once

// The battle area's public surface for other areas (src/core/battle).
//
// Most battle state is published by name instead (core.h): queries
// "BattleStateHelper.IsInBattle", "BattleStateHelper.IsBattleControllerAlive",
// "BattleItemMenuState.IsActive", "BattleMagicMenuState.IsActive", "BattleCommandState.IsActive",
// "BattleTargetState.IsTargetSelectionActive", "BattlePauseState.IsActive"; actions
// "BattleStateHelper.ForceClearBattleState", "BattleStateHelper.TryClearOnBattleEnd",
// "BattleCommandPatches.ClearCachedTargetController", "BattleResultPatches.ClearAllBattleMenuFlags",
// "BattlePausePatches.Reset", "BattleItemPatches.AnnounceCurrentDescription",
// "BattleMagicPatches.AnnounceCurrentDescription" (FF1). The menu_state keys BATTLE_* are set here.
namespace ffpr::battle {

/// FF1 BattleCommandState.CurrentActor: the OwnedCharacterData whose turn is being selected or
/// executed (set by BattleCommandSelectController.SetCommandData), or null. Its Parameter is the
/// live battle parameter. Used by "Mod.AnnounceCharacterStatus" (H key). Main thread.
void* current_actor();

/// FF1 BattleStateHelper.IsInBattle.
bool in_battle();

} // namespace ffpr::battle
