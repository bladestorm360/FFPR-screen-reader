#pragma once
#include <string>
#include <string_view>
#include "../refs.h"

// Shared between the shop area's own files only. Other areas use shop.h or the named
// queries/actions.
namespace ffpr::shop {

// --- ShopMenuTracker (shop_tracker.cpp) ---------------------------------------------------
/// GetCachedOrFindShopController: the live KeyInput.ShopController, or null.
void* cached_or_find_controller();
/// SetCachedController.
void set_cached_controller(void* controller);
/// GetCurrentState: ShopController.stateMachine's current tag, or -1.
int current_state(void* controller);
void set_in_magic_slot_selection(bool v);
void set_entered_equipment_submenu(bool v);
void set_last_item(const std::wstring& name, const std::wstring& price, int content_id, int content_type);

// --- ShopPatches (shop_patches.cpp) -------------------------------------------------------
/// ShopPatches.ResetQuantityTracking (called from ClearState).
void reset_quantity_tracking();

// --- ShopMagicPatches (shop_magic.cpp) ----------------------------------------------------
/// ShopMagicPatches.ResetState.
void magic_reset_state();

// --- helpers (shop_data.cpp) --------------------------------------------------------------
/// MasterManager.GetData<T>(id) for a master class T (Weapon, Armor, Content...), or null.
void* master_get_data(ref::Class& type, int id);
/// MessageManager.Instance.GetMessage(key, false), or empty.
std::wstring get_message(const std::wstring& key);
/// FF1 TextUtils.StripIconMarkup: drop single-token <...> tags, then trim.
std::wstring strip_icon_markup(std::wstring_view s);
/// A cached game pointer that is still a live object of class `k`.
bool live(void* obj, ref::Class& k);

} // namespace ffpr::shop
