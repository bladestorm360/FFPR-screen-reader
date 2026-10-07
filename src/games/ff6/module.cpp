#include "../../core/core.h"
#include "../../core/game.h"
#include "../../core/hooks.h"
#include "../../core/input/input.h"

// FINAL FANTASY VI (FF6): the game module.
//
// Ported from FF6ScreenReader (BlindGuyNW/FF6ScreenReader, by ZKline), a C# MelonLoader mod with
// its own architecture; included in the native mod with its author's permission (user,
// 2026-10-07). The core's shared features (field navigation, menus, battle, shops, config,
// bestiary, dialogue) run for FF6 as for FF1-FF5. The game's own screens are separate source
// files in this folder; each registers itself with a core::Install whose game mask is
// game::kFF6, so nothing in this file needs to list them.
namespace {

void configure(game::Flags& f)
{
    f.has_mp = true;
    f.atb = true;
    f.multi_hit_setting = false;   // FF6's mod had no "Multi-hit Damage" setting
    // The engine routes on CollisionMappingData AND HiddenPassageMappingData in FF6 too
    // (MapDataUtility.ParseHiddenPassageMappingData exists); the core retries a failed search
    // on the collision grid. FF6's map data was not checked for passage layers.
    f.has_hidden_passages = true;
    f.timer_freeze = true;         // FF6 Shift+T (TimerPatches.cs)
    f.bestiary_steal = true;       // the bestiary lists stealable items (Steal ability)
    // FF6 has no FieldAirShip class: has_airship stays false.
}

// FF6's own key bindings beyond the core table (docs/INPUT_BINDINGS.md): 9 toggles the audio
// beacons on the field, like F6 (FF6 InputManager).
int install_bindings()
{
    input::add_binding(input::Key::Alpha9, input::Mod::None, input::Context::Field, "Mod.ToggleAudioBeacons",
                       "Toggle audio beacons");
    return 0;
}

core::Install s_bindings{"FF6.Bindings", &install_bindings, 500, game::kFF6};

// The details key's game-module entry ("GameModule.HasDetails" / "GameModule.Details", read by
// the core's I-key cascade): FF6 has two screens with their own details, the ability/esper
// menus and the Coliseum. Each publishes its own pair; this joins them, after both installed.
int install_details()
{
    queries::define("GameModule.HasDetails", [] {
        return queries::is("AbilityMenuPatches.HasDetails") || queries::is("ColiseumPatches.HasDetails");
    });
    queries::define_text("GameModule.Details", [] {
        if (queries::is("AbilityMenuPatches.HasDetails")) return queries::text("AbilityMenuPatches.Details");
        return queries::text("ColiseumPatches.Details");
    });
    return 0;
}

core::Install s_details{"FF6.Details", &install_details, 900, game::kFF6};

} // namespace

extern const game::Module kFF6Module = {
    game::Id::FF6,
    "FINAL FANTASY VI",
    "FFVI_ScreenReader",
    "FFVI_ScreenReader.dll",
    &configure,
    nullptr,
};
