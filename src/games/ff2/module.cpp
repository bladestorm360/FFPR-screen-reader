#include "../../core/game.h"
#include "../../core/hooks.h"

// FINAL FANTASY II (FF2): the game module.
//
// configure() sets this game's flags and switches off core hooks that are folded or inlined
// here. The game's own screens (Serial.FF2 classes and game-only Last.* classes) are separate source
// files in this folder; each registers itself with a core::Install whose game mask is
// game::kFF2, so nothing in this file needs to list them.
namespace {

void configure(game::Flags& f)
{
    f.has_airship = true;
    f.has_vehicle_pads = true;
    f.multi_hit_setting = true;
    f.has_skill_levels = true;
}

} // namespace

extern const game::Module kFF2Module = {
    game::Id::FF2,
    "FINAL FANTASY II",
    "FFII_ScreenReader",
    "FFII_ScreenReader.dll",
    &configure,
    nullptr,
};
