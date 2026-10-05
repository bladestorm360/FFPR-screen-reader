// Ports the modal-input parts of FF1 Core/InputManager.cs (the TextInputWindow ->
// ConfirmationDialog -> ModMenu HandleInput chain) and the keyboard half of FF1
// Core/GamepadManager.cs that those three classes read (GetAsyncKeyState edge detection).
#include <windows.h>
#include <cstdint>
#include "core.h"
#include "frame.h"
#include "log.h"
#include "mod_ui.h"
#include "modal_keys.h"

namespace ui {
namespace modal_keys {
namespace {

// Every key the three modals read (FF1 GamepadManager.KeyCodeToVK, the entries they use).
constexpr int kOtherKeys[] = {
    VK_ESCAPE, VK_F8, VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_RETURN, VK_SPACE, VK_BACK,
    VK_HOME, VK_END, VK_LSHIFT, VK_RSHIFT,
    VK_OEM_MINUS, VK_OEM_PERIOD, VK_OEM_COMMA, VK_OEM_7 /* quote */, VK_OEM_1 /* semicolon */,
    VK_OEM_2 /* slash */, VK_OEM_3 /* backquote */, VK_OEM_4 /* [ */, VK_OEM_5 /* backslash */,
    VK_OEM_6 /* ] */, VK_OEM_PLUS /* equals */,
};

bool g_cur[256];
bool g_prev[256];
uint64_t g_sampled_frame = UINT64_MAX;

bool hw_down(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

template <typename F>
void for_each_key(F&& fn)
{
    for (int vk = 'A'; vk <= 'Z'; ++vk) fn(vk);
    for (int vk = '0'; vk <= '9'; ++vk) fn(vk);
    for (int vk : kOtherKeys) fn(vk);
}

} // namespace

void seed()
{
    for_each_key([](int vk) {
        g_cur[vk] = hw_down(vk);
        g_prev[vk] = g_cur[vk];
    });
    g_sampled_frame = frame::count();
}

void sample()
{
    const uint64_t f = frame::count();
    if (f == g_sampled_frame) return;
    g_sampled_frame = f;
    for_each_key([](int vk) {
        g_prev[vk] = g_cur[vk];
        g_cur[vk] = hw_down(vk);
    });
}

bool pressed(int vk) { return vk >= 0 && vk < 256 && g_cur[vk] && !g_prev[vk]; }
bool held(int vk) { return vk >= 0 && vk < 256 && g_cur[vk]; }

} // namespace modal_keys

bool handle_modal_input()
{
    // FF1 InputManager.Update: "Modal dialogs consume all keyboard input when open", then the
    // mod menu. Each returns early when it took the input.
    if (text_input::handle_input()) return true;
    if (confirmation::handle_input()) return true;
    if (mod_menu::handle_input()) return true;
    return false;
}

bool any_modal_open()
{
    return text_input::is_open() || confirmation::is_open() || mod_menu::is_open();
}

namespace {

// Drives the modals when the input area has not already done so this frame (each
// handle_input runs once per frame). Does nothing while no modal is open.
void modal_tick()
{
    if (any_modal_open()) handle_modal_input();
}

int install()
{
    // Registered late (order 490) so that it runs after the input area's per-frame handler:
    // when that handler already called handle_modal_input() this frame, this one is a no-op.
    frame::subscribe(&modal_tick, "ModUI.Input");
    return 0;
}

core::Install s_install{"ModUI.Input", &install, 490};

} // namespace
} // namespace ui
