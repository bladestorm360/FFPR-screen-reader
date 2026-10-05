#include <windows.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include "core.h"
#include "dedup.h"
#include "hooks.h"
#include "log.h"

namespace core {
namespace {
Install* g_installs = nullptr;
}

Install::Install(const char* name_, int (*fn_)(), int order_, uint8_t games_)
    : name(name_), fn(fn_), order(order_), games(games_)
{
    next = g_installs;
    g_installs = this;
}

int install_all()
{
    std::vector<Install*> list;
    for (Install* i = g_installs; i; i = i->next)
        if (game::in(i->games)) list.push_back(i);
    std::stable_sort(list.begin(), list.end(), [](Install* a, Install* b) { return a->order < b->order; });
    int total = 0;
    for (Install* i : list) {
        int n = hooks::guarded_r(i->name, 0, [&] { return i->fn(); });
        LOGF("install: %-32s %d", i->name, n);
        total += n;
    }
    return total;
}

} // namespace core

namespace actions {
namespace {
std::map<std::string, Fn>* g_actions = nullptr;
std::map<std::string, Fn>& table()
{
    if (!g_actions) g_actions = new std::map<std::string, Fn>();
    return *g_actions;
}
}

void define(const char* name, Fn fn) { table()[name] = std::move(fn); }

bool run(const char* name)
{
    auto it = table().find(name);
    if (it == table().end() || !it->second) {
        LOGF("action: %s is not available in %s", name, game::label());
        return false;
    }
    hooks::guarded(name, [&] { it->second(); });
    return true;
}

bool defined(const char* name) { return table().count(name) != 0; }

} // namespace actions

namespace menu_state {
namespace {
std::map<std::string, bool>* g_states = nullptr;
std::map<std::string, std::function<void()>>* g_resets = nullptr;
std::map<std::string, bool>& states() { if (!g_states) g_states = new std::map<std::string, bool>(); return *g_states; }
std::map<std::string, std::function<void()>>& resets() { if (!g_resets) g_resets = new std::map<std::string, std::function<void()>>(); return *g_resets; }
}

void set_active(const char* key, bool active) { states()[key] = active; }

void set_active_exclusive(const char* key)
{
    reset_all_except(key);
    states()[key] = true;
}

bool is_active(const char* key)
{
    auto it = states().find(key);
    return it != states().end() && it->second;
}

bool any_active()
{
    for (auto& kv : states()) if (kv.second) return true;
    return false;
}

std::vector<std::string> active_states()
{
    std::vector<std::string> out;
    for (auto& kv : states()) if (kv.second) out.push_back(kv.first);
    return out;
}

void register_reset(const char* key, std::function<void()> handler) { resets()[key] = std::move(handler); }

void reset(const char* key)
{
    states()[key] = false;
    auto it = resets().find(key);
    if (it != resets().end() && it->second) hooks::guarded(key, [&] { it->second(); });
}

void reset_all()
{
    for (auto& kv : states()) kv.second = false;
    for (auto& kv : resets()) if (kv.second) hooks::guarded(kv.first.c_str(), [&] { kv.second(); });
}

void reset_all_except(const char* key)
{
    std::string keep = key ? key : "";
    for (auto& kv : states()) if (kv.first != keep) kv.second = false;
    for (auto& kv : resets())
        if (kv.first != keep && kv.second) hooks::guarded(kv.first.c_str(), [&] { kv.second(); });
}

} // namespace menu_state

namespace events {
namespace {
std::vector<Handler>* g_handlers = nullptr;
std::vector<Handler>& handlers(Kind k)
{
    if (!g_handlers) g_handlers = new std::vector<Handler>[static_cast<int>(Kind::Count)];
    return g_handlers[static_cast<int>(k)];
}
}

void on(Kind k, Handler h) { handlers(k).push_back(std::move(h)); }

void fire(Kind k)
{
    switch (k) {
    case Kind::MenuOpened:
    case Kind::MenuClosed: dedup::reset_scope(dedup::Scope::Menu); break;
    case Kind::BattleStart:
    case Kind::BattleEnd: dedup::reset_scope(dedup::Scope::Battle); break;
    case Kind::MapChanged:
    case Kind::SceneLoaded: dedup::reset_scope(dedup::Scope::Map); break;
    case Kind::DialogueClosed: dedup::reset_scope(dedup::Scope::Dialogue); break;
    default: break;
    }
    for (Handler& h : handlers(k)) hooks::guarded("event handler", [&] { h(); });
}

} // namespace events
