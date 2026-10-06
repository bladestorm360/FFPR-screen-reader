// Ports FF2 Patches/BattleResultPatches.cs: FF2's phased victory screen. FF2 has no EXP: stats and
// skills grow through use. One hook per on-screen phase (KeyInput result UI):
//   ResultMenuController.Show                 -> new result: guards reset, battle state cleared
//   ResultPointController.ShowSkillLevels     -> gil, then weapon-skill gains / level-ups per character
//   ResultSkillController.SetLevelUpList      -> spell level-ups per character
//   ResultStatusUpController.SetData          -> that character's stat gains, one page each
//   ResultMenuController.ShowGetItemsInit     -> item drops: the core's (same wording as FF2)
// ResultPointController.ShowPointList never runs in FF2 (the screen always starts at
// ShowLevelUpAbilitys); its postfix is a gil fallback sharing the same guard, as in the C#.
//
// Core hooks switched off for FF2 in module.cpp: ResultMenuController.ShowPointsInit (FF1's gil +
// EXP + EXP tick; never runs in FF2) and ResultMenuController.ShowStatusUpInit (FF1's "X leveled up
// to N" with FF1's stat list; FF2 announces its stat pages here instead). The core's
// ShowPointsInit also cleared the battle state; in FF2 Show does it (the C# ClearBattleState).
//
// Parity: the per-result guards are flags per BattleResultData, as in the C# (lastResultPtr,
// announcedGil/SkillLevels/SpellLevels, the stat-page set). The C#'s SetLevelUpList call counter
// was a diagnostic log only and is not ported. No hook here is folded (one RVA each in FF2).
#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include "ff2.h"
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/unity.h"
#include "../../core/menus/menus.h"

namespace ffpr::ff2 {
namespace {

// --- game names ------------------------------------------------------------------------------
ref::Class kResultMenu{"Last.UI.KeyInput", "ResultMenuController", game::kFF2};
ref::Method mResultShow{kResultMenu, "Show", 2};

ref::Class kPointCtl{"Serial.FF2.UI.KeyInput", "ResultPointController", game::kFF2};
ref::Method mShowPointList{kPointCtl, "ShowPointList", 3};
ref::Method mShowSkillLevels{kPointCtl, "ShowSkillLevels", 3};

ref::Class kStatusUpCtl{"Serial.FF2.UI.KeyInput", "ResultStatusUpController", game::kFF2};
ref::Method mStatusUpSetData{kStatusUpCtl, "SetData", 1};

ref::Class kSkillCtl{"Last.UI.KeyInput", "ResultSkillController", game::kFF2};
ref::Method mSetLevelUpList{kSkillCtl, "SetLevelUpList", 1};

ref::Class kResultData{"Last.Data", "BattleResultData", game::kFF2};
ref::Field fGetGil{kResultData, "<GetGil>k__BackingField"};
ref::Field fCharacterList{kResultData, "<CharacterList>k__BackingField"};

ref::Class kResultChar{"Last.Data", "BattleResultData.BattleResultCharacterData", game::kFF2};
ref::Field fBeforData{kResultChar, "<BeforData>k__BackingField"};
ref::Field fAfterData{kResultChar, "<AfterData>k__BackingField"};

ref::Class kOwnedCharacter{"Last.Data.User", "OwnedCharacterData", game::kFF2};
ref::Method mCharName{kOwnedCharacter, "get_Name", 0};
ref::Method mCharParameter{kOwnedCharacter, "get_Parameter", 0};
ref::Method mCharSkillTargets{kOwnedCharacter, "get_SkillLevelTargets", 0};
ref::Method mCharAbilityList{kOwnedCharacter, "get_OwnedAbilityList", 0};

ref::Class kOwnedAbility{"Last.Data.User", "OwnedAbility", game::kFF2};
ref::Method mOwnedAbilityAbility{kOwnedAbility, "get_Ability", 0};
ref::Method mOwnedAbilitySkillLevel{kOwnedAbility, "get_SkillLevel", 0};
ref::Class kAbility{"Last.Data.Master", "Ability", game::kFF2};
ref::Method mAbilityId{kAbility, "get_Id", 0};

ref::Class kExpUtility{"Last.Systems", "ExpUtility", game::kFF2};
ref::Method mGetExpLevel{kExpUtility, "GetExpLevel", 3};
ref::Method mGetNextExp{kExpUtility, "GetNextExp", 3};
ref::Method mGetExpDifference{kExpUtility, "GetExpDifference", 3};
ref::Class kBattleUtility{"Last.Battle", "BattleUtility", game::kFF2};
ref::Method mGetSkillLevel{kBattleUtility, "GetSkillLevel", 2};

ref::Class kPlayerParam{"Last.Data", "PlayerCharacterParameter", game::kFF2};
ref::Method mAddMaxHp{kPlayerParam, "get_AddtionalMaxHp", 0};
ref::Method mAddMaxMp{kPlayerParam, "get_AddtionalMaxMp", 0};
ref::Method mAddPower{kPlayerParam, "get_AddtionalPower", 0};
ref::Method mAddVitality{kPlayerParam, "get_AddtionalVitality", 0};
ref::Method mAddAgility{kPlayerParam, "get_AddtionalAgility", 0};
ref::Method mAddIntelligence{kPlayerParam, "get_AddtionalIntelligence", 0};
ref::Method mAddSpirit{kPlayerParam, "get_AddtionalSpirit", 0};
ref::Method mAddAccuracyRate{kPlayerParam, "get_AddtionalAccuracyRate", 0};
ref::Method mAddDefense{kPlayerParam, "get_AddtionalDefense", 0};
ref::Method mAddEvasionCount{kPlayerParam, "get_AddtionalEvasionCount", 0};
ref::Method mAddEvasionRate{kPlayerParam, "get_AddtionalEvasionRate", 0};
ref::Method mAddMagicDefenseCount{kPlayerParam, "get_AddtionalMagicDefenseCount", 0};
ref::Method mAddAbilityDefenseRate{kPlayerParam, "get_AddtionalAbilityDefenseRate", 0};
ref::Method mAddAbilityEvasionRate{kPlayerParam, "get_AddtionalAbilityEvasionRate", 0};

constexpr int kLevelExp = 1;   // ExpTableType.LevelExp
// SkillLevelTarget
constexpr int kPhysicalAvoidance = 8;
constexpr int kAbilityAvoidance = 9;

// --- per-result guards -----------------------------------------------------------------------
constexpr const char* kCtxResult = "FF2.Result.Data";   // lastResultPtr (object identity)
bool g_announced_gil = false;
bool g_announced_skill_levels = false;
bool g_announced_spell_levels = false;
std::set<const void*> g_announced_status_ups;   // character-result identities (never dereferenced)

/// ResetTracking: a different result object resets every phase guard.
void reset_tracking(void* data)
{
    if (!dedup::object(kCtxResult, data, dedup::Scope::Session)) return;
    g_announced_gil = false;
    g_announced_skill_levels = false;
    g_announced_spell_levels = false;
    g_announced_status_ups.clear();
}

std::wstring n0(long long v)
{
    std::wstring digits = std::to_wstring(v < 0 ? -v : v);
    std::wstring out;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i > 0 && (digits.size() - i) % 3 == 0) out.push_back(L',');
        out.push_back(digits[i]);
    }
    return v < 0 ? L"-" + out : out;
}

std::vector<void*> list_items(void* list)
{
    std::vector<void*> out;
    for (int i = 0, n = unity::list_count(list); i < n; ++i) out.push_back(unity::list_at(list, i));
    return out;
}

int exp_call(ref::Method& m, int exp)
{
    return ref::call_static<int32_t>(m, static_cast<int32_t>(1), static_cast<int32_t>(exp), static_cast<int32_t>(kLevelExp));
}

// --- gil -------------------------------------------------------------------------------------
void announce_gil(void* data)
{
    if (g_announced_gil) return;
    g_announced_gil = true;
    const int gil = ref::rd<int32_t>(data, fGetGil);
    if (gil > 0) ffpr::say(TF(L"Gained {0} gil", {n0(gil)}), true);
}

// --- weapon skills ---------------------------------------------------------------------------
std::wstring weapon_skill_name(int target)
{
    switch (target) {
    case 0: return T(L"Sword");
    case 1: return T(L"Knife");
    case 2: return T(L"Spear");
    case 3: return T(L"Axe");
    case 4: return T(L"Staff");
    case 5: return T(L"Bow");
    case 6: return T(L"Shield");
    case 7: return T(L"Unarmed");
    case 8: return T(L"Evasion");
    case 9: return T(L"Magic Defense");
    case 10: return L"RightArm";   // the C# fell back to the enum name
    case 11: return L"LeftArm";
    default: return text::num(target);
    }
}

/// CalculatePercentInLevel: 1 - expToNext / expDifference, as the game's gauge; exp % 100 on failure.
int percent_in_level(int exp)
{
    bool ok = false;
    int pct = hooks::guarded_r("FF2.Result.Percent", 0, [&] {
        const int to_next = exp_call(mGetNextExp, exp);
        const int diff = exp_call(mGetExpDifference, exp);
        if (diff <= 0) return 0;
        ok = true;
        int p = static_cast<int>((1.0f - static_cast<float>(to_next) / static_cast<float>(diff)) * 100);
        return p < 0 ? 0 : (p > 99 ? 99 : p);
    });
    return ok ? pct : exp % 100;
}

struct SkillExp {
    int target;
    int exp;
};

std::vector<SkillExp> skill_exps(void* character)
{
    std::vector<SkillExp> out;
    void* dict = character ? ref::call<void*>(mCharSkillTargets, character) : nullptr;
    if (!dict) return out;
    unity::dict_for_each(dict, [](uint64_t k, uint64_t v, void* st) {
        static_cast<std::vector<SkillExp>*>(st)->push_back({static_cast<int32_t>(k), static_cast<int32_t>(v)});
    }, &out, 4, 4);
    return out;
}

int skill_level(void* character, int target)
{
    return ref::call_static<int32_t>(mGetSkillLevel, character, static_cast<int32_t>(target));
}

/// BuildWeaponSkillLine: "Firion: Sword lv3, Shield +12 percent", or empty.
std::wstring weapon_skill_line(void* char_result)
{
    void* after = ref::rd<void*>(char_result, fAfterData);
    void* before = ref::rd<void*>(char_result, fBeforData);
    if (!after) return {};
    const std::wstring name = ref::call_str(mCharName, after);
    const std::vector<SkillExp> after_skills = skill_exps(after);
    if (name.empty() || after_skills.empty()) return {};
    const std::vector<SkillExp> before_skills = skill_exps(before);

    std::vector<std::wstring> parts;
    for (const SkillExp& s : after_skills) {
        hooks::guarded("FF2.Result.WeaponSkill", [&] {
            if (s.target == kPhysicalAvoidance || s.target == kAbilityAvoidance) return;
            int before_exp = 0;
            for (const SkillExp& b : before_skills)
                if (b.target == s.target) before_exp = b.exp;
            if (s.exp <= before_exp) return;
            const std::wstring skill = weapon_skill_name(s.target);
            const int before_level = before ? skill_level(before, s.target) : 1;
            const int after_level = skill_level(after, s.target);
            if (after_level > before_level) {
                parts.push_back(TF(L"{0} lv{1}", {skill, text::num(after_level)}));
            } else {
                const int delta = percent_in_level(s.exp) - percent_in_level(before_exp);
                if (delta > 0) parts.push_back(TF(L"{0} +{1} percent", {skill, text::num(delta)}));
            }
        });
    }
    return parts.empty() ? std::wstring() : name + L": " + text::join(parts, L", ");
}

// --- spell level-ups -------------------------------------------------------------------------
int spell_level(int raw_exp)
{
    int level = exp_call(mGetExpLevel, raw_exp);
    return level < 1 ? 1 : (level > 16 ? 16 : level);
}

int ability_id(void* owned_ability)
{
    void* ability = owned_ability ? ref::call<void*>(mOwnedAbilityAbility, owned_ability) : nullptr;
    return ability ? ref::call<int32_t>(mAbilityId, ability) : -1;
}

/// BuildSpellLevelLine: "Maria: Fire lv3, Cure lv2", or empty.
std::wstring spell_level_line(void* char_result)
{
    void* after = ref::rd<void*>(char_result, fAfterData);
    void* after_list = after ? ref::call<void*>(mCharAbilityList, after) : nullptr;
    if (!after_list) return {};
    const std::wstring name = ref::call_str(mCharName, after);
    if (name.empty()) return {};
    void* before = ref::rd<void*>(char_result, fBeforData);
    void* before_list = before ? ref::call<void*>(mCharAbilityList, before) : nullptr;

    std::vector<std::wstring> parts;
    for (void* a : list_items(after_list)) {
        hooks::guarded("FF2.Result.SpellLevel", [&] {
            const int id = ability_id(a);
            if (id < 0) return;
            int before_exp = 0;
            for (void* b : list_items(before_list)) {
                if (b && ability_id(b) == id) {
                    before_exp = ref::call<int32_t>(mOwnedAbilitySkillLevel, b);
                    break;
                }
            }
            const int before_level = spell_level(before_exp);
            const int after_level = spell_level(ref::call<int32_t>(mOwnedAbilitySkillLevel, a));
            if (after_level <= before_level) return;
            const std::wstring spell = menus::spell_name(a);
            if (!spell.empty()) parts.push_back(TF(L"{0} lv{1}", {spell, text::num(after_level)}));
        });
    }
    return parts.empty() ? std::wstring() : name + L": " + text::join(parts, L", ");
}

// --- stat gains ------------------------------------------------------------------------------
/// CheckStatChange: "Strength +1", or "HP -3" (FF2 stats can drop).
void stat_change(std::vector<std::wstring>& changes, const std::wstring& stat, ref::Method& m, void* before, void* after)
{
    const int delta = ref::call<int32_t>(m, after) - ref::call<int32_t>(m, before);
    if (delta > 0) changes.push_back(stat + L" +" + text::num(delta));
    else if (delta < 0) changes.push_back(stat + L" " + text::num(delta));
}

/// BuildStatGainLine: "Firion: HP +5, Strength +1", or empty.
std::wstring stat_gain_line(void* char_result)
{
    void* before = ref::rd<void*>(char_result, fBeforData);
    void* after = ref::rd<void*>(char_result, fAfterData);
    void* bp = before ? ref::call<void*>(mCharParameter, before) : nullptr;
    void* ap = after ? ref::call<void*>(mCharParameter, after) : nullptr;
    if (!bp || !ap || !kPlayerParam.is(bp) || !kPlayerParam.is(ap)) return {};
    const std::wstring name = ref::call_str(mCharName, after);
    if (name.empty()) return {};

    std::vector<std::wstring> c;
    stat_change(c, T(L"HP"), mAddMaxHp, bp, ap);
    stat_change(c, T(L"MP"), mAddMaxMp, bp, ap);
    stat_change(c, T(L"Strength"), mAddPower, bp, ap);
    stat_change(c, T(L"Vitality"), mAddVitality, bp, ap);
    stat_change(c, T(L"Agility"), mAddAgility, bp, ap);
    stat_change(c, T(L"Intelligence"), mAddIntelligence, bp, ap);
    stat_change(c, T(L"Spirit"), mAddSpirit, bp, ap);
    stat_change(c, T(L"Accuracy"), mAddAccuracyRate, bp, ap);
    stat_change(c, T(L"Defense"), mAddDefense, bp, ap);
    // Evasion / magic defense show "Nx Y%" on the victory screen: count and rate both.
    stat_change(c, T(L"Evasion Count"), mAddEvasionCount, bp, ap);
    stat_change(c, T(L"Evasion"), mAddEvasionRate, bp, ap);
    stat_change(c, T(L"Magic Defense Count"), mAddMagicDefenseCount, bp, ap);
    stat_change(c, T(L"Magic Defense"), mAddAbilityDefenseRate, bp, ap);
    stat_change(c, T(L"Magic Evasion"), mAddAbilityEvasionRate, bp, ap);
    return c.empty() ? std::wstring() : name + L": " + text::join(c, L", ");
}

// --- detours ---------------------------------------------------------------------------------
using Show_t = void (*)(void* self, void* data, bool is_reverse, const void* mi);
using Points_t = void (*)(void* self, void* data, bool is_next, void* on_complete, const void* mi);
using Ptr_t = void (*)(void* self, void* arg, const void* mi);

Show_t o_ResultShow;
Points_t o_ShowPointList, o_ShowSkillLevels;
Ptr_t o_StatusUpSetData, o_SetLevelUpList;

void d_ResultShow(void* self, void* data, bool is_reverse, const void* mi)
{
    o_ResultShow(self, data, is_reverse, mi);
    hooks::guarded("FF2.Result.Show", [&] {
        if (!data) return;
        // The battle is over (isReverse is the back-attack layout flag, not a close).
        if (actions::defined("BattleResultPatches.ClearAllBattleMenuFlags"))
            actions::run("BattleResultPatches.ClearAllBattleMenuFlags");
        reset_tracking(data);
    });
}

void d_ShowPointList(void* self, void* data, bool is_next, void* on_complete, const void* mi)
{
    o_ShowPointList(self, data, is_next, on_complete, mi);
    hooks::guarded("FF2.Result.ShowPointList", [&] {
        if (!data) return;
        reset_tracking(data);
        announce_gil(data);
    });
}

void d_ShowSkillLevels(void* self, void* data, bool is_next, void* on_complete, const void* mi)
{
    o_ShowSkillLevels(self, data, is_next, on_complete, mi);
    hooks::guarded("FF2.Result.ShowSkillLevels", [&] {
        if (!data) return;
        reset_tracking(data);
        hooks::guarded("FF2.Result.Gil", [&] { announce_gil(data); });
        if (g_announced_skill_levels) return;
        g_announced_skill_levels = true;
        for (void* char_result : list_items(ref::rd<void*>(data, fCharacterList))) {
            if (!char_result) continue;
            std::wstring line = weapon_skill_line(char_result);
            if (!line.empty()) ffpr::say(line, false);
        }
    });
}

void d_StatusUpSetData(void* self, void* char_result, const void* mi)
{
    o_StatusUpSetData(self, char_result, mi);
    hooks::guarded("FF2.Result.StatusUpSetData", [&] {
        if (!char_result) return;
        if (!g_announced_status_ups.insert(char_result).second) return;
        std::wstring line = stat_gain_line(char_result);
        if (!line.empty()) ffpr::say(line, false);
    });
}

void d_SetLevelUpList(void* self, void* list, const void* mi)
{
    o_SetLevelUpList(self, list, mi);
    hooks::guarded("FF2.Result.SetLevelUpList", [&] {
        if (!list) return;
        if (g_announced_spell_levels) return;
        g_announced_spell_levels = true;
        for (void* char_result : list_items(list)) {
            if (!char_result) continue;
            std::wstring line = spell_level_line(char_result);
            if (!line.empty()) ffpr::say(line, false);
        }
    });
}

int install()
{
    // BattleResultPatches.ResetForNewBattle (BattleMessagePatches' battle-start hook).
    events::on(events::Kind::BattleStart, [] { dedup::reset(kCtxResult); });

    int n = 0;
    n += hooks::attach(mResultShow, &d_ResultShow, &o_ResultShow) ? 1 : 0;
    n += hooks::attach(mShowPointList, &d_ShowPointList, &o_ShowPointList) ? 1 : 0;
    n += hooks::attach(mShowSkillLevels, &d_ShowSkillLevels, &o_ShowSkillLevels) ? 1 : 0;
    n += hooks::attach(mStatusUpSetData, &d_StatusUpSetData, &o_StatusUpSetData) ? 1 : 0;
    n += hooks::attach(mSetLevelUpList, &d_SetLevelUpList, &o_SetLevelUpList) ? 1 : 0;
    LOGF("[FF2 BattleResult] %d of 5 hooks queued", n);
    return n;
}

core::Install s_install{"FF2.BattleResult", &install, 530, game::kFF2};

} // namespace
} // namespace ffpr::ff2
