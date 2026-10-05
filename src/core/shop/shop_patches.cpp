// Ports FF1 Patches/ShopPatches.cs.
//
// Shop item list focus (ShopInfoController.SetDescription), the command bar
// (ShopCommandMenuController.SetCursor), the quantity window (ShopTradeWindowController.Show,
// AddCount, TakeCount) and shop close (ShopController.Close). The magic shop's slot grid is in
// shop_magic.cpp. Every class is Last.UI.KeyInput and identical in all five games; no hooked
// body is folded in any game (one RVA each in every dump.cs).
#include "shop.h"
#include "shop_internal.h"
#include "../core.h"
#include "../dedup.h"
#include "../hooks.h"
#include "../log.h"
#include "../modtext.h"
#include "../say.h"
#include "../settings.h"
#include "../text.h"
#include "../ui_util.h"
#include "../unity.h"

namespace ffpr::shop {
namespace {

ref::Class  kShopController{"Last.UI.KeyInput", "ShopController"};
ref::Method mShopClose{kShopController, "Close", 0};

ref::Class  kShopInfoController{"Last.UI.KeyInput", "ShopInfoController"};
ref::Method mSetDescription{kShopInfoController, "SetDescription", 1};

ref::Class  kCommandMenu{"Last.UI.KeyInput", "ShopCommandMenuController"};
ref::Method mCommandSetCursor{kCommandMenu, "SetCursor", 1};
ref::Field  fCommandContentList{kCommandMenu, "contentList"};
ref::Class  kCommandContent{"Last.UI.KeyInput", "ShopCommandMenuContentController"};
ref::Field  fCommandId{kCommandContent, "commandId"};

ref::Class  kTradeWindow{"Last.UI.KeyInput", "ShopTradeWindowController"};
ref::Method mTradeShow{kTradeWindow, "Show", 10};
ref::Method mAddCount{kTradeWindow, "AddCount", 0};
ref::Method mTakeCount{kTradeWindow, "TakeCount", 0};
ref::Field  fSelectedCount{kTradeWindow, "selectedCount"};
ref::Field  fTradeView{kTradeWindow, "view"};
ref::Class  kTradeView{"Last.UI.KeyInput", "ShopTradeWindowView"};
ref::Field  fTotalPriceText{kTradeView, "totarlPriceText"};

ref::Class  kListMain{"Last.UI.KeyInput", "ShopListMainContentController"};
ref::Field  fListSelectCursor{kListMain, "selectCursor"};
ref::Field  fProductContentList{kListMain, "productContentList"};

ref::Class  kListItem{"Last.UI.KeyInput", "ShopListItemContentController"};
ref::Field  fItemContentView{kListItem, "shopListItemContentView"};
ref::Field  fItemIconTextView{kListItem, "iconTextView"};
ref::Field  fItemContentId{kListItem, "<ContentId>k__BackingField"};
ref::Class  kListItemView{"Last.UI.KeyInput", "ShopListItemContentView"};
ref::Field  fPriceText{kListItemView, "priceText"};
ref::Class  kIconTextView{"Last.UI", "IconTextView"};
ref::Field  fIconNameText{kIconTextView, "nameText"};

ref::Class  kWeapon{"Last.Data.Master", "Weapon"};
ref::Class  kArmor{"Last.Data.Master", "Armor"};

// Last.Defaine.ShopCommandId (identical in all five games).
constexpr int CMD_BUY = 1, CMD_SELL = 2, CMD_EQUIPMENT = 3, CMD_BACK = 4;

// ShopController.State values used by the gates.
constexpr int STATE_SELECT_COMMAND = 1, STATE_SELECT_PRODUCT = 2, STATE_SELECT_SELL_ITEM = 3;

// FF1 _lastAnnouncedListIndex. Scoped exception to the "no dedup" rule: SetDescription also
// fires on the stats/description panel toggle for the same focused item, and no non-dedup
// signal covers unaffordable items. Session scope: only this file's resets clear it, as in FF1.
constexpr const char* kItemCtx = "Shop.Item";

// FF1 _cachedMainList (re-checked with live() before use).
void* g_cached_main_list = nullptr;

// FFI_ScreenReaderMod.ClearOtherMenuStates("Shop"): "Shop" is not in its key map, so it is
// MenuStateRegistry.ResetAll(), and the shop itself is not cleared.
void clear_other_menu_states_for_shop() { menu_state::reset_all(); }

void* find_active_main_content_controller()
{
    if (g_cached_main_list) {
        if (live(g_cached_main_list, kListMain) && unity::active_in_hierarchy(g_cached_main_list))
            return g_cached_main_list;
        if (!live(g_cached_main_list, kListMain)) g_cached_main_list = nullptr;
    }
    for (void* candidate : unity::find_objects(kListMain)) {
        if (candidate && unity::active_in_hierarchy(candidate)) {
            g_cached_main_list = candidate;
            return candidate;
        }
    }
    return nullptr;
}

std::wstring extract_price(void* content)
{
    if (!content) return {};
    void* view = ref::rd<void*>(content, fItemContentView);
    if (!view) return {};
    void* price_text = ref::rd<void*>(view, fPriceText);
    if (!price_text) return {};
    return unity::ui_text(price_text);
}

/// ResolveShopItemType: 2 = weapon, 3 = armor, -1 = unknown.
int resolve_shop_item_type(int content_id)
{
    if (content_id <= 0) return -1;
    if (master_get_data(kWeapon, content_id)) return 2;   // CONTENT_TYPE_WEAPON
    if (master_get_data(kArmor, content_id)) return 3;    // CONTENT_TYPE_ARMOR
    return -1;
}

std::wstring command_name(int command_id)
{
    switch (command_id) {
    case CMD_BUY: return T(L"Buy");
    case CMD_SELL: return T(L"Sell");
    case CMD_EQUIPMENT: return T(L"Equipment");
    case CMD_BACK: return T(L"Back");
    default: return {};
    }
}

/// AnnounceFocusedItem: name and price (with AutoDetail, the description), cached for I and U.
/// Empty slots announce "Empty" but keep the cache, so those keys still describe the last real
/// item while the cursor passes gaps in the sell inventory.
void announce_focused_item(void* content, int index, int count)
{
    std::wstring item_name;
    if (content) {
        void* icon_view = ref::rd<void*>(content, fItemIconTextView);
        void* name_text = icon_view ? ref::rd<void*>(icon_view, fIconNameText) : nullptr;
        if (name_text) item_name = unity::ui_text(name_text);
    }
    item_name = strip_icon_markup(item_name);

    if (item_name.empty()) {
        ffpr::say(ui::menu_position(T(L"Empty"), index, count), true);
        return;
    }

    std::wstring price = extract_price(content);

    int content_id = ref::rd<int32_t>(content, fItemContentId);
    set_last_item(item_name, price, content_id, resolve_shop_item_type(content_id));

    std::wstring base = price.empty() ? item_name : item_name + L", " + price;
    std::wstring announcement = base;
    if (settings::get_bool("AutoDetail", true)) {
        std::wstring detail = description_from_ui();
        if (!text::is_blank(detail)) announcement = base + L": " + detail;
    }
    ffpr::say(ui::menu_position(announcement, index, count), true);
}

/// AnnounceFocusedFromList: the in-focus item (selectCursor.Index into productContentList).
void announce_focused_from_list()
{
    void* main_list = find_active_main_content_controller();
    if (!main_list) return;   // not in the item list (command menu, magic slot grid, etc.)

    // Reaching the item list means the magic spell slot grid (if it was active) was backed out.
    if (is_in_magic_slot_selection()) {
        set_in_magic_slot_selection(false);
        magic_reset_state();
    }

    clear_other_menu_states_for_shop();
    set_shop_menu_active(true);

    void* cursor = ref::rd<void*>(main_list, fListSelectCursor);
    if (!cursor) return;
    int index = ui::cursor_index(cursor);
    if (index < 0) return;

    if (!dedup::index(kItemCtx, index, dedup::Scope::Session)) return;

    void* list = ref::rd<void*>(main_list, fProductContentList);
    if (!list) return;
    int n = unity::list_count(list);
    if (index >= n) return;

    void* content = unity::list_at(list, index);

    // The 50-slot pool's unused entries carry OTHER product names, so count ACTIVE entries.
    int active_count = 0;
    for (int i = 0; i < n; ++i) {
        void* c = unity::list_at(list, i);
        if (c && unity::active_in_hierarchy(c)) ++active_count;
    }

    announce_focused_item(content, index, active_count);
}

// --- ShopInfoController.SetDescription(string) postfix ---------------------------------------
// Fires on every cursor move in the item list, affordable or not: a cursor-moved signal. The
// description argument is ignored; the item is read from the list itself.
using SetDescription_t = void (*)(void* self, void* value, const void* mi);
SetDescription_t o_SetDescription = nullptr;

void d_SetDescription(void* self, void* value, const void* mi)
{
    o_SetDescription(self, value, mi);
    hooks::guarded("ShopPatches.SetDescription", [&] {
        if (!self) return;
        // Gate on the ShopController state machine: 2/3 are SelectProduct (buy) and
        // SelectSellItem (sell); anything else is a transition flicker or a preview refresh.
        void* controller = cached_or_find_controller();
        if (controller) {
            int state = current_state(controller);
            if (state != STATE_SELECT_PRODUCT && state != STATE_SELECT_SELL_ITEM) {
                dedup::reset(kItemCtx);
                return;
            }
        }
        announce_focused_from_list();
    });
}

// --- ShopCommandMenuController.SetCursor(int) postfix ----------------------------------------
using CommandSetCursor_t = void (*)(void* self, int32_t index, const void* mi);
CommandSetCursor_t o_CommandSetCursor = nullptr;

void d_CommandSetCursor(void* self, int32_t index, const void* mi)
{
    o_CommandSetCursor(self, index, mi);
    hooks::guarded("ShopPatches.CommandSetCursor", [&] {
        if (!self) return;

        if (entered_equipment_submenu()) {
            set_entered_equipment_submenu(false);
            set_shop_menu_active(true);
            return;
        }

        // Only announce when the command bar is the active panel (State.SelectCommand == 1). On
        // shop entry the state goes straight to SelectProduct without settling here, so the
        // prep-time cursor flicker is suppressed; cancelling back to the bar announces. No dedup.
        void* gate = cached_or_find_controller();
        if (gate && current_state(gate) != STATE_SELECT_COMMAND) return;

        clear_other_menu_states_for_shop();
        if (!is_shop_menu_active()) {
            if (void* controller = unity::find_object(kShopController))
                set_cached_controller(controller);
        }
        set_shop_menu_active(true);

        void* content_list = ref::rd<void*>(self, fCommandContentList);
        int count = unity::list_count(content_list);
        if (!content_list || index < 0 || index >= count) return;

        void* command_content = unity::list_at(content_list, index);
        if (!command_content) return;

        std::wstring name = command_name(ref::rd<int32_t>(command_content, fCommandId));
        if (name.empty()) return;

        ffpr::say(ui::menu_position(name, index, count), true);
    });
}

// --- Trade window ----------------------------------------------------------------------------

std::wstring total_price_text(void* controller)
{
    void* view = ref::rd<void*>(controller, fTradeView);
    if (!view) return {};
    void* text_component = ref::rd<void*>(view, fTotalPriceText);
    if (!text_component) return {};
    return unity::ui_text(text_component);
}

void announce_quantity(void* controller)
{
    if (!controller) return;
    if (is_in_magic_slot_selection()) return;

    int selected = ref::rd<int32_t>(controller, fSelectedCount);
    std::wstring total = total_price_text(controller);

    std::wstring announcement = total.empty()
        ? TF(L"Quantity: {0}", {text::num(selected)})
        : TF(L"Quantity: {0}, Total: {1}", {text::num(selected), total});
    ffpr::say(announcement, true);
}

// Show(string name, int price, int possibleCount, int level, string titleMessageId, bool isSell,
//      Action<int> onAction, bool isEnable, int itemCount, int maxCount) - same in all five games.
using TradeShow_t = void (*)(void* self, void* name, int32_t price, int32_t possible_count, int32_t level,
                             void* title_message_id, bool is_sell, void* on_action, bool is_enable,
                             int32_t item_count, int32_t max_count, const void* mi);
TradeShow_t o_TradeShow = nullptr;

/// Fires once when the trade window opens (buy or sell confirm): starting quantity and total.
void d_TradeShow(void* self, void* name, int32_t price, int32_t possible_count, int32_t level,
                 void* title_message_id, bool is_sell, void* on_action, bool is_enable,
                 int32_t item_count, int32_t max_count, const void* mi)
{
    o_TradeShow(self, name, price, possible_count, level, title_message_id, is_sell, on_action, is_enable,
                item_count, max_count, mi);
    hooks::guarded("ShopPatches.TradeWindowShow", [&] {
        dedup::reset(kItemCtx);
        announce_quantity(self);
    });
}

// AddCount / TakeCount: each call is a discrete user event, so it announces unconditionally; a
// press against max/min replays the same value.
using TradeCount_t = void (*)(void* self, const void* mi);
TradeCount_t o_AddCount = nullptr;
TradeCount_t o_TakeCount = nullptr;

void d_AddCount(void* self, const void* mi)
{
    o_AddCount(self, mi);
    hooks::guarded("ShopPatches.AddCount", [&] { announce_quantity(self); });
}

void d_TakeCount(void* self, const void* mi)
{
    o_TakeCount(self, mi);
    hooks::guarded("ShopPatches.TakeCount", [&] { announce_quantity(self); });
}

// --- ShopController.Close postfix --------------------------------------------------------------
using ShopClose_t = void (*)(void* self, const void* mi);
ShopClose_t o_ShopClose = nullptr;

void d_ShopClose(void* self, const void* mi)
{
    o_ShopClose(self, mi);
    hooks::guarded("ShopPatches.ShopClose", [&] {
        g_cached_main_list = nullptr;
        dedup::reset(kItemCtx);
        clear_state();
    });
}

int install()
{
    int n = 0;
    n += hooks::attach(mSetDescription, &d_SetDescription, &o_SetDescription) ? 1 : 0;
    n += hooks::attach(mCommandSetCursor, &d_CommandSetCursor, &o_CommandSetCursor) ? 1 : 0;
    n += hooks::attach(mTradeShow, &d_TradeShow, &o_TradeShow) ? 1 : 0;
    n += hooks::attach(mAddCount, &d_AddCount, &o_AddCount) ? 1 : 0;
    n += hooks::attach(mTakeCount, &d_TakeCount, &o_TakeCount) ? 1 : 0;
    n += hooks::attach(mShopClose, &d_ShopClose, &o_ShopClose) ? 1 : 0;
    LOGF("[Shop] %d shop hooks", n);
    return n;
}

core::Install s_install{"ShopPatches", &install, 100};

} // namespace

/// ShopPatches.ResetQuantityTracking (from ShopMenuTracker.ClearState).
void reset_quantity_tracking()
{
    magic_reset_state();
}

} // namespace ffpr::shop
