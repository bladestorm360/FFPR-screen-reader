// Ports the FF5-only parts of FF5 Core/FFV_ScreenReaderMod.cs, Core/InputManager.cs and
// Core/AudioLoopManager.cs that the core (FF1's) does not have:
//   * the 9 key for beacons (FF3-FF5; F6 is the core's),
//   * Shift+; and the "LandingPings" setting (AudioLoopManager.ToggleLandingPings),
//   * the "EnemyLetters" setting toggle (FFV_ScreenReaderMod.ToggleEnemyLetters).
//
// Settings the core mod menu does not list yet: LandingPings (bool, off), LandingPingVolume
// (int 0-100, 50; read by the core's sound::play_landing_ping through Volume::LandingPing) and
// EnemyLetters (bool, off; read by FF5's battle target reader). The mod-menu rows are a core gap.
//
// NOT ported here: the landing-ping loop itself. It is an audio loop over FF5's terrain landing
// test (FieldNavigationHelper.GetNearbyLandingSpots), a navigation addition planned for the core;
// "Settings.Changed.LandingPings" is run after the toggle so that loop can start/stop when it
// exists.
#include <string>
#include "ff5_internal.h"
#include "../../core/core.h"
#include "../../core/modtext.h"
#include "../../core/say.h"
#include "../../core/settings.h"
#include "../../core/input/input.h"

namespace ffpr::ff5 {
namespace {

void run_if_defined(const char* name)
{
    if (actions::defined(name)) actions::run(name);
}

/// AudioLoopManager.ToggleLandingPings.
void toggle_landing_pings()
{
    bool value = settings::toggle("LandingPings", false);
    run_if_defined("Settings.Changed.LandingPings");
    ffpr::say(TF(L"Landing Pings {0}", {value ? T(L"on") : T(L"off")}));
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
    actions::define("Mod.ToggleLandingPings", [] { toggle_landing_pings(); });
    actions::define("Mod.ToggleEnemyLetters", [] { toggle_enemy_letters(); });
    queries::define("PreferencesManager.LandingPingsEnabled", [] { return settings::get_bool("LandingPings", false); });
    queries::define("PreferencesManager.EnemyLettersEnabled", [] { return settings::get_bool("EnemyLetters", false); });

    input::add_binding(input::Key::Alpha9, input::Mod::None, input::Context::Field, "Mod.ToggleAudioBeacons",
                       "Toggle audio beacons (alt)");
    input::add_binding(input::Key::Semicolon, input::Mod::Shift, input::Context::Field, "Mod.ToggleLandingPings",
                       "Toggle landing pings");
    return 0;
}

core::Install s_install{"FF5.Mod", &install, 506, game::kFF5};

} // namespace
} // namespace ffpr::ff5
