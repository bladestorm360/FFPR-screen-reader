// Dedicated SDL owner on Windows. Engine/native objects never enter this thread.
#include "physical_gamepad.h"
#include "../nonblocking_queue.h"
#include "../sdl_dyn.h"
#include "../audio/audio_engine.h"
#include "../core.h"
#include "../hooks.h"
#include "../log.h"
#include <windows.h>
#include <atomic>
#include <cmath>
#include <algorithm>
namespace input::physical {
namespace {
HANDLE g_wake = nullptr;
std::atomic<bool> g_requested{false}, g_stop{false};
NonblockingQueue<Pad, 8> g_snapshots;
std::atomic<uint32_t> g_down{0}, g_up{0};
std::atomic<uint8_t> g_rdown{0}, g_ldown{0};
std::atomic<bool> g_trigger_down{false};
Pad g_last; // native owner only
SDL_Gamepad* g_pad = nullptr; // SDL owner only
bool g_prime = true;
std::atomic<bool> g_ready{false};
std::atomic<DWORD> g_heartbeat{0};
void close_pad() { if (g_pad) sdl::SDL_CloseGamepad(g_pad); g_pad = nullptr; g_prime = true; }
void open_pad(SDL_JoystickID id) {
    close_pad(); g_pad = sdl::SDL_OpenGamepad(id);
    if (g_pad) LOGF("[GamepadManager] worker opened gamepad id=%u, name=%s", id, sdl::SDL_GetGamepadName(g_pad));
}
uint8_t directions(int16_t x, int16_t y) {
    const int ax = std::abs(static_cast<int>(x)), ay = std::abs(static_cast<int>(y));
    if (ay > 16000 && ay > ax + 8000) return y < 0 ? 1 : 2;
    if (ax > 16000 && ax > ay + 8000) return x < 0 ? 4 : 8;
    return 0;
}
void publish(const Pad& current, const Pad& before) {
    if (!g_prime) {
        g_down.fetch_or(current.held & ~before.held);
        g_up.fetch_or(before.held & ~current.held);
        g_rdown.fetch_or(current.right & ~before.right);
        g_ldown.fetch_or(current.left & ~before.left);
        if (current.lt > .5f && before.lt <= .5f) g_trigger_down = true;
    }
    g_prime = false;
    Pad value = current;
    if (!g_snapshots.push(std::move(value))) { Pad old; g_snapshots.pop(old); g_snapshots.push(std::move(value)); }
}
DWORD WINAPI worker(void*) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    WaitForSingleObject(g_wake, INFINITE);
    if (!g_requested || g_stop) return 0;
    try {
        if (!sdl::load() || !sdl::SDL_IsMainThread()) return 0;
        const bool gamepads = sdl::SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        // Same designated SDL owner initializes audio; driver/device operations
        // remain on their separate worker, and native frames never call SDL init.
        sound::engine::initialize();
        g_ready = gamepads;
        if (gamepads) {
            int count = 0; auto* ids = sdl::SDL_GetGamepads(&count);
            if (ids && count) open_pad(ids[0]); if (ids) sdl::SDL_free(ids);
        }
        Pad before;
        while (!g_stop.load()) {
            sdl::SDL_PumpEvents();
            SDL_Event event;
            while (sdl::SDL_PollEvent(&event)) {
                if (gamepads && event.type == SDL_EVENT_GAMEPAD_ADDED) open_pad(event.gdevice.which);
                else if (event.type == SDL_EVENT_GAMEPAD_REMOVED && g_pad && sdl::SDL_GetGamepadID(g_pad) == event.gdevice.which) close_pad();
            }
            Pad current;
            current.ready = gamepads;
            if (g_pad) {
                current.available = true; current.type = static_cast<int>(sdl::SDL_GetGamepadType(g_pad));
                for (int i = 0; i < 15; ++i) if (sdl::SDL_GetGamepadButton(g_pad, static_cast<SDL_GamepadButton>(i))) current.held |= 1u << i;
                const int16_t x = sdl::SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_LEFTX), y = sdl::SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_LEFTY);
                const int16_t rx = sdl::SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_RIGHTX), ry = sdl::SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_RIGHTY);
                auto axis = [](int16_t value) { return std::abs(static_cast<int>(value)) < 8000 ? 0.0f : float(value) / 32767.0f; };
                current.lx = axis(x); current.ly = -axis(y); current.rx = axis(rx); current.ry = -axis(ry);
                current.lt = std::max(0.0f, sdl::SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f);
                current.rt = std::max(0.0f, sdl::SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f);
                current.right = directions(rx, ry);
                if (y < -16000) current.left |= 1; if (y > 16000) current.left |= 2;
                if (x < -16000) current.left |= 4; if (x > 16000) current.left |= 8;
            }
            publish(current, before); before = current;
            g_heartbeat = GetTickCount();
            WaitForSingleObject(g_wake, 5); // physical polling only, never engine/game polling
        }
        close_pad(); if (gamepads) sdl::SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    } catch (...) { LOGF("[GamepadManager] SDL owner failed; native controller input retained"); }
    g_ready = false;
    return 0;
}
int install() { prepare(); return 0; }
core::Install s_install{"Input.PhysicalWorker", &install, 15};
}
void prepare() {
    if (g_wake) return;
    g_wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_wake) return;
    HANDLE thread = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
    if (thread) CloseHandle(thread);
    else { CloseHandle(g_wake); g_wake = nullptr; }
}
void request() { if (g_wake && !g_requested.exchange(true)) SetEvent(g_wake); }
Pad take() {
    Pad next; for (int i = 0; i < 8 && g_snapshots.pop(next); ++i) g_last = next;
    Pad result = g_last;
    result.down = g_down.exchange(0); result.up = g_up.exchange(0);
    result.right_down = g_rdown.exchange(0); result.left_down = g_ldown.exchange(0);
    result.trigger_down = g_trigger_down.exchange(false);
    if (g_stop || !g_ready || !result.available || GetTickCount() - g_heartbeat.load() > 100) result = {};
    return result;
}
void shutdown() { g_stop = true; if (g_wake) SetEvent(g_wake); }
}
