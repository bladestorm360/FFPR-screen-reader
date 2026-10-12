// Actual physical SDK owner and native adapter with fake SDL/Unity/Win32 input.
// No SDL library, device, game or real window/input manipulation is used.
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <thread>
#include <cstring>
namespace fixture {
bool focused = true;
HWND WINAPI foreground() { return focused ? reinterpret_cast<HWND>(1) : reinterpret_cast<HWND>(2); }
BOOL WINAPI valid(HWND) { return TRUE; }
SHORT WINAPI key(int) { return 0; }
}
#define GetForegroundWindow fixture::foreground
#define IsWindow fixture::valid
#define GetAsyncKeyState fixture::key
#include "../src/core/input/gamepad.cpp"
#undef GetForegroundWindow
#undef IsWindow
#undef GetAsyncKeyState
#include "../src/core/input/physical_gamepad.cpp"
namespace ref {
Class::Class(const char* n, const char* c, uint8_t g, const char* a) : ns(n), name(c), games(g), assembly(a) {}
Method::Method(Class& c, const char* n, int a) : cls(c), name(n), argc(a) {}
void* Method::ptr() { return nullptr; }
il2cpp::Method* Method::get() { return nullptr; }
il2cpp::Class* Class::get() { return nullptr; }
bool Class::is(void*) { return false; }
void call_failed_note(const char*) { std::abort(); }
}
namespace unity { void* array_at(void*, int) { return nullptr; } }
namespace core { Install::Install(const char* n, int (*f)(), int o, uint8_t g) : name(n), fn(f), order(o), games(g) {} }
namespace log_ { void write(const char*, ...) {} }
namespace fixture {
const DWORD native_owner = GetCurrentThreadId();
std::atomic<DWORD> sdk_owner{0};
std::atomic<bool> wrong_owner{false}, audio_ready{false}, quit{false};
std::atomic<int> pumps{0};
std::atomic<uint32_t> held{0};
std::atomic<int16_t> left_trigger{0};
std::atomic<int> hotplug{0};
bool fail_gamepads = false;
HANDLE loading, allow_load;
void sdk_check() { if (GetCurrentThreadId() != sdk_owner.load() || GetCurrentThreadId() == native_owner) wrong_owner = true; }
bool SDLCALL main_thread() { return GetCurrentThreadId() == sdk_owner; }
bool SDLCALL init(SDL_InitFlags) { sdk_check(); return !fail_gamepads; }
void SDLCALL pump() { sdk_check(); ++pumps; }
bool SDLCALL poll(SDL_Event* value) {
    sdk_check(); const int event = hotplug.exchange(0);
    if (!event) return false;
    value->type = event == 1 ? SDL_EVENT_GAMEPAD_REMOVED : SDL_EVENT_GAMEPAD_ADDED;
    value->gdevice.which = 1; return true;
}
SDL_JoystickID* SDLCALL pads(int* count) { sdk_check(); static SDL_JoystickID id = 1; *count = 1; return &id; }
SDL_Gamepad* SDLCALL open(SDL_JoystickID) { sdk_check(); return reinterpret_cast<SDL_Gamepad*>(1); }
void SDLCALL close(SDL_Gamepad*) { sdk_check(); }
const char* SDLCALL name(SDL_Gamepad*) { sdk_check(); return "fake"; }
SDL_JoystickID SDLCALL id(SDL_Gamepad*) { sdk_check(); return 1; }
SDL_GamepadType SDLCALL type(SDL_Gamepad*) { sdk_check(); return SDL_GAMEPAD_TYPE_STANDARD; }
bool SDLCALL button(SDL_Gamepad*, SDL_GamepadButton b) { sdk_check(); return (held.load() & (1u << int(b))) != 0; }
int16_t SDLCALL axis(SDL_Gamepad*, SDL_GamepadAxis a) { sdk_check(); return a == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? left_trigger.load() : 0; }
void SDLCALL free_memory(void*) { sdk_check(); }
void SDLCALL quit_subsystem(SDL_InitFlags) { sdk_check(); quit = true; }
}
namespace sdl {
#define DEFINE(name) decltype(&::name) name = nullptr;
SDL_DYNAMIC_FUNCTIONS(DEFINE)
#undef DEFINE
bool load() {
    fixture::sdk_owner = GetCurrentThreadId(); fixture::sdk_check();
    SetEvent(fixture::loading); WaitForSingleObject(fixture::allow_load, 5000); return true;
}
}
namespace sound::engine { bool initialize() { fixture::sdk_check(); fixture::audio_ready = true; return false; } }
int main(int argc, char**) {
    using namespace fixture;
    fail_gamepads = argc > 1;
    int failures = 0;
    auto expect = [&](bool ok, const char* line) { if (!ok) { ++failures; std::printf("FAIL %s\n", line); } };
    auto until = [&](auto condition) { const DWORD end = GetTickCount() + 5000; bool done = condition(); while (!done && GetTickCount() < end) { std::this_thread::yield(); done = condition(); } return done; };
    loading = CreateEventW(nullptr, TRUE, FALSE, nullptr); allow_load = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    sdl::SDL_IsMainThread = main_thread; sdl::SDL_InitSubSystem = init;
    sdl::SDL_PumpEvents = pump; sdl::SDL_PollEvent = poll; sdl::SDL_GetGamepads = pads;
    sdl::SDL_OpenGamepad = open; sdl::SDL_CloseGamepad = close; sdl::SDL_GetGamepadName = name;
    sdl::SDL_GetGamepadID = id; sdl::SDL_GetGamepadType = type;
    sdl::SDL_GetGamepadButton = button; sdl::SDL_GetGamepadAxis = axis;
    sdl::SDL_free = free_memory; sdl::SDL_QuitSubSystem = quit_subsystem;
    input::g_game_window = reinterpret_cast<HWND>(1);
    input::physical::prepare(); input::gamepad::update();
    expect(WaitForSingleObject(loading, 5000) == WAIT_OBJECT_0, "dedicated SDK owner reaches initialization");
    const DWORD start = GetTickCount();
    for (int i = 0; i < 10000; ++i) input::physical::take();
    expect(GetTickCount() - start < 1000 && !input::gamepad_available(), "native sampling returns while initialization is blocked");
    SetEvent(allow_load);
    expect(until([&] { return pumps > 2 && audio_ready; }), "SDK owner initializes audio and keeps pumping");
    if (!fail_gamepads) {
        expect(until([&] { input::gamepad::update(); return input::gamepad_available(); }), "owned controller snapshot reaches native adapter");
        held = 1u << input::kSouth;
        expect(until([&] { return input::physical::g_down.load() != 0; }), "short button press is retained between native frames");
        held = 0;
        expect(until([&] { return input::physical::g_up.load() != 0; }), "short release is retained independently");
        input::gamepad::update();
        expect(input::button_pressed(input::kSouth) && input::button_released(input::kSouth) && !input::button_held(input::kSouth), "native frame receives both edges of a quick tap");
        focused = false; input::gamepad::update(); held = 1u << input::kSouth; left_trigger = 32767;
        expect(until([&] { return input::physical::g_down.load() != 0; }), "background fixture receives hardware state");
        input::gamepad::update();
        expect(!input::button_held(input::kSouth) && input::left_trigger() == 0, "background input is gated");
        focused = true; input::gamepad::update();
        expect(!input::button_pressed(input::kSouth), "focus return seeds held state without replaying presses");
        held = 0; left_trigger = 0;
        expect(until([&] { return input::physical::g_up.load() != 0; }), "release reaches sampler"); input::gamepad::update();
        left_trigger = 32767;
        expect(until([&] { return input::physical::g_trigger_down.load(); }), "short trigger press retained");
        left_trigger = 0;
        // Wait for the release to be published before resuming focus; seed must drop the pulse.
        expect(until([&] { input::physical::Pad p; return input::physical::g_snapshots.pop(p) && p.lt == 0; }), "worker publishes trigger release");
        focused = false; input::gamepad::update(); focused = true; input::gamepad::update();
        expect(input::left_trigger() == 0, "focus return does not replay a retained short trigger pulse");
        hotplug = 1;
        expect(until([&] { input::gamepad::update(); return !input::gamepad_available(); }), "disconnect hands input back without SDK calls on native owner");
        hotplug = 2;
        expect(until([&] { input::gamepad::update(); return input::gamepad_available(); }), "reconnect is observed asynchronously");
        input::physical::g_heartbeat = GetTickCount() - 1000;
        input::physical::g_ready = false;
        input::gamepad::update();
        expect(!input::gamepad_available() && !input::button_pressed(input::kSouth), "failed/stale backend drops edges and restores native availability");
    } else {
        input::gamepad::update();
        expect(!input::gamepad_available() && !input::sdl_ready(), "gamepad init failure preserves native input");
        expect(pumps > 2 && audio_ready, "failed gamepads do not retire another subsystem's event owner");
    }
    input::gamepad::shutdown();
    expect(until([&] { return !input::physical::g_ready.load(); }), "shutdown requester never joins SDK owner");
    if (!fail_gamepads) expect(until([&] { return quit.load(); }), "SDK teardown stays on SDK owner");
    expect(!wrong_owner, "SDL initialization/pump/gamepad/audio-init share dedicated owner");
    std::printf("Controller worker check: %d failures\n", failures); return failures ? 1 : 0;
}
