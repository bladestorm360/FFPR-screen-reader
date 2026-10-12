// Ports FF1 Utils/GameObjectCache.cs.
//
// Keyed by the resolved IL2CPP class, so two areas that declare their own ref::Class for one
// type share its entry, as C#'s typeof(T) did. The C# lock is not needed: every caller runs
// on the game's main thread.
#include <algorithm>
#include <map>
#include "object_cache.h"
#include "../unity.h"

namespace field::cache {
namespace {

std::map<void*, void*> g_single;
std::map<void*, std::vector<void*>> g_multi;

/// IsValid: not destroyed (Unity's == null) and, for a component, its gameObject exists.
bool valid(void* obj)
{
    if (!unity::alive(obj)) return false;
    return unity::game_object(obj) != nullptr;
}

void* key_of(ref::Class& type) { return type.get(); }

} // namespace

void* get(ref::Class& type)
{
    void* key = key_of(type);
    if (!key) return nullptr;
    auto it = g_single.find(key);
    if (it == g_single.end()) return nullptr;
    if (type.is(it->second) && valid(it->second)) return it->second;
    g_single.erase(it);
    return nullptr;
}

void* refresh(ref::Class& type)
{
    // Lifecycle observers supply current instances. A missing instance is not
    // permission to search every engine object from a game callback.
    return get(type);
}

void* get_or_refresh(ref::Class& type)
{
    void* obj = get(type);
    return obj ? obj : refresh(type);
}

void register_object(ref::Class& type, void* obj)
{
    void* key = key_of(type);
    if (!key || !obj) return;
    g_single[key] = obj;
}

bool has(ref::Class& type)
{
    void* key = key_of(type);
    if (!key) return false;
    auto it = g_single.find(key);
    return it != g_single.end() && type.is(it->second) && valid(it->second);
}

void clear(ref::Class& type)
{
    if (void* key = key_of(type)) g_single.erase(key);
}

std::vector<void*> get_all(ref::Class& type)
{
    void* key = key_of(type);
    if (!key) return {};
    auto it = g_multi.find(key);
    if (it == g_multi.end()) return {};
    std::vector<void*> valid_objects;
    for (void* o : it->second)
        if (type.is(o) && valid(o)) valid_objects.push_back(o);
    it->second = valid_objects;
    return valid_objects;
}

void register_multiple(ref::Class& type, void* obj)
{
    void* key = key_of(type);
    if (!key || !obj) return;
    auto& list = g_multi[key];
    if (std::find(list.begin(), list.end(), obj) == list.end()) list.push_back(obj);
}

void unregister_multiple(ref::Class& type, void* obj)
{
    void* key = key_of(type);
    if (!key || !obj) return;
    auto it = g_multi.find(key);
    if (it == g_multi.end()) return;
    it->second.erase(std::remove(it->second.begin(), it->second.end(), obj), it->second.end());
}

void clear_multiple(ref::Class& type)
{
    if (void* key = key_of(type)) g_multi.erase(key);
}

void clear_all()
{
    g_single.clear();
    g_multi.clear();
}

} // namespace field::cache
