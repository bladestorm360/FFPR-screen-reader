// Ports FF3 Patches/NewGameNamingPatches.cs.
//
// New game: naming the four Onion Knights (Serial.FF3.UI.KeyInput.NewGameWindowController and the
// Last.UI.KeyInput lists it drives): "Character selection", "Onion Knight N: name" per slot,
// the name screens, the cycled suggested names and the start popup.
//
// Parity notes (behaviour as the FF3 C# actually ran, nothing fixed):
//  * InitNameSelect: GetCurrentSuggestedName used GetField("autoNameIndex"). Il2CppInterop exposes
//    IL2CPP fields as properties, so GetField returned null: ". Current: {name}" was never added
//    and the auto-name index was never seeded. Not added here (as in the FF1 port).
//  * InitStartPopup: unlike FF1, FF3's controller has a `popup` field, so the popup's Message
//    (NewGamePopup.get_Message) was read and spoken.
//  * Dedup contexts moved as they were (DEDUP_INVENTORY FF3 #15-19, tracker #30): slot index,
//    auto-name index, suggested name text, last name per slot. "NewGame.SlotName" was never
//    checked (dead) and is not ported. Session scope: reset only where FF3 reset them.
//  * The new-game lists do not silence the generic cursor reader (FF3 had no such check).
//
// Shared engine classes (Last.UI.KeyInput CharacterContentListController, CharacterContentController,
// NameContentListController, Last.Data.NewGameSelectData) are masked to FF3 here; FF1's module
// hooks the same methods for FF1 only. Every hooked RVA is unique in FF3's dump.cs.
#include <cstdint>
#include <string>
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/frame.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/ui/initial_speech.h"
#include "../../core/text.h"
#include "../../core/unity.h"
#include "../../core/dialogue/dialogue.h"

namespace ffpr::ff3 {
namespace {

ref::Class kNewGameWindow{"Serial.FF3.UI.KeyInput", "NewGameWindowController", game::kFF3};
ref::Field fNgwPopup{kNewGameWindow, "popup"};
ref::Field fNgwSelectedList{kNewGameWindow, "<SelectedDataList>k__BackingField"};
ref::Field fNgwCurrentData{kNewGameWindow, "<CurrentData>k__BackingField"};
ref::Method mInitSelect{kNewGameWindow, "InitSelect", 0};
ref::Method mInitNameSelect{kNewGameWindow, "InitNameSelect", 0};
ref::Method mInitNameInput{kNewGameWindow, "InitNameInput", 0};
ref::Method mInitStartPopup{kNewGameWindow, "InitStartPopup", 0};
ref::Method mGetAutoName{kNewGameWindow, "GetAutoName", 1};

ref::Class kNewGamePopup{"Serial.FF3.UI.KeyInput", "NewGamePopup", game::kFF3};

ref::Class kSelectData{"Last.Data", "NewGameSelectData", game::kFF3};
ref::Field fSelName{kSelectData, "<CharacterName>k__BackingField"};

ref::Class kCharList{"Last.UI.KeyInput", "CharacterContentListController", game::kFF3};
ref::Field fCharListContent{kCharList, "contentList"};
ref::Method mCharSetTarget{kCharList, "SetTargetSelectContent", 1};
ref::Method mCharUpdateView{kCharList, "UpdateView", 1};

ref::Class kCharContent{"Last.UI.KeyInput", "CharacterContentController", game::kFF3};
ref::Field fContentName{kCharContent, "<CharacterName>k__BackingField"};

ref::Class kNameList{"Last.UI.KeyInput", "NameContentListController", game::kFF3};
ref::Method mNameSetFocus{kNameList, "SetFocus", 1};

constexpr const char* kCtxName = "NewGame.Name";
constexpr const char* kCtxAutoIndex = "NewGame.AutoNameIndex";
constexpr const char* kCtxSelected = "NewGame.SelectedIndex";
constexpr const char* kCtxSlotName[4] = {"NewGame.LastSlotName0", "NewGame.LastSlotName1",
                                         "NewGame.LastSlotName2", "NewGame.LastSlotName3"};
constexpr dedup::Scope kScope = dedup::Scope::Session;

void* g_controller = nullptr;   // currentController (re-checked with unity::alive on use)
// The suggested-name list moves with Cursor.NextIndex/PrevIndex/Skip* (NameContentListController.
// <UpdateController>b__9_0), so the generic cursor reader speaks each move from the row's own
// text; FF2's mod found SetFocus speaking the same name on every move as well (FF2 debug.md,
// "Naming screen"). One line per move (2026-10-06): SetFocus speaks only during the list's entry
// (InitNameSelect and the frame after it), where no cursor move is read.
int64_t g_name_entry_frame = -100;
ui::InitialSpeech g_name_speech;
int g_last_target_index = -1;   // lastTargetIndex

void* live_controller()
{
    void* c = g_controller;
    return (c && kNewGameWindow.is(c) && unity::alive(c)) ? c : nullptr;
}

std::wstring format_slot(int display_index, const std::wstring& name)
{
    return name.empty() ? TF(L"Onion Knight {0}: unnamed", {text::num(display_index)})
                        : TF(L"Onion Knight {0}: {1}", {text::num(display_index), name});
}

/// GetCharacterSlotInfo: "Onion Knight N: name" from SelectedDataList, "Done" past the end.
std::wstring slot_info(void* controller, int index)
{
    const int display_index = index >= 0 ? index + 1 : 1;
    void* list = ref::rd<void*>(controller, fNgwSelectedList);
    if (!list) return format_slot(display_index, {});
    const int count = unity::list_count(list);
    if (index < 0 || index >= count) return index >= count ? T(L"Done") : std::wstring();
    void* item = unity::list_at(list, index);
    if (!item) return format_slot(display_index, {});
    return format_slot(display_index, ref::rd_str(item, fSelName));
}

/// GetCharacterSlotNameOnly: CharacterContentListController.ContentList[index].CharacterName.
std::wstring slot_name_only(void* list_ctrl, int index)
{
    void* list = ref::rd<void*>(list_ctrl, fCharListContent);
    if (!list || index < 0 || index >= unity::list_count(list)) return {};
    void* content = unity::list_at(list, index);
    return content ? ref::rd_str(content, fContentName) : std::wstring();
}

/// GetCurrentCharacterName: CurrentData.CharacterName.
std::wstring current_character_name(void* controller)
{
    void* data = ref::rd<void*>(controller, fNgwCurrentData);
    return data ? ref::rd_str(data, fSelName) : std::wstring();
}

void on_init_select(void* self)
{
    g_controller = self;
    dedup::reset(kCtxName);
    dedup::reset(kCtxAutoIndex);
    dedup::reset(kCtxSelected);
    for (const char* ctx : kCtxSlotName) dedup::reset(ctx);
    g_last_target_index = -1;
    ffpr::say(T(L"Character selection"));
}

void on_init_start_popup(void* self)
{
    void* popup = ref::rd<void*>(self, fNgwPopup);
    if (!popup) return;
    ffpr::dialogue::read_popup_message(popup, "NewGame.Confirmation");
}

void on_set_target(void* list_ctrl, int index)
{
    if (!dedup::index(kCtxSelected, index, kScope)) return;
    g_last_target_index = index;
    void* controller = live_controller();
    if (!controller) return;
    const std::wstring info = slot_info(controller, index);
    if (info.empty()) return;
    if (index >= 0 && index < 4) dedup::text(kCtxSlotName[index], slot_name_only(list_ctrl, index), kScope);
    ffpr::say(info);
}

void on_init_name_select(void* self)
{
    g_controller = self;
    const std::wstring name = current_character_name(self);
    g_name_speech.ready(name.empty() ? T(L"Select name") : TF(L"Select name for {0}", {name}));
}

void on_set_focus(int index)
{
    if (static_cast<int64_t>(frame::count()) - g_name_entry_frame > 1) return;   // a move: the cursor reader speaks it
    if (!dedup::index(kCtxAutoIndex, index, kScope)) return;
    void* controller = live_controller();
    if (!controller) return;
    const std::wstring name = ref::call_str(mGetAutoName, controller, static_cast<int32_t>(index));
    if (!name.empty() && dedup::text(kCtxName, name, kScope) && !g_name_speech.capture(name)) ffpr::say(name, false);
}

void on_update_view(void* list_ctrl)
{
    const int slot = g_last_target_index;
    if (slot < 0 || slot >= 4) return;
    const std::wstring name = slot_name_only(list_ctrl, slot);
    if (!name.empty() && dedup::text(kCtxSlotName[slot], name, kScope)) ffpr::say(name);
}

void on_init_name_input(void* self)
{
    const std::wstring name = current_character_name(self);
    ffpr::say(name.empty() ? T(L"Enter name using keyboard") : TF(L"Enter name for {0}. Type using keyboard.", {name}));
}

using Void_t = void (*)(void* self, const void* mi);
using Int_t = void (*)(void* self, int32_t value, const void* mi);
using Ptr_t = void (*)(void* self, void* value, const void* mi);
Void_t o_InitSelect;
Void_t o_InitNameSelect;
Void_t o_InitNameInput;
Void_t o_InitStartPopup;
Int_t o_SetTarget;
Int_t o_SetFocus;
Ptr_t o_UpdateView;

void d_InitSelect(void* self, const void* mi)
{
    o_InitSelect(self, mi);
    hooks::guarded("FF3.NewGame.InitSelect", [&] { on_init_select(self); });
}

void d_InitNameSelect(void* self, const void* mi)
{
    g_controller = self;
    dedup::reset(kCtxName);
    dedup::reset(kCtxAutoIndex);
    g_name_speech.begin();
    g_name_entry_frame = static_cast<int64_t>(frame::count());
    o_InitNameSelect(self, mi);
    g_name_entry_frame = static_cast<int64_t>(frame::count());
    hooks::guarded("FF3.NewGame.InitNameSelect", [&] { on_init_name_select(self); });
}

// Name entry: KeyboardManager.ShowKeyboard (called inside the input Init) opens the game's
// ChangeNamePopup, whose description ("Enter a name for X") the popup reader speaks. When that
// popup opened during the Init, the mod's own entry line is not spoken (2026-10-06, game text
// first); it remains for the path without the popup (Steam's gamepad text input).
void d_InitNameInput(void* self, const void* mi)
{
    const int popups_before = dialogue::change_name_popup_opens();
    o_InitNameInput(self, mi);
    if (dialogue::change_name_popup_opens() != popups_before) return;   // the game's popup text is read
    hooks::guarded("FF3.NewGame.InitNameInput", [&] { on_init_name_input(self); });
}

void d_InitStartPopup(void* self, const void* mi)
{
    o_InitStartPopup(self, mi);
    hooks::guarded("FF3.NewGame.InitStartPopup", [&] { on_init_start_popup(self); });
}

void d_SetTarget(void* self, int32_t index, const void* mi)
{
    o_SetTarget(self, index, mi);
    hooks::guarded("FF3.NewGame.SetTargetSelectContent", [&] { on_set_target(self, index); });
}

void d_SetFocus(void* self, int32_t index, const void* mi)
{
    o_SetFocus(self, index, mi);
    hooks::guarded("FF3.NewGame.SetFocus", [&] { on_set_focus(index); });
}

void d_UpdateView(void* self, void* data_list, const void* mi)
{
    o_UpdateView(self, data_list, mi);
    hooks::guarded("FF3.NewGame.UpdateView", [&] { on_update_view(self); });
}

int install()
{
    int n = 0;
    n += hooks::attach(mInitSelect, &d_InitSelect, &o_InitSelect) ? 1 : 0;
    n += hooks::attach(mInitNameSelect, &d_InitNameSelect, &o_InitNameSelect) ? 1 : 0;
    n += hooks::attach(mInitNameInput, &d_InitNameInput, &o_InitNameInput) ? 1 : 0;
    n += hooks::attach(mInitStartPopup, &d_InitStartPopup, &o_InitStartPopup) ? 1 : 0;
    n += hooks::attach(mCharSetTarget, &d_SetTarget, &o_SetTarget) ? 1 : 0;
    n += hooks::attach(mNameSetFocus, &d_SetFocus, &o_SetFocus) ? 1 : 0;
    n += hooks::attach(mCharUpdateView, &d_UpdateView, &o_UpdateView) ? 1 : 0;
    LOGF("[FF3 NewGame] %d naming hooks", n);
    return n;
}

core::Install s_install{"FF3.NewGame", &install, 501, game::kFF3};

} // namespace
} // namespace ffpr::ff3
