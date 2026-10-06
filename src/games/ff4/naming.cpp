// Ports FF4 Patches/NamingPatches.cs: the naming screen (Namingway and new characters),
// Last.UI.KeyInput.ChangeNameController. FF4 is the only game whose module hooks this class.
//
// NamingPatches.IsNamingMenuActive is the registry key "Naming" (ff4.h), so another menu state
// activating clears it (MenuStateRegistry.ClearOtherMenuStates called NamingPatches.ClearState).
//
// One line per event (2026-10-06): InitializeSelect (FF4 0x8B0BE0 +0x2D) and InitializeInput
// (+0x5E) call SelectContent themselves. The entry spoke the focused name and then the selected
// name; confirming a character spoke its name again before "Keyboard input active". Inside those
// two Inits SelectContent is silent; the InitializeSelect line falls back to the focused name.
#include <string>
#include "ff4.h"
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/hooks.h"
#include "../../core/modtext.h"
#include "../../core/say.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/system/cursor_event.h"
#include "../../core/dialogue/dialogue.h"

namespace ffpr::ff4 {
namespace {

ref::Class  kChangeName{"Last.UI.KeyInput", "ChangeNameController", game::kFF4};
ref::Field  fNameContentList{kChangeName, "contentList"};
ref::Field  fNameSelectedData{kChangeName, "selectedData"};
ref::Method mNameOpen{kChangeName, "Open", 0};
ref::Method mNameClose{kChangeName, "Close", 0};
ref::Method mNameSelectContent{kChangeName, "SelectContent", 1};
ref::Method mNameInitializeSelect{kChangeName, "InitializeSelect", 0};
ref::Method mNameInitializeInput{kChangeName, "InitializeInput", 0};
ref::Class  kChangeNameContent{"Last.UI.KeyInput", "ChangeNameContentController", game::kFF4};
ref::Field  fContentTargetData{kChangeNameContent, "targetData"};

// AnnouncementContexts.NAMING_SELECT (DEDUP_INVENTORY.md FF4 row 32): cursor index.
constexpr const char* kCtxNamingSelect = "Naming.Select";

// Set while InitializeSelect / InitializeInput run their own SelectContent call.
bool g_in_init = false;
int g_init_focus_index = -1;
std::wstring g_init_focus_name;

struct InitScope {
    bool was = g_in_init;
    InitScope()
    {
        g_in_init = true;
        g_init_focus_index = -1;
        g_init_focus_name.clear();
    }
    ~InitScope() { g_in_init = was; }
};

/// GetCharacterNameAtIndex: contentList[index].TargetData.Name.
std::wstring name_at(void* controller, int index)
{
    void* content = ui::list_item(ref::rd<void*>(controller, fNameContentList), index);
    void* data = content ? ref::rd<void*>(content, fContentTargetData) : nullptr;
    return character_name(data);
}

using Void_t = void (*)(void* self, const void* mi);
Void_t o_Open;
void d_Open(void* self, const void* mi)
{
    o_Open(self, mi);
    hooks::guarded("FF4 ChangeNameController.Open", [&] {
        set_active_exclusive(NAMING);
        dedup::reset(kCtxNamingSelect);
    });
}

Void_t o_Close;
void d_Close(void* self, const void* mi)
{
    o_Close(self, mi);
    hooks::guarded("FF4 ChangeNameController.Close", [&] { reset(NAMING); });   // the reset handler clears the guard
}

Void_t o_InitializeSelect;
void d_InitializeSelect(void* self, const void* mi)
{
    {
        InitScope scope;
        o_InitializeSelect(self, mi);
    }
    hooks::guarded("FF4 ChangeNameController.InitializeSelect", [&] {
        // The Init's own SelectContent was the focus read: the next move is new from its index.
        dedup::reset(kCtxNamingSelect);
        if (g_init_focus_index >= 0) dedup::index(kCtxNamingSelect, g_init_focus_index);
        std::wstring name = character_name(ref::rd<void*>(self, fNameSelectedData));
        if (name.empty()) name = g_init_focus_name;
        if (!name.empty()) ffpr::say(name, true);
    });
}

using SelectContent_t = void (*)(void* self, void* cursor, const void* mi);
SelectContent_t o_SelectContent;
void d_SelectContent(void* self, void* cursor, const void* mi)
{
    o_SelectContent(self, cursor, mi);
    hooks::guarded("FF4 ChangeNameController.SelectContent", [&] {
        if (!cursor) return;
        int index = ui::cursor_index(cursor);
        if (g_in_init) {   // the Init's own call: its postfix speaks the entry line
            g_init_focus_index = index;
            g_init_focus_name = name_at(self, index);
            return;
        }
        if (!dedup::index(kCtxNamingSelect, index)) return;
        std::wstring name = name_at(self, index);
        if (!name.empty()) ffpr::say(name, true);
    });
}

Void_t o_InitializeInput;
// Name entry: KeyboardManager.ShowKeyboard (called inside the input Init) opens the game's
// ChangeNamePopup, whose description ("Enter a name for X") the popup reader speaks. When that
// popup opened during the Init, the mod's own entry line is not spoken (2026-10-06, game text
// first); it remains for the path without the popup (Steam's gamepad text input).
void d_InitializeInput(void* self, const void* mi)
{
    const int popups_before = dialogue::change_name_popup_opens();
    {
        InitScope scope;
        o_InitializeInput(self, mi);
    }
    if (dialogue::change_name_popup_opens() != popups_before) return;   // the game's popup text is read
    hooks::guarded("FF4 ChangeNameController.InitializeInput", [&] { ffpr::say(T(L"Keyboard input active."), true); });
}

int install()
{
    // NamingPatches.ClearState
    menu_state::register_reset(NAMING, [] { dedup::reset(kCtxNamingSelect); });
    int n = 0;
    if (hooks::attach(mNameOpen, &d_Open, &o_Open)) ++n;
    if (hooks::attach(mNameClose, &d_Close, &o_Close)) ++n;
    if (hooks::attach(mNameInitializeSelect, &d_InitializeSelect, &o_InitializeSelect)) ++n;
    if (hooks::attach(mNameSelectContent, &d_SelectContent, &o_SelectContent)) ++n;
    if (hooks::attach(mNameInitializeInput, &d_InitializeInput, &o_InitializeInput)) ++n;
    queries::define("NamingPatches.ShouldSuppress", [] { return is_active(NAMING); });
    cursor::add_suppressor("NamingPatches.ShouldSuppress");
    return n;
}

core::Install s_install{"FF4.Naming", &install, 514, game::kFF4};

} // namespace
} // namespace ffpr::ff4
