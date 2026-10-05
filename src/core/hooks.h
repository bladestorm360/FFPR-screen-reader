#pragma once
#include "refs.h"
#include "log.h"

// The hook registry.
//
// One detour per native address, and every subscriber still runs. Harmony allowed several
// patches on one method, and folded method bodies are shared between methods (see
// OPEN_ISSUES.md, "Cross-cutting lessons"), but MinHook can detour an address only once. So a
// second request for an address that is already detoured is CHAINED: the new detour is put on
// the previous detour function, and each detour calls its own original. Where a body is
// folded, every subscriber class-checks `self` with ref::Class::is and ignores other classes.
//
// Per-game switches: a game module's configure() calls hooks::disable("Class.Method") for a
// core hook that is folded or inlined in that game; attach() then skips it and logs that.
//
// Every detour is QUEUED and enabled in one thread freeze by hooks::apply() (the boot thread
// calls it once after the core and the game module have attached everything).
//
// Writing a detour:
//
//     static ref::Class  kMwm{"Last.Message", "MessageWindowManager"};
//     static ref::Method mSetContent{kMwm, "SetContent", 0};
//     using SetContent_t = void (*)(void* self, const void* mi);
//     static SetContent_t o_SetContent;
//     static void d_SetContent(void* self, const void* mi)
//     {
//         o_SetContent(self, mi);                       // the game first (a "postfix")
//         hooks::guarded("MessageWindow.SetContent", [&] { ... read and speak ... });
//     }
//     ...
//     hooks::attach(mSetContent, &d_SetContent, &o_SetContent);
//
// The trailing `const void* mi` is IL2CPP's MethodInfo*: every compiled method takes it last.
// Static methods have no `self`. Value types pass natively (bool, int32_t, float, enums as
// int32_t; a struct such as Vector3 by value as in C).
namespace hooks {

bool attach_raw(ref::Method& m, void* detour, void** original);

/// Detour a ref'd method. False (logged) when absent in this game, disabled for this game,
/// or the address already has a detour.
template <typename D, typename O>
bool attach(ref::Method& m, D detour, O* original)
{
    return attach_raw(m, reinterpret_cast<void*>(detour), reinterpret_cast<void**>(original));
}

/// Detour by address (a compiler-generated nested type's MoveNext, resolved by the caller).
bool attach_addr(void* target, const char* label, void* detour, void** original);

/// Switch off a hook for the running game, by "Class.Method" (the class's short name).
void disable(const char* class_dot_method);
bool is_disabled(const char* cls, const char* method);

/// Enable every queued detour in one freeze. Boot thread only.
bool apply();

int installed();
int failed();

/// Run `fn`, absorbing any C++ exception, managed exception or access violation (/EHa).
/// Every detour body runs inside one: a reader bug must never take the game down.
void note_fault(const char* where);

template <typename F>
inline void guarded(const char* where, F&& fn)
{
    try { fn(); } catch (...) { note_fault(where); }
}

/// Guarded with a result; `fallback` on a fault.
template <typename R, typename F>
inline R guarded_r(const char* where, R fallback, F&& fn)
{
    try { return fn(); } catch (...) { note_fault(where); return fallback; }
}

} // namespace hooks
