#include <windows.h>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "hooks.h"
#include "minhook_dyn.h"

namespace hooks {
namespace {

struct Owner { std::string label; };
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

bool create(void* target, const std::string& label, void* detour, void** original)
{
    auto& map = owners();
    auto it = map.find(target);
    if (it != map.end()) {
        LOGF("hook: REFUSED %s: address %p already detoured by %s (folded body: serve both from "
             "the first detour and class-check self)", label.c_str(), target, it->second.label.c_str());
        ++g_failed;
        return false;
    }
    if (!mh::available()) { LOGF("hook: MinHook unavailable: %s", label.c_str()); ++g_failed; return false; }
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
    map.emplace(target, Owner{label});
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
