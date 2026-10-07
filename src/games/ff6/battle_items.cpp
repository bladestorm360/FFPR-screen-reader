// Ports FF6 Patches/BattleCommandPatches.cs (BattleItemInfomationController_SelectContent_Patch):
// the battle Item list and Edgar's Tools list. Both are Last.UI.KeyInput.
// BattleItemInfomationController; its isMachineState flag says which list the cursor is on
// (true: Tools, machineContentList; false: Items).
//
// Items keep the core's (FF1's) battle item reader behaviour and wording
// (src/core/battle/battle_item.cpp): "Name, count: description, (X of Y)" with Auto Detail, the
// I-key description ("BattleItemPatches.AnnounceCurrentDescription"), the BattleItem menu state
// (which also keeps the generic cursor reader silent) and the command back-out re-announce. The
// core reader is switched off for FF6 (its install mask) because it would read the item lists
// while Tools are shown, and its FF1 gate ("Items" is command index 2) drops every read in FF6,
// where Item is usually the fourth command. FF6's C# had no such gate.
// Tools (FF6 only, from the C#): the content's Data is null, so the name is the content view's
// IconTextView text (else NonItemTextView) and the description is the "description_text" Text
// under the view. Same format as an item without a count.
//
// Not ported: the C#'s lastAnnouncement text check. The same hook reads the item list, which
// follows the core reader, and that has none (FF1).
//
// SelectContent(Cursor, WithinRangeType) has one RVA in FF6's dump.cs.
#include <cstdint>
#include <string>
#include "../../core/core.h"
#include "../../core/hooks.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/battle/battle_util.h"
#include "../../core/system/system.h"
#include "../../core/ui/preferences.h"

namespace ffpr::ff6 {
namespace {

ref::Class  kItemCtl{"Last.UI.KeyInput", "BattleItemInfomationController", game::kFF6};
ref::Method mSelectContent{kItemCtl, "SelectContent", 2};
ref::Field  fDataList{kItemCtl, "dataList"};
ref::Field  fDisplayDataList{kItemCtl, "displayDataList"};
ref::Field  fIsMachineState{kItemCtl, "isMachineState"};
ref::Field  fMachineContentList{kItemCtl, "machineContentList"};

ref::Class kItemContent{"Last.UI.KeyInput", "BattleItemInfomationContentController", game::kFF6};
ref::Field fContentData{kItemContent, "<Data>k__BackingField"};
ref::Field fContentView{kItemContent, "view"};

ref::Class kItemContentView{"Last.UI.KeyInput", "BattleItemInfomationContentView", game::kFF6};
ref::Field fViewIconTextView{kItemContentView, "IconTextView"};
ref::Field fViewNonItemTextView{kItemContentView, "nonItemTextView"};

ref::Class kIconTextView{"Last.UI", "IconTextView", game::kFF6};
ref::Field fNameText{kIconTextView, "nameText"};

ref::Class kItemListContentData{"Last.UI", "ItemListContentData", game::kFF6};
ref::Field fIlcdName{kItemListContentData, "<Name>k__BackingField"};
ref::Field fIlcdDescription{kItemListContentData, "<Description>k__BackingField"};
ref::Field fIlcdCount{kItemListContentData, "<Count>k__BackingField"};
ref::Field fIlcdIsFocus{kItemListContentData, "<IsFocus>k__BackingField"};

// LastFocusedDescription: the focused item's or tool's stripped description, refreshed on every
// read whatever the Auto Detail setting, for the I key.
std::wstring g_last_focused_description;

bool blank(const std::wstring& s) { return text::is_blank(s); }

std::wstring stripped(const std::wstring& s)
{
    if (blank(s)) return {};
    std::wstring r = battle::strip_icon(s);
    return blank(r) ? std::wstring() : r;
}

/// The core's FormatItemAnnouncement: "Name, count: description".
std::wstring format_item(void* data)
{
    std::wstring name = stripped(ref::rd_str(data, fIlcdName));
    if (name.empty()) return {};

    std::wstring announcement = name;
    const int count = ref::rd<int32_t>(data, fIlcdCount);
    if (count > 1) announcement += L", " + text::num(count);

    std::wstring description = stripped(ref::rd_str(data, fIlcdDescription));
    g_last_focused_description = description;
    if (ui::prefs::auto_detail_enabled() && !description.empty()) announcement += L": " + description;
    return announcement;
}

/// The core's TryGetItemAnnouncement: displayDataList, then dataList, then the focused content.
std::wstring item_announcement(void* controller, int index, int& count)
{
    count = -1;

    void* display = ref::rd<void*>(controller, fDisplayDataList);
    if (display && index >= 0 && index < unity::list_count(display)) {
        if (void* data = unity::list_at(display, index)) {
            count = unity::list_count(display);
            return format_item(data);
        }
    }

    void* items = ref::rd<void*>(controller, fDataList);
    if (items && index >= 0 && index < unity::list_count(items)) {
        if (void* item = unity::list_at(items, index)) {
            std::wstring name = battle::owned_item_name(item);
            if (!name.empty()) {
                name = battle::strip_icon(name);
                count = unity::list_count(items);
                const int quantity = battle::owned_item_count(item);
                if (quantity > 1) return name + L", " + text::num(quantity);
                return name;
            }
        }
    }

    for (void* cc : unity::find_objects(kItemContent)) {
        if (!cc || !unity::active_in_hierarchy(cc)) continue;
        void* data = ref::rd<void*>(cc, fContentData);
        if (data && ref::rd<bool>(data, fIlcdIsFocus)) return format_item(data);
    }
    return {};
}

/// The tool's name: IconTextView.nameText, else NonItemTextView.nameText (the C#'s order).
std::wstring tool_name(void* view)
{
    void* icon = ref::rd<void*>(view, fViewIconTextView);
    void* name_text = icon ? ref::rd<void*>(icon, fNameText) : nullptr;
    if (!name_text) {
        void* non_item = ref::rd<void*>(view, fViewNonItemTextView);
        name_text = non_item ? ref::rd<void*>(non_item, fNameText) : nullptr;
    }
    return name_text ? unity::ui_text(name_text) : std::wstring();
}

/// The "description_text" Text under the tool's view (the C#'s FindTextInChildren).
std::wstring tool_description(void* view)
{
    void* t = unity::transform(view);
    void* found = t ? unity::find_descendant(t, L"description_text") : nullptr;
    void* text_component = found ? unity::get_component(found, unity::text_class()) : nullptr;
    return text_component ? stripped(unity::ui_text(text_component)) : std::wstring();
}

/// Entries of the Tools list that are shown (the list's size for "(X of Y)").
int shown_count(void* list, int fallback)
{
    int shown = 0;
    for (int i = 0, n = unity::list_count(list); i < n; ++i) {
        void* c = unity::list_at(list, i);
        if (c && unity::active_in_hierarchy(c)) ++shown;
    }
    return shown > 0 ? shown : fallback;
}

/// The Tools list (machineContentList): the C#'s Data-or-view read.
std::wstring tool_announcement(void* controller, int index, int& count)
{
    count = -1;
    void* list = ref::rd<void*>(controller, fMachineContentList);
    const int n = unity::list_count(list);
    if (n == 0 || index < 0 || index >= n) return {};
    void* content = unity::list_at(list, index);
    if (!content) return {};

    // Data first (an item), as the C# did.
    if (void* data = ref::rd<void*>(content, fContentData)) {
        count = shown_count(list, n);
        return format_item(data);
    }

    void* view = ref::rd<void*>(content, fContentView);
    if (!view) return {};
    std::wstring name = stripped(tool_name(view));
    if (name.empty()) return {};

    std::wstring announcement = name;
    std::wstring description = tool_description(view);
    g_last_focused_description = description;
    if (ui::prefs::auto_detail_enabled() && !description.empty()) announcement += L": " + description;
    count = shown_count(list, n);
    return announcement;
}

void announce_current_description()
{
    ffpr::say(blank(g_last_focused_description) ? T(L"No description") : text::trim(g_last_focused_description),
              true);
}

// SelectContent(Cursor targetCursor, WithinRangeType type): postfix.
using SelectContent_t = void (*)(void* self, void* cursor, int32_t type, const void* mi);
SelectContent_t o_SelectContent;

void d_SelectContent(void* self, void* cursor, int32_t type, const void* mi)
{
    o_SelectContent(self, cursor, type, mi);
    hooks::guarded("FF6 BattleItemInfomationController.SelectContent", [&] {
        if (!self || !cursor) return;
        const int index = ui::cursor_index(cursor);
        const bool tools = ref::rd<bool>(self, fIsMachineState);

        int count = -1;
        std::wstring announcement = tools ? tool_announcement(self, index, count)
                                          : item_announcement(self, index, count);
        if (announcement.empty()) return;

        // The state only AFTER the read succeeded (as the core's reader): ClearOtherMenuStates
        // ("BattleItem") resets every other state but MAIN_MENU, the target state included.
        ffpr::system::clear_other_menu_states("BattleItem");
        menu_state::set_active(menu_state::BATTLE_ITEM, true);
        announcement = ui::menu_position(announcement, index, count);
        // Restore the command window and arm its back-out re-announce.
        if (actions::defined("BattleCommandPatches.NotifyCommandSubmenuActive"))
            actions::run("BattleCommandPatches.NotifyCommandSubmenuActive");
        ffpr::say(announcement, true);
    });
}

int install()
{
    // The I key in the battle item / tool list (core cascade: BattleItemMenuState.IsActive).
    actions::define("BattleItemPatches.AnnounceCurrentDescription", [] { announce_current_description(); });
    return hooks::attach(mSelectContent, &d_SelectContent, &o_SelectContent) ? 1 : 0;
}

core::Install s_install{"FF6.BattleItems", &install, 510, game::kFF6};

} // namespace
} // namespace ffpr::ff6
