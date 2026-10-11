#pragma once
// The mod's own UI: the mod menu, the text input window and the Yes/No confirmation dialog.
// Ports FF1 Core/ModMenu.cs, Core/TextInputWindow.cs and Core/ConfirmationDialog.cs (the
// public surface). All functions: main thread.
//
// KEYBOARD. Each modal reads its keys itself (GetAsyncKeyState edge detection, as FF1's
// GamepadManager did), and only while it is open; the key state is seeded when it opens, so the
// key that opened it is not a press. Each handle_input() runs at most once per frame: a second
// call in the same frame returns the first call's result without acting again.
//
// Wiring (either works, without double handling):
//   * The input area calls ui::handle_modal_input() at FF1 InputManager.Update's place (after
//     the controller router, before the game-window focus check) and returns when it is true.
//   * Otherwise a frame subscriber registered late (install order 490) calls it while a modal
//     is open. The input area must then still skip its hotkeys while a modal is open
//     (queries "TextInputWindow.IsOpen", "ConfirmationDialog.IsOpen", "ModMenu.IsOpen").
//
// GAME INPUT. While any of the three is open, the game must receive no input: FF1's
// ControllerRouter.SuppressGameInput includes the three IsOpen queries.
//
// CONTROLLER. The controller router drives the mod menu through mod_menu::navigate_*,
// adjust_current_item and toggle_current_item (or the actions of the same names). Opening and
// closing the menu runs the actions "ControllerRouter.SyncWithModMenuOpen" and
// "ControllerRouter.SyncWithModMenuClosed" when the router defines them (FF1
// ControllerRouter.SyncWithModMenu(true/false)).
//
// Queries: "ModMenu.IsOpen", "TextInputWindow.IsOpen", "ConfirmationDialog.IsOpen".
// Actions:  "ModMenu.Open", "ModMenu.Close", "ModMenu.HandleInput", "ModMenu.NavigateNext",
//           "ModMenu.NavigatePrevious", "ModMenu.AdjustCurrentItemLeft" (-1),
//           "ModMenu.AdjustCurrentItemRight" (+1), "ModMenu.ToggleCurrentItem",
//           "TextInputWindow.HandleInput", "ConfirmationDialog.HandleInput".
#include <functional>
#include <string>

namespace ui {

/// FF1 InputManager.Update's modal block, in its order: TextInputWindow.HandleInput, then
/// ConfirmationDialog.HandleInput, then ModMenu.HandleInput. True when one of them is open and
/// took this frame's keyboard input; the caller then skips its own hotkeys this frame.
bool handle_modal_input();
/// True while the mod menu, the text input window or the confirmation dialog is open.
bool any_modal_open();

namespace mod_menu {
/// FF1 ModMenu.IsOpen.
bool is_open();
/// Opens the menu: "Mod menu", then the first item 0.15 s later. No-op when open.
void open();
/// Closes the menu: "Mod menu closed". No-op when closed.
void close();
/// Return to the category list, retaining focus; close when already at the root.
void back();
/// Keyboard while open: Up/Down move, Left/Right change, Enter or Space toggle, I describes,
/// Escape or F8 close. True while open (all keys are consumed).
bool handle_input();
void navigate_next();
void navigate_previous();
/// -1 = left (decrease), +1 = right (increase).
void adjust_current_item(int delta);
void toggle_current_item();
} // namespace mod_menu

namespace text_input {
using Confirm = std::function<void(const std::wstring&)>;
using Cancel = std::function<void()>;
/// FF1 TextInputWindow.IsOpen.
bool is_open();
/// Opens the text entry (used to name waypoints). The prompt (": initial" when there is
/// initial text) is spoken after 0.3 s. Enter confirms the trimmed text (empty is refused):
/// the window closes, and 0.3 s later "Confirmed: <text>" is spoken and on_done(text) runs.
/// Escape closes, says "Cancelled" 0.3 s later and runs on_cancel. No-op when already open.
void open(const std::wstring& prompt, const std::wstring& initial, Confirm on_done, Cancel on_cancel = {});
void close();
/// Keyboard while open. True while open (all keys are consumed).
bool handle_input();
} // namespace text_input

namespace confirmation {
using Callback = std::function<void()>;
/// FF1 ConfirmationDialog.IsOpen.
bool is_open();
/// Opens a Yes/No question ("<prompt> Yes or No", 0.1 s later; Yes is selected). Y, N,
/// Escape (= No, says "Cancelled"), Enter (the selection), Left/Right (switch). The answer is
/// spoken 0.1 s later, then its callback runs. A callback may open the next question: the
/// dialog then stays open and the new prompt is spoken at once (chained prompts).
void open(const std::wstring& prompt, Callback on_yes, Callback on_no = {});
void close();
/// Keyboard while open. True while open (all keys are consumed).
bool handle_input();
} // namespace confirmation

} // namespace ui
