#pragma once
// The field menu's item, equipment and magic screens and their detail announcers: the functions
// other areas call. Ports the public helpers of FF1 Patches/MagicMenuState.cs and
// Menus/CharacterSelectionReader.cs.
//
// Everything else these screens publish goes through the core registries (named in each file's
// install()):
//   queries  ItemMenuState.IsItemMenuActive, ItemMenuState.ShouldSuppress,
//            EquipMenuState.IsActive, EquipMenuState.EnteredFromShop, EquipMenuState.ShouldSuppress,
//            MagicMenuState.IsSpellListActive, MagicMenuState.IsTargetSelectionActive,
//            MagicMenuState.ShouldSuppress (FF1)
//   text     CharacterSelectionReader.TryReadCharacterSelection (reads cursor::current() and
//            sets its out_count, src/core/system/cursor_event.h),
//            UsableByAnnouncer.BuildShopItemAnnouncement
//   actions  ItemMenuState.ResetState, EquipMenuState.ClearState, MagicMenuState.ResetState,
//            EquipDetailsAnnouncer.AnnounceCurrentItemDetails,
//            ItemDescriptionAnnouncer.AnnounceCurrentDescription,
//            ItemDetailsAnnouncer.AnnounceEquipRequirements,
//            MagicDetailsAnnouncer.AnnounceCurrentSpellDescription,
//            UsableByAnnouncer.AnnounceForCurrentContext
//
// Main thread only, like the C# originals.
#include <string>

namespace ffpr::menus {

/// MagicMenuState.GetConditionName: the localized name of a status effect
/// (Last.Data.Master.Condition), or empty. Battle readers use it too. All five games.
std::wstring condition_name(void* condition);

/// MagicMenuState.GetSpellName / GetSpellDescription (Last.Data.User.OwnedAbility), markup
/// stripped; empty when missing. All five games.
std::wstring spell_name(void* owned_ability);
std::wstring spell_description(void* owned_ability);
/// MagicMenuState.GetSpellLevel: Ability.AbilityLv, 0 when missing.
int spell_level(void* owned_ability);

/// MagicMenuState.GetChargesForLevel: FF1's per-level spell charges (levels 1-8) of an
/// OwnedCharacterData. current = CurrentMpCountList[level], max = ConfirmedMaxMpCount(level)
/// (BaseMaxMpCountList[level] when that call fails). (0, 0) when unavailable.
void charges_for_level(void* owned_character, int level, int& current, int& max);

/// Feed the I-key spell description (MagicMenuState.LastAnnouncedAbility) from another game's
/// magic list. Copies what the detail key needs; the ability pointer is not kept. Null clears.
void set_last_announced_ability(void* owned_ability);

/// CharacterSelectionReader.TryReadCharacterSelection: "Name, Job, Level X, HP a/b, MP c/d"
/// for the character slot under a cursor, or empty. `count` gets the slot count when a
/// character list was found (also when no text was read, as in FF1), else -1.
std::wstring try_read_character_selection(void* cursor_transform, int cursor_index, int& count);

} // namespace ffpr::menus
