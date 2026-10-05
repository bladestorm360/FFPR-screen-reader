// Ports FF1 Core/GamepadManager.cs (SDL3 gamepad + GetAsyncKeyState keyboard), Core/SDL3.cs
// (replaced by sdl_dyn.h), Utils/WindowsFocusHelper.cs and Core/Handlers/InputHelper.cs.
//
// Differences from the C# mod, both by design of the native port:
//  * The keyboard is read only while the game window has the focus (the C# mod read it always
//    and gated only the hotkeys). While the window is in the background every key reads "up";
//    the first focused frame takes the keys as they are, without press edges, so the key that
//    switched back to the game (Alt+Tab) never fires a hotkey.
//  * SDL3 is loaded and the gamepad subsystem started on the first frame tick (sdl::load must
//    run from a game callback), not at mod initialisation.
#include <windows.h>
#include <cstring>
#include <vector>
#include "input_internal.h"
#include "../sdl_dyn.h"
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

// --- SDL state ----------------------------------------------------------------------------------
bool g_sdl_initialized = false;
SDL_Gamepad* g_pad = nullptr;
int g_pad_type = SDL_GAMEPAD_TYPE_UNKNOWN;
bool g_init_tried = false;
bool g_unity_gamepad_disabled = false;

bool g_btn_cur[kButtonCount]{};
bool g_btn_prev[kButtonCount]{};

float g_lx = 0, g_ly = 0, g_rx = 0, g_ry = 0, g_lt = 0, g_rt = 0;

constexpr int16_t kAxisDeadzone = 8000;
constexpr int16_t kRStickThreshold = 16000;
constexpr int16_t kRStickDominance = 8000;
constexpr int16_t kLStickDirThreshold = 16000;
constexpr int kDirUp = 0, kDirDown = 1, kDirLeft = 2, kDirRight = 3;

bool g_rs_cur[4]{}, g_rs_prev[4]{};
bool g_ls_cur[4]{}, g_ls_prev[4]{};

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
float apply_deadzone(int16_t raw)
{
    if (raw > -kAxisDeadzone && raw < kAxisDeadzone) return 0.f;
    const float max = 32767.f;
    const float dz = kAxisDeadzone;
    if (raw > 0) return (raw - dz) / (max - dz);
    return (raw + dz) / (max - dz);
}

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
/// through Unity's stack: SDL is the only source. Every device is checked, not only
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
/// pad is open, so devices Unity registers after SDL's ADDED event are caught.
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

void open_gamepad(SDL_JoystickID id)
{
    g_pad = sdl::SDL_OpenGamepad(id);
    if (g_pad) {
        g_pad_type = static_cast<int>(sdl::SDL_GetGamepadType(g_pad));
        const char* name = sdl::SDL_GetGamepadName(g_pad);
        std::wstring type = controller_type_name(g_pad_type);
        LOGF("[GamepadManager] Opened gamepad id=%u, name=%s, type=%ls", static_cast<unsigned>(id),
             name ? name : "Unknown", type.c_str());
    } else {
        LOGF("[GamepadManager] SDL_OpenGamepad(%u) returned null", static_cast<unsigned>(id));
    }
}

void open_first_gamepad()
{
    int count = 0;
    SDL_JoystickID* ids = sdl::SDL_GetGamepads(&count);
    if (!ids) return;
    if (count > 0) open_gamepad(ids[0]);
    sdl::SDL_free(ids);
}

/// Close the handle and wipe the cached state so a reconnect never sees a stale edge.
void close_and_clear_gamepad_state()
{
    if (g_pad) sdl::SDL_CloseGamepad(g_pad);
    g_pad = nullptr;
    g_pad_type = SDL_GAMEPAD_TYPE_UNKNOWN;
    std::memset(g_btn_cur, 0, sizeof g_btn_cur);
    std::memset(g_btn_prev, 0, sizeof g_btn_prev);
    std::memset(g_rs_cur, 0, sizeof g_rs_cur);
    std::memset(g_rs_prev, 0, sizeof g_rs_prev);
    std::memset(g_ls_cur, 0, sizeof g_ls_cur);
    std::memset(g_ls_prev, 0, sizeof g_ls_prev);
    g_lx = g_ly = g_rx = g_ry = 0.f;
    g_lt = g_rt = 0.f;
}

void handle_gamepad_removed(SDL_JoystickID id)
{
    if (!g_pad) return;
    // Only the removal of the pad we own matters.
    if (sdl::SDL_GetGamepadID(g_pad) != id) return;
    close_and_clear_gamepad_state();
    LOGF("[GamepadManager] Controller disconnected");
}

void handle_gamepad_added(SDL_JoystickID id)
{
    // Hot swap without a REMOVED in between: drop the old handle first.
    if (g_pad) close_and_clear_gamepad_state();
    open_gamepad(id);
    if (g_pad) disable_unity_gamepad();
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

void poll_gamepad()
{
    for (int i = 0; i < kButtonCount; ++i) {
        g_btn_prev[i] = g_btn_cur[i];
        g_btn_cur[i] = sdl::SDL_GetGamepadButton(g_pad, static_cast<SDL_GamepadButton>(i));
    }

    const int16_t raw_lx = sdl::SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_LEFTX);
    const int16_t raw_ly = sdl::SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_LEFTY);
    g_lx = apply_deadzone(raw_lx);
    g_ly = -apply_deadzone(raw_ly);   // SDL Y positive = down, Unity positive = up

    const int16_t raw_rx = sdl::SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_RIGHTX);
    const int16_t raw_ry = sdl::SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_RIGHTY);
    g_rx = apply_deadzone(raw_rx);
    g_ry = -apply_deadzone(raw_ry);

    const int16_t raw_lt = sdl::SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    const int16_t raw_rt = sdl::SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    g_lt = (raw_lt > 0 ? static_cast<int>(raw_lt) : 0) / 32767.f;
    g_rt = (raw_rt > 0 ? static_cast<int>(raw_rt) : 0) / 32767.f;

    // Right stick: cardinal isolation.
    for (int i = 0; i < 4; ++i) g_rs_prev[i] = g_rs_cur[i];
    const int abs_rx = raw_rx < 0 ? -static_cast<int>(raw_rx) : raw_rx;
    const int abs_ry = raw_ry < 0 ? -static_cast<int>(raw_ry) : raw_ry;
    const bool y_dom = abs_ry > kRStickThreshold && abs_ry > abs_rx + kRStickDominance;
    const bool x_dom = abs_rx > kRStickThreshold && abs_rx > abs_ry + kRStickDominance;
    g_rs_cur[kDirUp] = y_dom && raw_ry < 0;
    g_rs_cur[kDirDown] = y_dom && raw_ry > 0;
    g_rs_cur[kDirLeft] = x_dom && raw_rx < 0;
    g_rs_cur[kDirRight] = x_dom && raw_rx > 0;

    // Left stick: directions for menu navigation edges.
    for (int i = 0; i < 4; ++i) g_ls_prev[i] = g_ls_cur[i];
    g_ls_cur[kDirUp] = raw_ly < -kLStickDirThreshold;
    g_ls_cur[kDirDown] = raw_ly > kLStickDirThreshold;
    g_ls_cur[kDirLeft] = raw_lx < -kLStickDirThreshold;
    g_ls_cur[kDirRight] = raw_lx > kLStickDirThreshold;
}

bool rs_pressed(int d) { return g_rs_cur[d] && !g_rs_prev[d]; }
bool ls_pressed(int d) { return g_ls_cur[d] && !g_ls_prev[d]; }

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
    if (!g_game_window) return true;   // fail open: input is never blocked for good
    return GetForegroundWindow() == g_game_window;
}

// --- gamepad API ----------------------------------------------------------------------------------
bool gamepad_available() { return g_sdl_initialized && g_pad != nullptr; }
bool sdl_ready() { return g_sdl_initialized; }
int gamepad_type() { return g_pad_type; }

bool button_pressed(int b) { return in_range(b) && g_btn_cur[b] && !g_btn_prev[b]; }
bool button_held(int b) { return in_range(b) && g_btn_cur[b]; }
bool button_released(int b) { return in_range(b) && !g_btn_cur[b] && g_btn_prev[b]; }
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
bool lstick_up_held() { return g_ls_cur[kDirUp]; }
bool lstick_down_held() { return g_ls_cur[kDirDown]; }
bool lstick_left_held() { return g_ls_cur[kDirLeft]; }
bool lstick_right_held() { return g_ls_cur[kDirRight]; }

void initialize()
{
    if (g_init_tried) return;
    g_init_tried = true;
    if (!sdl::load()) {
        LOGF("[GamepadManager] SDL3.dll not available - gamepad features disabled");
        return;
    }
    // Subsystem-scoped init (not SDL_Init): the audio engine starts SDL_INIT_AUDIO on its own
    // and neither teardown takes the other down.
    if (!sdl::SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        const char* err = sdl::SDL_GetError();
        LOGF("[GamepadManager] SDL_InitSubSystem(GAMEPAD) failed: %s", err ? err : "");
        return;
    }
    g_sdl_initialized = true;
    open_first_gamepad();
    if (g_pad) disable_unity_gamepad();
    else LOGF("[GamepadManager] SDL3 initialized but no gamepad found");
}

void update()
{
    // Keyboard via GetAsyncKeyState: works without SDL.
    poll_keyboard();

    initialize();
    if (!g_sdl_initialized) return;

    sdl::SDL_PumpEvents();
    // Drain the queue and pick out gamepad add/remove so an unplugged and replugged
    // controller recovers. The keyboard does not use SDL events.
    SDL_Event evt;
    while (sdl::SDL_PollEvent(&evt)) {
        if (evt.type == SDL_EVENT_GAMEPAD_REMOVED) handle_gamepad_removed(evt.gdevice.which);
        else if (evt.type == SDL_EVENT_GAMEPAD_ADDED) handle_gamepad_added(evt.gdevice.which);
    }

    if (g_pad) {
        poll_gamepad();
        ensure_unity_gamepad_disabled();
    }
}

void shutdown()
{
    if (g_pad) {
        sdl::SDL_CloseGamepad(g_pad);
        g_pad = nullptr;
    }
    if (g_sdl_initialized) {
        // Only the gamepad subsystem: never SDL_Quit, which would close the audio device.
        sdl::SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        g_sdl_initialized = false;
    }
    enable_unity_gamepad();
}

} // namespace gamepad
} // namespace input
