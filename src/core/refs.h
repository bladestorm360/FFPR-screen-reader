#pragma once
#include <cstdint>
#include <string>
#include "il2cpp.h"
#include "game.h"
#include "timing.h"

// Declarative game-code names.
//
// Every class, field and method the mod uses is declared once, at file scope, as a ref:
//
//     static ref::Class  kMsgMgr{"Last.Message", "MessageWindowManager"};
//     static ref::Field  fMsgList{kMsgMgr, "messageList"};
//     static ref::Method mSetContent{kMsgMgr, "SetContent", 0};
//
// They resolve BY NAME on first use and cache the result: no RVAs, no offsets, nothing to
// re-dump when a game patches, and the per-game offset tables of the C# mods disappear
// because each game answers for its own layout.
//
// Each ref carries a game mask. A ref outside the running game resolves to "absent" without
// a complaint; a ref inside it that fails is logged once. All refs register themselves in one
// list, so the startup self-check (ref::self_check) resolves every name for the running game
// and logs every miss in one place - one launch per game to the title screen shows every
// missing name. tools/name_check.py reads the same declarations and checks them against each
// game's dump.cs offline, so keep each declaration on one line with literal strings.
//
// Construction only links the ref into a list (no allocation, no runtime call), so declaring
// refs at file scope is safe under the loader lock.
namespace ref {

struct Class {
    const char* ns;
    const char* name;          // "Outer.Inner" for a nested type
    uint8_t games;
    const char* assembly;      // null = search every assembly
    Class(const char* ns_, const char* name_, uint8_t games_ = game::kAll, const char* assembly_ = nullptr);
    /// The class, or null when absent in this game. Resolved once.
    il2cpp::Class* get();
    explicit operator bool() { return get() != nullptr; }
    bool applies() const { return game::in(games); }
    /// True when `obj`'s class is this class or derives from it.
    bool is(void* obj);

    il2cpp::Class* k_ = nullptr;
    bool tried_ = false;
    Class* next_ = nullptr;
};

struct Field {
    Class& cls;
    const char* name;
    const char* alt;           // optional second name (a game renamed the field)
    Field(Class& c, const char* name_, const char* alt_ = nullptr);
    /// Byte offset, or -1 when absent.
    int32_t get();
    explicit operator bool() { return get() >= 0; }

    int32_t off_ = -1;
    bool tried_ = false;
    Field* next_ = nullptr;
};

struct Method {
    Class& cls;
    const char* name;
    int argc;
    Method(Class& c, const char* name_, int argc_);
    il2cpp::Method* get();
    /// Native code address (what a hook detours / what call<> calls). Null when absent.
    void* ptr();
    explicit operator bool() { return get() != nullptr; }

    il2cpp::Method* m_ = nullptr;
    bool tried_ = false;
    Method* next_ = nullptr;
};

/// Resolve every registered ref that applies to the running game and log each miss.
/// Returns the number of misses. Boot thread, after the runtime is attached.
int self_check();

// --- reads ----------------------------------------------------------------------------

/// Field value at a ref'd offset. T{} on a null object, a missing field or a bad pointer.
template <typename T>
inline T rd(void* obj, Field& f) { return il2cpp::read_field<T>(obj, f.get()); }

/// Write a field (guarded). False when the object or field is missing.
template <typename T>
inline bool wr(void* obj, Field& f, T value)
{
    int32_t off = f.get();
    if (!obj || off < 0) return false;
    __try { *reinterpret_cast<T*>(static_cast<uint8_t*>(obj) + off) = value; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

/// A managed string as UTF-16 (empty on null/garbage). Copied out: never keeps the pointer.
std::wstring str(void* il2cpp_string);
/// A string field of an object.
std::wstring rd_str(void* obj, Field& f);
/// A new managed string (GC-owned; pass it straight into a call, never keep it).
void* new_str(const std::wstring& s);

// --- calls ----------------------------------------------------------------------------
//
// call<R>(method, self, args...) calls the compiled method directly with the IL2CPP calling
// convention (this, args..., MethodInfo*). Value types (float, Vector3, enums) pass and
// return natively. A managed exception or a fault becomes R{} - it never unwinds through
// the game (the DLL is built with /EHa, so catch (...) also takes access violations).
// Static methods pass nullptr as self.
//
// Use invoke (il2cpp::invoke_*) for generic static methods such as
// SingletonMonoBehaviour<T>.get_Instance: the runtime then runs the class constructor and
// supplies the generic context. ref::instance() does that for the common case.
void call_failed_note(const char* name);

template <typename R, typename... A>
inline R call(Method& m, void* self, A... args)
{
    void* fp = m.ptr();
    if (!fp) return R{};
    il2cpp::Method* mi = m.get();
    timing::Scope measured(m.cls.name, timing::Kind::Native, m.name, reinterpret_cast<uintptr_t>(fp));
    try {
        using F = R (*)(void*, A..., const void*);
        return reinterpret_cast<F>(fp)(self, args..., mi);
    } catch (...) {
        call_failed_note(m.name);
        return R{};
    }
}

/// Static method (no `this`).
template <typename R, typename... A>
inline R call_static(Method& m, A... args)
{
    void* fp = m.ptr();
    if (!fp) return R{};
    il2cpp::Method* mi = m.get();
    timing::Scope measured(m.cls.name, timing::Kind::Native, m.name, reinterpret_cast<uintptr_t>(fp));
    try {
        using S = R (*)(A..., const void*);
        return reinterpret_cast<S>(fp)(args..., mi);
    } catch (...) {
        call_failed_note(m.name);
        return R{};
    }
}

template <typename... A>
inline void call_void(Method& m, void* self, A... args)
{
    void* fp = m.ptr();
    if (!fp) return;
    il2cpp::Method* mi = m.get();
    timing::Scope measured(m.cls.name, timing::Kind::Native, m.name, reinterpret_cast<uintptr_t>(fp));
    try {
        using F = void (*)(void*, A..., const void*);
        reinterpret_cast<F>(fp)(self, args..., mi);
    } catch (...) {
        call_failed_note(m.name);
    }
}

/// Call a method that returns a managed string and copy it out.
template <typename... A>
inline std::wstring call_str(Method& m, void* self, A... args)
{
    return str(call<void*>(m, self, args...));
}

/// `Class.Instance` for the game's SingletonMonoBehaviour<T> / Singleton<T> classes, through
/// il2cpp_runtime_invoke (generic static getter). Null when absent or not yet created.
void* instance(Class& c);

} // namespace ref
