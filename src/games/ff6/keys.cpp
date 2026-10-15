// Ports the FF6-only key of FF6 Core/InputManager.cs that the core's table
// (docs/INPUT_BINDINGS.md) does not cover. Every other FF6 field key is the core's, with the
// same action: J/K/L, [ ] \ P and their Shift/Ctrl forms, ' ; = - G M Shift+M, Shift+K, the
// waypoint keys , . / and Ctrl+arrows. T / Shift+F5 are bound by timers.cpp.
//
//   H (field) - airship status when flying, else the core's character status
//               (FFVI_ScreenReaderMod.AnnounceAirshipOrCharacterStatus, airship.cpp). In battle
//               and menus the core's Global H runs unchanged. A Field binding is tried before
//               the core's Global one (input_manager.cpp: the more specific context first).
//
// Not bound, so FF6 keeps FF1's keys (mod behaviour is identical in every game): 0 (FF6's
// second "reset to All category" key; the core's is Shift+K), F1 / F3 (FF6 read walk/run and
// encounters on the key; the core announces both when they change,
// input/game_toggle_announcer.cpp) and F5 (FF6 cycled the enemy HP display outside battle; the
// core's F5 works in battle).
#include "../../core/core.h"
#include "../../core/game.h"
#include "../../core/input/input.h"

namespace ffpr::ff6 {
namespace {

int install()
{
    input::add_binding(input::Key::H, input::Mod::None, input::Context::Field, "Mod.AnnounceAirshipOrCharacterStatus",
                       "Announce airship status or character status");
    return 0;
}

core::Install s_install{"FF6.Keys", &install, 520, game::kFF6};

} // namespace
} // namespace ffpr::ff6
