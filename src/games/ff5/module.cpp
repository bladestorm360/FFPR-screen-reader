#include "../../core/game.h"
#include "../../core/hooks.h"

// FINAL FANTASY V (FF5): the game module.
//
// configure() sets this game's flags and switches off core hooks that are folded or inlined
// here. The game's own screens (Serial.FF5 classes and game-only Last.* classes) are separate source
// files in this folder; each registers itself with a core::Install whose game mask is
// game::kFF5, so nothing in this file needs to list them:
//   job_ability.cpp (jobs, abilities, FF5 magic, the details key), status_details.cpp,
//   naming.cpp, timers.cpp, ff5_mod.cpp (9, Shift+;, LandingPings / EnemyLetters),
//   battle_target.cpp ("All allies" / "All enemies"), battle_results.cpp (the L results log),
//   ff5_common.cpp (MenuFocusAnnouncer, CharacterStatusHelper).
// No core hook is switched off for FF5: every FF5 screen that replaces a core reader does so by
// re-defining the core's actions/queries (status navigation) or by hooking FF5-only classes.
namespace {

void configure(game::Flags& f)
{
    f.has_mp = true;
    f.has_vehicle_pads = true;
    f.has_hidden_passages = true;
    f.atb = true;
    f.has_jobs = true;
    f.title_idle_cinematic = true;
    f.timer_freeze = true;
    f.bestiary_steal = true;       // the bestiary lists stealable items (Steal ability)
}

} // namespace

extern const game::Module kFF5Module = {
    game::Id::FF5,
    "FINAL FANTASY V",
    "FFV_ScreenReader",
    "FFV_ScreenReader.dll",
    &configure,
    nullptr,
};
