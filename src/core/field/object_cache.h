#pragma once
#include <vector>
#include "../refs.h"

// FF1 Utils/GameObjectCache.cs: a cache of Unity components by type, so hotkeys avoid
// FindObjectOfType. Entries are re-validated on every read (a destroyed object is dropped).
// The cache is cleared on every scene load (the field area's scene-loaded hook). Main thread.
//
//     void* pc = field::cache::get(kFieldPlayerController);
//     if (!pc) pc = field::cache::refresh(kFieldPlayerController);
namespace field::cache {

/// GameObjectCache.Get<T>(): the cached instance, or null when not cached or destroyed.
void* get(ref::Class& type);
/// GameObjectCache.Refresh<T>(): FindObjectOfType, cached when found.
void* refresh(ref::Class& type);
/// Get, then Refresh when nothing valid is cached (the common C# pattern).
void* get_or_refresh(ref::Class& type);
/// GameObjectCache.Register<T>(obj).
void register_object(ref::Class& type, void* obj);
/// GameObjectCache.Has<T>().
bool has(ref::Class& type);
/// GameObjectCache.Clear<T>().
void clear(ref::Class& type);

/// GameObjectCache.GetAll / RegisterMultiple / UnregisterMultiple / ClearMultiple.
std::vector<void*> get_all(ref::Class& type);
void register_multiple(ref::Class& type, void* obj);
void unregister_multiple(ref::Class& type, void* obj);
void clear_multiple(ref::Class& type);

/// GameObjectCache.ClearAll() (scene transitions).
void clear_all();

} // namespace field::cache
