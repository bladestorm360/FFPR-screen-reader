// Ports FF1 Core/GamepadManager.cs (SDL3 gamepad + GetAsyncKeyState keyboard), Core/SDL3.cs
// (replaced by sdl_dyn.h), Utils/WindowsFocusHelper.cs and Core/Handlers/InputHelper.cs.
//
// Differences from the C# mod, both by design of the native port:
//  * The keyboard is read only while the game window has the focus (the C# mod read it always
//    and gated only the hotkeys). While the window is in the background every key reads "up";
//    the first focused frame takes the keys as they are, without press edges, so the key that
//    switched back to the game (Alt+Tab) never fires a hotkey.
//  * Physical controller snapshots are provided by the dedicated SDL owner.
#include <windows.h>
#include <cstring>
#include "input_internal.h"
#include "physical_gamepad.h"
#include "../refs.h"
#include "../unity.h"
#include "../log.h"

namespace input {
namespace {

// --- Unity InputSystem devices (DisableUnityGamepad) ------------------------------------------
ref::Class  kInputSystem{"UnityEngine.InputSystem", "InputSystem"};
ref::Method mGetDevices{kInputSystem, "get_devices", 0};
ref::Method mDisableDevice{kInputSystem, "DisableDevice", 2};
ref::Method mEnableDevice{kInputSystem, "EnableDevice", 1};
ref::Class  kInputDevice{"UnityEngine.InputSystem", "InputDevice"};
ref::Method mDeviceGetEnabled{kInputDevice, "get_enabled", 0};
ref::Class  kUnityGamepad{"UnityEngine.InputSystem", "Gamepad"};
ref::Class  kUnityJoystick{"UnityEngine.InputSystem", "Joystick"};

/// ReadOnlyArray<InputDevice> (m_Array, m_StartIndex, m_Length), returned by value.
struct DeviceArray {
    void* array;
    int32_t start;
    int32_t length;
};

// --- copied physical controller state -----------------------------------------------------------
bool g_physical_ready = false;
bool g_pad_available = false;
int g_pad_type = 0;
bool g_unity_gamepad_disabled = false;
uint32_t g_btn_held = 0, g_btn_down = 0, g_btn_up = 0;
float g_lx = 0, g_ly = 0, g_rx = 0, g_ry = 0, g_lt = 0, g_rt = 0;
constexpr int kDirUp = 0, kDirDown = 1, kDirLeft = 2, kDirRight = 3;
constexpr uint8_t kDirBits[4] = {1u, 2u, 4u, 8u};
uint8_t g_rs_held = 0, g_rs_down = 0, g_ls_held = 0, g_ls_down = 0;
bool g_pad_focus_was = false;
bool g_focus_seed = true;

// --- keyboard -----------------------------------------------------------------------------------
constexpr int kVkMax = 256;
bool g_key_cur[kVkMax]{};
bool g_key_prev[kVkMax]{};
bool g_any_key_down = false;
bool g_prime = false;           // next focused poll takes the state without press edges

// GamepadManager.KeyCodeToVK, in Key order.
constexpr uint8_t kVk[static_cast<int>(Key::Count)] = {
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D,
    0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x26, 0x28, 0x25, 0x27,                     // Up, Down, Left, Right
    0x0D, 0x1B, 0x08, 0x09, 0x20,               // Return, Escape, Backspace, Tab, Space
    0x24, 0x23,                                 // Home, End
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0x5B, 0x5C,
    0xBD, 0xBB, 0xDB, 0xDD, 0xDC, 0xBA, 0xDE, 0xC0, // - = [ ] \ ; ' `
    0xBC, 0xBE, 0xBF,                           // , . /
};

const char* const kKeyNames[static_cast<int>(Key::Count)] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
    "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    "Alpha0", "Alpha1", "Alpha2", "Alpha3", "Alpha4", "Alpha5", "Alpha6", "Alpha7", "Alpha8", "Alpha9",
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8",
    "UpArrow", "DownArrow", "LeftArrow", "RightArrow",
    "Return", "Escape", "Backspace", "Tab", "Space",
    "Home", "End",
    "LeftShift", "RightShift", "LeftControl", "RightControl", "LeftAlt", "RightAlt", "LeftWindows", "RightWindows",
    "Minus", "Equals", "LeftBracket", "RightBracket", "Backslash", "Semicolon", "Quote", "BackQuote",
    "Comma", "Period", "Slash",
};

// --- WindowsFocusHelper -------------------------------------------------------------------------
HWND g_game_window = nullptr;

struct FindWindowState {
    DWORD pid;
    HWND unity;     // UnityWndClass (the game window)
    HWND first;     // first visible unowned top-level window (Process.MainWindowHandle)
};

BOOL CALLBACK find_window_proc(HWND h, LPARAM lp)
{
    auto* st = reinterpret_cast<FindWindowState*>(lp);
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != st->pid || GetWindow(h, GW_OWNER) != nullptr || !IsWindowVisible(h)) return TRUE;
    wchar_t cls[64]{};
    GetClassNameW(h, cls, 64);
    if (wcscmp(cls, L"UnityWndClass") == 0) { st->unity = h; return FALSE; }
    if (!st->first) st->first = h;
    return TRUE;
}

// --- helpers --------------------------------------------------------------------------------------
bool in_range(int b) { return b >= 0 && b < kButtonCount; }

bool is_gamepad_like(void* device)
{
    return kUnityGamepad.is(device) || kUnityJoystick.is(device);
}

DeviceArray unity_devices()
{
    return ref::call_static<DeviceArray>(mGetDevices);
}

void* device_at(const DeviceArray& d, int i)
{
    return unity::array_at(d.array, d.start + i);
}

bool device_enabled(void* device)
{
    return ref::call<bool>(mDeviceGetEnabled, device);
}

void call_disable_device(void* device)
{
    void* fp = mDisableDevice.ptr();
    if (!fp) return;
    using F = void (*)(void*, bool, const void*);
    try { reinterpret_cast<F>(fp)(device, false, mDisableDevice.get()); }
    catch (...) { ref::call_failed_note("InputSystem.DisableDevice"); }
}

void call_enable_device(void* device)
{
    void* fp = mEnableDevice.ptr();
    if (!fp) return;
    using F = void (*)(void*, const void*);
    try { reinterpret_cast<F>(fp)(device, mEnableDevice.get()); }
    catch (...) { ref::call_failed_note("InputSystem.EnableDevice"); }
}

/// Disables every Unity InputSystem gamepad/joystick so the game cannot read the controller
/// through Unity's stack: copied physical snapshots are the source. Every device is checked, not only
/// Gamepad.current, because a reconnect creates a fresh device object.
void disable_unity_gamepad()
{
    int disabled = 0;
    DeviceArray d = unity_devices();
    for (int i = 0; i < d.length; ++i) {
        void* dev = device_at(d, i);
        if (!dev || !is_gamepad_like(dev)) continue;
        if (!device_enabled(dev)) continue;
        call_disable_device(dev);
        ++disabled;
    }
    if (disabled > 0) {
        g_unity_gamepad_disabled = true;
        LOGF("[GamepadManager] Disabled %d Unity gamepad/joystick device(s)", disabled);
    }
}

void enable_unity_gamepad()
{
    if (!g_unity_gamepad_disabled) return;
    DeviceArray d = unity_devices();
    for (int i = 0; i < d.length; ++i) {
        void* dev = device_at(d, i);
        if (!dev || !is_gamepad_like(dev)) continue;
        if (device_enabled(dev)) continue;
        call_enable_device(dev);
    }
    g_unity_gamepad_disabled = false;
}

/// Re-disable when Unity has re-enabled a pad (scene change, hotplug). Every frame while a
/// physical pad is available, so newly registered devices are caught.
void ensure_unity_gamepad_disabled()
{
    if (!gamepad_available()) return;
    DeviceArray d = unity_devices();
    for (int i = 0; i < d.length; ++i) {
        void* dev = device_at(d, i);
        if (!dev || !is_gamepad_like(dev)) continue;
        if (device_enabled(dev)) {
            disable_unity_gamepad();
            return;
        }
    }
}

void poll_keyboard()
{
    g_any_key_down = false;
    if (!game_window_focused()) {
        if (!g_prime) {
            std::memset(g_key_cur, 0, sizeof g_key_cur);
            std::memset(g_key_prev, 0, sizeof g_key_prev);
            g_prime = true;
        }
        return;
    }
    const bool prime = g_prime;
    g_prime = false;
    for (int i = 0; i < static_cast<int>(Key::Count); ++i) {
        const int v = kVk[i];
        const bool down = (GetAsyncKeyState(v) & 0x8000) != 0;
        g_key_prev[v] = prime ? down : g_key_cur[v];
        g_key_cur[v] = down;
        if (down && !g_key_prev[v]) g_any_key_down = true;
    }
}

bool rs_pressed(int d) { return (g_rs_down & kDirBits[d]) != 0; }
bool ls_pressed(int d) { return (g_ls_down & kDirBits[d]) != 0; }

} // namespace

// --- keyboard API ---------------------------------------------------------------------------------
int vk(Key k)
{
    const int i = static_cast<int>(k);
    return (i >= 0 && i < static_cast<int>(Key::Count)) ? kVk[i] : 0;
}

const char* key_name(Key k)
{
    const int i = static_cast<int>(k);
    return (i >= 0 && i < static_cast<int>(Key::Count)) ? kKeyNames[i] : "?";
}

bool key_pressed(Key k)
{
    const int v = vk(k);
    return v > 0 && v < kVkMax && g_key_cur[v] && !g_key_prev[v];
}

bool key_held(Key k)
{
    const int v = vk(k);
    return v > 0 && v < kVkMax && g_key_cur[v];
}

bool key_in_use(Key k)
{
    if (!game_window_focused()) return false;
    const int v = vk(k);
    return v > 0 && v < kVkMax &&
        (g_key_cur[v] || g_key_prev[v] || (GetAsyncKeyState(v) & 0x8000) != 0);
}

bool any_key_pressed() { return g_any_key_down; }

bool shift_held() { return key_held(Key::LeftShift) || key_held(Key::RightShift); }
bool ctrl_held() { return key_held(Key::LeftControl) || key_held(Key::RightControl); }
bool alt_held() { return key_held(Key::LeftAlt) || key_held(Key::RightAlt); }

bool game_window_focused()
{
    if (g_game_window && !IsWindow(g_game_window)) g_game_window = nullptr;
    if (!g_game_window) {
        FindWindowState st{GetCurrentProcessId(), nullptr, nullptr};
        EnumWindows(&find_window_proc, reinterpret_cast<LPARAM>(&st));
        g_game_window = st.unity ? st.unity : st.first;
    }
    if (!g_game_window) {
        // If Unity's window is not resolved yet, only this process's foreground window
        // may receive mod input. A missing cache must not admit another application's keys.
        DWORD foreground_pid = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &foreground_pid);
        return foreground_pid == GetCurrentProcessId();
    }
    return GetForegroundWindow() == g_game_window;
}

// --- gamepad API ----------------------------------------------------------------------------------
bool gamepad_available() { return g_pad_available; }
bool sdl_ready() { return g_physical_ready; }
int gamepad_type() { return g_pad_type; }

bool button_pressed(int b) { return in_range(b) && (g_btn_down & (1u << b)) != 0; }
bool button_held(int b) { return in_range(b) && (g_btn_held & (1u << b)) != 0; }
bool button_released(int b) { return in_range(b) && (g_btn_up & (1u << b)) != 0; }
float left_stick_x() { return g_lx; }
float left_stick_y() { return g_ly; }
float right_stick_x() { return g_rx; }
float right_stick_y() { return g_ry; }
float left_trigger() { return g_lt; }
float right_trigger() { return g_rt; }

namespace gamepad {

bool dpad_up_pressed() { return button_pressed(kDpadUp); }
bool dpad_down_pressed() { return button_pressed(kDpadDown); }
bool dpad_left_pressed() { return button_pressed(kDpadLeft); }
bool dpad_right_pressed() { return button_pressed(kDpadRight); }
bool rstick_up_pressed() { return rs_pressed(kDirUp); }
bool rstick_down_pressed() { return rs_pressed(kDirDown); }
bool rstick_left_pressed() { return rs_pressed(kDirLeft); }
bool rstick_right_pressed() { return rs_pressed(kDirRight); }
bool lstick_up_pressed() { return ls_pressed(kDirUp); }
bool lstick_down_pressed() { return ls_pressed(kDirDown); }
bool lstick_left_pressed() { return ls_pressed(kDirLeft); }
bool lstick_right_pressed() { return ls_pressed(kDirRight); }
bool lstick_up_held() { return (g_ls_held & kDirBits[kDirUp]) != 0; }
bool lstick_down_held() { return (g_ls_held & kDirBits[kDirDown]) != 0; }
bool lstick_left_held() { return (g_ls_held & kDirBits[kDirLeft]) != 0; }
bool lstick_right_held() { return (g_ls_held & kDirBits[kDirRight]) != 0; }

void initialize()
{
    physical::request();
}

void update()
{
    // Keyboard via GetAsyncKeyState: works without SDL.
    poll_keyboard();

    initialize();
    const physical::Pad pad = physical::take();
    const bool connected = pad.available;
    const bool focused = game_window_focused();
    if (connected && !g_pad_available) disable_unity_gamepad();
    if (!connected) enable_unity_gamepad();

    const bool seed = connected && (!g_pad_available || !g_pad_focus_was || g_focus_seed);
    g_physical_ready = pad.ready;
    g_pad_available = pad.available;
    g_pad_type = connected ? pad.type : 0;
    g_btn_held = g_rs_held = g_ls_held = 0;
    g_btn_down = g_btn_up = 0;
    g_rs_down = g_ls_down = 0;
    g_lx = g_ly = g_rx = g_ry = g_lt = g_rt = 0.f;

    if (!focused) {
        g_pad_focus_was = false;
        g_focus_seed = true;
    } else if (connected) {
        g_btn_held = pad.held;
        g_rs_held = pad.right;
        g_ls_held = pad.left;
        if (!seed) {
            g_btn_down = pad.down;
            g_btn_up = pad.up;
            g_rs_down = pad.right_down;
            g_ls_down = pad.left_down;
        }
        g_lx = pad.lx; g_ly = pad.ly; g_rx = pad.rx; g_ry = pad.ry;
        g_lt = !seed && pad.trigger_down && pad.lt <= .5f ? 1.f : pad.lt;
        g_rt = pad.rt;
        g_pad_focus_was = true;
        g_focus_seed = false;
        ensure_unity_gamepad_disabled();
    } else {
        g_pad_focus_was = focused;
    }
}

void shutdown()
{
    physical::shutdown();
    g_physical_ready = g_pad_available = false;
    g_pad_type = 0;
    g_btn_held = g_btn_down = g_btn_up = 0;
    g_rs_held = g_rs_down = g_ls_held = g_ls_down = 0;
    g_lx = g_ly = g_rx = g_ry = g_lt = g_rt = 0.f;
    enable_unity_gamepad();
}

} // namespace gamepad
} // namespace input
