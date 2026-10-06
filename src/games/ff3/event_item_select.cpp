// Ports FF3 Patches/EventItemSelectPatches.cs (EventItemSelectState, EventItemSelectPatches).
//
// NPC item selection events (Last.UI.KeyInput.SelectFieldContentController, opened by
// Last.UI.SelectFieldContentManager): "Item: description (n of N)" on each focus change.
//
// Dedup: the C#'s lastAnnouncement compared the text before the position was appended and was
// cleared by the manager's Close (DEDUP_INVENTORY FF3 tracker #19); moved to dedup:: as it was.
// SelectContent (0x7EB240) and SelectFieldContentManager.Close (0x732260) are unique in FF3.
#include <cstdint>
#include <string>
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/status/status.h"
#include "../../core/system/cursor_event.h"

namespace ffpr::ff3 {
namespace {

ref::Class kSelectBase{"Last.UI", "SelectFieldContentControllerBase", game::kFF3};
ref::Field fContentDataList{kSelectBase, "contentDataList"};
ref::Class kSelectController{"Last.UI.KeyInput", "SelectFieldContentController", game::kFF3};
ref::Method mSelectContent{kSelectController, "SelectContent", 2};

ref::Class kManager{"Last.UI", "SelectFieldContentManager", game::kFF3};
ref::Method mManagerClose{kManager, "Close", 0};

ref::Class kContentData{"Last.UI", "SelectFieldContentManager.SelectFieldContentData", game::kFF3};
ref::Field fNameMessageId{kContentData, "<NameMessageId>k__BackingField"};
ref::Field fDescriptionMessageId{kContentData, "<DescriptionMessageId>k__BackingField"};

constexpr const char* kCtxRow = "EventItemSelect.Row";

std::wstring clean(const std::wstring& s) { return text::trim(text::strip_markup(s)); }

/// EventItemSelectState.ShouldSuppress: active, and the manager still exists.
bool should_suppress()
{
    if (!menu_state::is_active(menu_state::EVENT_ITEM_SELECT)) return false;
    if (!ref::instance(kManager)) {
        menu_state::set_active(menu_state::EVENT_ITEM_SELECT, false);
        return false;
    }
    return true;
}

// SelectContent_Postfix
void on_select_content(void* self, int index)
{
    if (!self || index < 0) return;
    menu_state::set_active(menu_state::EVENT_ITEM_SELECT, true);

    void* list = ref::rd<void*>(self, fContentDataList);
    const int count = unity::list_count(list);
    if (count == 0 || index >= count) return;
    void* item = unity::list_at(list, index);
    if (!item) return;

    const std::wstring name_id = ref::rd_str(item, fNameMessageId);
    if (name_id.empty()) return;
    std::wstring name = ffpr::status::game_message(name_id, true);   // GetMessage(id): isReplace true
    if (name.empty()) return;
    name = clean(name);

    std::wstring announcement = name;
    const std::wstring desc_id = ref::rd_str(item, fDescriptionMessageId);
    if (!desc_id.empty()) {
        const std::wstring desc = ffpr::status::game_message(desc_id, true);
        if (!desc.empty()) announcement = name + L": " + clean(desc);
    }

    if (!dedup::text(kCtxRow, announcement, dedup::Scope::Session)) return;
    ffpr::say(ui::menu_position(announcement, index, count), true);
}

void on_manager_close()
{
    menu_state::set_active(menu_state::EVENT_ITEM_SELECT, false);
    dedup::reset(kCtxRow);
}

using SelectContent_t = void (*)(void* self, int32_t index, int32_t scroll_type, const void* mi);
using Close_t = void (*)(void* self, const void* mi);
SelectContent_t o_SelectContent;
Close_t o_ManagerClose;

void d_SelectContent(void* self, int32_t index, int32_t scroll_type, const void* mi)
{
    o_SelectContent(self, index, scroll_type, mi);
    hooks::guarded("FF3.EventItemSelect.SelectContent", [&] { on_select_content(self, index); });
}

void d_ManagerClose(void* self, const void* mi)
{
    o_ManagerClose(self, mi);
    hooks::guarded("FF3.EventItemSelect.Close", [&] { on_manager_close(); });
}

int install()
{
    int n = 0;
    n += hooks::attach(mSelectContent, &d_SelectContent, &o_SelectContent) ? 1 : 0;
    n += hooks::attach(mManagerClose, &d_ManagerClose, &o_ManagerClose) ? 1 : 0;
    LOGF("[FF3 EventItemSelect] %d hooks", n);

    queries::define("EventItemSelectState.IsActive", [] { return menu_state::is_active(menu_state::EVENT_ITEM_SELECT); });
    queries::define("EventItemSelectState.ShouldSuppress", [] { return should_suppress(); });
    cursor::add_suppressor("EventItemSelectState.ShouldSuppress");
    // FFIII_ScreenReaderMod.ClearMenuFlagsForMapTransition also cleared EVENT_ITEM_SELECT.
    events::on(events::Kind::MapChanged, [] { menu_state::reset(menu_state::EVENT_ITEM_SELECT); });
    return n;
}

core::Install s_install{"FF3.EventItemSelect", &install, 502, game::kFF3};

} // namespace
} // namespace ffpr::ff3
