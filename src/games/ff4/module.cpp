#include "../../core/core.h"
#include "../../core/game.h"
#include "../../core/hooks.h"
#include "../../core/input/input.h"

// FINAL FANTASY IV (FF4): the game module.
//
// configure() sets this game's flags and switches off core hooks that are folded or inlined
// here. The game's own screens (Serial.FF4 classes and game-only Last.* classes) are separate source
// files in this folder; each registers itself with a core::Install whose game mask is
// game::kFF4, so nothing in this file needs to list them:
//   ability_menu.cpp      field Ability menu (FF4's magic lists), Phase 1 proof hook
//   status_details.cpp    status details screen and FF4's 17-stat navigation
//   party_setting.cpp     party setting screen and formation rows
//   naming.cpp            naming screen
//   timers.cpp            T / Shift+T escape timers
//   battle_abilities.cpp  battle spell / ability list and its I-key description
//   battle_targets.cpp    "All allies" / "All enemies"
//   vehicles.cpp          FF4's vehicle words (boarding speech, V key)
//   character_row.cpp     front/back row in character-slot reads
//   ff4.h, ff4_util.cpp   shared helpers (MenuState keys, messages, character status)
namespace {

void configure(game::Flags& f)
{
    f.has_mp = true;
    f.has_airship = true;
    f.has_vehicle_pads = true;
    f.has_hidden_passages = true;
    f.multi_hit_setting = false;
    f.atb = true;
    f.timer_freeze = true;   // FF4 had Shift+T too (timers.cpp)

    // The core's boarding speech names FF1's vehicles (ship, canoe): FF4's hovercraft and
    // chocobos need their own words. vehicles.cpp replaces both subscribers (attached by address).
    hooks::disable("FieldPlayer.GetOn");
    hooks::disable("FieldPlayer.GetOff");
}

// FF4's own key bindings beyond the core table (docs/INPUT_BINDINGS.md): 9 toggles the audio
// beacons on the field, like F6 (FF4 InputManager). T / Shift+T are bound by timers.cpp.
int install_bindings()
{
    input::add_binding(input::Key::Alpha9, input::Mod::None, input::Context::Field, "Mod.ToggleAudioBeacons",
                       "Toggle audio beacons");
    return 0;
}

core::Install s_bindings{"FF4.Bindings", &install_bindings, 500, game::kFF4};

} // namespace

extern const game::Module kFF4Module = {
    game::Id::FF4,
    "FINAL FANTASY IV",
    "FFIV_ScreenReader",
    "FFIV_ScreenReader.dll",
    &configure,
    nullptr,
};
