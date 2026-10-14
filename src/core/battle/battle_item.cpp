// Native battle item panels. Read the rendered row, independent of command order.
// Retains FF6 Items/Tools behavior ported from BlindGuyNW/FF6ScreenReader
// (ZKline), with the inclusion permission recorded in the project.
#include "battle_internal.h"
#include "battle_util.h"
#include "../core.h"
#include "../hooks.h"
#include "../modtext.h"
#include "../say.h"
#include "../ui_util.h"
#include "../unity.h"
#include "../displayed_ui.h"
#include "../ui/preferences.h"

namespace ffpr::battle {
namespace {
ref::Class kItemCtl{"Last.UI.KeyInput", "BattleItemInfomationController"};
ref::Class kItemContent{"Last.UI.KeyInput", "BattleItemInfomationContentController"};
ref::Method mSelectContent{kItemCtl, "SelectContent", 2};
ref::Method mUseInit{kItemCtl, "UseSelectInit", 0}, mClose{kItemCtl, "Close", 0};
ref::Class kItem6{"Last.UI.KeyInput", "BattleItemInfomationController", game::kFF6};
ref::Field fMachine{kItem6, "isMachineState"};
void* g_item_owner = nullptr;
uint64_t g_focus_sequence = 0;

bool panel_ready(void* self) {
    return unity::alive(self) && displayed::container_visible(
        displayed::field(displayed::field(self, "view"), "rootObject"));
}
std::wstring item_announcement(void* self, void* cursor, int index, int& count) {
    count = -1;
    if (!panel_ready(self)) return {};
    const bool tools = game::is(game::Id::FF6) && ref::rd<bool>(self, fMachine);
    void* list = displayed::field(self, tools ? "machineContentList" : "contentList");
    const int slots = unity::list_count(list);
    if (slots < 0 || slots > 128) return {};
    void* content = unity::get_component_in_parent(cursor, kItemContent);
    if (!content && index >= 0 && index < slots) content = unity::list_at(list, index);
    void* view = displayed::field(content, "view");
    auto name = displayed::text_field(displayed::field(view, "IconTextView"), "nameText");
    if (name.empty()) name = displayed::text_field(displayed::field(view, "nonItemTextView"), "nameText");
    if (name.empty()) name = displayed::text_field(displayed::field(self, "view"), "noItemText");
    if (name.empty()) return {};
    const auto quantity = displayed::text_field(view, "countText");
    if (!quantity.empty()) name += L", " + quantity;
    count = tools ? slots : unity::list_count(displayed::field(self, "displayDataList"));
    return name;
}
void announce(void* self, void* cursor) {
    if (!cursor) return;
    const int index = ui::cursor_index(cursor);
    int count = -1;
    auto line = item_announcement(self, cursor, index, count);
    if (line.empty()) return;
    ++g_focus_sequence;
    g_item_owner = self;
    clear_other_menu_states(menu_state::BATTLE_ITEM);
    state_set(menu_state::BATTLE_ITEM, true);
    set_target_selection_active(false);
    notify_command_submenu_active();
    const auto description = displayed::text_field(displayed::field(self, "view"), "descriptionText");
    if (ui::prefs::auto_detail_enabled() && !description.empty()) line += L": " + description;
    ffpr::say(ui::menu_position(line, index, count), true);
}
void announce_current_description() {
    const auto description = panel_ready(g_item_owner) ?
        displayed::text_field(displayed::field(g_item_owner, "view"), "descriptionText") : std::wstring();
    ffpr::say(description.empty() ? T(L"No description") : description, true);
}
using Select = void (*)(void*, void*, int32_t, const void*);
Select o_SelectContent;
void d_SelectContent(void* self, void* cursor, int32_t type, const void* mi) {
    o_SelectContent(self, cursor, type, mi);
    hooks::guarded("BattleItem.SelectContent", [&] { announce(self, cursor); });
}
using Phase = void (*)(void*, const void*);
Phase o_use_init, o_close;
void d_use_init(void* self, const void* mi) {
    const auto before = g_focus_sequence;
    o_use_init(self, mi);
    hooks::guarded("BattleItem.UseSelectInit", [self, before] {
        // Even an empty Items panel owns the cancel-return event.
        if (panel_ready(self)) {
            g_item_owner = self; notify_command_submenu_active();
            // Native entry selects before enabling its root in some games.
            // Read that completed entry once if its nested focus was unready.
            if (before == g_focus_sequence) announce(self, displayed::field(self, "selectCursor"));
        }
    });
}
void d_close(void* self, const void* mi) {
    o_close(self, mi);
    hooks::guarded("BattleItem.Close", [self] {
        if (g_item_owner == self) { g_item_owner = nullptr; state_set(menu_state::BATTLE_ITEM, false); }
    });
}
int install() {
    actions::define("BattleItemPatches.AnnounceCurrentDescription", [] { announce_current_description(); });
    return (hooks::attach(mSelectContent, &d_SelectContent, &o_SelectContent) ? 1 : 0) +
        (hooks::attach(mUseInit, &d_use_init, &o_use_init) ? 1 : 0) +
        (hooks::attach(mClose, &d_close, &o_close) ? 1 : 0);
}
core::Install s_install{"Battle.Item", &install, 110};
}
}
