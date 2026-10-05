// Helpers for the shop area: the C# mod's MasterManager.GetData<T>, MessageManager.GetMessage
// and TextUtils.StripIconMarkup calls (used by Patches/ShopPatches.cs and
// Patches/ShopMagicPatches.cs). Kept in the shop area because the core has no master-data
// helper yet.
#include "shop_internal.h"
#include "../il2cpp.h"
#include "../text.h"
#include "../unity.h"

namespace ffpr::shop {
namespace {

// MasterManager (all five games): masterList is Dictionary<DataType, object> whose values are
// the Dictionary<int, T> built by MasterParse<T>; masterTypeList is Dictionary<Type, DataType>.
// GetData<T> is a shared generic body that cannot be called without an inflated MethodInfo, so
// the same lookup is done by reading the two dictionaries.
ref::Class kMasterManager{"Last.Data.Master", "MasterManager"};
ref::Field fMasterList{kMasterManager, "masterList"};
ref::Field fMasterTypeList{kMasterManager, "masterTypeList"};

ref::Class kMessageManager{"Last.Management", "MessageManager"};
ref::Method mGetMessage{kMessageManager, "GetMessage", 2};

struct TypeMatch {
    void* type_obj;
    int32_t data_type;
    bool found;
};

void match_type(uint64_t key, uint64_t value, void* state)
{
    auto* s = static_cast<TypeMatch*>(state);
    if (s->found || reinterpret_cast<void*>(key) != s->type_obj) return;
    s->data_type = static_cast<int32_t>(static_cast<uint32_t>(value));
    s->found = true;
}

struct IntKeyMatch {
    int32_t key;
    void* value;
    bool found;
};

void match_int_key(uint64_t key, uint64_t value, void* state)
{
    auto* s = static_cast<IntKeyMatch*>(state);
    if (s->found || static_cast<int32_t>(static_cast<uint32_t>(key)) != s->key) return;
    s->value = reinterpret_cast<void*>(value);
    s->found = true;
}

struct FirstValue {
    void* value;
};

void first_value(uint64_t, uint64_t value, void* state)
{
    auto* s = static_cast<FirstValue*>(state);
    if (!s->value) s->value = reinterpret_cast<void*>(value);
}

struct ClassProbe {
    il2cpp::Class* k;
    int32_t data_type;
    bool found;
};

void probe_class(uint64_t key, uint64_t value, void* state)
{
    auto* s = static_cast<ClassProbe*>(state);
    if (s->found || !value) return;
    FirstValue fv{nullptr};
    unity::dict_for_each(reinterpret_cast<void*>(value), &first_value, &fv, 4, 8);
    if (fv.value && il2cpp::object_class(fv.value) == s->k) {
        s->data_type = static_cast<int32_t>(static_cast<uint32_t>(key));
        s->found = true;
    }
}

// DataType per master class, found once per process (the enum is fixed per game).
struct CachedType {
    il2cpp::Class* k;
    int32_t data_type;
};
CachedType g_types[8];
int g_type_count = 0;

bool data_type_of(void* mm, il2cpp::Class* k, int32_t& out)
{
    for (int i = 0; i < g_type_count; ++i)
        if (g_types[i].k == k) { out = g_types[i].data_type; return true; }

    bool found = false;
    // masterTypeList: the key is the class's System.Type object (typeof(T)).
    if (void* type_obj = il2cpp::class_type_object(k)) {
        TypeMatch tm{type_obj, 0, false};
        unity::dict_for_each(ref::rd<void*>(mm, fMasterTypeList), &match_type, &tm, 8, 4);
        if (tm.found) { out = tm.data_type; found = true; }
    }
    // Fallback: the masterList entry whose records are of class k.
    if (!found) {
        ClassProbe cp{k, 0, false};
        unity::dict_for_each(ref::rd<void*>(mm, fMasterList), &probe_class, &cp, 4, 8);
        if (cp.found) { out = cp.data_type; found = true; }
    }
    if (found && g_type_count < static_cast<int>(sizeof(g_types) / sizeof(g_types[0])))
        g_types[g_type_count++] = CachedType{k, out};
    return found;
}

} // namespace

void* master_get_data(ref::Class& type, int id)
{
    il2cpp::Class* k = type.get();
    if (!k) return nullptr;
    void* mm = ref::instance(kMasterManager);
    if (!mm) return nullptr;
    int32_t data_type = 0;
    if (!data_type_of(mm, k, data_type)) return nullptr;

    IntKeyMatch list{data_type, nullptr, false};
    unity::dict_for_each(ref::rd<void*>(mm, fMasterList), &match_int_key, &list, 4, 8);
    if (!list.found || !list.value) return nullptr;

    IntKeyMatch rec{id, nullptr, false};
    unity::dict_for_each(list.value, &match_int_key, &rec, 4, 8);
    return rec.found ? rec.value : nullptr;
}

std::wstring get_message(const std::wstring& key)
{
    if (key.empty()) return {};
    void* mm = ref::instance(kMessageManager);
    if (!mm) return {};
    void* s = ref::new_str(key);
    if (!s) return {};
    return ref::call_str(mGetMessage, mm, s, false);
}

std::wstring strip_icon_markup(std::wstring_view s)
{
    if (s.empty()) return {};
    return text::trim(text::strip_markup(s));
}

bool live(void* obj, ref::Class& k)
{
    return obj && k.is(obj) && unity::alive(obj);
}

} // namespace ffpr::shop
