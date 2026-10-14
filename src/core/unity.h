#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "refs.h"

// Native access to the Unity objects the readers need. These are the C# mods' everyday calls
// (GetComponent<T>, GetComponentsInChildren<Text>, FindObjectOfType<T>, transform.position,
// Text.text) resolved by name and called directly. Main thread only, like the C# originals.
//
// "obj" parameters accept a GameObject or any Component unless a function says otherwise.
namespace unity {

struct Vec3 { float x, y, z; };
struct Vec2 { float x, y; };

bool init();

// --- objects -------------------------------------------------------------------------
/// False for null, and for a destroyed Unity object (its native pointer m_CachedPtr is 0).
bool alive(void* unity_object);
std::wstring name(void* unity_object);
const char* class_name(void* obj);

/// Object.FindObjectOfType(Type) / FindObjectsOfType(Type): active objects only, as in Unity.
void* find_object(ref::Class& type);
std::vector<void*> find_objects(ref::Class& type);
/// GameObject.Find(name) (active objects, path allowed).
void* find_game_object(const wchar_t* name);

// --- components ----------------------------------------------------------------------
void* game_object(void* obj);
void* transform(void* obj);
void* get_component(void* obj, ref::Class& type);
void* get_component_in_children(void* obj, ref::Class& type, bool include_inactive = false);
void* get_component_in_parent(void* obj, ref::Class& type);
std::vector<void*> components_in_children(void* obj, ref::Class& type, bool include_inactive = false);

bool active_in_hierarchy(void* obj);
bool active_self(void* obj);

// --- transforms ----------------------------------------------------------------------
void* parent(void* transform);
int child_count(void* transform);
void* child(void* transform, int index);
/// Transform.Find(path) - a direct child or "a/b" path, or null.
void* find_child(void* transform, const wchar_t* path);
/// Depth-first search of the whole subtree by exact name (FF1 TextUtils.FindTransformInChildren).
void* find_descendant(void* transform, const wchar_t* name, int max_depth = 16);
Vec3 position(void* transform);
Vec3 local_position(void* transform);

// --- text --------------------------------------------------------------------------
/// UnityEngine.UI.Text's string (m_Text). Empty when not a Text or null.
std::wstring ui_text(void* text_component);
/// First UnityEngine.UI.Text on obj or below (optionally inactive ones too).
std::wstring first_text(void* obj, bool include_inactive = false);
/// Every UI.Text below obj, in hierarchy order.
std::vector<void*> texts(void* obj, bool include_inactive = false);
ref::Class& text_class();
ref::Class& image_class();

// --- managed collections ---------------------------------------------------------------
int array_length(void* arr);
void* array_at(void* arr, int index);            // reference-type element
template <typename T> T array_value(void* arr, int index);   // value-type element
int list_count(void* list);
void* list_at(void* list, int index);
/// Bounded reference-type IEnumerable traversal, including lists, arrays and LINQ iterators.
bool enumerate(void* enumerable, std::vector<void*>& out);
/// Reject oversized/unfinished native collections; used by bounded targeting snapshots.
bool enumerate_bounded(void* enumerable, std::vector<void*>& out, int maximum);
template <typename T> T list_value(void* list, int index);
/// Dictionary<TKey,TValue> walk for reference keys/values or 4-byte ints (layout: hash,next,key,value).
/// Calls fn(key, value) for each live entry; keys/values are raw 8-byte slots.
void dict_for_each(void* dict, void (*fn)(uint64_t key, uint64_t value, void* state), void* state,
                   int key_size = 8, int value_size = 8);
int dict_count(void* dict);
/// Current int-key/reference-value lookup, using the native dictionary's bucket logic.
void* dict_reference(void* dict, int32_t key);
/// Read one current dictionary slot on its native owner; false for a free/missing slot.
bool dict_entry(void* dict, int index, uint64_t& key, uint64_t& value,
                int key_size = 8, int value_size = 8);

// --- implementation details for the templates ------------------------------------------
constexpr int32_t kArrayLengthOff = 0x18;
constexpr int32_t kArrayDataOff = 0x20;
constexpr int32_t kListItemsOff = 0x10;
constexpr int32_t kListSizeOff = 0x18;

template <typename T>
T array_value(void* arr, int index)
{
    if (!arr || index < 0 || index >= array_length(arr)) return T{};
    return il2cpp::read_field<T>(arr, kArrayDataOff + index * static_cast<int32_t>(sizeof(T)));
}

template <typename T>
T list_value(void* list, int index)
{
    if (!list || index < 0 || index >= list_count(list)) return T{};
    void* items = il2cpp::read_field<void*>(list, kListItemsOff);
    return array_value<T>(items, index);
}

} // namespace unity
