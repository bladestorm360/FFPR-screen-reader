#include <windows.h>
#include "runtime_gate.h"
#include "il2cpp.h"
#include "minhook_dyn.h"
#include "log.h"

namespace {

HANDLE        g_event = nullptr;
volatile LONG g_fired = 0;
int           g_which = -1;

// Uniform pass-through signature.
//
// We do not care what these functions do or what they return — we only need to know that
// one of them was CALLED, and then hand the call straight on. Under the Microsoft x64 ABI
// the first four arguments travel in RCX/RDX/R8/R9, so a detour declared with four pointer
// parameters can forward a call to any of these (all take four arguments or fewer) without
// knowing the real prototype: surplus registers are ignored by a callee that takes fewer,
// and none of them takes a fifth argument, so there is no stack argument to get wrong. A
// void-returning target simply leaves RAX unread. proxy.cpp forwards
// GetFileVersionInfoByHandle on the same reasoning.
using fn_any = void* (WINAPI*)(void*, void*, void*, void*);

struct Wire {
    const char* name;
    void*       target;
    fn_any      original;
};

// Every one of these is resolved by UnityPlayer (confirmed against its own string table)
// and is called only after il2cpp_init has returned.
Wire g_wires[] = {
    { "il2cpp_runtime_class_init", nullptr, nullptr },   // Unity bootstraps its bindings
    { "il2cpp_object_new",         nullptr, nullptr },   // native -> managed object
    { "il2cpp_string_new",         nullptr, nullptr },   // native -> managed string
    { "il2cpp_runtime_invoke",     nullptr, nullptr },   // native -> managed call
};
constexpr int kWireCount = static_cast<int>(sizeof(g_wires) / sizeof(g_wires[0]));

/// Runs on a GAME thread, inside a call the game is waiting on. Two interlocked ops on the
/// first call and one on every call after — no allocation, no logging, no runtime calls.
void trip(int which)
{
    if (InterlockedCompareExchange(&g_fired, 1, 0) != 0) return;
    g_which = which;
    if (g_event) SetEvent(g_event);
}

// One detour per wire: each needs its own trampoline, so each needs its own function.
#define WIRE_DETOUR(n)                                                        \
    void* WINAPI detour##n(void* a, void* b, void* c, void* d)                \
    {                                                                         \
        trip(n);                                                              \
        return g_wires[n].original ? g_wires[n].original(a, b, c, d) : nullptr;\
    }
WIRE_DETOUR(0)
WIRE_DETOUR(1)
WIRE_DETOUR(2)
WIRE_DETOUR(3)
#undef WIRE_DETOUR

fn_any detour_for(int i)
{
    switch (i) {
    case 0: return detour0;
    case 1: return detour1;
    case 2: return detour2;
    case 3: return detour3;
    default: return nullptr;
    }
}

} // namespace

namespace gate {

bool arm()
{
    g_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_event) { LOGF("gate: CreateEvent failed (err=%lu)", GetLastError()); return false; }

    if (!mh::load()) return false;                 // no MinHook -> silent mod, game fine
    if (mh::initialize() != MH_OK) { LOGF("gate: MinHook initialize failed"); return false; }

    HMODULE game = GetModuleHandleW(L"GameAssembly.dll");
    if (!game) { LOGF("gate: GameAssembly.dll vanished"); return false; }

    int armed = 0;
    for (int i = 0; i < kWireCount; ++i) {
        // GetProcAddress reads the export table. It does NOT execute any game code, so it
        // is safe even while il2cpp_init is still running.
        g_wires[i].target = reinterpret_cast<void*>(GetProcAddress(game, g_wires[i].name));
        if (!g_wires[i].target) { LOGF("gate: %s not exported", g_wires[i].name); continue; }

        void* orig = nullptr;
        if (mh::create(g_wires[i].target, reinterpret_cast<void*>(detour_for(i)), &orig) != MH_OK) {
            LOGF("gate: could not hook %s", g_wires[i].name);
            continue;
        }
        g_wires[i].original = reinterpret_cast<fn_any>(orig);
        if (mh::enable(g_wires[i].target) != MH_OK) {
            LOGF("gate: could not enable %s", g_wires[i].name);
            continue;
        }
        ++armed;
    }

    LOGF("gate: %d/%d trip-wire(s) armed — waiting for the game to call its own runtime",
         armed, kWireCount);
    return armed > 0;
}

bool wait(int timeout_ms)
{
    if (!g_event) return false;
    if (InterlockedCompareExchange(&g_fired, 1, 1)) return true;   // already fired
    return WaitForSingleObject(g_event, timeout_ms) == WAIT_OBJECT_0;
}

const char* fired_by()
{
    return (g_which >= 0 && g_which < kWireCount) ? g_wires[g_which].name : "(none)";
}

// The wires ARE taken out once they have fired (2026-09-25). They sit on the runtime's
// hottest entry points — il2cpp_string_new and il2cpp_object_new run on every managed
// allocation the engine makes — and "one predictable branch" times millions of calls on the
// game's main thread is exactly the kind of interception this mod must not keep. Disabling
// is queued here and applied by hooks::install's single MH_ApplyQueued, inside the same
// thread freeze that enables the readers' hooks; MinHook's freeze fixes up any thread that
// is mid-way through the patched bytes, and the hooks are DISABLED, never removed, so the
// trampoline a thread may still be running through stays mapped.
void disarm()
{
    int queued = 0;
    for (int i = 0; i < kWireCount; ++i) {
        if (!g_wires[i].original) continue;    // never armed
        if (mh::queue_disable(g_wires[i].target) == MH_OK) ++queued;
    }
    LOGF("gate: %d trip-wire(s) queued for removal", queued);
}

} // namespace gate
