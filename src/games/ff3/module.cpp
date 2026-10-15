#include "../../core/game.h"
#include "../../core/hooks.h"

// FINAL FANTASY III (FF3): the game module.
//
// configure() sets this game's flags and switches off core hooks that are folded or inlined
// here, or that FF3 replaces. The game's own screens (Serial.FF3 classes and game-only Last.*
// classes) are separate source files in this folder; each registers itself with a core::Install
// whose game mask is game::kFF3, so nothing in this file needs to list them.
//
// Keyboard bindings use the shared FF1 table, including R for dialogue and F6 for beacons.
namespace {

/// FF3's transportation master has no message_id for its canoe (id 2, type 5 "Content"); its
/// name is the game's key-item name (tools/mapdump/ff3_master, 2026-10-10).
const wchar_t* vehicle_message_id(int transportation_id)
{
    return transportation_id == 2 ? L"MSG_KEY_NAME_01" : nullptr;   // Canoe
}

void configure(game::Flags& f)
{
    f.has_hidden_passages = true;
    f.has_jobs = true;
    f.vehicle_message_id = &vehicle_message_id;

    // FF3 speaks line-fade story text line by line (line_fade.cpp replaces the core's SetData hook).
    hooks::disable("LineFadeMessageWindowController.SetData");
    // FF3's vehicle table and ChangeTransportation hook (movement_speech.cpp replaces both).
    hooks::disable("FieldPlayer.GetOn");
    hooks::disable("FieldPlayer.GetOff");
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
