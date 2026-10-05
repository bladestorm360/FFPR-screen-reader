#include "../../core/game.h"
#include "../../core/hooks.h"

// FINAL FANTASY IV (FF4): the game module.
//
// configure() sets this game's flags and switches off core hooks that are folded or inlined
// here. The game's own screens (Serial.FF4 classes and game-only Last.* classes) are separate source
// files in this folder; each registers itself with a core::Install whose game mask is
// game::kFF4, so nothing in this file needs to list them.
namespace {

void configure(game::Flags& f)
{
    f.has_airship = true;
    f.has_vehicle_pads = true;
    f.has_hidden_passages = true;
    f.multi_hit_setting = false;
    f.atb = true;
}

} // namespace

extern const game::Module kFF4Module = {
    game::Id::FF4,
    "FINAL FANTASY IV",
    "FFIV_ScreenReader",
    "FFIV_ScreenReader.dll",
    &configure,
    nullptr,
};
