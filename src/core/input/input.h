#pragma once
#include <cstdint>
#include <string>

// The input area's public API: keyboard state, gamepad state, controller labels, the
// controller router and the key-binding registry. Other areas read keys and router state
// through this header (or through the queries listed in docs/INPUT_BINDINGS.md).
//
// Ports FF1 Core/GamepadManager.cs, Core/ControllerRouter.cs, Core/ControllerLabels.cs,
// Core/InputManager.cs, Core/KeyBinding.cs, Core/KeyBindingRegistry.cs,
// Core/Handlers/InputHelper.cs and Utils/WindowsFocusHelper.cs (public members).
//
// Everything here is main-thread only (the frame tick, hooks, actions).
namespace input {

// --- keys ---------------------------------------------------------------------------------
// The Unity KeyCode names the mod uses. Each maps to a Windows virtual key (gamepad.cpp,
// GamepadManager.KeyCodeToVK). The keyboard is read with GetAsyncKeyState once per frame, and
// only while the game window has the focus.
enum class Key : uint8_t {
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Alpha0, Alpha1, Alpha2, Alpha3, Alpha4, Alpha5, Alpha6, Alpha7, Alpha8, Alpha9,
    F1, F2, F3, F4, F5, F6, F7, F8,
    UpArrow, DownArrow, LeftArrow, RightArrow,
    Return, Escape, Backspace, Tab, Space,
    Home, End,
    LeftShift, RightShift, LeftControl, RightControl, LeftAlt, RightAlt, LeftWindows, RightWindows,
    Minus, Equals, LeftBracket, RightBracket, Backslash, Semicolon, Quote, BackQuote,
    Comma, Period, Slash,
    Count
};

/// GamepadManager.IsKeyCodePressed: true on the frame the key went down.
bool key_pressed(Key k);
/// GamepadManager.IsKeyCodeHeld.
bool key_held(Key k);
/// Down/held/release ownership, including the physical state before the frame poll.
bool key_in_use(Key k);
/// GamepadManager.AnyKeyboardKeyDown: any tracked key went down this frame.
bool any_key_pressed();
/// InputHelper.IsShiftHeld / IsCtrlHeld (either side).
bool shift_held();
bool ctrl_held();
bool alt_held();
/// The Windows virtual key for a Key, and its Unity name ("LeftBracket").
int vk(Key k);
const char* key_name(Key k);

/// WindowsFocusHelper.IsGameWindowFocused: the game's main window is the foreground window.
/// Unresolved windows fall back to comparing the foreground process ID.
bool game_window_focused();

// --- gamepad (SDL3) ------------------------------------------------------------------------
/// SDL3 button indices (SDL_GAMEPAD_BUTTON_SOUTH = 0 .. DPAD_RIGHT = 14); the mod tracks 15.
constexpr int kButtonCount = 15;

/// GamepadManager.IsAvailable: SDL3 loaded and a gamepad is open.
bool gamepad_available();
/// GamepadManager.IsSdlReady.
bool sdl_ready();
/// GamepadManager.GamepadType (SDL_GamepadType).
int gamepad_type();

bool button_pressed(int button);
bool button_held(int button);
bool button_released(int button);
float left_stick_x();
float left_stick_y();
float right_stick_x();
float right_stick_y();
float left_trigger();
float right_trigger();

// --- ControllerLabels ----------------------------------------------------------------------
std::wstring button_label(int sdl_button);
std::wstring left_trigger_label();
std::wstring right_trigger_label();
std::wstring left_stick_click_label();
std::wstring right_stick_click_label();
std::wstring controller_type_name(int sdl_gamepad_type);

// --- ControllerRouter ----------------------------------------------------------------------
enum class RouterState : uint8_t { Normal, ModMode, ModMenu };
RouterState router_state();
/// ControllerRouter.IsFieldActive (computed once per frame).
bool field_active();
/// ControllerRouter.SuppressGameInput.
bool suppress_game_input();
/// ControllerRouter.SyncWithModMenu, called by ModMenu.Open/Close (also defined as the actions
/// "ControllerRouter.SyncWithModMenuOpen" / "ControllerRouter.SyncWithModMenuClosed").
void sync_with_mod_menu(bool menu_open);
/// ControllerRouter.SpeakModMenuUnavailable.
void speak_mod_menu_unavailable();
/// ControllerRouter.AnnounceContextControls.
void announce_context_controls();

// --- key bindings --------------------------------------------------------------------------
enum class Mod : uint8_t { None = 0, Shift = 1, Ctrl = 2, CtrlShift = 3 };
/// KeyContext, in FF1's order (the registry sorts the higher value first). Shop is the native
/// mod's own (2026-10-10): the shop is open (ShopMenuTracker), for the number row.
enum class Context : uint8_t { Global, Field, Battle, Shop, Status, BestiaryDetail, Results, KeyHelp };

/// Add a key binding that runs a named action. A game module adds its own keys here (from its
/// install); the most specific modifier, then the most specific context, is tried first.
void add_binding(Key key, Mod mod, Context ctx, const char* action, const char* description);

// --- FFI_ScreenReaderMod helpers -----------------------------------------------------------
/// FFI_ScreenReaderMod.SaveAndAnnounce: speaks "<feature> on" / "<feature> off".
void save_and_announce(const std::wstring& feature, bool value);

/// GamepadManager.Shutdown + ControllerRouter.Reset (close the pad, quit the SDL gamepad
/// subsystem, give Unity its gamepads back). Main thread; not needed at process exit.
void shutdown();

} // namespace input
