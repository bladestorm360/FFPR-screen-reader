#pragma once
// Helpers and shared state of the menus area (src/core/menus). Not for other areas: they use
// menus.h and the named queries/actions.
//
// Ports the small FF1 helpers these screens lean on: TextUtils.StripIconMarkup,
// MessageManager.GetMessage, MenuManager.IsOpen, PreferencesManager.AutoDetailEnabled,
// FFI_ScreenReaderMod.ClearOtherMenuStates, `new List<T>(IEnumerable<T>)`,
// MasterManager.GetData<T>/GetList<T>, and the ShopMenuTracker members they read.
#include <string>
#include <string_view>
#include <vector>
#include "../refs.h"

namespace ffpr::menus::detail {

// --- text and game data -------------------------------------------------------------------

/// TextUtils.StripIconMarkup: drop every <tag> without whitespace inside, then trim.
std::wstring strip_icon(std::wstring_view s);
/// C# string.IsNullOrWhiteSpace.
bool blank(std::wstring_view s);
/// string.Join(sep, parts) - every part, blank or not.
std::wstring join_all(const std::vector<std::wstring>& parts, std::wstring_view sep);

/// MessageManager.Instance (null before the game made it).
void* message_manager();
/// MessageManager.GetMessage(id, isReplace) on a given manager; empty on any miss.
std::wstring message_from(void* mm, const std::wstring& id, bool is_replace);
/// Same, fetching the manager.
std::wstring message(const std::wstring& id, bool is_replace);

/// MenuManager.Instance.IsOpen (false when there is no manager).
bool menu_manager_open();
/// PreferencesManager.AutoDetailEnabled ("AutoDetail", default on).
bool auto_detail();

/// FFI_ScreenReaderMod.ClearOtherMenuStates(except_menu) with FF1's menu names ("Item",
/// "Equip", "Magic"): the system area's port (src/core/system/system.h).
void clear_other_menu_states(const char* except_menu);

/// `new List<T>(IEnumerable<T>)` for reference elements: List`1 and T[] are read directly; any
/// other enumerable class is walked with IEnumerable.GetEnumerator / MoveNext / Current.
/// False when it cannot be read (null, or a boxed struct enumerator).
bool enumerate(void* enumerable, std::vector<void*>& out);

/// A virtual int method with no arguments, dispatched to `obj`'s override (C#'s obj.M()).
int call_virtual_int(ref::Method& m, void* obj);

// --- characters (Last.Data.User.OwnedCharacterData) -----------------------------------------

std::wstring character_name(void* character);
/// The PlayerCharacterParameter of a character (null when missing).
void* character_parameter(void* character);
/// Parameter.currentHP, Parameter.ConfirmedMaxHp() and the localized names of
/// Parameter.CurrentConditionList (blank names left out). has_parameter false when the
/// character has no parameter.
struct Vitals {
    bool has_parameter = false;
    int hp = 0;
    int max_hp = 0;
    std::vector<std::wstring> conditions;
};
Vitals character_vitals(void* character);

// --- master data (Last.Data.Master.MasterManager) -------------------------------------------

/// MasterManager.Instance (null before it exists).
void* master_manager();
/// MasterManager.GetList<T>(): the Dictionary<int, T> of a master type, or null.
void* master_list(ref::Class& type);
/// MasterManager.GetData<T>(id), or null.
void* master_data(ref::Class& type, int id);
/// Entry `id` of a list returned by master_list (one GetData on a list already fetched).
void* master_find(void* list, int id);
/// The values of a master list in dictionary order (C#'s foreach over GetList<T>()).
std::vector<void*> master_values(void* list);

// --- shop tracker (owned by the shop area; read through the core registries) ----------------

/// ShopMenuTracker.IsShopMenuActive.
bool shop_menu_active();
/// ShopMenuTracker.IsShopMenuActive = true (restoring the shop after its equipment screen).
void shop_set_menu_active();
/// ShopMenuTracker.ClearForEquipmentSubmenu().
void shop_clear_for_equipment_submenu();
/// ShopMenuTracker.ValidateState().
bool shop_validate_state();
/// ShopMenuTracker.LastItemName.
std::wstring shop_last_item_name();

// --- state shared between the files of this area ---------------------------------------------

/// ItemMenuState.IsItemMenuActive (menu_state ITEM_MENU).
bool item_menu_active();
/// ItemMenuState.LastSelectedItem, as the two values the detail keys read
/// (ItemListContentData.ItemType / ItemId). False when nothing is selected.
bool item_last_selected(int& item_type, int& item_id);

/// EquipMenuState.IsActive (menu_state EQUIP_MENU).
bool equip_menu_active();
/// EquipDetailsAnnouncer.GetDescriptionFromUI: the equipment description panel's text.
std::wstring equip_description_from_ui();

/// MagicMenuState (src/core/menus/magic_menu_state.cpp).
bool magic_spell_list_active();
bool magic_target_selection_active();
void magic_on_spell_list_focused();
void magic_on_target_selection_active();
void magic_on_target_selection_closed();
void magic_reset_state();
void* magic_current_character();
void magic_set_current_character(void* owned_character);
/// MagicMenuState.LastAnnouncedAbility = ability (copies the description's message id).
void magic_set_last_announced_ability(void* owned_ability);
/// The description of LastAnnouncedAbility (GetSpellDescription), and whether one is set.
bool magic_last_announced_description(std::wstring& description);

} // namespace ffpr::menus::detail
