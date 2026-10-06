#include "../../core/game.h"
#include "../../core/hooks.h"

// FINAL FANTASY II (FF2): the game module.
//
// configure() sets this game's flags and switches off core hooks that are folded or inlined
// here. The game's own screens (Serial.FF2 classes and game-only Last.* classes) are separate source
// files in this folder; each registers itself with a core::Install whose game mask is
// game::kFF2, so nothing in this file needs to list them:
//   ff2_util.cpp       MenuDetailCache, FF2 menu-state keys, LocalizationUtility.GetMessage
//   keywords.cpp       KeywordPatches (Ask / Learn / Key Items, Words menu)
//   new_game.cpp       NewGameNamingPatches
//   shop_command.cpp   ShopCommandReader (generic-reader strategy)
//   magic_menu.cpp     MagicMenuPatches (field magic, spell skill levels)
//   battle_magic.cpp   BattleMagicPatches (battle spell list)
//   battle_result.cpp  BattleResultPatches (phased results: skills, spells, stat pages)
//   status_details.cpp StatusDetailsReader / StatusDetailsPatches (23 stats, weapon skills)
//   game_info.cpp      GameInfoAnnouncer (H with MP), the U key (no equip restrictions)
namespace {

void configure(game::Flags& f)
{
    f.has_airship = true;
    f.has_vehicle_pads = true;
    f.multi_hit_setting = true;
    f.has_skill_levels = true;

    // FF2 has no EXP. FF1's ShowPointsInit (gil + EXP + EXP tick) never runs in FF2, and FF1's
    // ShowStatusUpInit ("X leveled up to N" with FF1's stats) would double FF2's own stat pages:
    // battle_result.cpp announces FF2's phases instead.
    hooks::disable("ResultMenuController.ShowPointsInit");
    hooks::disable("ResultMenuController.ShowStatusUpInit");
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
