#pragma once
// FINAL FANTASY IV module: helpers and state shared by the files in src/games/ff4.
//
// Ports the parts of FF4 Core/MenuState.cs the core's menu_state:: does not already cover (the
// FF4-only registry keys and SetActiveExclusive), Utils/MessageHelper.cs,
// Utils/TextUtils.StripIconMarkup and Utils/CharacterStatusHelper.cs.
//
// Main thread only, like the C# originals.
#include <string>
#include <string_view>
#include "../../core/core.h"
#include "../../core/refs.h"

namespace ffpr::ff4 {

// --- MenuState.cs -----------------------------------------------------------------------------
// FF1's keys (core.h, menu_state::) cover Item, Equip, Shop, Config, Status, Popup, SaveLoad and
// the battle menus. FF4's own SimpleMenuState names:
//
// MenuStates.Ability is the field Ability menu: FF4's magic list. It uses the core's MAGIC_MENU
// key, so the core's I-key cascade (MagicMenuState.IsSpellListActive ->
// MagicDetailsAnnouncer) and the generic cursor suppression (MagicMenuState.ShouldSuppress falls
// back to the MAGIC_MENU flag) work for FF4 unchanged.
constexpr const char* ABILITY = menu_state::MAGIC_MENU;
constexpr const char* PARTY = "Party";     // MenuStates.Party (party setting screen)
constexpr const char* TITLE = "Title";     // MenuStates.Title (title / extras menu)
constexpr const char* NAMING = "Naming";   // NamingPatches (naming screen)

/// SimpleMenuState.SetActive = MenuStateRegistry.SetActiveExclusive(key), with FF1's clearing
/// rule (src/core/system ClearOtherMenuStates): every other active state is reset (its reset
/// handler runs) except MAIN_MENU, the shop tracker is cleared, then `key` is set.
void set_active_exclusive(const char* key);
/// SimpleMenuState.Reset: the flag only (its reset handler runs).
inline void reset(const char* key) { menu_state::reset(key); }
inline bool is_active(const char* key) { return menu_state::is_active(key); }

// --- MessageHelper / TextUtils ----------------------------------------------------------------

/// MessageHelper.GetLocalizedMessage: MessageManager.GetMessage(id) (isReplace = true); empty
/// when the id is blank, the manager is missing or the message is blank.
std::wstring message(const std::wstring& mes_id);
/// TextUtils.StripIconMarkup: the game's markup removed, trimmed.
std::wstring strip_icon(std::wstring_view s);
/// PreferencesManager.AutoDetailEnabled ("AutoDetail", default on; FF4's AutoDetailOnFocus is
/// imported into it).
bool auto_detail();

// --- virtual calls ----------------------------------------------------------------------------
/// A C# virtual/abstract call `obj.M()` with no arguments, dispatched to obj's override (a ref to
/// an abstract base method has no code of its own). R{} on a null object or a fault.
int call_virtual_int(ref::Method& m, void* obj);
void* call_virtual_ptr(ref::Method& m, void* obj);

// --- CharacterStatusHelper ----------------------------------------------------------------------
// `parameter` is a Last.Data.CharacterParameterBase (OwnedCharacterData.parameter, or a battle
// unit's BattleUnitDataInfo.Parameter).

/// GetVitalsString: "HP 100/200, MP 50/100", or empty.
std::wstring vitals_string(void* parameter);
/// GetStatusConditions: "Poison, Blind" from ConfirmedConditionList(), or empty.
std::wstring status_conditions(void* parameter);
/// GetFullStatus: ", HP 100/200, MP 50/100, Poison" (leading comma), or empty.
std::wstring full_status(void* parameter);

/// The Corps (front/back row) id of the party slot at a display index: FieldController
/// .GetCorpsListCloneWithApparentOrder(), else UserDataManager.GetCorpsListClone() (FF4
/// CharacterSelectionReader / FormationRowPatches). 1 = Front, 2 = Back, -1 when unreadable.
/// Defined in party_setting.cpp.
int corps_id_at(int slot_index);

/// OwnedCharacterData.Name (empty for null).
std::wstring character_name(void* owned_character);
/// OwnedCharacterData.parameter (null for null).
void* character_parameter(void* owned_character);

} // namespace ffpr::ff4
