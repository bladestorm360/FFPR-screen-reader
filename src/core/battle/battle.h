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
/// Shared renderer-backed H/T/number-row readers, including targeting panels.
void announce_active_status();
void announce_active_turn();
std::wstring displayed_player(void* player);
// Native target controller's serialized party panel, including independently rooted views.
void observe_target_party(void* owner);
/// FF6 redraws a single target every update; native whole-side entry retires that focus.
void reset_target_focus();
/// Native action lifecycle; stores an already identified actor name and party identity.
void action_actor_started(void* unit);
void action_actor_finished();
/// Called at the native main-game transition, before its field/battle dispatch.
void main_game_transition(int state);

} // namespace ffpr::battle
