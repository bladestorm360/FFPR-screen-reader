// Ports FF1 Patches/BattleItemPatches.cs: the battle item list (name, count, description with
// AutoDetail) and the on-demand description (I key / right stick up).
//
// Last.UI.KeyInput.BattleItemInfomationController.SelectContent(Cursor, WithinRangeType) exists in
// all five games, unique RVA in each; displayDataList / dataList by name (offsets differ in FF5).
//
// Not FF6: the same controller also shows FF6's Tools list (isMachineState), where this reader
// would speak the item list, and its "Items is command index 2" gate drops every read in FF6.
// FF6's own reader (src/games/ff6/battle_items.cpp) keeps this one's behaviour for items.
#include "../ui/preferences.h"
#include <cstdint>
#include <string>
#include "battle_internal.h"
#include "battle_util.h"
#include "../core.h"
#include "../hooks.h"
#include "../modtext.h"
#include "../say.h"
#include "../settings.h"
#include "../text.h"
#include "../ui_util.h"
#include "../unity.h"
#include "../displayed_ui.h"

namespace ffpr::battle {
namespace {

constexpr uint8_t kFF12345 = game::kFF1 | game::kFF2 | game::kFF3 | game::kFF4 | game::kFF5;

ref::Class kItemCtl{"Last.UI.KeyInput", "BattleItemInfomationController"};
ref::Method mSelectContent{kItemCtl, "SelectContent", 2};
ref::Field fDataList{kItemCtl, "dataList"};
ref::Field fDisplayDataList{kItemCtl, "displayDataList"};

ref::Class kItemContent{"Last.UI.KeyInput", "BattleItemInfomationContentController"};
ref::Field fContentData{kItemContent, "<Data>k__BackingField"};

ref::Class kItemListContentData{"Last.UI", "ItemListContentData"};
ref::Field fIlcdName{kItemListContentData, "<Name>k__BackingField"};
ref::Field fIlcdDescription{kItemListContentData, "<Description>k__BackingField"};
ref::Field fIlcdCount{kItemListContentData, "<Count>k__BackingField"};
ref::Field fIlcdIsFocus{kItemListContentData, "<IsFocus>k__BackingField"};

// LastFocusedDescription: the focused item's stripped description, refreshed on every cursor move
// whatever the AutoDetail setting, for the on-demand reader.
std::wstring g_last_focused_description;

/// Stripped description, or empty (TryGetItemDescription).
std::wstring item_description(void* data)
{
    std::wstring description = ref::rd_str(data, fIlcdDescription);
    if (blank(description)) return std::wstring();
    description = strip_icon(description);
    return blank(description) ? std::wstring() : description;
}

/// FormatItemAnnouncement: "Name, count: description".
std::wstring format_item(void* data)
{
    std::wstring name = ref::rd_str(data, fIlcdName);
    if (name.empty()) return std::wstring();
    name = strip_icon(name);
    if (name.empty()) return std::wstring();

    std::wstring announcement = name;
    int count = ref::rd<int32_t>(data, fIlcdCount);
    if (count > 1) announcement += L", " + text::num(count);

    std::wstring description = item_description(data);
    g_last_focused_description = description;
    if (ui::prefs::auto_detail_enabled() && !blank(description)) announcement += L": " + description;
    return announcement;
}

/// TryGetItemAnnouncement: displayDataList, then dataList, then the focused content controller.
std::wstring item_announcement(void* controller, int index, int& count)
{
    count = -1;

    // 1: displayDataList (ItemListContentData: name and description).
    void* display = ref::rd<void*>(controller, fDisplayDataList);
    if (display && index >= 0 && index < unity::list_count(display)) {
        if (void* data = unity::list_at(display, index)) {
            count = unity::list_count(display);
            return format_item(data);
        }
    }

    // 2: dataList (OwnedItemData).
    void* items = ref::rd<void*>(controller, fDataList);
    if (items && index >= 0 && index < unity::list_count(items)) {
        if (void* item = unity::list_at(items, index)) {
            std::wstring name = owned_item_name(item);
            if (!name.empty()) {
                name = strip_icon(name);
                count = unity::list_count(items);
                int quantity = owned_item_count(item);
                if (quantity > 1) return name + L", " + text::num(quantity);
                return name;
            }
        }
    }

    // 3: the focused content controller.
    void* contents = displayed::field(controller, "contentList");
    for (int i = 0; i < unity::list_count(contents); ++i) {
        void* cc = unity::list_at(contents, i);
        if (!cc || !unity::active_in_hierarchy(cc)) continue;
        void* data = ref::rd<void*>(cc, fContentData);
        if (data && ref::rd<bool>(data, fIlcdIsFocus)) return format_item(data);
    }
    return std::wstring();
}

/// AnnounceCurrentDescription (I key / right stick up).
void announce_current_description()
{
    const std::wstring& desc = g_last_focused_description;
    ffpr::say(blank(desc) ? T(L"No description") : text::trim(desc), true);
}

// SelectContent(Cursor targetCursor, WithinRangeType type): postfix.
using SelectContent_t = void (*)(void* self, void* cursor, int32_t type, const void* mi);
SelectContent_t o_SelectContent;

void d_SelectContent(void* self, void* cursor, int32_t type, const void* mi)
{
    o_SelectContent(self, cursor, type, mi);
    hooks::guarded("BattleItemInfomationController.SelectContent", [&] {
        if (!self || !cursor) return;
        int index = ui::cursor_index(cursor);

        // Command menu active but "Items" (index 2) not selected: a stale callback from leaving the
        // item menu.
        if (state_active(menu_state::BATTLE_COMMAND) && last_selected_command_index() != 2) return;

        int count = -1;
        std::wstring announcement = item_announcement(self, index, count);
        if (announcement.empty()) return;

        // Set the state only AFTER the read succeeded (setting it early suppressed transitions).
        clear_other_menu_states(menu_state::BATTLE_ITEM);
        state_set(menu_state::BATTLE_ITEM, true);
        set_target_selection_active(false);

        announcement = ui::menu_position(announcement, index, count);
        notify_command_submenu_active();   // restore the command window + arm the back-out re-announce
        ffpr::say(announcement, true);
    });
}

int install()
{
    actions::define("BattleItemPatches.AnnounceCurrentDescription", [] { announce_current_description(); });
    return hooks::attach(mSelectContent, &d_SelectContent, &o_SelectContent) ? 1 : 0;
}
core::Install s_install{"Battle.Item", &install, 110, kFF12345};

} // namespace
} // namespace ffpr::battle
