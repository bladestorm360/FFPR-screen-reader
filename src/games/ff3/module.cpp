#include "../../core/game.h"
#include "../../core/hooks.h"

// FINAL FANTASY III (FF3): the game module.
//
// configure() sets this game's flags and switches off core hooks that are folded or inlined
// here. The game's own screens (Serial.FF3 classes and game-only Last.* classes) are separate source
// files in this folder; each registers itself with a core::Install whose game mask is
// game::kFF3, so nothing in this file needs to list them.
namespace {

void configure(game::Flags& f)
{
    f.has_hidden_passages = true;
    f.multi_hit_setting = true;
    f.has_jobs = true;
}

} // namespace

extern const game::Module kFF3Module = {
    game::Id::FF3,
    "FINAL FANTASY III",
    "FFIII_ScreenReader",
    "FFIII_ScreenReader.dll",
    &configure,
    nullptr,
};
