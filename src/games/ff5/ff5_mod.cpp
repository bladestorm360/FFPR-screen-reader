// Ports the FF5-only parts of FF5 Core/FFV_ScreenReaderMod.cs, Core/InputManager.cs and
// Core/AudioLoopManager.cs that the core (FF1's) does not have:
//   * the "EnemyLetters" setting toggle (FFV_ScreenReaderMod.ToggleEnemyLetters).
//
// The landing-ping loop itself is in the core (src/core/fieldnav/landing_spots.cpp and
// audio_loops.cpp, every game with a ship), and the core mod menu lists LandingPings,
// LandingPingVolume and, for FF5, EnemyLetters. Landing pings use that shared settings menu;
// keyboard bindings follow FF1. EnemyLetters has no effect until FF5's own
// battle target reader is ported.
#include <string>
#include "ff5_internal.h"
#include "../../core/core.h"
#include "../../core/modtext.h"
#include "../../core/say.h"
#include "../../core/settings.h"

namespace ffpr::ff5 {
namespace {

void run_if_defined(const char* name)
{
    if (actions::defined(name)) actions::run(name);
}

/// FFV_ScreenReaderMod.ToggleEnemyLetters.
void toggle_enemy_letters()
{
    bool value = settings::toggle("EnemyLetters", false);
    run_if_defined("Settings.Changed.EnemyLetters");
    ffpr::say(value ? T(L"Enemy Letters on") : T(L"Enemy Letters off"));
}

int install()
{
    actions::define("Mod.ToggleEnemyLetters", [] { toggle_enemy_letters(); });
    queries::define("PreferencesManager.LandingPingsEnabled", [] { return settings::get_bool("LandingPings", false); });
    queries::define("PreferencesManager.EnemyLettersEnabled", [] { return settings::get_bool("EnemyLetters", false); });

    return 0;
}

core::Install s_install{"FF5.Mod", &install, 506, game::kFF5};

} // namespace
} // namespace ffpr::ff5
