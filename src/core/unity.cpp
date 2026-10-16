#include <windows.h>
#include "unity.h"
#include "log.h"
#include <cstring>
#include <map>

namespace unity {
namespace {
constexpr int kMaxEnumerated = 4096;
bool class_is(il2cpp::Class* k, const char* ns, const char* name) {
    return k && std::strcmp(il2cpp::class_name(k), name) == 0 && std::strcmp(il2cpp::class_namespace(k), ns) == 0;
}
bool ends_with(const char* s, const char* suffix)
{
    size_t a = std::strlen(s), b = std::strlen(suffix);
    return a >= b && std::strcmp(s + a - b, suffix) == 0;
}

ref::Class  kIEnumerable{"System.Collections", "IEnumerable"};
ref::Method mGetEnumerator{kIEnumerable, "GetEnumerator", 0};
ref::Class  kIEnumerator{"System.Collections", "IEnumerator"};
ref::Method mMoveNext{kIEnumerator, "MoveNext", 0};
ref::Method mGetCurrent{kIEnumerator, "get_Current", 0};


ref::Class kObject{"UnityEngine", "Object"};
ref::Class kGameObject{"UnityEngine", "GameObject"};
ref::Class kComponent{"UnityEngine", "Component"};
ref::Class kTransform{"UnityEngine", "Transform"};
ref::Class kText{"UnityEngine.UI", "Text"};
ref::Class kImage{"UnityEngine.UI", "Image"};
ref::Class kType{"System", "Type"};

ref::Field fCachedPtr{kObject, "m_CachedPtr"};
ref::Field fText{kText, "m_Text"};

ref::Method mGetName{kObject, "get_name", 0};
ref::Method mGoActiveInHierarchy{kGameObject, "get_activeInHierarchy", 0};
ref::Method mGoActiveSelf{kGameObject, "get_activeSelf", 0};
ref::Method mGoTransform{kGameObject, "get_transform", 0};
ref::Method mCompGameObject{kComponent, "get_gameObject", 0};
ref::Method mCompTransform{kComponent, "get_transform", 0};
ref::Method mParent{kTransform, "get_parent", 0};
ref::Method mChildCount{kTransform, "get_childCount", 0};
ref::Method mGetChild{kTransform, "GetChild", 1};
ref::Method mFind{kTransform, "Find", 1};
ref::Method mPosition{kTransform, "get_position", 0};
ref::Method mLocalPosition{kTransform, "get_localPosition", 0};
ref::Method mGoFind{kGameObject, "Find", 1};

// Overloaded by argument count with a non-Type first parameter (GetComponent(string),
// GetComponentsInChildren<T>(bool, List<T>)...), so these resolve on a System.Type first
// parameter instead of by name and count alone.
struct TypedMethod {
    ref::Class& cls;
    const char* name;
    int argc;
    il2cpp::Method* m = nullptr;
    bool tried = false;
    il2cpp::Method* get()
    {
        if (tried) return m;
        tried = true;
        il2cpp::Class* k = cls.get();
        il2cpp::Class* t = kType.get();
        if (k && t) m = il2cpp::find_method_overload(k, name, argc, t);
        if (!m) LOGF("unity: no %s.%s(Type, %d args)", cls.name, name, argc);
        return m;
    }
};

TypedMethod tFindObjectOfType{kObject, "FindObjectOfType", 1};
TypedMethod tFindObjectsOfType{kObject, "FindObjectsOfType", 1};
TypedMethod tGoGetComponent{kGameObject, "GetComponent", 1};
TypedMethod tGoGetComponentInChildren{kGameObject, "GetComponentInChildren", 2};
TypedMethod tGoGetComponentInParent{kGameObject, "GetComponentInParent", 1};
TypedMethod tGoGetComponentsInChildren{kGameObject, "GetComponentsInChildren", 2};

template <typename R, typename... A>
R call_m(il2cpp::Method* m, void* self, A... args)
{
    if (!m) return R{};
    void* fp = il2cpp::method_pointer(m);
    if (!fp) return R{};
    timing::Scope measured("Unity instance call", timing::Kind::Native, nullptr, reinterpret_cast<uintptr_t>(fp));
    try {
        using F = R (*)(void*, A..., const void*);
        return reinterpret_cast<F>(fp)(self, args..., m);
    } catch (...) {
        ref::call_failed_note("Unity call");
        return R{};
    }
}

template <typename R, typename... A>
R call_static_m(il2cpp::Method* m, A... args)
{
    if (!m) return R{};
    void* fp = il2cpp::method_pointer(m);
    if (!fp) return R{};
    timing::Scope measured("Unity static call", timing::Kind::Native, nullptr, reinterpret_cast<uintptr_t>(fp));
    try {
        using F = R (*)(A..., const void*);
        return reinterpret_cast<F>(fp)(args..., m);
    } catch (...) {
        ref::call_failed_note("static Unity call");
        return R{};
    }
}

void* type_object(ref::Class& c)
{
    il2cpp::Class* k = c.get();
    return k ? il2cpp::class_type_object(k) : nullptr;
}

bool is_game_object(void* obj) { return kGameObject.is(obj); }

void* walk_find(void* t, const std::wstring& target, int depth, int max_depth)
{
    if (!t || depth > max_depth) return nullptr;
    int n = child_count(t);
    for (int i = 0; i < n; ++i) {
        void* c = child(t, i);
        if (!c) continue;
        if (name(c) == target) return c;
        if (void* hit = walk_find(c, target, depth + 1, max_depth)) return hit;
    }
    return nullptr;
}

} // namespace

bool init()
{
    bool ok = kObject.get() && kGameObject.get() && kComponent.get() && kTransform.get();
    LOGF("unity: core classes %s", ok ? "resolved" : "MISSING");
    return ok;
}

bool alive(void* o)
{
    if (!o) return false;
    int32_t off = fCachedPtr.get();
    if (off < 0) return true;
    return il2cpp::read_field<void*>(o, off) != nullptr;
}

std::wstring name(void* o)
{
    if (!alive(o)) return {};
    return ref::call_str(mGetName, o);
}

const char* class_name(void* obj) { return il2cpp::object_class_name(obj); }

void* find_object(ref::Class& type)
{
    void* t = type_object(type);
    return t ? call_static_m<void*>(tFindObjectOfType.get(), t) : nullptr;
}

std::vector<void*> find_objects(ref::Class& type)
{
    std::vector<void*> out;
    void* t = type_object(type);
    if (!t) return out;
    void* arr = call_static_m<void*>(tFindObjectsOfType.get(), t);
    int n = array_length(arr);
    out.reserve(static_cast<size_t>(n > 0 ? n : 0));
    for (int i = 0; i < n; ++i)
        if (void* e = array_at(arr, i)) out.push_back(e);
    return out;
}

void* find_game_object(const wchar_t* n)
{
    void* s = il2cpp::new_string(n);
    return s ? ref::call_static<void*>(mGoFind, s) : nullptr;
}

void* game_object(void* obj)
{
    if (!alive(obj)) return nullptr;
    if (is_game_object(obj)) return obj;
    return ref::call<void*>(mCompGameObject, obj);
}

void* transform(void* obj)
{
    if (!alive(obj)) return nullptr;
    if (kTransform.is(obj)) return obj;
    if (is_game_object(obj)) return ref::call<void*>(mGoTransform, obj);
    return ref::call<void*>(mCompTransform, obj);
}

void* get_component(void* obj, ref::Class& type)
{
    void* go = game_object(obj);
    void* t = type_object(type);
    return (go && t) ? call_m<void*>(tGoGetComponent.get(), go, t) : nullptr;
}

void* get_component_in_children(void* obj, ref::Class& type, bool include_inactive)
{
    void* go = game_object(obj);
    void* t = type_object(type);
    return (go && t) ? call_m<void*>(tGoGetComponentInChildren.get(), go, t, include_inactive) : nullptr;
}

void* get_component_in_parent(void* obj, ref::Class& type)
{
    void* go = game_object(obj);
    void* t = type_object(type);
    return (go && t) ? call_m<void*>(tGoGetComponentInParent.get(), go, t) : nullptr;
}

std::vector<void*> components_in_children(void* obj, ref::Class& type, bool include_inactive)
{
    std::vector<void*> out;
    void* go = game_object(obj);
    void* t = type_object(type);
    if (!go || !t) return out;
    void* arr = call_m<void*>(tGoGetComponentsInChildren.get(), go, t, include_inactive);
    int n = array_length(arr);
    for (int i = 0; i < n; ++i)
        if (void* e = array_at(arr, i)) out.push_back(e);
    return out;
}

bool active_in_hierarchy(void* obj)
{
    void* go = game_object(obj);
    return go && ref::call<bool>(mGoActiveInHierarchy, go);
}

bool active_self(void* obj)
{
    void* go = game_object(obj);
    return go && ref::call<bool>(mGoActiveSelf, go);
}

void* parent(void* t) { return alive(t) ? ref::call<void*>(mParent, t) : nullptr; }
int child_count(void* t) { return alive(t) ? ref::call<int32_t>(mChildCount, t) : 0; }

void* child(void* t, int index)
{
    if (!alive(t) || index < 0 || index >= child_count(t)) return nullptr;
    return ref::call<void*>(mGetChild, t, static_cast<int32_t>(index));
}

void* find_child(void* t, const wchar_t* path)
{
    if (!alive(t)) return nullptr;
    void* s = il2cpp::new_string(path);
    return s ? ref::call<void*>(mFind, t, s) : nullptr;
}

void* find_descendant(void* t, const wchar_t* n, int max_depth)
{
    return walk_find(transform(t), n ? std::wstring(n) : std::wstring(), 0, max_depth);
}

Vec3 position(void* t)
{
    void* tr = transform(t);
    return tr ? ref::call<Vec3>(mPosition, tr) : Vec3{};
}

Vec3 local_position(void* t)
{
    void* tr = transform(t);
    return tr ? ref::call<Vec3>(mLocalPosition, tr) : Vec3{};
}

std::wstring ui_text(void* c)
{
    if (!c || !kText.is(c)) return {};
    return ref::rd_str(c, fText);
}

std::vector<void*> texts(void* obj, bool include_inactive)
{
    return components_in_children(obj, kText, include_inactive);
}

std::wstring first_text(void* obj, bool include_inactive)
{
    for (void* t : texts(obj, include_inactive)) {
        std::wstring s = ui_text(t);
        if (!s.empty()) return s;
    }
    return {};
}

ref::Class& text_class() { return kText; }
ref::Class& image_class() { return kImage; }

int array_length(void* arr)
{
    if (!arr) return 0;
    int64_t n = il2cpp::read_field<int64_t>(arr, kArrayLengthOff);
    return (n < 0 || n > (1 << 24)) ? 0 : static_cast<int>(n);
}

void* array_at(void* arr, int index)
{
    if (!arr || index < 0 || index >= array_length(arr)) return nullptr;
    return il2cpp::read_field<void*>(arr, kArrayDataOff + index * 8);
}

int list_count(void* list)
{
    if (!list) return 0;
    int n = il2cpp::read_field<int32_t>(list, kListSizeOff);
    return (n < 0 || n > (1 << 24)) ? 0 : n;
}

void* list_at(void* list, int index)
{
    if (!list || index < 0 || index >= list_count(list)) return nullptr;
    return array_at(il2cpp::read_field<void*>(list, kListItemsOff), index);
}

namespace {
struct DictLayout { int32_t entries = -1, count = -1; };
DictLayout dict_layout(void* dict)
{
    DictLayout l;
    il2cpp::Class* k = il2cpp::object_class(dict);
    if (!k) return l;
    static const char* entries[] = { "entries", "_entries" };
    static const char* count[] = { "count", "_count" };
    l.entries = il2cpp::field_offset_any(k, entries, 2);
    l.count = il2cpp::field_offset_any(k, count, 2);
    return l;
}
}

void* dict_reference(void* dict, int32_t key)
{
    if (!dict) return nullptr;
    auto* cls = il2cpp::object_class(dict);
    static std::map<il2cpp::Class*, il2cpp::Method*> methods; // native owner only
    auto found = methods.find(cls);
    if (found == methods.end()) found = methods.emplace(cls, il2cpp::find_method(cls, "TryGetValue", 2)).first;
    auto* method = found->second;
    auto* pointer = method ? il2cpp::method_pointer(method) : nullptr;
    if (!pointer) return nullptr;
    void* value = nullptr;
    using Lookup = bool (*)(void*, int32_t, void**, const il2cpp::Method*);
    return reinterpret_cast<Lookup>(pointer)(dict, key, &value, method) ? value : nullptr;
}

int dict_count(void* dict)
{
    if (!dict) return 0;
    DictLayout l = dict_layout(dict);
    return l.count >= 0 ? il2cpp::read_field<int32_t>(dict, l.count) : 0;
}

bool dict_entry(void* dict, int index, uint64_t& key, uint64_t& value, int key_size, int value_size)
{
    key = value = 0;
    if (!dict || index < 0) return false;
    const auto l = dict_layout(dict);
    if (l.entries < 0 || l.count < 0) return false;
    void* arr = il2cpp::read_field<void*>(dict, l.entries);
    if (index >= il2cpp::read_field<int32_t>(dict, l.count) || index >= array_length(arr)) return false;
    auto align = [](int off, int a) { return (off + a - 1) / a * a; };
    const int key_off = align(8, key_size), value_off = align(key_off + key_size, value_size);
    const int stride = align(value_off + value_size, std::max(4, std::max(key_size, value_size)));
    const int base = kArrayDataOff + index * stride;
    if (il2cpp::read_field<int32_t>(arr, base) < 0) return false;
    key = key_size == 8 ? il2cpp::read_field<uint64_t>(arr, base + key_off)
                        : static_cast<uint32_t>(il2cpp::read_field<int32_t>(arr, base + key_off));
    value = value_size == 8 ? il2cpp::read_field<uint64_t>(arr, base + value_off)
                          : static_cast<uint32_t>(il2cpp::read_field<int32_t>(arr, base + value_off));
    return true;
}

void dict_for_each(void* dict, void (*fn)(uint64_t, uint64_t, void*), void* state, int key_size, int value_size)
{
    if (!dict || !fn) return;
    DictLayout l = dict_layout(dict);
    if (l.entries < 0 || l.count < 0) return;
    void* arr = il2cpp::read_field<void*>(dict, l.entries);
    int used = il2cpp::read_field<int32_t>(dict, l.count);
    int n = array_length(arr);
    if (used > n) used = n;
    // Entry<TKey,TValue> { int hashCode; int next; TKey key; TValue value; }, each field
    // aligned to its own size, the struct to its largest member.
    auto align = [](int off, int a) { return (off + a - 1) / a * a; };
    int key_off = align(8, key_size);
    int value_off = align(key_off + key_size, value_size);
    int max_align = key_size > value_size ? key_size : value_size;
    if (max_align < 4) max_align = 4;
    int stride = align(value_off + value_size, max_align);
    for (int i = 0; i < used; ++i) {
        int32_t base = kArrayDataOff + i * stride;
        int32_t hash = il2cpp::read_field<int32_t>(arr, base);
        if (hash < 0) continue;   // free slot
        uint64_t key = 0, value = 0;
        if (key_size == 8) key = il2cpp::read_field<uint64_t>(arr, base + key_off);
        else key = static_cast<uint32_t>(il2cpp::read_field<int32_t>(arr, base + key_off));
        if (value_size == 8) value = il2cpp::read_field<uint64_t>(arr, base + value_off);
        else value = static_cast<uint32_t>(il2cpp::read_field<int32_t>(arr, base + value_off));
        fn(key, value, state);
    }
}

static bool enumerate_impl(void* e, std::vector<void*>& out, int maximum, bool strict)
{
    out.clear();
    if (!e || maximum <= 0 || maximum > kMaxEnumerated) return false;
    il2cpp::Class* k = il2cpp::object_class(e);
    if (!k) return false;

    if (class_is(k, "System.Collections.Generic", "List`1")) {
        int n = list_count(e);
        if (n < 0 || (strict && n > maximum)) return false;
        n = std::min(n, maximum);
        out.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) out.push_back(list_at(e, i));
        return true;
    }
    if (ends_with(il2cpp::class_name(k), "[]")) {
        int n = array_length(e);
        if (n < 0 || (strict && n > maximum)) return false;
        n = std::min(n, maximum);
        out.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) out.push_back(array_at(e, i));
        return true;
    }

    // Any other IEnumerable (a LINQ iterator): enumerate it the way the List constructor did.
    il2cpp::Method* get_enum = mGetEnumerator.get();
    if (!get_enum) return false;
    void* it = il2cpp::invoke_ptr(il2cpp::virtual_method(e, get_enum), e, nullptr, 0);
    if (!it) return false;
    il2cpp::Class* ik = il2cpp::object_class(it);
    il2cpp::Class* parent = ik ? il2cpp::class_parent(ik) : nullptr;
    if (parent && class_is(parent, "System", "ValueType")) {
        static bool logged = false;
        if (!logged) {
            logged = true;
            LOGF("unity: cannot walk a boxed %s enumerator", il2cpp::class_name(ik));
        }
        return false;
    }
    il2cpp::Method* move_next = mMoveNext.get();
    il2cpp::Method* current = mGetCurrent.get();
    if (!move_next || !current) return false;
    move_next = il2cpp::virtual_method(it, move_next);
    current = il2cpp::virtual_method(it, current);
    for (int i = 0; i <= maximum; ++i) {
        if (!il2cpp::invoke_bool(move_next, it, nullptr, 0)) return true;
        if (i == maximum) { if (strict) out.clear(); return !strict; }
        out.push_back(il2cpp::invoke_ptr(current, it, nullptr, 0));
    }
    return false;
}

bool enumerate(void* e, std::vector<void*>& out)
{
    return enumerate_impl(e, out, kMaxEnumerated, false);
}
bool enumerate_bounded(void* e, std::vector<void*>& out, int maximum)
{
    return enumerate_impl(e, out, maximum, true);
}

} // namespace unity
