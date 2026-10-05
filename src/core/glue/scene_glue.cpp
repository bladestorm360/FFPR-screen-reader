// Ports the one part of FF1 Core/FFI_ScreenReaderMod.cs OnSceneLoaded that no area owns: the
// "left a battle scene" clear. The rest of OnSceneLoaded lives with its owners, each
// subscribed to events::Kind::SceneLoaded (fired by src/core/field after it records the scene
// names): title prompt (dialogue/title), object cache (field), ConfigMenuState.ResetState
// (status/config), controls help (system/key_help), audio loops and the delayed rescan
// (fieldnav/audio_loops), movement sounds (audio), main-game state (system/game_state),
// LocationMessageTracker.Reset (fieldnav/map_transition).
#include <string>
#include "../core.h"
#include "../log.h"
#include "../field/scene_events.h"

namespace {

void on_scene_loaded()
{
    const std::wstring& now = field::scene_name();
    const std::wstring& before = field::previous_scene_name();
    bool was_in_battle = before.find(L"Battle") != std::wstring::npos;
    bool is_in_battle = now.find(L"Battle") != std::wstring::npos;
    if (!was_in_battle || is_in_battle) return;
    LOGF("[Scene] Left battle scene - clearing all battle states");
    actions::run("BattleStateHelper.TryClearOnBattleEnd");
    actions::run("Mod.ClearAllMenuStates");
    actions::run("BattleCommandPatches.ClearCachedTargetController");
}

int install()
{
    events::on(events::Kind::SceneLoaded, [] { on_scene_loaded(); });
    return 0;
}

core::Install s_install{"SceneGlue", &install, 90};

} // namespace
