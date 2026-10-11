// Ports FF1 Patches/GameStatePatches.cs: the main-game state machine (SubSceneManagerMainGame) for
// the field-toggle gate (IsFieldPlayerState) and the config-menu bestiary dispatch, and the
// library-return fade-in callback (config re-announce after the bestiary).
//
// FF1 learns battle start/end and the title screen elsewhere (BattlePatches / BattleStartPatches
// and the "Title" scene load), so this file fires no events.
#include "core.h"
#include "hooks.h"
#include "log.h"
#include "refs.h"
#include "field/scene_events.h"
#include "system/system.h"
#include "battle/battle.h"

namespace ffpr::system {
namespace {

ref::Class kMainGame{"Last.Management", "SubSceneManagerMainGame"};
ref::Method mChangeState{kMainGame, "ChangeState", 1};
ref::Method mGetCurrentState{kMainGame, "GetCurrentState", 0};

// FieldMap.<InitMenu>b__N_0: the fade-in completion callback FieldMap.InitMenu builds only in its
// library-return branch (FF1 found it by name pattern; the compiler's number differs per game).
// FF6: b__99_0, the only InitMenu lambda in its dump.cs (RVA 0x3564F0, unshared); FF6's InitMenu
// has FF1's call sequence (FadeManager.FadeIn, then the delegate built for its callback).
ref::Class kFieldMap1{"", "FieldMap", game::kFF1};
ref::Class kFieldMap23{"", "FieldMap", game::kFF2 | game::kFF3};
ref::Class kFieldMap4{"", "FieldMap", game::kFF4};
ref::Class kFieldMap5{"", "FieldMap", game::kFF5};
ref::Class kFieldMap6{"", "FieldMap", game::kFF6};
ref::Method mLibraryReturn1{kFieldMap1, "<InitMenu>b__92_0", 0};
ref::Method mLibraryReturn23{kFieldMap23, "<InitMenu>b__90_0", 0};
ref::Method mLibraryReturn4{kFieldMap4, "<InitMenu>b__88_0", 0};
ref::Method mLibraryReturn5{kFieldMap5, "<InitMenu>b__89_0", 0};
ref::Method mLibraryReturn6{kFieldMap6, "<InitMenu>b__99_0", 0};

// SubSceneManagerMainGame.State: the field states are the same in all six games.
constexpr int kStateChangeMap = 1;
constexpr int kStateFieldReady = 2;
constexpr int kStatePlayer = 3;
constexpr int kStateTransport = 4;   // the last field state; Menu is 5, Shop 9, Battle 13

// MenuLibraryUi (list) / MenuLibraryInfo (detail): FF1 19/20, FF2 18/19, FF3-FF6 17/18.
int state_library_ui()
{
    switch (game::current()) {
    case game::Id::FF1: return 19;
    case game::Id::FF2: return 18;
    default: return 17;
    }
}
int state_library_info() { return state_library_ui() + 1; }

// The main-game manager, cached from ChangeState. A plain managed object (not a Unity object):
// re-checked by class before each use, and dropped on the title (FF1 ResetMainGameState).
void* g_main_game = nullptr;

void run_action(const char* name)
{
    if (actions::defined(name)) actions::run(name);
}

void config_bestiary_exit()
{
    run_action("ConfigBestiaryStateHandler.HandleExit");
    // Returning from the bestiary lands back on the config menu, which resumes after the
    // bestiary's loading screen without re-firing SelectCommand. Arm the config re-announce; the
    // library-return fade-in callback reads the row (it never runs if the exit went to the field).
    run_action("ConfigController_SetActive_Patch.ArmReannounceAfterLibrary");
}

// ChangeState_Postfix
void change_state_postfix(void* self, int state)
{
    if (self) g_main_game = self;

    const bool was_in_bestiary = queries::is("ConfigBestiaryStateHandler.WasInConfigBestiary");
    if (state == kStateChangeMap || state == kStateFieldReady || state == kStatePlayer) {
        if (was_in_bestiary) config_bestiary_exit();
    } else if (state == state_library_ui()) {
        run_action("ConfigBestiaryStateHandler.HandleStateChange.MenuLibraryUi");
    } else if (state == state_library_info()) {
        run_action("ConfigBestiaryStateHandler.HandleStateChange.MenuLibraryInfo");
    } else if (was_in_bestiary) {
        config_bestiary_exit();
    }
}

using ChangeState_t = void (*)(void* self, int32_t state, const void* mi);
ChangeState_t o_ChangeState;

void d_ChangeState(void* self, int32_t state, const void* mi)
{
    hooks::guarded("GameState.BattleTransition", [&] {
        if (self && kMainGame.is(self)) battle::main_game_transition(state);
    });
    o_ChangeState(self, state, mi);
    hooks::guarded("GameState.ChangeState", [&] {
        if (self && kMainGame.is(self)) change_state_postfix(self, state);
    });
}

// MenuResumedAfterLibrary_Postfix: the config menu is back on screen after the bestiary.
using Callback_t = void (*)(void* self, const void* mi);
Callback_t o_LibraryReturn;

void d_LibraryReturn(void* self, const void* mi)
{
    o_LibraryReturn(self, mi);
    hooks::guarded("GameState.MenuResumedAfterLibrary", [] {
        run_action("ConfigController_SetActive_Patch.OnMenuResumedAfterLibrary");
    });
}

ref::Method& library_return_method()
{
    switch (game::current()) {
    case game::Id::FF1: return mLibraryReturn1;
    case game::Id::FF4: return mLibraryReturn4;
    case game::Id::FF5: return mLibraryReturn5;
    case game::Id::FF6: return mLibraryReturn6;
    default: return mLibraryReturn23;
    }
}

int install()
{
    int n = 0;
    if (hooks::attach(mChangeState, &d_ChangeState, &o_ChangeState)) {
        ++n;
        LOGF("[GameState] ChangeState patch applied");
    } else {
        LOGF("[GameState] Could not find SubSceneManagerMainGame.ChangeState method");
    }
    if (hooks::attach(library_return_method(), &d_LibraryReturn, &o_LibraryReturn)) ++n;
    else LOGF("[GameState] FieldMap InitMenu fade-in callback not found - no config re-announce after the bestiary");

    queries::define("GameStatePatches.IsFieldPlayerState", [] { return is_field_player_state(); });
    queries::define("GameStatePatches.IsFieldState", [] { return is_field_state(); });
    actions::define("GameStatePatches.ResetMainGameState", [] { reset_main_game_state(); });
    // FF1 FFI_ScreenReaderMod.OnSceneLoaded calls ResetMainGameState on the "Title" scene load:
    // the next MainGame instance reports its own states.
    events::on(events::Kind::SceneLoaded, [] {
        if (field::scene_name() == L"Title") reset_main_game_state();
    });
    return n;
}

core::Install s_install{"GameState", &install, 100};

} // namespace

// IsFieldPlayerState: the main game is in its Player state (on the field, player in control),
// the only state whose FieldMap update handles the field toggle keys.
bool is_field_player_state()
{
    return hooks::guarded_r("GameState.IsFieldPlayerState", false, [] {
        void* mg = g_main_game;
        if (!mg || !kMainGame.is(mg)) return false;
        return ref::call<int32_t>(mGetCurrentState, mg) == kStatePlayer;
    });
}

// IsFieldState: the main game is in one of its field states (Init, ChangeMap, FieldReady, Player,
// Transport: 0-4 in all six dump.cs files), or no state is known yet. No menu, shop or battle
// screen can have focus then, so a menu reader's deferred read is a stray.
//
// Why it exists (FF2, 2026-10-10): on the first field after a load, the game pre-loads every UI
// scene (CampMenu, SecondMenu, Battle, ShopMenu, ...) and initialises their controllers while
// their objects are still active. The item window came up in its command-select state with its
// cursor set, the command-bar reader's one-frame read found an active window in state 1, and
// "Use" was spoken on the world map. The window's own state machine cannot tell this
// initialisation apart from a real open; the main-game state can.
bool is_field_state()
{
    return hooks::guarded_r("GameState.IsFieldState", true, [] {
        void* mg = g_main_game;
        if (!mg || !kMainGame.is(mg)) return true;
        const int32_t state = ref::call<int32_t>(mGetCurrentState, mg);
        return state >= 0 && state <= kStateTransport;
    });
}

void reset_main_game_state() { g_main_game = nullptr; }

} // namespace ffpr::system
