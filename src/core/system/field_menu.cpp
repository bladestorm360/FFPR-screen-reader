// Ports FF1 Patches/FieldMenuPatches.cs: the field (main) menu's focused command on open and on
// every return to command select (from a sub-menu or the quick-save popup), and the command count
// for the generic reader's "(n of N)".
//
// Navigation already flows through the generic cursor reader; the initial cursor placement never
// fires Cursor.NextIndex, so MainMenuController.Show and InitNone reuse that reader. Gated on
// MenuManager.IsOpen so it never reads during a map load's scene construction. (Not
// MainMenuController.focusId: that is the SELECTED command, not the focused one.)
#include "core.h"
#include "frame.h"
#include "hooks.h"
#include "log.h"
#include "refs.h"
#include "unity.h"
#include "system/system.h"

namespace ffpr::system {
namespace {

ref::Class kMainMenu{"Last.UI.KeyInput", "MainMenuController"};
ref::Class kCommandMenu{"Last.UI", "CommandMenuController"};
ref::Class kMenuManager{"Last.UI", "MenuManager"};
ref::Field fCommandMenuController{kMainMenu, "commandMenuController"};
ref::Field fCommandMenuCursor{kCommandMenu, "selectCursor"};
ref::Field fCommandMenuContents{kCommandMenu, "contents"};
ref::Method mShow{kMainMenu, "Show", 1};
ref::Method mInitNone{kMainMenu, "InitNone", 0};
ref::Method mMenuIsOpen{kMenuManager, "get_IsOpen", 0};

// Bumped on every announce_focus; a delayed read aborts when a newer call superseded it. On open
// both Show and InitNone fire within a frame or two: the latch collapses them to one announce
// (the later wins) while every sub-menu back-out still re-announces.
int g_gen = 0;

// MenuManager.IsOpen: reliably false during a map/asset load.
bool is_field_menu_open()
{
    void* mm = ref::instance(kMenuManager);
    return mm && ref::call<bool>(mMenuIsOpen, mm);
}

// FieldMenuReader.AnnounceFocus + DelayedAnnounce
void announce_focus(void* inst)
{
    if (!inst) return;
    const int gen = ++g_gen;
    frame::next_frame([inst, gen] {
        if (gen != g_gen) return;   // a newer announce_focus superseded this one
        void* cur = nullptr;
        hooks::guarded("FieldMenu.DelayedAnnounce", [&] {
            if (unity::alive(inst) && unity::active_in_hierarchy(inst) && is_field_menu_open()) {
                if (void* cmd = ref::rd<void*>(inst, fCommandMenuController))
                    cur = ref::rd<void*>(cmd, fCommandMenuCursor);
            }
        });
        if (cur) wait_and_read_cursor(cur);
    }, "FieldMenu.DelayedAnnounce");
}

using Show_t = void (*)(void* self, bool is_save_point, const void* mi);
using Void_t = void (*)(void* self, const void* mi);
Show_t o_Show;
Void_t o_InitNone;

void d_Show(void* self, bool is_save_point, const void* mi)
{
    o_Show(self, is_save_point, mi);
    hooks::guarded("FieldMenu.Show", [&] { if (kMainMenu.is(self)) announce_focus(self); });
}

// Return to command select (None) from any sub-menu or the quick-save popup.
void d_InitNone(void* self, const void* mi)
{
    o_InitNone(self, mi);
    hooks::guarded("FieldMenu.InitNone", [&] { if (kMainMenu.is(self)) announce_focus(self); });
}

int install()
{
    int n = 0;
    if (hooks::attach(mShow, &d_Show, &o_Show)) ++n;
    if (hooks::attach(mInitNone, &d_InitNone, &o_InitNone)) ++n;
    return n;
}

core::Install s_install{"FieldMenu", &install, 100};

} // namespace

// FieldMenuReader.TryGetFieldCommandCount: the field menu's command count (its C# list), or -1
// when `cursor` is not that menu's cursor (inert on every other menu).
int field_command_count(void* cursor)
{
    return hooks::guarded_r("FieldMenu.CommandCount", -1, [&]() -> int {
        if (!cursor) return -1;
        void* cmd = unity::find_object(kCommandMenu);
        if (!cmd || !unity::active_in_hierarchy(cmd)) return -1;
        void* field_cursor = ref::rd<void*>(cmd, fCommandMenuCursor);
        if (!field_cursor || field_cursor != cursor) return -1;
        void* contents = ref::rd<void*>(cmd, fCommandMenuContents);
        return contents ? unity::list_count(contents) : -1;
    });
}

} // namespace ffpr::system
