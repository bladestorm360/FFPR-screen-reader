// Game-data helpers of the status, config and bestiary areas: the C# mod's
// MessageManager.Instance.GetMessage(id, isReplace) and MasterManager.GetList<T>() (FF1
// StatusMenuHelpers.GetCurrentJobName, BestiaryReader.ResolveMonsterName/ResolveContentName).
// GetList<T> is a shared generic method that cannot be called by name, so it is read the way it
// works: masterList[masterTypeList[typeof(T)]] is a DataElement<T> whose DataList is the
// Dictionary<int, T>. All five games (same fields and enum layout in every dump.cs).
#include <cstring>
#include "status.h"
#include "../il2cpp.h"
#include "../refs.h"
#include "../unity.h"

namespace ffpr::status {
namespace {

ref::Class  kMessageManager{"Last.Management", "MessageManager"};
ref::Method mGetMessage{kMessageManager, "GetMessage", 2};

ref::Class  kMasterManager{"Last.Data.Master", "MasterManager"};
ref::Field  fMasterList{kMasterManager, "masterList"};
ref::Field  fMasterTypeList{kMasterManager, "masterTypeList"};

// System.Type._impl (RuntimeTypeHandle: the Il2CppType*): the type identity behind a Type
// object, compared when two Type objects for one class are not the same instance.
ref::Class  kSystemType{"System", "Type"};
ref::Field  fTypeImpl{kSystemType, "_impl"};

bool is_dictionary(void* obj)
{
    il2cpp::Class* k = obj ? il2cpp::object_class(obj) : nullptr;
    return k && std::strcmp(il2cpp::class_name(k), "Dictionary`2") == 0 &&
           std::strcmp(il2cpp::class_namespace(k), "System.Collections.Generic") == 0;
}

// DataElement<T>.DataList of one masterList value (or the value itself when it is already the
// dictionary).
void* element_data_list(void* element)
{
    if (!element) return nullptr;
    if (is_dictionary(element)) return element;
    il2cpp::Class* k = il2cpp::object_class(element);
    int32_t off = il2cpp::field_offset(k, "<DataList>k__BackingField");
    if (off < 0) off = il2cpp::field_offset(k, "DataList");
    return off >= 0 ? il2cpp::read_field<void*>(element, off) : nullptr;
}

// The class of the first value of a Dictionary<int, T>, or null when it is empty.
il2cpp::Class* first_value_class(void* dict)
{
    struct S { void* value = nullptr; } s;
    unity::dict_for_each(dict, [](uint64_t, uint64_t value, void* st) {
        auto* p = static_cast<S*>(st);
        if (!p->value && value) p->value = reinterpret_cast<void*>(value);
    }, &s, 4, 8);
    return s.value ? il2cpp::object_class(s.value) : nullptr;
}

} // namespace

std::wstring game_message(const std::wstring& mes_id, bool is_replace)
{
    if (mes_id.empty()) return {};
    void* mm = ref::instance(kMessageManager);
    if (!mm) return {};
    void* key = ref::new_str(mes_id);
    if (!key) return {};
    return ref::call_str(mGetMessage, mm, key, is_replace);
}

void* master_list(ref::Class& type)
{
    il2cpp::Class* want = type.get();
    void* mm = ref::instance(kMasterManager);
    if (!want || !mm) return nullptr;
    void* masters = ref::rd<void*>(mm, fMasterList);
    if (!masters) return nullptr;

    // masterTypeList[typeof(T)] -> DataType.
    struct TypeHit {
        uint64_t type_obj;
        void* handle;
        int32_t impl_off;
        bool found = false;
        uint32_t data_type = 0;
    };
    void* type_obj = il2cpp::class_type_object(want);
    int32_t impl_off = fTypeImpl.get();
    TypeHit th{reinterpret_cast<uint64_t>(type_obj),
               impl_off >= 0 ? il2cpp::read_field<void*>(type_obj, impl_off) : nullptr, impl_off};
    if (type_obj) {
        unity::dict_for_each(ref::rd<void*>(mm, fMasterTypeList), [](uint64_t key, uint64_t value, void* st) {
            auto* p = static_cast<TypeHit*>(st);
            if (p->found || !key) return;
            bool same = key == p->type_obj;
            if (!same && p->handle && p->impl_off >= 0)
                same = il2cpp::read_field<void*>(reinterpret_cast<void*>(key), p->impl_off) == p->handle;
            if (same) { p->found = true; p->data_type = static_cast<uint32_t>(value); }
        }, &th, 8, 4);
    }
    if (th.found) {
        // masterList[DataType] -> DataElement<T>.
        struct ElemHit { uint32_t data_type; void* element = nullptr; } eh{th.data_type};
        unity::dict_for_each(masters, [](uint64_t key, uint64_t value, void* st) {
            auto* p = static_cast<ElemHit*>(st);
            if (!p->element && static_cast<uint32_t>(key) == p->data_type) p->element = reinterpret_cast<void*>(value);
        }, &eh, 4, 8);
        if (void* list = element_data_list(eh.element)) return list;
    }

    // No Type match: the master whose entries are of class T.
    struct Scan { il2cpp::Class* want; void* list = nullptr; } sc{want};
    unity::dict_for_each(masters, [](uint64_t, uint64_t value, void* st) {
        auto* p = static_cast<Scan*>(st);
        if (p->list || !value) return;
        void* list = element_data_list(reinterpret_cast<void*>(value));
        if (list && first_value_class(list) == p->want) p->list = list;
    }, &sc, 4, 8);
    return sc.list;
}

void* master_entry(ref::Class& type, int id)
{
    void* list = master_list(type);
    if (!list) return nullptr;
    struct Hit { uint32_t id; void* value = nullptr; } h{static_cast<uint32_t>(id)};
    unity::dict_for_each(list, [](uint64_t key, uint64_t value, void* st) {
        auto* p = static_cast<Hit*>(st);
        if (!p->value && static_cast<uint32_t>(key) == p->id) p->value = reinterpret_cast<void*>(value);
    }, &h, 4, 8);
    return h.value;
}

bool int_dict_get(void* dict, int key, int& value)
{
    if (!dict) return false;
    struct Hit { uint32_t key; bool found = false; uint32_t value = 0; } h{static_cast<uint32_t>(key)};
    unity::dict_for_each(dict, [](uint64_t k, uint64_t v, void* st) {
        auto* p = static_cast<Hit*>(st);
        if (!p->found && static_cast<uint32_t>(k) == p->key) { p->found = true; p->value = static_cast<uint32_t>(v); }
    }, &h, 4, 4);
    if (h.found) value = static_cast<int>(h.value);
    return h.found;
}

} // namespace ffpr::status
