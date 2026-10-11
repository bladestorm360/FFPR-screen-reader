#pragma once
#include <string>
#include <functional>
#include "refs.h"

// The dialogue area: message windows, fade/scroll/line-fade system messages, popups and the
// title screen. Ports FF1 Patches/MessageWindowPatches.cs, ScrollMessagePatches.cs,
// PopupPatches.cs, PopupState.cs, TitleMenuPatches.cs and TitleScreenPatches.cs.
//
// These functions are what other areas call. Areas that do not include this header use the
// published names instead:
//   queries  "MessageWindowPatches.IsInDialogue", "PopupState.IsConfirmationPopupActive",
//            "PopupState.ShouldSuppress"
//   actions  "MessageWindowPatches.RepeatLastDialogue", "TitleScreenPatches.ArmPressPrompt",
//            "PopupState.Clear"
// Main thread only, like the C# originals.
namespace ffpr::dialogue {

/// One native opening/update event, bounded wait for rendered body, cancelled on close.
void read_popup_when_ready(void* popup, const char* context, std::function<std::wstring()> reader,
                           std::function<bool()> current = {});
/// Non-Popup confirmation layouts (e.g. new-game confirmation); reads the actual Text field.
void read_popup_message(void* popup, const char* context);

// --- MessageWindowPatches -------------------------------------------------------------
/// MessageWindowPatches.IsInDialogue: a message window has content (SetContent .. Close).
bool in_dialogue();
/// MessageWindowPatches.RepeatLastDialogue (R; pad mod mode + Square).
void repeat_last_dialogue();

// --- PopupState ------------------------------------------------------------------------
namespace popup_state {
/// PopupState.IsConfirmationPopupActive (the menu_state POPUP flag).
bool is_confirmation_popup_active();
/// PopupState.CurrentPopupType ("" when none).
const std::string& current_popup_type();
/// PopupState.ActivePopupPtr (null when none). An identity: re-check unity::alive before use.
void* active_popup();
/// PopupState.CommandListOffset >= 0: the popup has a button list.
bool has_command_list();
/// PopupState.HasOwnFocusReader.
bool has_own_focus_reader();
/// PopupState.SetActive. `command_list` is the popup's List<CommonCommand> field, or null for a
/// popup without buttons (the C# offset -1). The field ref must outlive the popup (file scope).
void set_active(const char* type_name, void* popup, ref::Field* command_list, bool own_focus_reader = false);
/// PopupState.Clear.
void clear();
/// PopupState.ShouldSuppress: a popup with buttons is active.
bool should_suppress();
} // namespace popup_state

// --- PopupPatches ----------------------------------------------------------------------
/// PopupPatches.ReadCurrentButton: speak the active popup's button at the cursor's index.
void read_current_button(void* cursor);
/// PopupPatches.ReadFocusedButton: the focused button label of a Cursor-driven button popup
/// (e.g. SavePopup), markup stripped, or empty.
std::wstring read_focused_button(void* popup, ref::Field& select_cursor, ref::Field& command_list);
/// Counts ChangeNamePopup opens (the name-entry popup; its description, "Enter a name for X", is
/// spoken one frame later). The naming screens compare it around their input-state Init
/// (KeyboardManager.ShowKeyboard opens the popup inside it) and drop their own entry line when
/// the game's popup text is being read: an event identity, not a text comparison.
int change_name_popup_opens();

// --- TitleMenuPatches / TitleScreenPatches -----------------------------------------------
/// TitleMenuReader.TryGetActiveCommandCount: visible title commands when `cursor` is the title
/// menu's cursor, else -1.
int title_active_command_count(void* cursor);
/// TitleScreenPatches.ArmPressPrompt (FF1 calls it when the "Title" Unity scene loads).
void arm_press_prompt();

} // namespace ffpr::dialogue
