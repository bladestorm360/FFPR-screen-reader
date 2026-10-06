#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "../refs.h"
#include "../il2cpp.h"

// Shared helpers for the battle readers (src/core/battle/*). Ports the name and status lookups
// FF1 keeps in Patches/BattleMessagePatches.cs (GetUnitName, GetConditionName(id), GetActionName,
// GetItemName, GetAbilityName), Patches/MagicMenuState.cs (GetConditionName(Condition) and its
// ConditionType fallback) and Utils/TextUtils.cs (StripIconMarkup), plus the native plumbing the
// C# code got from Il2CppInterop: virtual calls, overload resolution and List<T> walks.
//
// Every class here exists in all five games' dump.cs with the same member names.
// Main thread only, like the C# originals.
namespace ffpr::battle {

// --- refs shared by the battle files ----------------------------------------------------------
extern ref::Class kBattleUnitData;
extern ref::Class kBattlePlayerData;
extern ref::Class kBattleEnemyData;
extern ref::Class kCharParam;
extern ref::Class kOwnedCharacterData;

// --- text ---------------------------------------------------------------------------------------

/// MessageManager.Instance.GetMessage(key, isReplace). Empty when the key is empty or unresolved.
std::wstring message(const std::wstring& mes_id, bool is_replace = true);
/// TextUtils.StripIconMarkup: removes <tag> runs without whitespace, then trims.
std::wstring strip_icon(std::wstring_view s);
/// int.ToString("N0") (group separators every three digits).
std::wstring n0(long long v);
/// string.IsNullOrWhiteSpace for a copied-out string.
bool blank(const std::wstring& s);

// --- native call helpers ----------------------------------------------------------------------

/// Call a virtual/abstract instance method through the object's own override (the C# call
/// `param.ConfirmedMaxHp()`). False when the method is missing or the call faulted/threw, so
/// the caller can take the C# catch-branch fallback.
template <typename R, typename... A>
bool vcall(ref::Method& m, void* self, R& out, A... args)
{
    il2cpp::Method* base = m.get();
    if (!base || !self) return false;
    il2cpp::Method* v = il2cpp::virtual_method(self, base);
    void* fp = il2cpp::method_pointer(v);
    if (!fp) return false;
    try {
        using F = R (*)(void*, A..., const void*);
        out = reinterpret_cast<F>(fp)(self, args..., v);
        return true;
    } catch (...) {
        ref::call_failed_note(m.name);
        return false;
    }
}

/// A method resolved at install time (equal-argc overloads), callable directly.
struct Resolved {
    il2cpp::Method* m = nullptr;
    void* fp = nullptr;
    const char* label = "";
    explicit operator bool() const { return fp != nullptr; }
};

template <typename R, typename... A>
R call_static(const Resolved& r, A... args)
{
    if (!r.fp) return R{};
    try {
        using S = R (*)(A..., const void*);
        return reinterpret_cast<S>(r.fp)(args..., r.m);
    } catch (...) {
        ref::call_failed_note(r.label);
        return R{};
    }
}

/// The overload of `name` (argc args) whose parameter `param_index` has a type whose full name
/// contains `type_needle` (FF1's "typeString.Contains(\"BattleEnemyData\")" overload pick).
/// Install time only. Null when absent or ambiguous.
il2cpp::Method* find_method_param_type(il2cpp::Class* k, const char* name, int argc, int param_index,
                                       const char* type_needle);

/// il2cpp List<T> as a vector of element pointers (empty for null / not a List).
std::vector<void*> list_items(void* list);
/// True when `obj` is a System.Collections.Generic.List`1 (the C# TryCast<List<T>> check).
bool is_list(void* obj);

/// Dictionary<int, int> lookup (ContainsKey + indexer). False when absent.
bool dict_int_int(void* dict, int key, int& out);

// --- battle units -------------------------------------------------------------------------------

/// BattleUnitData.BattleUnitDataInfo.
void* unit_info(void* unit);
/// BattleUnitData.BattleUnitDataInfo.Parameter (CharacterParameterBase).
void* unit_parameter(void* unit);
/// BattleUnitDataInfo.Parameter.
void* info_parameter(void* info);
/// BattleUnitDataInfo.BattleConditionFunction (List<BattleConditionFunction>).
void* info_functions(void* info);
/// BattleConditionFunction.condition / .BattleUnitData.
void* function_condition(void* fn);
void* function_unit(void* fn);
/// BattlePlayerData.ownedCharacterData (null when not a player).
void* player_owned_character(void* unit);
/// BattleEnemyData.GetMesIdName() resolved through MessageManager (isReplace true); empty on miss.
std::wstring enemy_name(void* enemy);
/// BattleMessagePatches.GetUnitName: character name, enemy name, else T("Unknown").
std::wstring unit_name(void* unit);

// --- characters ---------------------------------------------------------------------------------
int character_id(void* owned_character);
std::wstring character_name(void* owned_character);
/// OwnedCharacterData.Parameter (PlayerCharacterParameter).
void* character_parameter(void* owned_character);

// --- character parameters ---------------------------------------------------------------------
int param_current_hp(void* param);
int param_current_mp(void* param);
int param_base_max_hp(void* param);
int param_base_level(void* param);
/// Confirmed* (virtual). False when the call failed (the C# catch branch).
enum class Stat { MaxHp, Level, Power, Agility, Vitality, Intelligence, Luck, AccuracyRate, EvasionRate, MaxMp };
bool param_confirmed(void* param, Stat s, int& out);
/// Base* auto-property backing field for the same stat.
int param_base(void* param, Stat s);
/// try { Confirmed } catch { Base } - the pattern every FF1 battle reader uses.
int param_confirmed_or_base(void* param, Stat s);
/// CharacterParameterBase.ConfirmedConditionList() (virtual) / .CurrentConditionList.
std::vector<void*> confirmed_conditions(void* param);
std::vector<void*> current_conditions(void* param);
/// ConditionUtility.IsOutBattle(CharacterParameterBase).
bool is_out_battle(void* param);

// --- conditions ---------------------------------------------------------------------------------
int condition_id(void* condition);
int condition_type(void* condition);
std::wstring condition_mes_id(void* condition);
/// MagicMenuState.GetConditionName(Condition): localized name (unstripped, "$" echoes rejected),
/// else the ConditionType fallback; empty when neither.
std::wstring condition_name(void* condition);
/// BattleMessagePatches.GetConditionName(int id): master Condition by id, stripped; empty on miss.
std::wstring condition_name_by_id(int id);
/// BattleConditionController_Add_Patch.ResolveConditionName(condition, id).
std::wstring resolve_condition_name(void* condition, int id);

constexpr int kConditionTypeKO = 5;      // ConditionType.UnableFight
constexpr int kConditionTypeStone = 11;  // ConditionType.Mineralization

// --- actions and items --------------------------------------------------------------------------
/// BattleMessagePatches.GetActionName(BattleActData): item, ability, then command name; empty on miss.
std::wstring action_name(void* act_data);
/// OwnedItemData.Name / .Count.
std::wstring owned_item_name(void* owned_item);
int owned_item_count(void* owned_item);
/// ContentUtitlity.GetMesIdItemName(int) resolved with GetMessage(mesId, false); empty on miss.
std::wstring item_name_by_content(int content_id);
/// Command.MesIdName.
std::wstring command_mes_id(void* command);

} // namespace ffpr::battle
