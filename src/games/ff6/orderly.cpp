// Ports FF6 Patches/OrderlyMenuPatches.cs (OrderlyControllerBase.SelectCommand postfix): the sort
// submenu opened with Q / Square from the Skills (magic) list speaks each sort option with its
// description. The core has nothing for this window (no FF1-FF5 mod read it); the core's list
// conventions apply: the option's position is appended, and the existing text check
// (lastAnnouncement) is a Menu-scope dedup context like the other ported checks.
//
// The generic cursor reader stays silent on this window's cursor (the FF6 C# skipped every cursor
// under an "ability" object): "OrderlyMenuPatches.ShouldSuppress" is true while the cursor being
// read is the live controller's own selectCursor.
//
// SelectCommand's body is not folded in FF6 (RVA 0x74A680, 1 method).
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
#include "../../core/battle/battle_util.h"
#include "../../core/system/cursor_event.h"

namespace ffpr::ff6 {
namespace {

constexpr uint8_t k6 = game::kFF6;

ref::Class  kOrderly{"Last.UI.KeyInput", "OrderlyControllerBase", k6};
ref::Field  fCommandList{kOrderly, "commandList"};
ref::Field  fDescriptionText{kOrderly, "descriptionText"};
ref::Field  fSelectCursor{kOrderly, "selectCursor"};
ref::Method mSelectCommand{kOrderly, "SelectCommand", 1};
ref::Class  kOrderlyCommand{"Last.UI.KeyInput", "OrderlyCommandController", k6};
ref::Field  fCommandView{kOrderlyCommand, "view"};
ref::Class  kOrderlyCommandView{"Last.UI.KeyInput", "OrderlyCommandView", k6};
ref::Field  fViewNameText{kOrderlyCommandView, "nameText"};

// lastAnnouncement: the C#'s existing text check (never reset there; Menu scope here, as the
// other ported lastAnnounced* checks).
constexpr const char* kCtxOrderly = "Orderly.Select";
// The controller of the last SelectCommand (identity for the cursor suppressor; re-checked with
// unity::alive before any read).
void* g_controller = nullptr;

void on_select_command(void* self, int index)
{
    if (!self || !unity::active_in_hierarchy(self)) return;
    g_controller = self;

    void* list = ref::rd<void*>(self, fCommandList);
    int count = list ? unity::list_count(list) : 0;
    if (count == 0 || index < 0 || index >= count) return;
    void* command = unity::list_at(list, index);
    if (!command) return;

    // The option name from the view's text.
    void* view = ref::rd<void*>(command, fCommandView);
    std::wstring name = view ? unity::ui_text(ref::rd<void*>(view, fViewNameText)) : std::wstring();
    if (text::is_blank(name)) return;
    name = battle::strip_icon(name);
    if (text::is_blank(name)) return;

    std::wstring description = unity::ui_text(ref::rd<void*>(self, fDescriptionText));
    if (!text::is_blank(description)) description = battle::strip_icon(description);

    std::wstring announcement = text::is_blank(description) ? name : name + L", " + description;
    if (!dedup::text(kCtxOrderly, announcement)) return;
    // The core's list convention: the option's position (MenuPositionAnnouncements).
    ffpr::say(ui::menu_position(announcement, index, count), true);
}

using SelectCommand_t = void (*)(void* self, int32_t index, const void* mi);
SelectCommand_t o_SelectCommand;
void d_SelectCommand(void* self, int32_t index, const void* mi)
{
    o_SelectCommand(self, index, mi);
    hooks::guarded("FF6.OrderlyControllerBase.SelectCommand", [&] { on_select_command(self, index); });
}

/// The generic reader is handling the sort window's own cursor.
bool should_suppress()
{
    void* cur = cursor::current().cursor;
    if (!cur || !g_controller || !unity::alive(g_controller)) return false;
    return cur == ref::rd<void*>(g_controller, fSelectCursor);
}

int install()
{
    int n = hooks::attach(mSelectCommand, &d_SelectCommand, &o_SelectCommand) ? 1 : 0;
    queries::define("OrderlyMenuPatches.ShouldSuppress", [] {
        return hooks::guarded_r("FF6.Orderly.ShouldSuppress", false, [] { return should_suppress(); });
    });
    cursor::add_suppressor("OrderlyMenuPatches.ShouldSuppress");
    return n;
}

core::Install s_install{"FF6.Orderly", &install, 520, game::kFF6};

} // namespace
} // namespace ffpr::ff6
