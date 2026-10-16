// Ports FF1 Patches/BattlePatches.cs: BattleCommandState, BattleTargetState, BattleItemMenuState,
// BattleMagicMenuState, BattleStateHelper, BattleControllerPatches and MainMenuControllerPatches.
// Also FFI_ScreenReaderMod.ClearOtherMenuStates / ManualPatches.ClearAllMenuStates as the battle
// readers call them.
//
// All names exist in all five games (BattleController fade-out callbacks, Exit(bool),
// KeyInput.MainMenuController.Show(bool)/Close(), Behaviour.get_isActiveAndEnabled); every RVA is
// unique in every dump.cs.
#include <string>
#include "battle.h"
#include "battle_internal.h"
#include "../core.h"
#include "../hooks.h"
#include "../log.h"
#include "../unity.h"

namespace ffpr::battle {
namespace {

// BattleStateHelper.IsInBattle: set on battle start, cleared on battle end.
bool g_in_battle = false;
bool g_battle_ready = false;
int g_main_state = -1;
// BattleCommandState.LastSelectedCommandIndex (0 Attack, 1 Magic, 2 Items, 3 Defend/Run).
int g_last_selected_command_index = -1;
// BattleCommandState.CurrentActor (OwnedCharacterData). Cleared with the BATTLE_COMMAND state.
void* g_current_actor = nullptr;

ref::Class kBattleController{"Last.Battle", "BattleController"};
ref::Method mEndWinFadeOutCallback{kBattleController, "EndWinFadeOutCallback", 0};
ref::Method mEndLoseFadeOutCallback{kBattleController, "EndLoseFadeOutCallback", 0};
ref::Method mEndEscapeFadeOut{kBattleController, "EndEscapeFadeOut", 0};
ref::Method mEndFadeOutCallback{kBattleController, "EndFadeOutCallback", 0};
ref::Method mExit{kBattleController, "Exit", 1};

ref::Class kBehaviour{"UnityEngine", "Behaviour"};
ref::Method mIsActiveAndEnabled{kBehaviour, "get_isActiveAndEnabled", 0};

ref::Class kMainMenuController{"Last.UI.KeyInput", "MainMenuController"};
ref::Method mMainMenuShow{kMainMenuController, "Show", 1};
ref::Method mMainMenuClose{kMainMenuController, "Close", 0};

/// BattleStateHelper.IsBattleControllerAlive: an active BattleController in the scene. On-demand
/// scene scan, for key handlers only.
bool is_battle_controller_alive()
{
    // The native transition owns combat/results state. A key handler must not
    // scan the scene or wait for a controller while loading or returning to field.
    return g_main_state == 13 || (g_main_state < 0 && g_in_battle);
}

/// BattleStateHelper.ForceClearBattleState: the Tab key's fallback, only after
/// IsBattleControllerAlive() found no live battle.
void force_clear_battle_state()
{
    if (g_in_battle) {
        g_in_battle = false;
        g_battle_ready = false;
        clear_all_battle_menu_flags();
        events::fire(events::Kind::BattleEnd);
    }
}

/// ManualPatches.ClearAllMenuStates.
void clear_all_menu_states()
{
    menu_state::reset_all();
    if (actions::defined("ShopMenuTracker.ClearState")) actions::run("ShopMenuTracker.ClearState");
    pause_reset();
}

// --- BattleControllerPatches: fade-out callbacks (postfix) and Exit (prefix) ---------------------
using Void_t = void (*)(void* self, const void* mi);
Void_t o_fade[4];

template <int N>
void d_fade(void* self, const void* mi)
{
    o_fade[N](self, mi);
    hooks::guarded("BattleController.FadeOut", [&] {
        if (!self) return;   // guard against null instances during initialization
        try_clear_on_battle_end();
    });
}

using Exit_t = void (*)(void* self, bool is_unload_asset, const void* mi);
Exit_t o_Exit;

void d_Exit(void* self, bool is_unload_asset, const void* mi)
{
    hooks::guarded("BattleController.Exit", [&] {
        if (!self) return;
        try_clear_on_battle_end();
    });
    o_Exit(self, is_unload_asset, mi);
}

// --- MainMenuControllerPatches ------------------------------------------------------------------
using Show_t = void (*)(void* self, bool is_save_point, const void* mi);
Show_t o_Show;

void d_Show(void* self, bool is_save_point, const void* mi)
{
    o_Show(self, is_save_point, mi);
    hooks::guarded("MainMenuController.Show", [&] {
        // Clear sub-menu states when returning to the main menu command bar.
        clear_all_menu_states();
        // Mark the main menu active so the controller router knows a menu is open.
        menu_state::set_active(menu_state::MAIN_MENU, true);
        // When the main menu opens after a battle, also clear the battle states.
        if (g_in_battle) {
            try_clear_on_battle_end();
            clear_cached_target_controller();
        }
    });
}

Void_t o_Close;

void d_Close(void* self, const void* mi)
{
    o_Close(self, mi);
    hooks::guarded("MainMenuController.Close", [&] { state_set(menu_state::MAIN_MENU, false); });
}

int install()
{
    // BattleCommandState's reset handler (MenuStateBase.OnReset override).
    menu_state::register_reset(menu_state::BATTLE_COMMAND, [] {
        g_last_selected_command_index = -1;
    });

    queries::define("BattleStateHelper.IsInBattle", [] { return g_in_battle; });
    queries::define("BattleStateHelper.IsBattleControllerAlive", [] { return is_battle_controller_alive(); });
    queries::define("BattleCommandState.IsActive", [] { return menu_state::is_active(menu_state::BATTLE_COMMAND); });
    queries::define("BattleTargetState.IsTargetSelectionActive", [] { return menu_state::is_active(menu_state::BATTLE_TARGET); });
    queries::define("BattleItemMenuState.IsActive", [] { return menu_state::is_active(menu_state::BATTLE_ITEM); });
    queries::define("BattleMagicMenuState.IsActive", [] { return menu_state::is_active(menu_state::BATTLE_MAGIC); });
    actions::define("BattleStateHelper.ForceClearBattleState", [] { force_clear_battle_state(); });
    actions::define("BattleStateHelper.TryClearOnBattleEnd", [] { try_clear_on_battle_end(); });
    actions::define("BattleStateHelper.ClearAllBattleMenuFlags", [] { clear_all_battle_menu_flags(); });

    int n = 0;
    // OnDestroy is NOT hooked: it crashed during asset loading in the C# mod.
    n += hooks::attach(mEndWinFadeOutCallback, &d_fade<0>, &o_fade[0]) ? 1 : 0;
    n += hooks::attach(mEndLoseFadeOutCallback, &d_fade<1>, &o_fade[1]) ? 1 : 0;
    n += hooks::attach(mEndEscapeFadeOut, &d_fade<2>, &o_fade[2]) ? 1 : 0;
    n += hooks::attach(mEndFadeOutCallback, &d_fade<3>, &o_fade[3]) ? 1 : 0;
    n += hooks::attach(mExit, &d_Exit, &o_Exit) ? 1 : 0;
    n += hooks::attach(mMainMenuShow, &d_Show, &o_Show) ? 1 : 0;
    n += hooks::attach(mMainMenuClose, &d_Close, &o_Close) ? 1 : 0;
    return n;
}
core::Install s_install{"Battle.State", &install, 110};

} // namespace

// --- menu state facades -------------------------------------------------------------------------

void state_set(const char* key, bool active)
{
    // FF1's MenuStateRegistry.SetActive(key, false) runs the key's reset handler; the core's
    // menu_state::reset is that operation.
    if (active) menu_state::set_active(key, true);
    else menu_state::reset(key);
}

bool state_active(const char* key) { return menu_state::is_active(key); }

void set_target_selection_active(bool active)
{
    state_set(menu_state::BATTLE_TARGET, active);
    if (active) {
        // Clear the magic/item menu states when entering target selection: leftover data lists
        // would otherwise produce "Empty" announcements.
        state_set(menu_state::BATTLE_MAGIC, false);
        state_set(menu_state::BATTLE_ITEM, false);
    }
}

int last_selected_command_index() { return g_last_selected_command_index; }
void set_last_selected_command_index(int index) { g_last_selected_command_index = index; }
void set_current_actor(void* owned_character) { g_current_actor = owned_character; }

void clear_other_menu_states(const char* except_key)
{
    // Reset every registry state except the one being activated AND keep MAIN_MENU active: it is
    // the parent container for all sub-menus.
    std::string except = except_key ? except_key : "";
    for (const std::string& key : menu_state::active_states()) {
        if (key != except && key != menu_state::MAIN_MENU) state_set(key.c_str(), false);
    }
    // Shop is not in the registry: clear it separately.
    if (actions::defined("ShopMenuTracker.ClearState")) actions::run("ShopMenuTracker.ClearState");
}

void on_battle_start()
{
    if (g_battle_ready) return;
    if (!g_in_battle) {
        g_in_battle = true;
        events::fire(events::Kind::BattleEntering);
    }
    g_battle_ready = true;
    timing::Scope ready("Battle ready cleanup");
    g_current_actor = nullptr;
    { timing::Scope reset("Battle result reset",timing::Kind::Mod,nullptr,0,true); result_reset_state(); }
    { timing::Scope reset("Battle removal reset"); removal_reset_state(); }
    events::fire(events::Kind::BattleStart);
}

void clear_all_battle_menu_flags()
{
    state_set(menu_state::BATTLE_COMMAND, false);
    state_set(menu_state::BATTLE_TARGET, false);
    state_set(menu_state::BATTLE_ITEM, false);
    state_set(menu_state::BATTLE_MAGIC, false);
}

void try_clear_on_battle_end()
{
    if (g_main_state == 13) return; // Fade callbacks/results teardown precede native return.
    // Not in battle: skip (prevents loops during game initialization).
    if (!g_in_battle) return;
    g_in_battle = false;
    g_battle_ready = false;
    g_current_actor = nullptr;
    clear_all_battle_menu_flags();
    result_reset_state();   // BattleStartPatches.ResetState
    events::fire(events::Kind::BattleEnd);
}

bool is_in_battle() { return g_in_battle; }

void* current_actor() { return g_current_actor; }
bool in_battle() { return g_in_battle; }

void main_game_transition(int state)
{
    g_main_state = state;
    // This enum is identical in all six matching dumps. Battle covers the transition,
    // combat and results; return to a native field/title state ends the context.
    if (state == 13 && !g_in_battle) {
        // Only constant-time admission/cancellation here. Native battle loading
        // and its initial audio must run before resetting readers/route caches.
        g_in_battle = true;
        g_battle_ready = false;
        g_current_actor = nullptr;
        events::fire(events::Kind::BattleEntering);
    }
    else if ((state >= 0 && state <= 4) || state == 14) try_clear_on_battle_end();
}

} // namespace ffpr::battle
