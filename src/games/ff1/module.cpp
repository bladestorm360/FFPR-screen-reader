#include "../../core/game.h"
#include "../../core/hooks.h"

// FINAL FANTASY (FF1): the game module.
//
// configure() sets this game's flags and switches off core hooks that are folded or inlined
// here. The game's own screens (Serial.FF1 classes and game-only Last.* classes) are separate source
// files in this folder; each registers itself with a core::Install whose game mask is
// game::kFF1, so nothing in this file needs to list them.
namespace {

void configure(game::Flags& f)
{
    f.has_airship = true;
}

} // namespace

extern const game::Module kFF1Module = {
    game::Id::FF1,
    "FINAL FANTASY",
    "FFI_ScreenReader",
    "FFI_ScreenReader.dll",
    &configure,
    nullptr,
};
