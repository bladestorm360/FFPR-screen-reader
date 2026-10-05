// Ports FF1 Patches/ShopMenuTracker.cs (ShopMenuTracker and ShopDetailsAnnouncer).
//
// Shop menu state for the I key and for suppressing the generic cursor reader. The ShopController
// state machine says which panel has focus; only a None state clears the shop.
#include "shop.h"
#include "shop_internal.h"
#include "../core.h"
#include "../hooks.h"
#include "../modtext.h"
#include "../say.h"
#include "../text.h"
#include "../ui_util.h"
#include "../unity.h"

namespace ffpr::shop {
namespace {

ref::Class kShopController{"Last.UI.KeyInput", "ShopController"};
ref::Field fStateMachine{kShopController, "stateMachine"};

ref::Class kShopInfoController{"Last.UI.KeyInput", "ShopInfoController"};
ref::Field fInfoView{kShopInfoController, "view"};
ref::Class kShopInfoView{"Last.UI.KeyInput", "ShopInfoView"};
ref::Field fDescriptionText{kShopInfoView, "descriptionText"};

// ShopController.State (identical in all five games).
constexpr int STATE_NONE = 0;
constexpr int STATE_SELECT_COMMAND = 1;           // Command bar (Buy/Sell/Equipment/Back)
constexpr int STATE_SELECT_PRODUCT = 2;           // Buy item list
constexpr int STATE_SELECT_SELL_ITEM = 3;         // Sell item list
constexpr int STATE_SELECT_ABILITY_TARGET = 4;    // Magic shop character select
constexpr int STATE_SELECT_EQUIPMENT = 5;         // Equipment sub-menu
constexpr int STATE_CONFIRMATION_BUY_ITEM = 6;    // Buy quantity confirmation
constexpr int STATE_CONFIRMATION_SELL_ITEM = 7;   // Sell quantity confirmation
constexpr int STATE_CONFIRMATION_FORGET_MAGIC = 8; // Magic forget confirmation
constexpr int STATE_CONFIRMATION_BUY_MAGIC = 9;   // Magic buy confirmation

bool g_shop_menu_active = false;
bool g_in_magic_slot_selection = false;
bool g_entered_equipment_submenu = false;
// Cached controller (used only after live() re-checks it).
void* g_cached_controller = nullptr;

std::wstring g_last_item_name;
std::wstring g_last_item_price;
int g_last_item_content_id = 0;
int g_last_item_content_type = -1;

int install()
{
    queries::define("ShopMenuTracker.ValidateState", [] { return validate_state(); });
    queries::define("ShopMenuTracker.IsShopMenuActive", [] { return g_shop_menu_active; });
    queries::define("ShopMenuTracker.IsInMagicSlotSelection", [] { return g_in_magic_slot_selection; });
    queries::define("ShopMenuTracker.EnteredEquipmentSubmenu", [] { return g_entered_equipment_submenu; });
    queries::define_text("ShopMenuTracker.LastItemName", [] { return g_last_item_name; });
    queries::define_text("ShopMenuTracker.LastItemPrice", [] { return g_last_item_price; });
    queries::define_text("ShopMenuTracker.GetDescriptionFromUI", [] { return description_from_ui(); });

    actions::define("ShopMenuTracker.ClearState", [] { clear_state(); });
    actions::define("ShopMenuTracker.ClearForEquipmentSubmenu", [] { clear_for_equipment_submenu(); });
    actions::define("ShopMenuTracker.SetShopMenuActive", [] { set_shop_menu_active(true); });
    actions::define("ShopDetailsAnnouncer.AnnounceCurrentItemDetails", [] { announce_current_item_details(); });
    return 0;
}

core::Install s_install{"ShopMenuTracker", &install, 100};

} // namespace

// --- state ----------------------------------------------------------------------------------

bool is_shop_menu_active() { return g_shop_menu_active; }

void set_shop_menu_active(bool active)
{
    g_shop_menu_active = active;
    menu_state::set_active(menu_state::SHOP_MENU, active);
}

bool is_in_magic_slot_selection() { return g_in_magic_slot_selection; }
void set_in_magic_slot_selection(bool v) { g_in_magic_slot_selection = v; }
bool entered_equipment_submenu() { return g_entered_equipment_submenu; }
void set_entered_equipment_submenu(bool v) { g_entered_equipment_submenu = v; }

const std::wstring& last_item_name() { return g_last_item_name; }
const std::wstring& last_item_price() { return g_last_item_price; }
int last_item_content_id() { return g_last_item_content_id; }
int last_item_content_type() { return g_last_item_content_type; }

void set_last_item(const std::wstring& name, const std::wstring& price, int content_id, int content_type)
{
    g_last_item_name = name;
    g_last_item_price = price;
    g_last_item_content_id = content_id;
    g_last_item_content_type = content_type;
}

void set_cached_controller(void* controller) { g_cached_controller = controller; }

void* cached_or_find_controller()
{
    if (live(g_cached_controller, kShopController)) return g_cached_controller;
    g_cached_controller = nullptr;
    void* found = unity::find_object(kShopController);
    if (found) g_cached_controller = found;
    return found;
}

int current_state(void* controller)
{
    if (!controller) return -1;
    return ui::state_tag(controller, fStateMachine);
}

bool validate_state()
{
    // Magic slot selection always suppresses.
    if (g_in_magic_slot_selection) return true;
    if (!g_shop_menu_active) return false;

    // Use the cached controller (no FindObjectOfType here); drop it once it is gone. The C#
    // try/catch around this (ClearState on a fault) has nothing to catch: every read is guarded.
    if (g_cached_controller && !live(g_cached_controller, kShopController))
        g_cached_controller = nullptr;

    if (g_cached_controller) {
        int s = current_state(g_cached_controller);
        if (s == STATE_NONE) {
            clear_state();
            return false;
        }
        if (s == STATE_SELECT_COMMAND || s == STATE_SELECT_PRODUCT || s == STATE_SELECT_SELL_ITEM ||
            s == STATE_SELECT_ABILITY_TARGET || s == STATE_SELECT_EQUIPMENT ||
            s == STATE_CONFIRMATION_BUY_ITEM || s == STATE_CONFIRMATION_SELL_ITEM ||
            s == STATE_CONFIRMATION_FORGET_MAGIC || s == STATE_CONFIRMATION_BUY_MAGIC)
            return true;
    }
    return g_shop_menu_active;
}

std::wstring description_from_ui()
{
    return hooks::guarded_r("ShopMenuTracker.GetDescriptionFromUI", std::wstring(), [] {
        void* info = unity::find_object(kShopInfoController);
        if (!info) return std::wstring();
        void* view = ref::rd<void*>(info, fInfoView);
        if (!view) return std::wstring();
        void* text_component = ref::rd<void*>(view, fDescriptionText);
        if (!text_component) return std::wstring();
        return unity::ui_text(text_component);
    });
}

void clear_for_equipment_submenu()
{
    g_entered_equipment_submenu = true;
    set_shop_menu_active(false);
    g_in_magic_slot_selection = false;
}

void clear_state()
{
    set_shop_menu_active(false);
    g_in_magic_slot_selection = false;
    g_entered_equipment_submenu = false;
    g_cached_controller = nullptr;
    g_last_item_name.clear();
    g_last_item_price.clear();
    g_last_item_content_id = 0;
    g_last_item_content_type = -1;
    reset_quantity_tracking();
}

// --- ShopDetailsAnnouncer -------------------------------------------------------------------

void announce_current_item_details()
{
    hooks::guarded("ShopDetailsAnnouncer.AnnounceCurrentItemDetails", [] {
        if (!validate_state()) return;
        std::wstring announcement = description_from_ui();
        if (announcement.empty()) announcement = T(L"No description");
        ffpr::say(announcement, true);
    });
}

} // namespace ffpr::shop
