#include <string>
#include "../../core/game.h"
#include "../../core/hooks.h"
#include "../../core/text.h"

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

bool contains(const std::wstring& s, const wchar_t* part) { return s.find(part) != std::wstring::npos; }

/// FF2 Field/EntityScanner.IsPlaceholderEntity, by the raw (Japanese) property name: entities
/// the old mod never listed. Ported whole on 2026-10-10 after the Dreadnought (a vehicle spawn
/// NPC with an action but no script) was read on the world map; the core's scenery rule only
/// sees objects without an action. The translations of these names stay in translation.json.
bool placeholder_entity(const wchar_t* raw_name)
{
    const std::wstring name = raw_name ? raw_name : L"";
    if (name.empty()) return true;   // over-filter rather than under-filter (the C#)

    static const wchar_t* const kExact[] = {
        // Vehicle spawn points
        L"飛空艇", L"大戦艦",
        // Town/village location markers (duplicated by map exits)
        L"アルテア", L"ガテア", L"パルム", L"ポフト", L"サラマンド", L"バフスク", L"フィン", L"ミシディア",
        L"ディスト", L"ジェイド", L"パンデモニウム", L"ガデアの村",
        // Castle/dungeon location markers (duplicated by map exits)
        L"カシュオーン城", L"ディストの城", L"パラメキア城", L"フィン城", L"ミシディアの塔", L"闘技場",
        // Zone markers (world map sub-regions)
        L"ミシディアA", L"ミシディアB", L"ミシディアC",
        // Collision/event objects (dev triggers, not real interactables)
        L"ミシディアの塔上部コリジョン",
    };
    for (const wchar_t* exact : kExact)
        if (name == exact) return true;

    // Names with variable suffixes: 飛竜 (wyvern), e.g. 飛竜(sc_e_0080用)
    if (name.rfind(L"飛竜", 0) == 0) return true;

    // Event descriptions: "(no icon)" markers, "occurs when passing while holding", "cannot go
    // to", effect placeholders.
    if (contains(name, L"アイコンなし") || contains(name, L"所持して通行すると発生") || contains(name, L"に行けない"))
        return true;
    if (contains(name, L"エフェクト") && contains(name, L"仮置き")) return true;

    // Object-name patterns (when the property has no name and the GameObject's is used).
    const std::wstring lower = text::lower(name);
    return contains(lower, L"wyvern") || contains(lower, L"wyrm") || contains(lower, L"hiryu") ||
           contains(lower, L"airship") || contains(lower, L"transport") || contains(lower, L"movearea");
}

/// FF2's transportation master (tools/mapdump/ff2_master/transportation.txt) has no message_id
/// for its two type-5 ("Content") vehicles, the canoe (id 2) and the snowcraft (id 5); the
/// FF1-style type table named both "canoe". Their names are the game's own key-item names.
const wchar_t* vehicle_message_id(int transportation_id)
{
    switch (transportation_id) {
    case 2: return L"MSG_KEY_NAME_02";   // Canoe
    case 5: return L"MSG_KEY_NAME_05";   // Snowcraft
    default: return nullptr;
    }
}

void configure(game::Flags& f)
{
    f.has_mp = true;
    f.has_airship = true;
    f.has_vehicle_pads = true;
    f.multi_hit_setting = true;
    f.has_skill_levels = true;
    f.has_levels = false;   // characters grow by use; no level anywhere (user, 2026-10-10)
    f.reads_job_slot = false;   // no jobs: the slot's job text repeats the name (user, 2026-10-10)
    f.placeholder_entity = &placeholder_entity;
    f.vehicle_message_id = &vehicle_message_id;

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
