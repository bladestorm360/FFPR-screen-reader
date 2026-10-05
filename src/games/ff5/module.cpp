#include "../../core/game.h"
#include "../../core/hooks.h"

// FINAL FANTASY V (FF5): the game module.
//
// configure() sets this game's flags and switches off core hooks that are folded or inlined
// here. The game's own screens (Serial.FF5 classes and game-only Last.* classes) are separate source
// files in this folder; each registers itself with a core::Install whose game mask is
// game::kFF5, so nothing in this file needs to list them.
namespace {

void configure(game::Flags& f)
{
    f.has_vehicle_pads = true;
    f.has_hidden_passages = true;
    f.multi_hit_setting = false;
    f.atb = true;
    f.has_jobs = true;
    f.timer_freeze = true;
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
