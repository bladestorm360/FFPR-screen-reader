#include <windows.h>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "hooks.h"
#include "minhook_dyn.h"

namespace hooks {
namespace {

struct Owner {
    std::string label;
    std::vector<void*> detours;   // every detour on this address, oldest first
};
std::unordered_map<void*, Owner>* g_owners = nullptr;    // created on first attach (boot thread)
std::unordered_set<std::string>* g_disabled = nullptr;
int g_installed = 0;
int g_failed = 0;
volatile LONG g_faults = 0;

std::unordered_map<void*, Owner>& owners()
{
    if (!g_owners) g_owners = new std::unordered_map<void*, Owner>();
    return *g_owners;
}

/// What a game function starts with when another mod (MelonLoader, BepInEx) hooked it before
/// us, or nullptr. An IL2CPP method can also start with a plain tail-call jump, so this only
/// names what it sees; the hook stacks on top either way (MinHook relocates the jump).
const char* foreign_jump(const void* target)
{
    const auto* b = static_cast<const unsigned char*>(target);
    __try {
        if (b[0] == 0xE9) return "JMP rel32 (funchook, MonoMod, or a tail call)";
        if (b[0] == 0xFF && b[1] == 0x25) return "JMP [RIP] (Dobby, MonoMod)";
        if (b[0] == 0x68 && b[5] == 0xC3) return "PUSH/RET";
        if (b[0] == 0x48 && b[1] == 0xB8 && b[10] == 0xFF && b[11] == 0xE0) return "MOV RAX/JMP RAX";
        if (b[0] == 0x49 && b[1] == 0xBB && b[10] == 0x41 && b[11] == 0xFF && b[12] == 0xE3)
            return "MOV R11/JMP R11";
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return nullptr;
}

bool detour_one(void* target, const std::string& label, void* detour, void** original)
{
    if (mh::create(target, detour, original) != MH_OK) {
        LOGF("hook: create failed: %s @ %p", label.c_str(), target);
        ++g_failed;
        return false;
    }
    if (mh::queue_enable(target) != MH_OK) {
        LOGF("hook: queue failed: %s", label.c_str());
        ++g_failed;
        return false;
    }
    return true;
}

bool create(void* target, const std::string& label, void* detour, void** original)
{
    if (!mh::available()) { LOGF("hook: MinHook unavailable: %s", label.c_str()); ++g_failed; return false; }
    auto& map = owners();
    auto it = map.find(target);
    if (it != map.end()) {
        // A second subscriber on the same address (two readers on one method, or a folded
        // body shared by two methods). MinHook can detour an address once, so the new detour
        // goes on the previous TOP detour instead: game -> newest -> ... -> oldest -> game.
        // Every detour calls its own original, so every subscriber runs; each class-checks
        // `self` where the body is folded.
        void* top = it->second.detours.back();
        if (!detour_one(top, label, detour, original)) return false;
        it->second.detours.push_back(detour);
        LOGF("hook: %-48s -> %p (chained after %s)", label.c_str(), target, it->second.label.c_str());
        it->second.label += " + " + label;
        ++g_installed;
        return true;
    }
    if (const char* other = foreign_jump(target))
        LOGF("hook: %s already starts with a %s: hooked by another mod first? Stacking on top.",
             label.c_str(), other);
    if (!detour_one(target, label, detour, original)) return false;
    map.emplace(target, Owner{label, {detour}});
    LOGF("hook: %-48s -> %p", label.c_str(), target);
    ++g_installed;
    return true;
}

} // namespace

bool attach_raw(ref::Method& m, void* detour, void** original)
{
    std::string label = std::string(m.cls.name) + "." + m.name + "(" + std::to_string(m.argc) + ")";
    if (!m.cls.applies()) return false;   // not this game's class: silent by design
    if (is_disabled(m.cls.name, m.name)) {
        LOGF("hook: disabled for %s: %s", game::label(), label.c_str());
        return false;
    }
    void* target = m.ptr();
    if (!target) { LOGF("hook: unresolved: %s", label.c_str()); ++g_failed; return false; }
    return create(target, label, detour, original);
}

bool attach_addr(void* target, const char* label, void* detour, void** original)
{
    if (!target) { LOGF("hook: unresolved: %s", label ? label : "?"); ++g_failed; return false; }
    // A "Class.Method" label (anything after '(' ignored) honours hooks::disable too, so a game
    // module can switch off a core hook that was resolved by address.
    if (label) {
        std::string l(label);
        size_t paren = l.find('(');
        if (paren != std::string::npos) l.resize(paren);
        size_t dot = l.find('.');
        if (dot != std::string::npos && is_disabled(l.substr(0, dot).c_str(), l.substr(dot + 1).c_str())) {
            LOGF("hook: disabled for %s: %s", game::label(), label);
            return false;
        }
    }
    return create(target, label ? label : "?", detour, original);
}

void disable(const char* class_dot_method)
{
    if (!g_disabled) g_disabled = new std::unordered_set<std::string>();
    g_disabled->insert(class_dot_method);
}

bool is_disabled(const char* cls, const char* method)
{
    if (!g_disabled || g_disabled->empty()) return false;
    std::string key = std::string(cls) + "." + method;
    return g_disabled->count(key) != 0;
}

bool apply()
{
    if (!mh::available()) return false;
    MH_STATUS st = mh::apply_queued();
    LOGF("hooks: applied (%d installed, %d failed) status=%d", g_installed, g_failed, (int)st);
    // Coexistence: a hook whose trampoline resumes inside the first MH_COEXIST_RESUME bytes is
    // the one kind that can still break if another mod hooks the same game function after us.
    unsigned long_resume = 0, short_resume = 0, patched_over = 0;
    mh::stats(long_resume, short_resume, patched_over);
    LOGF("hooks: coexistence: %u resume past %d bytes or never, %u resume sooner, %u kept "
         "because another mod hooked over them", long_resume, MH_COEXIST_RESUME, short_resume,
         patched_over);
    if (short_resume && g_owners) {
        for (const auto& kv : *g_owners) {
            int off = mh::resume_offset(kv.first);
            if (off > 0 && off < MH_COEXIST_RESUME)
                LOGF("hooks: resumes at byte %d: %s", off, kv.second.label.c_str());
        }
    }
    return st == MH_OK;
}

int installed() { return g_installed; }
int failed() { return g_failed; }

void note_fault(const char* where)
{
    LONG n = InterlockedIncrement(&g_faults);
    if (n <= 100) LOGF("fault absorbed in %s (%ld so far)", where ? where : "?", n);
}

} // namespace hooks
