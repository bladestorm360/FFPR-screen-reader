#pragma once
#include <string>
#include <vector>

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

/// Victory-screen lines a game module adds to the core's ResultMenuController.ShowPointsInit
/// announcement (FF6: espers' Magic AP and the spells learned). Called on the main thread inside
/// that hook, once per result screen; each returned line is spoken queued. after_gil gets the
/// BattleResultData and runs right after "Gained N gil"; after_character gets one
/// BattleResultData.BattleResultCharacterData and runs after that character's EXP line (also
/// when the character gained no EXP). Null members add nothing (FF1-FF5 register none).
struct ResultExtras {
    std::vector<std::wstring> (*after_gil)(void* result_data) = nullptr;
    std::vector<std::wstring> (*after_character)(void* character_result) = nullptr;
};
void set_result_extras(const ResultExtras& extras);

} // namespace ffpr::battle
