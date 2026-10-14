#include "../../core/displayed_ui.h"
// Ports FF6 Patches/StatusDetailsPatches.cs (StatusWindowController_SelectContent_Patch: the
// Status menu's character list, read through Menus/CharacterSelectionReader.cs).
//
// The core reads this list for FF1-FF5 (src/core/status/status_menu.cpp, FF1's
// StatusMenuPatches). FF6 is the same reader with one addition, the character's row ("Front Row"
// / "Back Row"), which the FF6 C# spoke on every character list of the main menu (Skills, Equip,
// Relics, Status, Row): on the Row command it is the only way to hear a character's current row.
// The core's SelectContent hook is therefore masked to FF1-FF5 and this file is FF6's variant:
// the core's gate, menu-state handling and wording, with the row appended last (as FF4's native
// character_row.cpp does for the generic reader).
//
// SelectContent's body is not folded in FF6 (RVA 0x909C70, 1 method).
#include <string>
#include "../../core/core.h"
#include "../../core/hooks.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/unity.h"
#include "../../core/status/status.h"
#include "../../core/system/system.h"

namespace ffpr::ff6 {
namespace {

constexpr uint8_t k6 = game::kFF6;

ref::Class  kMenuManager{"Last.UI", "MenuManager", k6};
ref::Method mMenuIsOpen{kMenuManager, "get_IsOpen", 0};
ref::Class  kStatusWindow{"Last.UI.KeyInput", "StatusWindowController", k6};
ref::Method mSelectContent{kStatusWindow, "SelectContent", 3};
ref::Class  kStatusContent{"Serial.Template.UI", "StatusWindowContentControllerBase", k6};
ref::Field  fContentCharacterData{kStatusContent, "<CharacterData>k__BackingField"};
ref::Class  kParamBase{"Last.Data", "CharacterParameterBase", k6};
ref::Field  fParamCurrentMp{kParamBase, "currentMP"};
ref::Class  kPlayerParam{"Last.Data", "PlayerCharacterParameter", k6};
ref::Method mConfirmedMaxMp{kPlayerParam, "ConfirmedMaxMp", 0};

// StatusMenuPatches.SelectContent_Postfix (core) + the FF6 row.
void on_select_content(void* contents, int index, void* cursor, void* window = nullptr)
{
    // Only announce when the menu is actually open (SelectContent also fires during game load).
    void* menu_manager = ref::instance(kMenuManager);
    if (!menu_manager || !ref::call<bool>(mMenuIsOpen, menu_manager)) return;

    std::vector<void*> slots;
    if (!unity::enumerate_bounded(contents, slots, 9)) return;
    const int count = static_cast<int>(slots.size());
    if (count == 0) return;
    if (index < 0 || index >= count) return;
    void* content = displayed::cursor_row(cursor, contents);
    if (!content && cursor) content = unity::get_component_in_parent(cursor, kStatusContent);
    if (!content) content = slots[static_cast<size_t>(index)];
    if (!content) return;
    if (!displayed::container_visible(displayed::field(content, "view"))) return;
    void* data = ref::rd<void*>(content, fContentCharacterData);

    // Menu confirmed open with valid data: clear the other menu states, then set this one.
    ffpr::system::clear_other_menu_states("Status");
    menu_state::set_active(menu_state::STATUS_MENU, true);
    if (!data) { ffpr::say(T(L"Empty"), true); return; }

    void* view = displayed::field(content, "view");
    std::wstring announcement = displayed::target(content);
    if (announcement.empty()) return;
    auto job = displayed::text_field(view, "jobNameText");
    auto level = displayed::pair(view, "fixedLevelText", "currentLevelText");
    auto next = displayed::pair(view, "fixedNextText", "nextExpText");
    if (!job.empty()) announcement += L", " + job;
    if (!level.empty()) announcement += L", " + level;
    if (!next.empty()) announcement += L", " + next;
    auto row = displayed::formation_row(window, content);
    if (!row.empty()) announcement += L", " + row;

    ffpr::say(announcement, true);
}

using SelectContent_t = void (*)(void* self, void* contents, int32_t index, void* cursor, const void* mi);
SelectContent_t o_SelectContent;
void d_SelectContent(void* self, void* contents, int32_t index, void* cursor, const void* mi)
{
    o_SelectContent(self, contents, index, cursor, mi);
    hooks::guarded("FF6.StatusWindowController.SelectContent", [&] { on_select_content(contents, index, cursor, self); });
}

int install()
{
    return hooks::attach(mSelectContent, &d_SelectContent, &o_SelectContent) ? 1 : 0;
}

core::Install s_install{"FF6.StatusList", &install, 520, game::kFF6};

} // namespace
} // namespace ffpr::ff6
