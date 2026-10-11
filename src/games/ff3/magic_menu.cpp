#include "../../core/displayed_ui.h"
// Ports FF3 Patches/MagicMenuPatches.cs (MagicMenuState, MagicMenuPatches) and
// Menus/AbilityCommandReader.cs, on FF3's own classes (Serial.FF3.UI.KeyInput).
//
// The field magic menu: the command bar (Use / Forget / Learn / Remove / Exchange), the spell
// lists of the Use, Remove, Forget and Exchange screens, the spell tomes of the Learn screen,
// and the target list. The core's magic list, target list and magic command bar are FF1's
// (Serial.FF1 classes, game::kFF1); the shared MagicMenuState (registry key MAGIC_MENU, the
// I-key action, menus.h helpers) is the core's.
//
// Parity decisions (FF1 is the gold standard for mod-side wording):
//  * Spell line as FF1: "Fire LV1: MP: 3/5: description (2 of 8)" (FF3: "Fire: MP: 3/5. desc").
//    Charges per level are the same data in both games (CurrentMpCountList, ConfirmedMaxMpCount).
//  * Dedup as FF1: the full spoken line in "Magic.List", reset when a list gains focus; Empty is
//    not recorded (FF3 stored -1, so a second Empty in a row was silent: DEDUP_INVENTORY finding 7).
//  * Targets as FF1: "Name: HP: a / b status (n of N)", no text check (FF3's "MagicTarget" text
//    check was never reset).
//  * The command bar as FF1's CommandBarPatches (focused command with its position on open, on
//    each move and on return from a list), names from AbilityCommandId; FF3 shows Memorize as
//    "Learn" (AbilityCommandReader), so that id reads T("Learn"). This replaces FF3's generic-path
//    AbilityCommandReader.
//  * Spell tomes (FF3 only): "Name: description" (description with Auto Detail), as FF3.
//  * The I key reads the last focused description (spell or tome), as FF3: the core's
//    "MagicDetailsAnnouncer.AnnounceCurrentSpellDescription" is replaced for FF3.
//
// Hooks: AbilityWindowController UseListInit / MemorizeListSelectInit / RemoveListInit /
// ForgetInit / ExchangeInit / CommandInit / SetActive, AbilityContentListController.SetCursor,
// AbilityUseContentListController.SetCursor, AbilityCommandController.SetCommandSelectCursor:
// unique RVAs. AbilityWindowController.SetNextState (0x2A0B70) is shared by 9 methods:
// class-checked.
#include "../../core/ui/preferences.h"
#include <cstdint>
#include <string>
#include <vector>
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/frame.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/settings.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/menus/menus.h"
#include "../../core/status/status.h"
#include "../../core/system/cursor_event.h"
#include "../../core/system/system.h"

namespace ffpr::ff3 {
namespace {

ref::Class kWindow{"Serial.FF3.UI.KeyInput", "AbilityWindowController", game::kFF3};
ref::Field fWindowCommand{kWindow, "commandController"};
ref::Field fWindowList{kWindow, "listController"};
ref::Field fWindowState{kWindow, "stateMachine"};
ref::Method mUseListInit{kWindow, "UseListInit", 0};
ref::Method mMemorizeListInit{kWindow, "MemorizeListSelectInit", 0};
ref::Method mRemoveListInit{kWindow, "RemoveListInit", 0};
ref::Method mForgetInit{kWindow, "ForgetInit", 0};
ref::Method mExchangeInit{kWindow, "ExchangeInit", 0};
ref::Method mCommandInit{kWindow, "CommandInit", 0};
ref::Method mSetNextState{kWindow, "SetNextState", 1};
ref::Method mWindowSetActive{kWindow, "SetActive", 1};

ref::Class kList{"Serial.FF3.UI.KeyInput", "AbilityContentListController", game::kFF3};
ref::Field fListContents{kList, "contentList"};
ref::Field fListItems{kList, "abilityItemList"};
ref::Field fListCharacter{kList, "targetCharacterData"};
ref::Field fListWindow{kList, "abilityWindowController"};
ref::Method mListSetCursor{kList, "SetCursor", 3};

ref::Class kUseList{"Serial.FF3.UI.KeyInput", "AbilityUseContentListController", game::kFF3};
ref::Field fUseContents{kUseList, "contentList"};
ref::Method mUseSetCursor{kUseList, "SetCursor", 1};

ref::Class kCommand{"Serial.FF3.UI.KeyInput", "AbilityCommandController", game::kFF3};
ref::Field fCommandCursor{kCommand, "selectCursor"};
ref::Field fCommandContents{kCommand, "contentList"};
ref::Method mSetCommandSelectCursor{kCommand, "SetCommandSelectCursor", 0};
ref::Class kCommandView{"Serial.FF3.UI.KeyInput", "AbilityCommandContentView", game::kFF3};
ref::Field fCommandViewData{kCommandView, "<Data>k__BackingField"};
ref::Class kCommandData{"Last.UI", "AbilityCommandContentData", game::kFF3};
ref::Field fCommandDataId{kCommandData, "<Id>k__BackingField"};

ref::Class kAbilityContent{"Last.UI.KeyInput", "BattleAbilityInfomationContentController", game::kFF3};
ref::Field fAbilityContentData{kAbilityContent, "<Data>k__BackingField"};
ref::Class kTargetContent{"Last.UI.KeyInput", "ItemTargetSelectContentController", game::kFF3};
ref::Field fTargetContentData{kTargetContent, "<CurrentData>k__BackingField"};

ref::Class kOwnedItem{"Last.Data.User", "OwnedItemData", game::kFF3};
ref::Method mItemName{kOwnedItem, "get_Name", 0};
ref::Method mItemDescription{kOwnedItem, "get_Deiscription", 0};   // (sic)

ref::Class kParamBase{"Last.Data", "CharacterParameterBase", game::kFF3};
ref::Method mGetConditionList{kParamBase, "get_CurrentConditionList", 0};

// AbilityWindowController.State (FF3).
constexpr int kStateNone = 0;
constexpr int kStateUseList = 1;
constexpr int kStateMemorizeList = 3;
constexpr int kStateRemoveList = 4;
constexpr int kStateCommand = 7;

constexpr const char* kCtxSpell = "Magic.List";
constexpr const char* kCtxCommand = "Magic.Command";
constexpr int kCommandReadMaxFrames = 10;

void* g_character = nullptr;        // MagicMenuState.CurrentCharacter (party data)
std::wstring g_last_description;    // MagicMenuState.LastFocusedDescription
int g_command_gen = 0;

bool auto_detail() { return ui::prefs::auto_detail_enabled(); }
std::wstring strip(const std::wstring& s) { return text::trim(text::strip_markup(s)); }
bool spell_list_active() { return menu_state::is_active(menu_state::MAGIC_MENU); }

void* live_window(void* candidate)
{
    return (candidate && kWindow.is(candidate) && unity::alive(candidate) && unity::active_in_hierarchy(candidate))
        ? candidate : nullptr;
}

/// GetWindowControllerState: the window's state tag, -1 when it is not shown.
int window_state(void* window)
{
    window = live_window(window);
    return window ? ui::state_tag(window, fWindowState) : -1;
}

void* find_window() { return unity::find_object(kWindow); }

void reset_state() { menu_state::set_active(menu_state::MAGIC_MENU, false); }

/// MagicMenuState.ShouldSuppress.
bool should_suppress()
{
    if (!spell_list_active()) return false;
    const int state = window_state(find_window());
    if (state == kStateCommand || state == kStateNone) {
        reset_state();
        return false;
    }
    return true;
}

// --- spell list ----------------------------------------------------------------------------------

void announce_empty(int index, int count)
{
    g_last_description.clear();
    ffpr::say(ui::menu_position(T(L"Empty"), index, count), true);
}

void announce_spell(void* ability, void* content, int index, int count)
{
    std::wstring line = displayed::spell_row(content);
    if (line.empty()) return;
    const std::wstring description = ffpr::menus::spell_description(ability);
    if (auto_detail() && !text::is_blank(description)) line += L": " + description;
    line = ui::menu_position(line, index, count);
    if (!dedup::text(kCtxSpell, line)) return;
    g_last_description = description;
    ffpr::say(line, true);
}

/// TryAnnounceFromContentList: the spell slots (Use, Remove, Forget, Exchange).
bool try_content_list(void* list_ctrl, int index)
{
    void* list = ref::rd<void*>(list_ctrl, fListContents);
    const int count = unity::list_count(list);
    if (count == 0 || index < 0 || index >= count) return false;
    void* content = unity::list_at(list, index);
    void* ability = content ? ref::rd<void*>(content, fAbilityContentData) : nullptr;
    if (!ability) announce_empty(index, count);
    else announce_spell(ability, content, index, count);
    return true;
}

/// TryAnnounceFromItemList: the spell tomes of the Learn screen.
bool try_item_list(void* list_ctrl, int index)
{
    void* list = ref::rd<void*>(list_ctrl, fListItems);
    const int count = unity::list_count(list);
    if (count == 0 || index < 0 || index >= count) return false;
    void* item = unity::list_at(list, index);
    if (!item) {
        announce_empty(index, count);
        return true;
    }
    const std::wstring name = strip(ref::call_str(mItemName, item));
    const std::wstring description = ref::call_str(mItemDescription, item);
    if (name.empty()) {
        announce_empty(index, count);
        return true;
    }
    std::wstring line = name;   // AnnouncementBuilder.FormatWithDescription(name, desc, ": ")
    if (auto_detail() && !text::is_blank(description)) line += L": " + strip(description);
    line = ui::menu_position(line, index, count);
    if (!dedup::text(kCtxSpell, line)) return true;
    g_last_description = strip(description);
    ffpr::say(line, true);
    return true;
}

// SetCursor_Postfix (AbilityContentListController)
void on_list_set_cursor(void* self, void* cursor)
{
    if (!spell_list_active() || !self || !cursor) return;
    if (!unity::active_in_hierarchy(self)) return;
    const int index = ui::cursor_index(cursor);
    const int state = window_state(ref::rd<void*>(self, fListWindow));
    if (state == kStateMemorizeList) try_item_list(self, index);
    else if (state == kStateUseList || state == kStateRemoveList) try_content_list(self, index);
    else if (!try_content_list(self, index)) try_item_list(self, index);
}

// SpellListStateInit_Postfix (Use / Learn / Remove / Forget / Exchange entry)
void on_spell_list_init(void* window)
{
    if (!window) return;
    if (!spell_list_active()) {
        menu_state::set_active_exclusive(menu_state::MAGIC_MENU);   // OnSpellListFocused
        dedup::reset(kCtxSpell);
    }
    void* list = ref::rd<void*>(window, fWindowList);
    if (void* character = list ? ref::rd<void*>(list, fListCharacter) : nullptr) g_character = character;
}

// SetNextState_Postfix: back to the command bar (or closed) leaves the spell list.
void on_set_next_state(void* self, int state)
{
    if (!kWindow.is(self)) return;   // folded body
    if ((state == kStateCommand || state == kStateNone) && spell_list_active()) reset_state();
}

// --- targets (AbilityUseContentListController.SetCursor) -----------------------------------------

void on_use_set_cursor(void* self, void* cursor)
{
    if (!self || !cursor) return;
    const int index = ui::cursor_index(cursor);
    void* list = ref::rd<void*>(self, fUseContents);
    const int count = unity::list_count(list);
    if (index < 0 || index >= count) return;
    void* content = unity::list_at(list, index);
    void* character = content ? ref::rd<void*>(content, fTargetContentData) : nullptr;
    if (!character) return;
    std::wstring line = displayed::target(content);
    if (line.empty()) return;
    ffpr::say(ui::menu_position(line, index, count), true);
}

// --- command bar (FF1 CommandBarPatches, magic bar) ------------------------------------------------

std::wstring command_name(int id)
{
    if (id == 3) return T(L"Learn");   // AbilityCommandId.Memorize, shown as "Learn" in FF3
    return ffpr::system::ability_command_name(id);
}

/// False only to retry (the focused command's data is not set yet).
bool try_read_command(void* ctrl)
{
    if (!unity::alive(ctrl) || !unity::active_in_hierarchy(ctrl)) return true;
    // Not on the field: the UI pre-load initialises the window in its command state there
    // (core game_state.cpp, IsFieldState).
    if (ffpr::system::is_field_state() || window_state(find_window()) != kStateCommand) {
        dedup::reset(kCtxCommand);
        return true;
    }
    void* cur = ref::rd<void*>(ctrl, fCommandCursor);
    void* list = ref::rd<void*>(ctrl, fCommandContents);
    if (!cur || !list) return false;
    const int index = ui::cursor_index(cur);
    const int count = unity::list_count(list);
    if (index < 0 || index >= count) return false;
    void* view = unity::list_at(list, index);
    void* data = view ? ref::rd<void*>(view, fCommandViewData) : nullptr;
    if (!data) return false;
    const std::wstring name = command_name(ref::rd<int32_t>(data, fCommandDataId));
    if (name.empty()) return false;
    if (!dedup::index(kCtxCommand, index, dedup::Scope::Session)) return true;
    ffpr::system::announce_command(name, index, count);
    return true;
}

void schedule_command_read(void* ctrl)
{
    if (!ctrl) return;
    const int gen = ++g_command_gen;
    int tries = 0;
    frame::every_frame([gen, ctrl, tries]() mutable {
        if (gen != g_command_gen) return false;
        const bool done = hooks::guarded_r("FF3.Magic.CommandRead", true, [&] { return try_read_command(ctrl); });
        return !done && ++tries < kCommandReadMaxFrames;
    }, "FF3.Magic.Command", kCommandReadMaxFrames);
}

bool command_bar_active() { return window_state(find_window()) == kStateCommand; }

// --- detours -------------------------------------------------------------------------------------

using Void_t = void (*)(void* self, const void* mi);
using Int_t = void (*)(void* self, int32_t value, const void* mi);
using Bool_t = void (*)(void* self, bool value, const void* mi);
using Cursor_t = void (*)(void* self, void* cursor, const void* mi);
using ListCursor_t = void (*)(void* self, void* cursor, bool is_scroll, int32_t range_type, const void* mi);
Void_t o_UseListInit, o_MemorizeListInit, o_RemoveListInit, o_ForgetInit, o_ExchangeInit, o_CommandInit;
Void_t o_SetCommandSelectCursor;
Int_t o_SetNextState;
Bool_t o_WindowSetActive;
ListCursor_t o_ListSetCursor;
Cursor_t o_UseSetCursor;

void list_init(void* self) { hooks::guarded("FF3.Magic.ListInit", [&] { on_spell_list_init(self); }); }
void d_UseListInit(void* self, const void* mi) { o_UseListInit(self, mi); list_init(self); }
void d_MemorizeListInit(void* self, const void* mi) { o_MemorizeListInit(self, mi); list_init(self); }
void d_RemoveListInit(void* self, const void* mi) { o_RemoveListInit(self, mi); list_init(self); }
void d_ForgetInit(void* self, const void* mi) { o_ForgetInit(self, mi); list_init(self); }
void d_ExchangeInit(void* self, const void* mi) { o_ExchangeInit(self, mi); list_init(self); }

void d_CommandInit(void* self, const void* mi)
{
    o_CommandInit(self, mi);
    hooks::guarded("FF3.Magic.CommandInit", [&] {
        dedup::reset(kCtxCommand);
        if (kWindow.is(self)) schedule_command_read(ref::rd<void*>(self, fWindowCommand));
    });
}

void d_SetCommandSelectCursor(void* self, const void* mi)
{
    o_SetCommandSelectCursor(self, mi);
    hooks::guarded("FF3.Magic.CommandCursor", [&] { if (kCommand.is(self)) schedule_command_read(self); });
}

void d_SetNextState(void* self, int32_t state, const void* mi)
{
    o_SetNextState(self, state, mi);
    hooks::guarded("FF3.Magic.SetNextState", [&] { on_set_next_state(self, state); });
}

void d_WindowSetActive(void* self, bool active, const void* mi)
{
    o_WindowSetActive(self, active, mi);
    if (!active) hooks::guarded("FF3.Magic.SetActive", [] { reset_state(); });
}

void d_ListSetCursor(void* self, void* cursor, bool is_scroll, int32_t range_type, const void* mi)
{
    o_ListSetCursor(self, cursor, is_scroll, range_type, mi);
    hooks::guarded("FF3.Magic.ListSetCursor", [&] { on_list_set_cursor(self, cursor); });
}

void d_UseSetCursor(void* self, void* cursor, const void* mi)
{
    o_UseSetCursor(self, cursor, mi);
    hooks::guarded("FF3.Magic.UseSetCursor", [&] { on_use_set_cursor(self, cursor); });
}

int install()
{
    int n = 0;
    n += hooks::attach(mUseListInit, &d_UseListInit, &o_UseListInit) ? 1 : 0;
    n += hooks::attach(mMemorizeListInit, &d_MemorizeListInit, &o_MemorizeListInit) ? 1 : 0;
    n += hooks::attach(mRemoveListInit, &d_RemoveListInit, &o_RemoveListInit) ? 1 : 0;
    n += hooks::attach(mForgetInit, &d_ForgetInit, &o_ForgetInit) ? 1 : 0;
    n += hooks::attach(mExchangeInit, &d_ExchangeInit, &o_ExchangeInit) ? 1 : 0;
    n += hooks::attach(mCommandInit, &d_CommandInit, &o_CommandInit) ? 1 : 0;
    n += hooks::attach(mSetCommandSelectCursor, &d_SetCommandSelectCursor, &o_SetCommandSelectCursor) ? 1 : 0;
    n += hooks::attach(mSetNextState, &d_SetNextState, &o_SetNextState) ? 1 : 0;
    n += hooks::attach(mWindowSetActive, &d_WindowSetActive, &o_WindowSetActive) ? 1 : 0;
    n += hooks::attach(mListSetCursor, &d_ListSetCursor, &o_ListSetCursor) ? 1 : 0;
    n += hooks::attach(mUseSetCursor, &d_UseSetCursor, &o_UseSetCursor) ? 1 : 0;
    LOGF("[FF3 Magic] %d of 11 hooks", n);

    menu_state::register_reset(menu_state::MAGIC_MENU, [] {
        g_character = nullptr;
        g_last_description.clear();
    });
    queries::define("MagicMenuState.ShouldSuppress", [] { return should_suppress(); });
    queries::define("MagicMenuState.IsSpellListActive", [] { return spell_list_active(); });
    queries::define("FF3.AbilityCommandBar.IsActive", [] { return command_bar_active(); });
    cursor::add_suppressor("FF3.AbilityCommandBar.IsActive");
    // The I key: the last focused spell or tome description (replaces the core's for FF3).
    actions::define("MagicDetailsAnnouncer.AnnounceCurrentSpellDescription", [] {
        ffpr::say(text::is_blank(g_last_description) ? T(L"No description") : text::trim(g_last_description), true);
    });
    return n;
}

core::Install s_install{"FF3.MagicMenu", &install, 505, game::kFF3};

} // namespace
} // namespace ffpr::ff3
