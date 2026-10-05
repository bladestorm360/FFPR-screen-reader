#pragma once

// Cross-file calls inside src/core/battle (the C# files call each other's statics). Not for other
// areas: they use battle.h, the named queries/actions, or menu_state.
namespace ffpr::battle {

// battle_state.cpp (FF1 Patches/BattlePatches.cs) ----------------------------------------------
/// FF1 MenuStateRegistry.SetActive semantics: setting a state inactive also runs its reset
/// handler (BattleCommandState.OnReset clears LastSelectedCommandIndex and CurrentActor).
void state_set(const char* key, bool active);
bool state_active(const char* key);
/// BattleTargetState.SetTargetSelectionActive.
void set_target_selection_active(bool active);
/// BattleCommandState.LastSelectedCommandIndex / CurrentActor.
int last_selected_command_index();
void set_last_selected_command_index(int index);
void set_current_actor(void* owned_character);
/// FFI_ScreenReaderMod.ClearOtherMenuStates("BattleItem" / "BattleMagic").
void clear_other_menu_states(const char* except_key);
/// BattleStateHelper.OnBattleStart / TryClearOnBattleEnd / ClearAllBattleMenuFlags.
void on_battle_start();
void try_clear_on_battle_end();
void clear_all_battle_menu_flags();
bool is_in_battle();

// battle_command.cpp (FF1 Patches/BattleCommandPatches.cs) -------------------------------------
void notify_command_submenu_active();
void command_reset_state();
void clear_cached_target_controller();

// battle_magic.cpp (FF1 Patches/BattleMagicPatches.cs) -----------------------------------------
void magic_reset_state();

// battle_result.cpp (FF1 Patches/BattleResultPatches.cs) ---------------------------------------
void result_reset_state();
void result_clear_all_battle_states();

// battle_message.cpp (FF1 Patches/BattleMessagePatches.cs) -------------------------------------
/// BattleConditionRemovalPatches.ResetState (and the FF2-FF5 removal variants' latches).
void removal_reset_state();

// battle_pause.cpp (FF1 Patches/BattlePausePatches.cs) -----------------------------------------
void pause_reset();

} // namespace ffpr::battle
