#pragma once
#include <string>

// The shop area's public API (FF1 Patches/ShopMenuTracker.cs, ShopDetailsAnnouncer).
//
// Other areas reach the same state by name, without this header:
//   queries  "ShopMenuTracker.ValidateState", "ShopMenuTracker.IsShopMenuActive",
//            "ShopMenuTracker.IsInMagicSlotSelection", "ShopMenuTracker.EnteredEquipmentSubmenu"
//   text     "ShopMenuTracker.LastItemName", "ShopMenuTracker.LastItemPrice",
//            "ShopMenuTracker.GetDescriptionFromUI"
//   actions  "ShopMenuTracker.ClearState", "ShopMenuTracker.ClearForEquipmentSubmenu",
//            "ShopMenuTracker.SetShopMenuActive" (IsShopMenuActive = true),
//            "ShopDetailsAnnouncer.AnnounceCurrentItemDetails"
// Main thread only, like the C# original.
namespace ffpr::shop {

/// ShopMenuTracker.ValidateState: true while the shop owns the cursor (suppress the generic
/// reader). Clears the shop state when the controller's state machine reads None.
bool validate_state();

/// ShopMenuTracker.IsShopMenuActive (the setter also writes menu_state SHOP_MENU).
bool is_shop_menu_active();
void set_shop_menu_active(bool active);

/// ShopMenuTracker.IsInMagicSlotSelection (FF1 magic shop spell-slot grid).
bool is_in_magic_slot_selection();
/// ShopMenuTracker.EnteredEquipmentSubmenu.
bool entered_equipment_submenu();

/// ShopMenuTracker.ClearForEquipmentSubmenu / ClearState.
void clear_for_equipment_submenu();
void clear_state();

/// The last real item the shop list announced (cache for the I and U keys).
const std::wstring& last_item_name();
const std::wstring& last_item_price();
int last_item_content_id();
/// 2 = weapon, 3 = armor, -1 = unknown or none.
int last_item_content_type();

/// ShopMenuTracker.GetDescriptionFromUI: the shop info panel's description text, or empty.
std::wstring description_from_ui();

/// ShopDetailsAnnouncer.AnnounceCurrentItemDetails (the I key in a shop).
void announce_current_item_details();

} // namespace ffpr::shop
