#pragma once
#include <vector>
#include "../refs.h"

// FF1 Utils/GameObjectCache.cs: a cache of Unity components by type, so hotkeys avoid
// FindObjectOfType. Entries are re-validated on every read (a destroyed object is dropped).
// Native lifecycle producers replace entries. Valid field entries survive additive UI
// preload; destroyed objects are retired on read, and title return clears them. Main owner.
//
//     void* pc = field::cache::get(kFieldPlayerController);
//     if (!pc) pc = field::cache::refresh(kFieldPlayerController);
namespace field::cache {

/// GameObjectCache.Get<T>(): the cached instance, or null when not cached or destroyed.
void* get(ref::Class& type);
/// Validate the native-lifecycle entry; never perform a scene search.
void* refresh(ref::Class& type);
/// Validated lifecycle entry, or null until native initialization provides it.
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
