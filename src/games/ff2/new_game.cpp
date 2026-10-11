// Ports FF2 Patches/NewGameNamingPatches.cs (NewGameNamingState and NewGameNamingPatches) and
// the NewGameNaming entry of Core/CursorSuppressionCheck.cs.
//
// New game: naming the four characters (Serial.FF2.UI.KeyInput.NewGameWindowController and the
// Last.UI.KeyInput lists it drives). FF2 has no class choice; each character has suggested names
// to cycle through. The generic cursor reader is silent in the Select and NameSelect states; the
// keyboard entry, the start popup (its Yes/No IS read by the generic reader), the error popup and
// leaving the screen hand the cursor back.
//
// Parity notes (behaviour as the C# actually ran, nothing fixed):
//  * InitNameSelect: the C# looked up the field "autoNameIndex" with GetField. Il2CppInterop
//    exposes IL2CPP fields as properties, so GetField returned null and ". Current: {name}" was
//    never spoken (and _lastAutoIndex was not seeded). Not added here.
//  * GetAutoName is called directly, as the FF1 port does.
//
// The Last.UI.KeyInput list classes are also hooked by the FF1 module, masked to FF1 there; these
// refs are masked to FF2. No hook here is folded (one RVA each in FF2's dump.cs).
#include <cstdint>
#include <string>
#include "ff2.h"
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/unity.h"
#include "../../core/system/cursor_event.h"
#include "../../core/dialogue/dialogue.h"

namespace ffpr::ff2 {
namespace {

// --- game names ------------------------------------------------------------------------------
ref::Class kNewGameWindow{"Serial.FF2.UI.KeyInput", "NewGameWindowController", game::kFF2};
ref::Field fNgwCurrentData{kNewGameWindow, "<CurrentData>k__BackingField"};
ref::Field fNgwSelectedDataList{kNewGameWindow, "<SelectedDataList>k__BackingField"};
ref::Field fNgwPopup{kNewGameWindow, "popup"};
ref::Method mInitSelect{kNewGameWindow, "InitSelect", 0};
ref::Method mInitNameSelect{kNewGameWindow, "InitNameSelect", 0};
ref::Method mInitNameInput{kNewGameWindow, "InitNameInput", 0};
ref::Method mInitStartPopup{kNewGameWindow, "InitStartPopup", 0};
ref::Method mInitErrorPopup{kNewGameWindow, "InitErrorPopup", 0};
ref::Method mInitNone{kNewGameWindow, "InitNone", 0};
ref::Method mGetAutoName{kNewGameWindow, "GetAutoName", 1};

ref::Class kNewGamePopup{"Serial.FF2.UI.KeyInput", "NewGamePopup", game::kFF2};

ref::Class kSelectData{"Last.Data", "NewGameSelectData", game::kFF2};
ref::Field fSelName{kSelectData, "<CharacterName>k__BackingField"};

ref::Class kCharList{"Last.UI.KeyInput", "CharacterContentListController", game::kFF2};
ref::Field fCharListContent{kCharList, "contentList"};
ref::Method mCharSetTarget{kCharList, "SetTargetSelectContent", 1};
ref::Method mCharUpdateView{kCharList, "UpdateView", 1};

ref::Class kCharContent{"Last.UI.KeyInput", "CharacterContentController", game::kFF2};
ref::Field fContentName{kCharContent, "<CharacterName>k__BackingField"};

ref::Class kNameList{"Last.UI.KeyInput", "NameContentListController", game::kFF2};
ref::Method mNameSetFocus{kNameList, "SetFocus", 1};

// --- state -----------------------------------------------------------------------------------
void* g_controller = nullptr;     // currentController (re-checked with unity::alive)
int g_last_target_index = -1;     // lastTargetIndex: the slot UpdateView reads

// The C# local guards, moved to dedup:: as they were (Session scope: reset only where the C#
// reset them).
constexpr const char* kCtxTarget = "FF2.Naming.Target";          // lastTargetIndex check
constexpr const char* kCtxAutoIndex = "FF2.Naming.AutoIndex";    // _lastAutoIndex
constexpr const char* kCtxAutoName = "FF2.Naming.AutoName";      // _lastNameAnnounced
constexpr const char* kCtxSlotName[4] = {"FF2.Naming.SlotName.1", "FF2.Naming.SlotName.2",
                                         "FF2.Naming.SlotName.3", "FF2.Naming.SlotName.4"};

// --- NewGameNamingState ----------------------------------------------------------------------
bool naming_active() { return menu_state::is_active(NEW_GAME_NAMING); }
void naming_clear()
{
    if (naming_active()) menu_state::set_active(NEW_GAME_NAMING, false);
}

// --- helpers ---------------------------------------------------------------------------------
void* live_controller()
{
    void* c = g_controller;
    return (c && kNewGameWindow.is(c) && unity::alive(c)) ? c : nullptr;
}

/// GetCurrentCharacterName: controller.CurrentData.CharacterName.
std::wstring current_character_name(void* controller)
{
    void* data = ref::rd<void*>(controller, fNgwCurrentData);
    return data ? ref::rd_str(data, fSelName) : std::wstring();
}

/// GetAutoNameByIndex: the controller's private GetAutoName(int).
std::wstring auto_name(void* controller, int index)
{
    return ref::call_str(mGetAutoName, controller, static_cast<int32_t>(index));
}

/// GetCharacterSlotNameOnly: ContentList[index].CharacterName, empty for none.
std::wstring slot_name(void* list_ctrl, int index)
{
    void* content = ref::rd<void*>(list_ctrl, fCharListContent);
    if (!content || index < 0 || index >= unity::list_count(content)) return {};
    void* item = unity::list_at(content, index);
    return item ? ref::rd_str(item, fContentName) : std::wstring();
}

/// GetCharacterSlotInfo: "Character {n}: {name}" / ": unnamed", "Done" past the list.
std::wstring slot_info(void* controller, int index)
{
    const std::wstring display = text::num(index >= 0 ? index + 1 : 1);
    const std::wstring unnamed = TF(L"Character {0}: unnamed", {display});
    void* list = ref::rd<void*>(controller, fNgwSelectedDataList);
    if (!list) return unnamed;
    const int count = unity::list_count(list);
    if (index < 0 || index >= count) return index >= count ? T(L"Done") : std::wstring();
    void* item = unity::list_at(list, index);
    if (!item) return unnamed;
    std::wstring name = ref::rd_str(item, fSelName);
    if (name.empty()) return unnamed;
    return TF(L"Character {0}: {1}", {display, name});
}

// --- postfix bodies --------------------------------------------------------------------------
void on_init_select(void* self)
{
    g_controller = self;
    set_active_exclusive(NEW_GAME_NAMING);
    dedup::reset(kCtxAutoName);
    dedup::reset(kCtxAutoIndex);
    for (const char* ctx : kCtxSlotName) dedup::reset(ctx);
    g_last_target_index = -1;
    dedup::reset(kCtxTarget);
    // Mode entry only: the slot is announced on the first navigation.
    ffpr::say(T(L"Character selection"));
}

void on_init_name_input(void* self, bool game_popup_read)
{
    naming_clear();
    if (game_popup_read) return;   // the game's ChangeNamePopup text is read
    std::wstring name = current_character_name(self);
    ffpr::say(name.empty() ? T(L"Enter name using keyboard") : TF(L"Enter name for {0}. Type using keyboard.", {name}));
}

void on_init_start_popup(void* self)
{
    naming_clear();   // the popup's Yes/No is read by the generic cursor reader
    void* popup = ref::rd<void*>(self, fNgwPopup);
    if (!popup || !kNewGamePopup.is(popup)) return;
    dialogue::read_popup_message(popup, "NewGame.Confirmation");
}

void on_set_target(void* self, int index)
{
    if (!dedup::index(kCtxTarget, index, dedup::Scope::Session)) return;
    g_last_target_index = index;
    void* controller = live_controller();
    if (!controller) return;
    std::wstring info = slot_info(controller, index);
    if (info.empty()) return;
    // Track the slot's current name for UpdateView.
    if (index >= 0 && index < 4) {
        dedup::reset(kCtxSlotName[index]);
        std::wstring name = slot_name(self, index);
        if (!name.empty()) dedup::text(kCtxSlotName[index], name, dedup::Scope::Session);
    }
    ffpr::say(info);
}

void on_init_name_select(void* self)
{
    g_controller = self;
    set_active_exclusive(NEW_GAME_NAMING);
    dedup::reset(kCtxAutoName);
    dedup::reset(kCtxAutoIndex);
    std::wstring name = current_character_name(self);
    ffpr::say(name.empty() ? T(L"Select name") : TF(L"Select name for {0}", {name}));
}

void on_name_set_focus(int index)
{
    if (!dedup::index(kCtxAutoIndex, index, dedup::Scope::Session)) return;
    void* controller = live_controller();
    if (!controller) return;
    std::wstring name = auto_name(controller, index);
    if (!name.empty() && dedup::text(kCtxAutoName, name, dedup::Scope::Session)) ffpr::say(name);
}

void on_update_view(void* self)
{
    const int slot = g_last_target_index;
    if (slot < 0 || slot >= 4) return;
    std::wstring name = slot_name(self, slot);
    if (!name.empty() && dedup::text(kCtxSlotName[slot], name, dedup::Scope::Session)) ffpr::say(name);
}

// --- detours ---------------------------------------------------------------------------------
using Void_t = void (*)(void* self, const void* mi);
using VoidInt_t = void (*)(void* self, int32_t arg, const void* mi);
using VoidPtr_t = void (*)(void* self, void* arg, const void* mi);

Void_t o_InitSelect, o_InitNameSelect, o_InitNameInput, o_InitStartPopup, o_InitErrorPopup, o_InitNone;
VoidInt_t o_CharSetTarget, o_NameSetFocus;
VoidPtr_t o_CharUpdateView;

void d_InitSelect(void* self, const void* mi)
{
    o_InitSelect(self, mi);
    hooks::guarded("FF2.Naming.InitSelect", [&] { on_init_select(self); });
}

void d_InitNameSelect(void* self, const void* mi)
{
    o_InitNameSelect(self, mi);
    hooks::guarded("FF2.Naming.InitNameSelect", [&] { on_init_name_select(self); });
}

// Name entry: KeyboardManager.ShowKeyboard (called inside the input Init) opens the game's
// ChangeNamePopup, whose description ("Enter a name for X") the popup reader speaks. When that
// popup opened during the Init, the mod's own entry line is not spoken (2026-10-06, game text
// first); it remains for the path without the popup (Steam's gamepad text input).
void d_InitNameInput(void* self, const void* mi)
{
    const int popups_before = dialogue::change_name_popup_opens();
    o_InitNameInput(self, mi);
    const bool game_popup_read = dialogue::change_name_popup_opens() != popups_before;
    hooks::guarded("FF2.Naming.InitNameInput", [&] { on_init_name_input(self, game_popup_read); });
}

void d_InitStartPopup(void* self, const void* mi)
{
    o_InitStartPopup(self, mi);
    hooks::guarded("FF2.Naming.InitStartPopup", [&] { on_init_start_popup(self); });
}

void d_InitErrorPopup(void* self, const void* mi)
{
    o_InitErrorPopup(self, mi);
    hooks::guarded("FF2.Naming.InitErrorPopup", [&] { naming_clear(); });
}

void d_InitNone(void* self, const void* mi)
{
    o_InitNone(self, mi);
    hooks::guarded("FF2.Naming.InitNone", [&] { naming_clear(); });
}

void d_CharSetTarget(void* self, int32_t index, const void* mi)
{
    o_CharSetTarget(self, index, mi);
    hooks::guarded("FF2.Naming.SetTargetSelectContent", [&] { on_set_target(self, index); });
}

void d_NameSetFocus(void* self, int32_t index, const void* mi)
{
    o_NameSetFocus(self, index, mi);
    hooks::guarded("FF2.Naming.NameSetFocus", [&] { on_name_set_focus(index); });
}

void d_CharUpdateView(void* self, void* data_list, const void* mi)
{
    o_CharUpdateView(self, data_list, mi);
    hooks::guarded("FF2.Naming.UpdateView", [&] { on_update_view(self); });
}

int install()
{
    queries::define("NewGameNamingState.ShouldSuppress", [] { return naming_active(); });
    queries::define("NewGameNamingState.IsActive", [] { return naming_active(); });
    cursor::add_suppressor("NewGameNamingState.ShouldSuppress");

    int n = 0;
    n += hooks::attach(mInitNameSelect, &d_InitNameSelect, &o_InitNameSelect) ? 1 : 0;
    n += hooks::attach(mInitSelect, &d_InitSelect, &o_InitSelect) ? 1 : 0;
    n += hooks::attach(mInitNameInput, &d_InitNameInput, &o_InitNameInput) ? 1 : 0;
    n += hooks::attach(mInitStartPopup, &d_InitStartPopup, &o_InitStartPopup) ? 1 : 0;
    n += hooks::attach(mInitErrorPopup, &d_InitErrorPopup, &o_InitErrorPopup) ? 1 : 0;
    n += hooks::attach(mInitNone, &d_InitNone, &o_InitNone) ? 1 : 0;
    n += hooks::attach(mCharSetTarget, &d_CharSetTarget, &o_CharSetTarget) ? 1 : 0;
    n += hooks::attach(mNameSetFocus, &d_NameSetFocus, &o_NameSetFocus) ? 1 : 0;
    n += hooks::attach(mCharUpdateView, &d_CharUpdateView, &o_CharUpdateView) ? 1 : 0;
    LOGF("[FF2 Naming] %d of 9 hooks queued", n);
    return n;
}

core::Install s_install{"FF2.NewGameNaming", &install, 510, game::kFF2};

} // namespace
} // namespace ffpr::ff2
