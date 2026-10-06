// Ports FF5 Patches/NamingPatches.cs: the Name Change screen (Last.UI.KeyInput.ChangeNameController).
// "Name: {character}" on entering the character select, the focused character on each move,
// "Keyboard input active" on entering text input; NamingMenu state from Open/Close.
//
// The C# read contentList by reflection with a raw-offset fallback; here the same fields are
// read by name (contentList -> ChangeNameContentController.targetData -> OwnedCharacterData.Name;
// selectedData for the entry line). Its inputField text fallback is unreachable (the KeyInput
// controller has no inputField member) and is not ported. NamingPatches.ShouldSuppress fed FF5's
// own MessageWindowView.SetMessage reader, which the core's dialogue reader replaces; it is
// published as the query "NamingPatches.ShouldSuppress".
#include <cstdint>
#include <string>
#include "ff5_internal.h"
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/hooks.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/ui_util.h"
#include "../../core/battle/battle_util.h"

namespace ffpr::ff5 {
namespace {

constexpr uint8_t k5 = game::kFF5;

ref::Class  kChangeName{"Last.UI.KeyInput", "ChangeNameController", k5};
ref::Field  fContentList{kChangeName, "contentList"};
ref::Field  fSelectedData{kChangeName, "selectedData"};
ref::Method mOpen{kChangeName, "Open", 0};
ref::Method mClose{kChangeName, "Close", 0};
ref::Method mSelectContent{kChangeName, "SelectContent", 1};
ref::Method mInitializeSelect{kChangeName, "InitializeSelect", 0};
ref::Method mInitializeInput{kChangeName, "InitializeInput", 0};
ref::Class  kChangeNameContent{"Last.UI.KeyInput", "ChangeNameContentController", k5};
ref::Field  fContentTargetData{kChangeNameContent, "targetData"};

constexpr const char* kCtxCursor = "FF5.ChangeNameController.SelectContent";   // _lastCursorIndex

void set_active(bool active)
{
    menu_state::set_active(NAMING_MENU, active);
    dedup::reset(kCtxCursor);
}

std::wstring name_at(void* controller, int index)
{
    void* content = ui::list_item(ref::rd<void*>(controller, fContentList), index);
    void* data = content ? ref::rd<void*>(content, fContentTargetData) : nullptr;
    return data ? battle::character_name(data) : std::wstring();
}

using V_t = void (*)(void* self, const void* mi);
using VP_t = void (*)(void* self, void* p, const void* mi);

V_t o_Open;
void d_Open(void* self, const void* mi)
{
    o_Open(self, mi);
    set_active(true);
}

V_t o_Close;
void d_Close(void* self, const void* mi)
{
    o_Close(self, mi);
    set_active(false);
}

V_t o_InitializeSelect;
void d_InitializeSelect(void* self, const void* mi)
{
    o_InitializeSelect(self, mi);
    hooks::guarded("FF5.ChangeName.InitializeSelect", [&] {
        if (!self) return;
        dedup::reset(kCtxCursor);   // the SelectContent right after this also speaks
        void* selected = ref::rd<void*>(self, fSelectedData);
        std::wstring name = selected ? battle::character_name(selected) : std::wstring();
        if (!name.empty()) ffpr::say(TF(L"Name: {0}", {name}), true);
    });
}

VP_t o_SelectContent;
void d_SelectContent(void* self, void* cursor, const void* mi)
{
    o_SelectContent(self, cursor, mi);
    hooks::guarded("FF5.ChangeName.SelectContent", [&] {
        if (!cursor) return;
        int index = cursor_index(cursor);
        if (!dedup::index(kCtxCursor, index, dedup::Scope::Session)) return;
        std::wstring name = name_at(self, index);
        if (!name.empty()) ffpr::say(name, true);
    });
}

V_t o_InitializeInput;
void d_InitializeInput(void* self, const void* mi)
{
    o_InitializeInput(self, mi);
    ffpr::say(T(L"Keyboard input active"), true);
}

int install()
{
    int n = 0;
    n += hooks::attach(mSelectContent, &d_SelectContent, &o_SelectContent) ? 1 : 0;
    n += hooks::attach(mInitializeInput, &d_InitializeInput, &o_InitializeInput) ? 1 : 0;
    n += hooks::attach(mInitializeSelect, &d_InitializeSelect, &o_InitializeSelect) ? 1 : 0;
    n += hooks::attach(mOpen, &d_Open, &o_Open) ? 1 : 0;
    n += hooks::attach(mClose, &d_Close, &o_Close) ? 1 : 0;
    menu_state::register_reset(NAMING_MENU, [] { dedup::reset(kCtxCursor); });
    queries::define("NamingPatches.ShouldSuppress", [] { return menu_state::is_active(NAMING_MENU); });
    actions::define("NamingPatches.ClearState", [] { set_active(false); });
    return n;
}

core::Install s_install{"FF5.Naming", &install, 510, game::kFF5};

} // namespace
} // namespace ffpr::ff5
