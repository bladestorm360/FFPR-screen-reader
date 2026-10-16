// Ports FF1 Patches/NewGamePatches.cs and Patches/NewGameHelpers.cs.
//
// New game: naming the four Light Warriors and choosing their classes
// (Serial.FF1.UI.KeyInput.NewGameWindowController and the Last.UI.KeyInput lists it drives).
// This is also the Phase 1 proof hook for FF1 (UNIFIED_MOD_PLAN.md): the install logs every
// NewGameWindowController hook by name, and InitSelect logs when it fires.
//
// Parity notes (behaviour as FF1 actually ran, nothing fixed):
//  * InitStartPopup: the C# looked up a field "newGamePopup" by reflection. FF1's KeyInput
//    controller has no such field (it is "popup"). The shared native reader now uses the
//    actual popup's rendered message instead of that fallback.
//  * InitNameSelect: the C# tried to append ". Current: {auto name}" through
//    GetField("autoNameIndex"). Il2CppInterop exposes IL2CPP fields as properties, so GetField
//    always returned null and the suffix was never spoken. Not added here.
//  * The one-time ContentList diagnostic dump computed values and logged nothing; not ported.
//
// Shared engine classes used here (Last.UI.KeyInput CharacterContentListController,
// CharacterContentController, NameContentListController, Last.Data.NewGameSelectData,
// CharacterSelectCommandContentView/CharacterSelectCommandData) exist in
// all five games; they are masked to FF1 because only FF1's module hooks them here.
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <string>
#include "../../core/core.h"
#include "../../core/displayed_ui.h"
#include "../../core/dedup.h"
#include "../../core/frame.h"
#include "../../core/hooks.h"
#include "../../core/il2cpp.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/ui/initial_speech.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/dialogue/dialogue.h"

namespace ffpr::ff1 {
namespace {

// --- game names ----------------------------------------------------------------------------
ref::Class kNewGameWindow{"Serial.FF1.UI.KeyInput", "NewGameWindowController", game::kFF1};
ref::Field fNgwCurrentData{kNewGameWindow, "<CurrentData>k__BackingField"};
ref::Method mInitSelect{kNewGameWindow, "InitSelect", 0};
ref::Method mInitNameSelect{kNewGameWindow, "InitNameSelect", 0};
ref::Method mInitNameInput{kNewGameWindow, "InitNameInput", 0};
ref::Method mInitStartPopup{kNewGameWindow, "InitStartPopup", 0};
ref::Method mGetAutoName{kNewGameWindow, "GetAutoName", 1};

ref::Class kSelectData{"Last.Data", "NewGameSelectData", game::kFF1};
ref::Field fSelName{kSelectData, "<CharacterName>k__BackingField"};

ref::Class kCharList{"Last.UI.KeyInput", "CharacterContentListController", game::kFF1};
ref::Field fCharListContent{kCharList, "contentList"};
ref::Method mCharSelectContent{kCharList, "SelectContent", 1};
ref::Method mCharSetTarget{kCharList, "SetTargetSelectContent", 1};
ref::Method mCharUpdateView{kCharList, "UpdateView", 1};

ref::Class kCharContent{"Last.UI.KeyInput", "CharacterContentController", game::kFF1};
ref::Field fContentName{kCharContent, "<CharacterName>k__BackingField"};
ref::Field fContentCommands{kCharContent, "<CommandList>k__BackingField"};
ref::Class kCreationCommandView{"Last.UI.KeyInput", "CharacterSelectCommandContentView", game::kFF1};
ref::Field fCreationCommandData{kCreationCommandView, "<Data>k__BackingField"};
ref::Class kCreationCommandData{"Last.UI", "CharacterSelectCommandData", game::kFF1};
ref::Field fCreationCommandId{kCreationCommandData, "<Id>k__BackingField"};

ref::Class kNameList{"Last.UI.KeyInput", "NameContentListController", game::kFF1};
ref::Method mNameSetFocus{kNameList, "SetFocus", 1};

// --- state (NewGamePatches statics) --------------------------------------------------------
void* g_controller = nullptr;     // currentController: the NewGameWindowController
int g_last_target_index = -1;     // lastTargetIndex (SetTargetSelectContent, UpdateView fallback)
int g_decoded_slot = -1;          // decodedCharacterSlot (from SelectContent)
bool g_handling_cursor = false;   // IsHandlingCursor
// The suggested-name list moves with Cursor.NextIndex/PrevIndex/Skip* (NameContentListController.
// <UpdateController>b__9_0), so the generic cursor reader speaks each move from the row's own
// text; FF2's mod found SetFocus speaking the same name on every move as well (FF2 debug.md,
// "Naming screen"). One line per move (2026-10-06): SetFocus speaks only during the list's entry
// (InitNameSelect and the frame after it), where no cursor move is read.
int64_t g_name_entry_frame = -100;
ui::InitialSpeech g_name_speech;

// lastCharacterIndex / lastFieldType / lastSlotNames[4], moved to dedup:: as they were: reset
// only where the C# reset them (Session scope = no automatic resets).
constexpr const char* kCtxSlot = "Naming.Slot";
constexpr const char* kCtxField = "Naming.Field";
constexpr const char* kCtxSlotName[4] = {"Naming.SlotName.1", "Naming.SlotName.2",
                                         "Naming.SlotName.3", "Naming.SlotName.4"};

// --- NewGameHelpers: grid decoding ---------------------------------------------------------
// Character-major encoding: flat index = characterIndex * 2 + commandIndex.
//   0: LW1 Name   2: LW2 Name   4: LW3 Name   6: LW4 Name
//   1: LW1 Class  3: LW2 Class  5: LW3 Class  7: LW4 Class
//   8+: Done
struct Grid {
    int character = -1;
    bool is_class = false;
    bool done = false;
};

Grid decode_grid(int cursor_index)
{
    Grid g;
    if (cursor_index >= 8) {
        g.done = true;
        return g;
    }
    g.character = cursor_index / 2;
    g.is_class = cursor_index % 2 == 1;
    return g;
}

// --- NewGameHelpers: names -------------------------------------------------------------------
/// GetCharacterSlotNameOnly: ContentList[index].CharacterName, empty for none (C# null).
std::wstring slot_name(void* list_ctrl, int index)
{
    void* content = ref::rd<void*>(list_ctrl, fCharListContent);
    if (!content || index < 0 || index >= unity::list_count(content)) return {};
    void* item = unity::list_at(content, index);
    if (!item) return {};
    return ref::rd_str(item, fContentName);
}

/// GetCurrentCharacterName: controller.CurrentData.CharacterName.
std::wstring current_character_name(void* controller)
{
    void* data = ref::rd<void*>(controller, fNgwCurrentData);
    if (!data) return {};
    return ref::rd_str(data, fSelName);
}

/// GetAutoNameByIndex: the controller's private GetAutoName(int).
std::wstring auto_name(void* controller, int index)
{
    return ref::call_str(mGetAutoName, controller, static_cast<int32_t>(index));
}

// --- class field: the native summary's rendered value --------------------------------------
// SetJobName/SetData write CommandList[1].text in all six native bodies. The
// command's Data.Id is Job (1); validate it so missing/reordered pooled rows can
// never turn another slot or the character name into the class. Only FF1 exposes
// class selection during character creation. Other games' naming adapters don't
// use this reader. Never reconstruct a displayed label through master dictionaries.
std::wstring job_name(void* list_ctrl, int character_index)
{
    void* content = ref::rd<void*>(list_ctrl, fCharListContent);
    if (!content || character_index < 0 || character_index >= 4) return {};
    const int count = unity::list_count(content);
    if (count <= character_index || count > 4) return {};
    void* slot = unity::list_at(content, character_index);
    if (!kCharContent.is(slot) || !displayed::container_visible(slot)) return {};
    void* commands = ref::rd<void*>(slot, fContentCommands);
    if (unity::list_count(commands) != 2) return {};
    void* command = unity::list_at(commands, 1);
    if (!kCreationCommandView.is(command) || !displayed::container_visible(command)) return {};
    void* data = ref::rd<void*>(command, fCreationCommandData);
    if (!kCreationCommandData.is(data) || ref::rd<int32_t>(data, fCreationCommandId) != 1) return {};
    return displayed::text_field(command, "text");
}

// --- SelectContent (primary navigation) --------------------------------------------------------
/// AnnounceFieldAfterDelay, one frame after SelectContent.
void announce_field(void* list_ctrl, int cursor_index)
{
    struct ClearFlag {
        ~ClearFlag() { g_handling_cursor = false; }   // the C# finally
    } clear_flag;

    // Backing out to the title fires a transient SelectContent during teardown: stay silent
    // when the new-game screen is no longer active.
    if (!kCharList.is(list_ctrl) || !unity::alive(list_ctrl) || !unity::active_in_hierarchy(list_ctrl)) return;

    Grid g = decode_grid(cursor_index);
    if (g.done) {
        dedup::reset(kCtxSlot);
        dedup::reset(kCtxField);
        ffpr::say(T(L"Done"));
        return;
    }

    int field_type = g.is_class ? 1 : 0;
    std::wstring field_label = g.is_class ? T(L"Class") : T(L"Name");

    std::wstring field_value;
    if (g.is_class) {
        field_value = job_name(list_ctrl, g.character);
        if (field_value.empty()) field_value = T(L"unknown");
    } else {
        field_value = slot_name(list_ctrl, g.character);
        if (field_value.empty()) field_value = slot_name(list_ctrl, cursor_index);
        if (field_value.empty()) field_value = T(L"unnamed");
    }

    // Both checks always run: each stores its value, as the C# stored both after speaking.
    bool character_changed = dedup::index(kCtxSlot, g.character, dedup::Scope::Session);
    bool field_changed = dedup::index(kCtxField, field_type, dedup::Scope::Session);

    std::wstring announcement;
    if (character_changed)
        announcement = TF(L"Light Warrior {0}: {1}: {2}", {text::num(g.character + 1), field_label, field_value});
    else if (field_changed)
        announcement = field_label + L": " + field_value;
    else
        return;

    ffpr::say(announcement);
}

void on_select_content(void* self, void* cursor)
{
    g_handling_cursor = true;
    if (!cursor) return;
    int cursor_index = ui::cursor_index(cursor);

    Grid g = decode_grid(cursor_index);
    if (!g.done && g.character >= 0 && g.character < 4) g_decoded_slot = g.character;

    frame::next_frame([self, cursor_index] { announce_field(self, cursor_index); }, "NewGame.AnnounceField");
}

// --- the other postfixes ---------------------------------------------------------------------
void on_init_select(void* self)
{
    LOGF("FF1 NewGame: NewGameWindowController.InitSelect fired (character selection)");
    g_controller = self;
    g_last_target_index = -1;
    dedup::reset(kCtxSlot);
    dedup::reset(kCtxField);
    g_decoded_slot = -1;
    for (const char* ctx : kCtxSlotName) dedup::reset(ctx);
    ffpr::say(T(L"Character selection"), false);
}

void on_set_target(int index)
{
    Grid g = decode_grid(index);
    if (!g.done && g.character >= 0 && g.character < 4) g_last_target_index = g.character;
}

void on_init_name_select(void* self)
{
    g_controller = self;
    std::wstring character_name = current_character_name(self);
    std::wstring announcement = character_name.empty() ? T(L"Select name") : TF(L"Select name for {0}", {character_name});
    g_name_speech.ready(std::move(announcement));
}

void on_name_set_focus(int index)
{
    if (static_cast<int64_t>(frame::count()) - g_name_entry_frame > 1) return;   // a move: the cursor reader speaks it
    void* ctrl = g_controller;
    if (!ctrl || !kNewGameWindow.is(ctrl) || !unity::alive(ctrl)) return;
    std::wstring name = auto_name(ctrl, index);
    if (!name.empty() && !g_name_speech.capture(name)) ffpr::say(name, false);
}

void on_update_view(void* self)
{
    int slot = g_decoded_slot >= 0 ? g_decoded_slot : g_last_target_index;
    if (slot < 0 || slot >= 4) return;
    std::wstring name = slot_name(self, slot);
    if (!name.empty() && dedup::text(kCtxSlotName[slot], name, dedup::Scope::Session)) ffpr::say(name);
}

void on_init_name_input(void* self)
{
    std::wstring character_name = current_character_name(self);
    std::wstring announcement = character_name.empty() ? T(L"Enter name using keyboard")
                                                       : TF(L"Enter name for {0}. Type using keyboard.", {character_name});
    ffpr::say(announcement);
}

void on_init_start_popup(void* self)
{
    dialogue::read_popup_message(displayed::field(self, "popup"), "NewGame.Confirmation");
}

// --- detours ---------------------------------------------------------------------------------
using Void_t = void (*)(void* self, const void* mi);
using VoidPtr_t = void (*)(void* self, void* arg, const void* mi);
using VoidInt_t = void (*)(void* self, int32_t arg, const void* mi);

VoidPtr_t o_CharSelectContent;
Void_t o_InitSelect;
VoidInt_t o_CharSetTarget;
Void_t o_InitNameSelect;
VoidInt_t o_NameSetFocus;
VoidPtr_t o_CharUpdateView;
Void_t o_InitNameInput;
Void_t o_InitStartPopup;

void d_CharSelectContent(void* self, void* cursor, const void* mi)
{
    o_CharSelectContent(self, cursor, mi);
    hooks::guarded("NewGame.SelectContent", [&] { on_select_content(self, cursor); });
}

void d_InitSelect(void* self, const void* mi)
{
    o_InitSelect(self, mi);
    hooks::guarded("NewGame.InitSelect", [&] { on_init_select(self); });
}

void d_CharSetTarget(void* self, int32_t index, const void* mi)
{
    o_CharSetTarget(self, index, mi);
    hooks::guarded("NewGame.SetTargetSelectContent", [&] { on_set_target(index); });
}

void d_InitNameSelect(void* self, const void* mi)
{
    g_controller = self;
    g_name_speech.begin();
    g_name_entry_frame = static_cast<int64_t>(frame::count());
    o_InitNameSelect(self, mi);
    g_name_entry_frame = static_cast<int64_t>(frame::count());
    hooks::guarded("NewGame.InitNameSelect", [&] { on_init_name_select(self); });
}

void d_NameSetFocus(void* self, int32_t index, const void* mi)
{
    o_NameSetFocus(self, index, mi);
    hooks::guarded("NewGame.NameSetFocus", [&] { on_name_set_focus(index); });
}

void d_CharUpdateView(void* self, void* data_list, const void* mi)
{
    o_CharUpdateView(self, data_list, mi);
    hooks::guarded("NewGame.UpdateView", [&] { on_update_view(self); });
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
    hooks::guarded("NewGame.InitNameInput", [&] { on_init_name_input(self); });
}

void d_InitStartPopup(void* self, const void* mi)
{
    o_InitStartPopup(self, mi);
    hooks::guarded("NewGame.InitStartPopup", [&] { on_init_start_popup(self); });
}

template <typename D, typename O>
int attach_logged(ref::Method& m, D detour, O* original)
{
    bool ok = hooks::attach(m, detour, original);
    LOGF("FF1 NewGame: %s %s.%s", ok ? "hook queued:" : "hook FAILED:", m.cls.name, m.name);
    return ok ? 1 : 0;
}

int install()
{
    LOGF("FF1 NewGame: %s.%s %s", kNewGameWindow.ns, kNewGameWindow.name,
         kNewGameWindow.get() ? "resolved" : "NOT FOUND");
    int n = 0;
    n += attach_logged(mCharSelectContent, &d_CharSelectContent, &o_CharSelectContent);
    n += attach_logged(mInitSelect, &d_InitSelect, &o_InitSelect);
    n += attach_logged(mCharSetTarget, &d_CharSetTarget, &o_CharSetTarget);
    n += attach_logged(mInitNameSelect, &d_InitNameSelect, &o_InitNameSelect);
    n += attach_logged(mNameSetFocus, &d_NameSetFocus, &o_NameSetFocus);
    n += attach_logged(mCharUpdateView, &d_CharUpdateView, &o_CharUpdateView);
    n += attach_logged(mInitNameInput, &d_InitNameInput, &o_InitNameInput);
    n += attach_logged(mInitStartPopup, &d_InitStartPopup, &o_InitStartPopup);
    LOGF("FF1 NewGame: %d of 8 hooks queued", n);

    // Read by the cursor router (FF1 CursorNavigation_Postfix) so the generic reader stays quiet.
    queries::define("NewGamePatches.IsHandlingCursor", [] { return g_handling_cursor; });
    actions::define("NewGamePatches.ClearHandlingFlag", [] { g_handling_cursor = false; });
    return n;
}

core::Install s_install{"FF1.NewGame", &install, 500, game::kFF1};

} // namespace
} // namespace ffpr::ff1
