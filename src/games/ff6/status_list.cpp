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
#include "status.h"
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
void on_select_content(void* contents, int index)
{
    // Only announce when the menu is actually open (SelectContent also fires during game load).
    void* menu_manager = ref::instance(kMenuManager);
    if (!menu_manager || !ref::call<bool>(mMenuIsOpen, menu_manager)) return;

    int count = unity::list_count(contents);
    if (!contents || count == 0) return;
    if (index < 0 || index >= count) return;
    void* content = unity::list_at(contents, index);
    if (!content) return;
    void* data = ref::rd<void*>(content, fContentCharacterData);
    if (!data) return;

    // Menu confirmed open with valid data: clear the other menu states, then set this one.
    ffpr::system::clear_other_menu_states("Status");
    menu_state::set_active(menu_state::STATUS_MENU, true);

    void* view = displayed::field(content, "view");
    std::wstring announcement = displayed::target(content);
    if (announcement.empty()) return;
    auto job = displayed::text_field(view, "jobNameText");
    auto level = displayed::pair(view, "levelLabelText", "levelValueText");
    if (!job.empty()) announcement += L", " + job;
    if (!level.empty()) announcement += L", " + level;

    // FF6: the row of the slot (CharacterSelectionReader, corpsList[slotIndex]).
    announcement += row_suffix(index);

    ffpr::say(announcement, true);
}

using SelectContent_t = void (*)(void* self, void* contents, int32_t index, void* cursor, const void* mi);
SelectContent_t o_SelectContent;
void d_SelectContent(void* self, void* contents, int32_t index, void* cursor, const void* mi)
{
    o_SelectContent(self, contents, index, cursor, mi);
    hooks::guarded("FF6.StatusWindowController.SelectContent", [&] { on_select_content(contents, index); });
}

int install()
{
    return hooks::attach(mSelectContent, &d_SelectContent, &o_SelectContent) ? 1 : 0;
}

core::Install s_install{"FF6.StatusList", &install, 520, game::kFF6};

} // namespace
} // namespace ffpr::ff6
