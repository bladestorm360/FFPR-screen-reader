// Ports the shared battle lookups of FF1 Patches/BattleMessagePatches.cs (GetUnitName,
// GetConditionName(int), GetActionName, GetItemName, GetAbilityName), Patches/MagicMenuState.cs
// (GetConditionName(Condition), ConditionTypeFallback) and Utils/TextUtils.cs (StripIconMarkup).
#include "battle_util.h"
#include <cstring>
#include "../core.h"
#include "../log.h"
#include "../modtext.h"
#include "../text.h"
#include "../unity.h"

namespace ffpr::battle {

ref::Class kBattleUnitData{"Last.Battle", "BattleUnitData"};
ref::Class kBattlePlayerData{"", "BattlePlayerData"};
ref::Class kBattleEnemyData{"Last.Battle", "BattleEnemyData"};
ref::Class kCharParam{"Last.Data", "CharacterParameterBase"};
ref::Class kOwnedCharacterData{"Last.Data.User", "OwnedCharacterData"};

namespace {

ref::Class kMessageManager{"Last.Management", "MessageManager"};
ref::Method mGetMessage{kMessageManager, "GetMessage", 2};

ref::Class kContentUtil{"Last.Systems", "ContentUtitlity"};
ref::Class kConditionUtil{"Last.Systems", "ConditionUtility"};
ref::Class kAbility{"Last.Data.Master", "Ability"};
ref::Class kInt32{"System", "Int32"};

ref::Field fUnitInfo{kBattleUnitData, "<BattleUnitDataInfo>k__BackingField"};
ref::Class kBattleUnitDataInfo{"Last.Battle", "BattleUnitDataInfo"};
ref::Field fInfoParameter{kBattleUnitDataInfo, "<Parameter>k__BackingField"};
ref::Field fInfoFunctions{kBattleUnitDataInfo, "<BattleConditionFunction>k__BackingField"};
ref::Class kBattleConditionFunction{"Last.Battle", "BattleConditionFunction"};
ref::Field fFuncUnit{kBattleConditionFunction, "<BattleUnitData>k__BackingField"};
ref::Field fFuncCondition{kBattleConditionFunction, "condition"};
ref::Field fOwnedCharacterData{kBattlePlayerData, "<ownedCharacterData>k__BackingField"};
ref::Method mGetMesIdName{kBattleEnemyData, "GetMesIdName", 0};

ref::Method mCharId{kOwnedCharacterData, "get_Id", 0};
ref::Method mCharName{kOwnedCharacterData, "get_Name", 0};
ref::Field fCharParameter{kOwnedCharacterData, "parameter"};

ref::Field fCurrentHP{kCharParam, "currentHP"};
ref::Field fBaseLevel{kCharParam, "<BaseLevel>k__BackingField"};
ref::Field fBaseMaxHp{kCharParam, "<BaseMaxHp>k__BackingField"};
ref::Field fBasePower{kCharParam, "<BasePower>k__BackingField"};
ref::Field fBaseVitality{kCharParam, "<BaseVitality>k__BackingField"};
ref::Field fBaseAgility{kCharParam, "<BaseAgility>k__BackingField"};
ref::Field fBaseIntelligence{kCharParam, "<BaseIntelligence>k__BackingField"};
ref::Field fBaseLuck{kCharParam, "<BaseLuck>k__BackingField"};
ref::Field fBaseAccuracyRate{kCharParam, "<BaseAccuracyRate>k__BackingField"};
ref::Field fBaseEvasionRate{kCharParam, "<BaseEvasionRate>k__BackingField"};
ref::Field fCurrentConditionList{kCharParam, "<CurrentConditionList>k__BackingField"};
ref::Method mConfirmedMaxHp{kCharParam, "ConfirmedMaxHp", 0};
ref::Method mConfirmedLevel{kCharParam, "ConfirmedLevel", 0};
ref::Method mConfirmedPower{kCharParam, "ConfirmedPower", 0};
ref::Method mConfirmedVitality{kCharParam, "ConfirmedVitality", 0};
ref::Method mConfirmedAgility{kCharParam, "ConfirmedAgility", 0};
ref::Method mConfirmedIntelligence{kCharParam, "ConfirmedIntelligence", 0};
ref::Method mConfirmedLuck{kCharParam, "ConfirmedLuck", 0};
ref::Method mConfirmedAccuracyRate{kCharParam, "ConfirmedAccuracyRate", 1};
ref::Method mConfirmedEvasionRate{kCharParam, "ConfirmedEvasionRate", 1};
ref::Method mConfirmedConditionList{kCharParam, "ConfirmedConditionList", 0};

ref::Class kCondition{"Last.Data.Master", "Condition"};
ref::Method mCondId{kCondition, "get_Id", 0};
ref::Method mCondType{kCondition, "get_ConditionType", 0};
ref::Method mCondMesId{kCondition, "get_MesIdName", 0};

ref::Class kCommand{"Last.Data.Master", "Command"};
ref::Method mCommandMesId{kCommand, "get_MesIdName", 0};

ref::Class kOwnedItemData{"Last.Data.User", "OwnedItemData"};
ref::Method mItemName{kOwnedItemData, "get_Name", 0};
ref::Method mItemCount{kOwnedItemData, "get_Count", 0};

ref::Class kBattleActData{"Last.Battle", "BattleActData"};
ref::Field fActItemList{kBattleActData, "itemList"};
ref::Field fActAbilityList{kBattleActData, "abilityList"};
ref::Field fActCommand{kBattleActData, "<Command>k__BackingField"};

// MasterManager.GetList<Condition>() is generic; the same dictionary is reached through the
// manager's own masterList (DataType.Condition = 20 in all five games) -> DataElement<T>.DataList.
ref::Class kMasterManager{"Last.Data.Master", "MasterManager"};
ref::Field fMasterList{kMasterManager, "masterList"};
constexpr int kDataTypeCondition = 20;

// Equal-argc overloads, resolved once at install by their parameter class.
Resolved g_get_ability_name;          // ContentUtitlity.GetAbilityName(Ability)
Resolved g_get_mes_id_ability_name;   // ContentUtitlity.GetMesIdAbilityName(Ability)
Resolved g_get_mes_id_item_name;      // ContentUtitlity.GetMesIdItemName(int)
Resolved g_is_out_battle;             // ConditionUtility.IsOutBattle(CharacterParameterBase)

Resolved resolve(ref::Class& k, const char* name, int argc, ref::Class& param, const char* label)
{
    Resolved r;
    r.label = label;
    il2cpp::Class* kc = k.get();
    il2cpp::Class* pc = param.get();
    if (!kc || !pc) {
        LOGF("battle: %s unresolved (class missing)", label);
        return r;
    }
    r.m = il2cpp::find_method_overload_param(kc, name, argc, pc, 0);
    r.fp = il2cpp::method_pointer(r.m);
    if (!r.fp) LOGF("battle: %s unresolved", label);
    return r;
}

int install()
{
    g_get_ability_name = resolve(kContentUtil, "GetAbilityName", 1, kAbility, "ContentUtitlity.GetAbilityName(Ability)");
    g_get_mes_id_ability_name = resolve(kContentUtil, "GetMesIdAbilityName", 1, kAbility, "ContentUtitlity.GetMesIdAbilityName(Ability)");
    g_get_mes_id_item_name = resolve(kContentUtil, "GetMesIdItemName", 1, kInt32, "ContentUtitlity.GetMesIdItemName(int)");
    g_is_out_battle = resolve(kConditionUtil, "IsOutBattle", 1, kCharParam, "ConditionUtility.IsOutBattle(CharacterParameterBase)");
    return 0;
}
core::Install s_install{"Battle.Util", &install, 100};

// --- raw il2cpp metadata exports (overload pick by parameter type name) -----------------------
using fn_get_methods = const void* (*)(void*, void**);
using fn_method_name = const char* (*)(const void*);
using fn_param_count = uint32_t (*)(const void*);
using fn_param = const void* (*)(const void*, uint32_t);
using fn_type_name = char* (*)(const void*);
using fn_free = void (*)(void*);

template <typename F>
F export_of(HMODULE ga, const char* name)
{
    return ga ? reinterpret_cast<F>(GetProcAddress(ga, name)) : nullptr;
}

struct FindKey {
    int key;
    uint64_t value;
    bool found;
};

void find_key_cb(uint64_t key, uint64_t value, void* state)
{
    auto* s = static_cast<FindKey*>(state);
    if (!s->found && static_cast<int32_t>(key) == s->key) {
        s->value = value;
        s->found = true;
    }
}

std::wstring condition_type_fallback(int condition_type)
{
    switch (condition_type) {
    case 4: return T(L"Critical");          // Dying (low HP)
    case 5: return T(L"KO");                // UnableFight
    case 6: return T(L"Silence");
    case 7: return T(L"Sleep");
    case 8: return T(L"Paralysis");
    case 9: return T(L"Blind");
    case 10: return T(L"Poison");
    case 11: return T(L"Stone");            // Mineralization
    case 12: return T(L"Confusion");
    case 16: return T(L"Slow");
    case 17: return T(L"Stop");
    case 32: return T(L"Aging");
    case 34: return T(L"Zombie");
    case 204: return T(L"Venom");
    case 404: return T(L"Doom");
    case 405: return T(L"Gradual Petrify"); // SlowlyMineralization
    case 406: return T(L"Curse");
    default: return std::wstring();
    }
}

ref::Method& confirmed_method(Stat s)
{
    switch (s) {
    case Stat::MaxHp: return mConfirmedMaxHp;
    case Stat::Level: return mConfirmedLevel;
    case Stat::Power: return mConfirmedPower;
    case Stat::Agility: return mConfirmedAgility;
    case Stat::Vitality: return mConfirmedVitality;
    case Stat::Intelligence: return mConfirmedIntelligence;
    case Stat::Luck: return mConfirmedLuck;
    case Stat::AccuracyRate: return mConfirmedAccuracyRate;
    default: return mConfirmedEvasionRate;
    }
}

ref::Field& base_field(Stat s)
{
    switch (s) {
    case Stat::MaxHp: return fBaseMaxHp;
    case Stat::Level: return fBaseLevel;
    case Stat::Power: return fBasePower;
    case Stat::Agility: return fBaseAgility;
    case Stat::Vitality: return fBaseVitality;
    case Stat::Intelligence: return fBaseIntelligence;
    case Stat::Luck: return fBaseLuck;
    case Stat::AccuracyRate: return fBaseAccuracyRate;
    default: return fBaseEvasionRate;
    }
}

} // namespace

// --- text ---------------------------------------------------------------------------------------

std::wstring message(const std::wstring& mes_id, bool is_replace)
{
    if (mes_id.empty()) return std::wstring();
    void* mm = ref::instance(kMessageManager);
    if (!mm) return std::wstring();
    return ref::call_str(mGetMessage, mm, ref::new_str(mes_id), is_replace);
}

std::wstring strip_icon(std::wstring_view s)
{
    if (s.empty()) return std::wstring();
    return text::trim(text::strip_markup(s));
}

std::wstring n0(long long v)
{
    bool neg = v < 0;
    unsigned long long u = neg ? 0ull - static_cast<unsigned long long>(v) : static_cast<unsigned long long>(v);
    std::wstring digits = std::to_wstring(u);
    std::wstring out;
    int n = static_cast<int>(digits.size());
    for (int i = 0; i < n; ++i) {
        out.push_back(digits[static_cast<size_t>(i)]);
        int left = n - 1 - i;
        if (left > 0 && left % 3 == 0) out.push_back(L',');
    }
    return neg ? L"-" + out : out;
}

bool blank(const std::wstring& s) { return text::is_blank(s); }

// --- native helpers -----------------------------------------------------------------------------

il2cpp::Method* find_method_param_type(il2cpp::Class* k, const char* name, int argc, int param_index,
                                       const char* type_needle)
{
    if (!k || !name || !type_needle || param_index < 0 || param_index >= argc) return nullptr;
    HMODULE ga = GetModuleHandleW(L"GameAssembly.dll");
    auto get_methods = export_of<fn_get_methods>(ga, "il2cpp_class_get_methods");
    auto method_name = export_of<fn_method_name>(ga, "il2cpp_method_get_name");
    auto param_count = export_of<fn_param_count>(ga, "il2cpp_method_get_param_count");
    auto param = export_of<fn_param>(ga, "il2cpp_method_get_param");
    auto type_name = export_of<fn_type_name>(ga, "il2cpp_type_get_name");
    auto il_free = export_of<fn_free>(ga, "il2cpp_free");
    if (!get_methods || !method_name || !param_count || !param || !type_name) return nullptr;
    il2cpp::Method* found = nullptr;
    try {
        void* iter = nullptr;
        while (const void* m = get_methods(k, &iter)) {
            const char* mn = method_name(m);
            if (!mn || std::strcmp(mn, name) != 0) continue;
            if (param_count(m) != static_cast<uint32_t>(argc)) continue;
            const void* t = param(m, static_cast<uint32_t>(param_index));
            if (!t) continue;
            char* tn = type_name(t);
            bool hit = tn && std::strstr(tn, type_needle) != nullptr;
            if (tn && il_free) il_free(tn);
            if (!hit) continue;
            if (found) return nullptr;   // ambiguous: fail rather than guess
            found = const_cast<il2cpp::Method*>(m);
        }
    } catch (...) {
        return nullptr;
    }
    return found;
}

std::vector<void*> list_items(void* list)
{
    std::vector<void*> out;
    if (!is_list(list)) return out;
    int n = unity::list_count(list);
    out.reserve(static_cast<size_t>(n > 0 ? n : 0));
    for (int i = 0; i < n; ++i) out.push_back(unity::list_at(list, i));
    return out;
}

bool is_list(void* obj)
{
    if (!obj) return false;
    return std::strcmp(il2cpp::object_class_name(obj), "List`1") == 0;
}

bool dict_int_int(void* dict, int key, int& out)
{
    if (!dict) return false;
    FindKey s{key, 0, false};
    unity::dict_for_each(dict, &find_key_cb, &s, 4, 4);
    if (!s.found) return false;
    out = static_cast<int32_t>(s.value);
    return true;
}

// --- battle units -------------------------------------------------------------------------------

void* unit_info(void* unit) { return unit ? ref::rd<void*>(unit, fUnitInfo) : nullptr; }
void* info_parameter(void* info) { return info ? ref::rd<void*>(info, fInfoParameter) : nullptr; }
void* unit_parameter(void* unit) { return info_parameter(unit_info(unit)); }
void* info_functions(void* info) { return info ? ref::rd<void*>(info, fInfoFunctions) : nullptr; }
void* function_condition(void* fn) { return fn ? ref::rd<void*>(fn, fFuncCondition) : nullptr; }
void* function_unit(void* fn) { return fn ? ref::rd<void*>(fn, fFuncUnit) : nullptr; }

void* player_owned_character(void* unit)
{
    if (!unit || !kBattlePlayerData.is(unit)) return nullptr;
    return ref::rd<void*>(unit, fOwnedCharacterData);
}

std::wstring enemy_name(void* enemy)
{
    if (!enemy) return std::wstring();
    std::wstring mes_id = ref::call_str(mGetMesIdName, enemy);
    if (mes_id.empty()) return std::wstring();
    return message(mes_id);
}

std::wstring unit_name(void* unit)
{
    if (!unit) return T(L"Unknown");
    if (kBattlePlayerData.is(unit)) {
        if (void* owned = ref::rd<void*>(unit, fOwnedCharacterData))
            return character_name(owned);
    }
    if (kBattleEnemyData.is(unit)) {
        std::wstring name = enemy_name(unit);
        if (!name.empty()) return name;
    }
    return T(L"Unknown");
}

// --- characters ---------------------------------------------------------------------------------

int character_id(void* owned) { return owned ? ref::call<int32_t>(mCharId, owned) : 0; }
std::wstring character_name(void* owned) { return owned ? ref::call_str(mCharName, owned) : std::wstring(); }
void* character_parameter(void* owned) { return owned ? ref::rd<void*>(owned, fCharParameter) : nullptr; }

// --- parameters ---------------------------------------------------------------------------------

int param_current_hp(void* p) { return p ? ref::rd<int32_t>(p, fCurrentHP) : 0; }
int param_base_max_hp(void* p) { return p ? ref::rd<int32_t>(p, fBaseMaxHp) : 0; }
int param_base_level(void* p) { return p ? ref::rd<int32_t>(p, fBaseLevel) : 0; }

bool param_confirmed(void* param, Stat s, int& out)
{
    if (!param) return false;
    int32_t v = 0;
    bool ok = (s == Stat::AccuracyRate || s == Stat::EvasionRate)
                  ? vcall(confirmed_method(s), param, v, false)   // ConfirmedXRate(false)
                  : vcall(confirmed_method(s), param, v);
    if (ok) out = v;
    return ok;
}

int param_base(void* param, Stat s) { return param ? ref::rd<int32_t>(param, base_field(s)) : 0; }

int param_confirmed_or_base(void* param, Stat s)
{
    int v = 0;
    if (param_confirmed(param, s, v)) return v;
    return param_base(param, s);
}

std::vector<void*> confirmed_conditions(void* param)
{
    void* list = nullptr;
    if (!param || !vcall(mConfirmedConditionList, param, list)) return {};
    return list_items(list);
}

std::vector<void*> current_conditions(void* param)
{
    if (!param) return {};
    return list_items(ref::rd<void*>(param, fCurrentConditionList));
}

bool is_out_battle(void* param)
{
    if (!param) return false;
    return call_static<bool>(g_is_out_battle, param);
}

// --- conditions ---------------------------------------------------------------------------------

int condition_id(void* c) { return c ? ref::call<int32_t>(mCondId, c) : 0; }
int condition_type(void* c) { return c ? ref::call<int32_t>(mCondType, c) : 0; }
std::wstring condition_mes_id(void* c) { return c ? ref::call_str(mCondMesId, c) : std::wstring(); }

std::wstring condition_name(void* condition)
{
    if (!condition) return std::wstring();
    std::wstring mes_id = condition_mes_id(condition);
    int cond_type = condition_type(condition);
    // Primary: localize via MesIdName. Reject a "$"-prefixed return: MessageManager echoes the
    // raw key when unresolved.
    if (!mes_id.empty() && mes_id != L"None") {
        std::wstring localized = message(mes_id, false);
        if (!blank(localized) && !text::starts_with(localized, L"$")) return localized;
    }
    return condition_type_fallback(cond_type);
}

std::wstring condition_name_by_id(int id)
{
    void* mm = ref::instance(kMasterManager);
    if (!mm) return std::wstring();
    void* master_list = ref::rd<void*>(mm, fMasterList);
    if (!master_list) return std::wstring();
    FindKey el{kDataTypeCondition, 0, false};
    unity::dict_for_each(master_list, &find_key_cb, &el, 4, 8);
    void* element = el.found ? reinterpret_cast<void*>(el.value) : nullptr;
    if (!element) return std::wstring();
    int32_t off = il2cpp::field_offset(il2cpp::object_class(element), "<DataList>k__BackingField");
    void* data_list = il2cpp::read_field<void*>(element, off);
    if (!data_list) return std::wstring();
    FindKey c{id, 0, false};
    unity::dict_for_each(data_list, &find_key_cb, &c, 4, 8);
    void* condition = c.found ? reinterpret_cast<void*>(c.value) : nullptr;
    if (!condition) return std::wstring();
    std::wstring mes_id = condition_mes_id(condition);
    if (mes_id.empty() || mes_id == L"None") return std::wstring();
    return strip_icon(message(mes_id));
}

std::wstring resolve_condition_name(void* condition, int id)
{
    std::wstring name = condition ? condition_name(condition) : std::wstring();
    if (name.empty()) name = condition_name_by_id(id);
    return name;
}

// --- actions and items --------------------------------------------------------------------------

std::wstring owned_item_name(void* item) { return item ? ref::call_str(mItemName, item) : std::wstring(); }
int owned_item_count(void* item) { return item ? ref::call<int32_t>(mItemCount, item) : 0; }
std::wstring command_mes_id(void* command) { return command ? ref::call_str(mCommandMesId, command) : std::wstring(); }

std::wstring item_name_by_content(int content_id)
{
    if (content_id <= 0) return std::wstring();
    std::wstring mes_id = ref::str(call_static<void*>(g_get_mes_id_item_name, static_cast<int32_t>(content_id)));
    if (mes_id.empty()) return std::wstring();
    return message(mes_id, false);
}

std::wstring action_name(void* act)
{
    if (!act) return std::wstring();
    // Item name first (Item command).
    void* items = ref::rd<void*>(act, fActItemList);
    if (unity::list_count(items) > 0) {
        if (void* owned = unity::list_at(items, 0)) {
            std::wstring name = owned_item_name(owned);
            if (!name.empty()) {
                name = strip_icon(name);
                if (!name.empty()) return name;
            }
        }
    }
    // Ability name (spells, skills).
    void* abilities = ref::rd<void*>(act, fActAbilityList);
    if (unity::list_count(abilities) > 0) {
        if (void* ability = unity::list_at(abilities, 0)) {
            std::wstring name = ref::str(call_static<void*>(g_get_ability_name, ability));
            if (!name.empty()) {
                name = strip_icon(name);
            } else {
                std::wstring mes_id = ref::str(call_static<void*>(g_get_mes_id_ability_name, ability));
                std::wstring localized = message(mes_id, false);
                if (!localized.empty()) name = strip_icon(localized);
            }
            if (!name.empty()) return name;
        }
    }
    // Command name (Attack, Defend...).
    if (void* command = ref::rd<void*>(act, fActCommand)) {
        std::wstring localized = message(command_mes_id(command));
        if (!localized.empty()) return strip_icon(localized);
    }
    return std::wstring();
}

} // namespace ffpr::battle
