// Ports the FF1 helpers the item, equipment and magic screens share: TextUtils.StripIconMarkup,
// MessageManager.GetMessage, MenuManager.IsOpen, PreferencesManager.AutoDetailEnabled,
// FFI_ScreenReaderMod.ClearOtherMenuStates (Core/FFI_ScreenReaderMod.cs, ManualPatches),
// Il2CppInterop's `new List<T>(IEnumerable<T>)`, MasterManager.GetData<T>/GetList<T>, and the
// character reads of Patches/ItemMenuPatches.cs and Patches/MagicTargetPatches.cs.
#include "menus_internal.h"

#include <cstring>
#include "../core.h"
#include "../il2cpp.h"
#include "../log.h"
#include "menus.h"
#include "../settings.h"
#include "../system/system.h"
#include "../text.h"
#include "../unity.h"

namespace ffpr::menus::detail {
namespace {

ref::Class  kMessageManager{"Last.Management", "MessageManager"};
ref::Method mGetMessage{kMessageManager, "GetMessage", 2};

ref::Class  kMenuManager{"Last.UI", "MenuManager"};
ref::Method mMenuIsOpen{kMenuManager, "get_IsOpen", 0};

ref::Class  kIEnumerable{"System.Collections", "IEnumerable"};
ref::Method mGetEnumerator{kIEnumerable, "GetEnumerator", 0};
ref::Class  kIEnumerator{"System.Collections", "IEnumerator"};
ref::Method mMoveNext{kIEnumerator, "MoveNext", 0};
ref::Method mGetCurrent{kIEnumerator, "get_Current", 0};

ref::Class  kOwnedCharacterData{"Last.Data.User", "OwnedCharacterData"};
ref::Method mCharacterGetName{kOwnedCharacterData, "get_Name", 0};
ref::Method mCharacterGetParameter{kOwnedCharacterData, "get_Parameter", 0};
ref::Class  kCharacterParameterBase{"Last.Data", "CharacterParameterBase"};
ref::Field  fCurrentHP{kCharacterParameterBase, "currentHP"};
ref::Field  fCurrentConditionList{kCharacterParameterBase, "<CurrentConditionList>k__BackingField"};
ref::Class  kPlayerCharacterParameter{"Last.Data", "PlayerCharacterParameter"};
ref::Method mConfirmedMaxHp{kPlayerCharacterParameter, "ConfirmedMaxHp", 0};

ref::Class  kMasterManager{"Last.Data.Master", "MasterManager"};
ref::Field  fMasterList{kMasterManager, "masterList"};
ref::Field  fMasterTypeList{kMasterManager, "masterTypeList"};

// Il2CppInterop walked any IEnumerable; a game list longer than this is not a menu list.
constexpr int kMaxEnumerated = 4096;

bool class_is(il2cpp::Class* k, const char* ns, const char* name)
{
    return k && std::strcmp(il2cpp::class_name(k), name) == 0 && std::strcmp(il2cpp::class_namespace(k), ns) == 0;
}

bool ends_with(const char* s, const char* suffix)
{
    size_t a = std::strlen(s), b = std::strlen(suffix);
    return a >= b && std::strcmp(s + a - b, suffix) == 0;
}

// DataElement<T>.DataList (the Dictionary<int, T> behind GetList<T>) of one masterList value.
void* element_data_list(void* element)
{
    if (!element) return nullptr;
    il2cpp::Class* k = il2cpp::object_class(element);
    if (class_is(k, "System.Collections.Generic", "Dictionary`2")) return element;
    int32_t off = il2cpp::field_offset(k, "<DataList>k__BackingField");
    if (off < 0) off = il2cpp::field_offset(k, "DataList");
    return off >= 0 ? il2cpp::read_field<void*>(element, off) : nullptr;
}

// The class of the first value in a Dictionary<int, T>, or null when it is empty.
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

// --- text and game data -------------------------------------------------------------------

std::wstring strip_icon(std::wstring_view s)
{
    if (s.empty()) return {};
    return text::trim(text::strip_markup(s));
}

bool blank(std::wstring_view s) { return text::is_blank(s); }

std::wstring join_all(const std::vector<std::wstring>& parts, std::wstring_view sep)
{
    std::wstring out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out.append(sep);
        out += parts[i];
    }
    return out;
}

void* message_manager() { return ref::instance(kMessageManager); }

std::wstring message_from(void* mm, const std::wstring& id, bool is_replace)
{
    if (!mm || id.empty()) return {};
    void* key = ref::new_str(id);
    if (!key) return {};
    return ref::call_str(mGetMessage, mm, key, is_replace);
}

std::wstring message(const std::wstring& id, bool is_replace)
{
    return message_from(message_manager(), id, is_replace);
}

bool menu_manager_open()
{
    void* mm = ref::instance(kMenuManager);
    return mm && ref::call<bool>(mMenuIsOpen, mm);
}

bool auto_detail() { return settings::get_bool("AutoDetail", true); }

void clear_other_menu_states(const char* except_menu)
{
    ffpr::system::clear_other_menu_states(except_menu ? except_menu : "");
}

bool enumerate(void* e, std::vector<void*>& out)
{
    out.clear();
    if (!e) return false;
    il2cpp::Class* k = il2cpp::object_class(e);
    if (!k) return false;

    if (class_is(k, "System.Collections.Generic", "List`1")) {
        int n = unity::list_count(e);
        if (n > kMaxEnumerated) n = kMaxEnumerated;
        out.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) out.push_back(unity::list_at(e, i));
        return true;
    }
    if (ends_with(il2cpp::class_name(k), "[]")) {
        int n = unity::array_length(e);
        if (n > kMaxEnumerated) n = kMaxEnumerated;
        out.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) out.push_back(unity::array_at(e, i));
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
            LOGF("menus: cannot walk a boxed %s enumerator", il2cpp::class_name(ik));
        }
        return false;
    }
    il2cpp::Method* move_next = mMoveNext.get();
    il2cpp::Method* current = mGetCurrent.get();
    if (!move_next || !current) return false;
    move_next = il2cpp::virtual_method(it, move_next);
    current = il2cpp::virtual_method(it, current);
    for (int i = 0; i < kMaxEnumerated && il2cpp::invoke_bool(move_next, it, nullptr, 0); ++i)
        out.push_back(il2cpp::invoke_ptr(current, it, nullptr, 0));
    return true;
}

int call_virtual_int(ref::Method& m, void* obj)
{
    il2cpp::Method* base = m.get();
    if (!base || !obj) return 0;
    il2cpp::Method* vm = il2cpp::virtual_method(obj, base);
    void* fp = il2cpp::method_pointer(vm);
    if (!fp) return 0;
    try {
        using F = int32_t (*)(void*, const void*);
        return reinterpret_cast<F>(fp)(obj, vm);
    } catch (...) {
        ref::call_failed_note(m.name);
        return 0;
    }
}

// --- characters -----------------------------------------------------------------------------

std::wstring character_name(void* character)
{
    return character ? ref::call_str(mCharacterGetName, character) : std::wstring();
}

void* character_parameter(void* character)
{
    return character ? ref::call<void*>(mCharacterGetParameter, character) : nullptr;
}

Vitals character_vitals(void* character)
{
    Vitals v;
    void* param = character_parameter(character);
    if (!param) return v;
    v.has_parameter = true;
    v.hp = ref::rd<int32_t>(param, fCurrentHP);
    v.max_hp = call_virtual_int(mConfirmedMaxHp, param);
    void* conditions = ref::rd<void*>(param, fCurrentConditionList);
    int n = unity::list_count(conditions);
    for (int i = 0; i < n; ++i) {
        std::wstring name = ffpr::menus::condition_name(unity::list_at(conditions, i));
        if (!blank(name)) v.conditions.push_back(std::move(name));
    }
    return v;
}

// --- master data ----------------------------------------------------------------------------

void* master_manager() { return ref::instance(kMasterManager); }

void* master_list(ref::Class& type)
{
    il2cpp::Class* want = type.get();
    void* mm = master_manager();
    if (!want || !mm) return nullptr;
    void* masters = ref::rd<void*>(mm, fMasterList);
    if (!masters) return nullptr;

    // GetMaster<T>: masterList[masterTypeList[typeof(T)]].
    struct TypeHit { uint64_t type_obj; bool found = false; uint32_t data_type = 0; };
    TypeHit th{reinterpret_cast<uint64_t>(il2cpp::class_type_object(want))};
    if (th.type_obj) {
        unity::dict_for_each(ref::rd<void*>(mm, fMasterTypeList), [](uint64_t key, uint64_t value, void* st) {
            auto* p = static_cast<TypeHit*>(st);
            if (!p->found && key == p->type_obj) { p->found = true; p->data_type = static_cast<uint32_t>(value); }
        }, &th, 8, 4);
    }
    if (th.found) {
        struct ElemHit { uint32_t data_type; void* element = nullptr; } eh{th.data_type};
        unity::dict_for_each(masters, [](uint64_t key, uint64_t value, void* st) {
            auto* p = static_cast<ElemHit*>(st);
            if (!p->element && static_cast<uint32_t>(key) == p->data_type) p->element = reinterpret_cast<void*>(value);
        }, &eh, 4, 8);
        if (void* list = element_data_list(eh.element)) return list;
    }

    // Fallback without the Type identity: the master whose entries are of class T.
    struct Scan { il2cpp::Class* want; void* list = nullptr; } sc{want};
    unity::dict_for_each(masters, [](uint64_t, uint64_t value, void* st) {
        auto* p = static_cast<Scan*>(st);
        if (p->list || !value) return;
        void* list = element_data_list(reinterpret_cast<void*>(value));
        if (list && first_value_class(list) == p->want) p->list = list;
    }, &sc, 4, 8);
    return sc.list;
}

void* master_find(void* list, int id)
{
    if (!list) return nullptr;
    struct Hit { uint32_t id; void* value = nullptr; } h{static_cast<uint32_t>(id)};
    unity::dict_for_each(list, [](uint64_t key, uint64_t value, void* st) {
        auto* p = static_cast<Hit*>(st);
        if (!p->value && static_cast<uint32_t>(key) == p->id) p->value = reinterpret_cast<void*>(value);
    }, &h, 4, 8);
    return h.value;
}

void* master_data(ref::Class& type, int id) { return master_find(master_list(type), id); }

std::vector<void*> master_values(void* list)
{
    std::vector<void*> out;
    unity::dict_for_each(list, [](uint64_t, uint64_t value, void* st) {
        static_cast<std::vector<void*>*>(st)->push_back(reinterpret_cast<void*>(value));
    }, &out, 4, 8);
    return out;
}

// --- shop tracker ---------------------------------------------------------------------------
// The shop area owns ShopMenuTracker and publishes it under these names. Until it does, the
// registry's SHOP_MENU key (which the tracker's IsShopMenuActive setter mirrors in FF1) stands in.

bool shop_menu_active()
{
    if (queries::defined("ShopMenuTracker.IsShopMenuActive")) return queries::is("ShopMenuTracker.IsShopMenuActive");
    return menu_state::is_active(menu_state::SHOP_MENU);
}

void shop_set_menu_active()
{
    if (actions::defined("ShopMenuTracker.SetShopMenuActive")) actions::run("ShopMenuTracker.SetShopMenuActive");
    else menu_state::set_active(menu_state::SHOP_MENU, true);
}

void shop_clear_for_equipment_submenu()
{
    if (actions::defined("ShopMenuTracker.ClearForEquipmentSubmenu")) actions::run("ShopMenuTracker.ClearForEquipmentSubmenu");
    else menu_state::set_active(menu_state::SHOP_MENU, false);
}

bool shop_validate_state() { return queries::is("ShopMenuTracker.ValidateState"); }

std::wstring shop_last_item_name() { return queries::text("ShopMenuTracker.LastItemName"); }

} // namespace ffpr::menus::detail
