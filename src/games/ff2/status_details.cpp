// Ports FF2 Menus/StatusDetailsReader.cs (StatusDetailsReader, StatusNavigationReader and FF2's
// 23-stat list) and the StatusDetailsPatches class of Patches/StatusMenuPatches.cs.
//
// The core's status details screen is FF1's (Serial.FF1 StatusDetailsController, FF1's stat list).
// FF2's is Serial.FF2.UI.KeyInput.StatusDetailsController with FF2's stats: Name | HP, MP |
// Strength, Spirit, Intellect, Stamina, Agility, Magic | Attack, Accuracy, Defense, Evasion,
// Magic Defense, Magic Interference | the 8 weapon skill levels. FF2 has no level or EXP.
//
// This file fills the core's StatusNavigationTracker (status::tracker(), so the core's Status key
// context and its "StatusNavigationTracker.*" queries work unchanged) and replaces the
// "StatusNavigationReader.*" actions with FF2's list. Weapon skills and the accuracy hit count are
// read from the screen once, when it opens (the game's own numbers), as the C# did.
//
// Like the core's FF1 screen, opening it sets STATUS_DETAILS (the generic reader stays silent);
// closing it also resets the status menu state, as FF2's ExitDisplay did.
//
// Folded body: StatusDetailsController.ExitDisplay shares its RVA with 10 other methods in FF2;
// the detour class-checks self (the C# postfix ran for all 11 while navigation was active).
// InitDisplay is unique.
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "../../core/core.h"
#include "../../core/frame.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/status/status.h"
#include "../../core/system/system.h"

namespace ffpr::ff2 {
namespace {

// --- game names ------------------------------------------------------------------------------
ref::Class kDetails{"Serial.FF2.UI.KeyInput", "StatusDetailsController", game::kFF2};
ref::Field fDetailsStatusController{kDetails, "statusController"};
ref::Field fDetailsSkillList{kDetails, "skillLevelContentList"};
ref::Method mInitDisplay{kDetails, "InitDisplay", 0};
ref::Method mExitDisplay{kDetails, "ExitDisplay", 0};
ref::Class kDetailsBase{"Serial.Template.UI.KeyInput", "StatusDetailsControllerBase", game::kFF2};
ref::Field fDetailsContentList{kDetailsBase, "contentList"};

ref::Class kCharaStatus{"Serial.FF2.UI.KeyInput", "AbilityCharaStatusController", game::kFF2};
ref::Field fCharaStatusTarget{kCharaStatus, "targetData"};

ref::Class kSkillContent{"Serial.FF2.UI", "SkillLevelContentController", game::kFF2};
ref::Field fSkillView{kSkillContent, "view"};
ref::Field fSkillWeaponType{kSkillContent, "weaponType"};
ref::Class kSkillView{"Serial.FF2.UI", "SkillLevelContentView", game::kFF2};
ref::Field fSkillLevelText{kSkillView, "levelText"};
ref::Field fSkillGauge{kSkillView, "gauge"};
ref::Class kCommonGauge{"Last.UI", "CommonGauge", game::kFF2};
ref::Field fGaugeImage{kCommonGauge, "gaugeImage"};
ref::Class kImage{"UnityEngine.UI", "Image", game::kFF2};
ref::Method mImageFillAmount{kImage, "get_fillAmount", 0};

ref::Class kParamContent{"Last.UI.KeyInput", "ParameterContentController", game::kFF2};
ref::Field fParamContentType{kParamContent, "type"};
ref::Field fParamContentView{kParamContent, "view"};
ref::Class kParamView{"Last.UI.KeyInput", "ParameterContentView", game::kFF2};
ref::Field fParamMultipliedValueText{kParamView, "multipliedValueText"};

ref::Class kOwnedCharacter{"Last.Data.User", "OwnedCharacterData", game::kFF2};
ref::Method mCharName{kOwnedCharacter, "get_Name", 0};
ref::Method mCharParameter{kOwnedCharacter, "get_Parameter", 0};
ref::Class kParamBase{"Last.Data", "CharacterParameterBase", game::kFF2};
ref::Field fCurrentHP{kParamBase, "currentHP"};
ref::Field fCurrentMP{kParamBase, "currentMP"};
ref::Class kPlayerParam{"Last.Data", "PlayerCharacterParameter", game::kFF2};
ref::Method mMaxHp{kPlayerParam, "ConfirmedMaxHp", 0};
ref::Method mMaxMp{kPlayerParam, "ConfirmedMaxMp", 0};
ref::Method mPower{kPlayerParam, "ConfirmedPower", 0};
ref::Method mAgility{kPlayerParam, "ConfirmedAgility", 0};
ref::Method mVitality{kPlayerParam, "ConfirmedVitality", 0};
ref::Method mIntelligence{kPlayerParam, "ConfirmedIntelligence", 0};
ref::Method mSpirit{kPlayerParam, "ConfirmedSpirit", 0};
ref::Method mMagic{kPlayerParam, "ConfirmedMagic", 0};
ref::Method mAttack{kPlayerParam, "ConfirmedAttack", 0};
ref::Method mAccuracyRate{kPlayerParam, "ConfirmedAccuracyRate", 1};
ref::Method mDefense{kPlayerParam, "ConfirmedDefense", 0};
ref::Method mEvasionCount{kPlayerParam, "ConfirmedEvasionCount", 0};
ref::Method mEvasionRate{kPlayerParam, "ConfirmedEvasionRate", 1};
ref::Method mMagicDefenseCount{kPlayerParam, "ConfirmedMagicDefenseCount", 0};
ref::Method mAbilityDefense{kPlayerParam, "ConfirmedAbilityDefense", 0};
ref::Method mAbilityDisturbedRate{kPlayerParam, "ConfirmedAbilityDisturbedRate", 1};

constexpr int kParameterTypeAccuracyRate = 16;   // ParameterType.AccuracyRate

// --- StatusDetailsReader / UI cache -----------------------------------------------------------
void* g_character = nullptr;                          // currentCharacterData
std::map<int, std::pair<int, int>> g_weapon_skills;   // SkillLevelTarget -> (level, percent)
int g_accuracy_count = -1;

std::wstring na() { return T(L"N/A"); }

void* player_param(void* data)
{
    void* p = data ? ref::call<void*>(mCharParameter, data) : nullptr;
    return (p && kPlayerParam.is(p)) ? p : nullptr;
}

int pint(ref::Method& m, void* p) { return ref::call<int32_t>(m, p); }
int pint_b(ref::Method& m, void* p, bool b) { return ref::call<int32_t>(m, p, b); }

/// ReadStatusDetails: "Name. HP: a / b. MP: c / d" (FF2 has no level).
std::wstring read_status_details()
{
    if (!g_character) return T(L"No character data available");
    std::vector<std::wstring> parts;
    hooks::guarded("FF2.Status.Details", [&] {
        std::wstring name = ref::call_str(mCharName, g_character);
        if (!text::is_blank(name)) parts.push_back(name);
        if (void* p = player_param(g_character)) {
            parts.push_back(L"HP: " + text::num(ref::rd<int32_t>(p, fCurrentHP)) + L" / " + text::num(pint(mMaxHp, p)));
            parts.push_back(L"MP: " + text::num(ref::rd<int32_t>(p, fCurrentMP)) + L" / " + text::num(pint(mMaxMp, p)));
        }
    });
    return parts.empty() ? T(L"No data") : text::join(parts, L". ");
}

int parse_int(const std::wstring& s, int fallback)
{
    std::wstring t = text::trim(s);
    if (t.empty()) return fallback;
    int v = 0;
    for (wchar_t c : t) {
        if (c < L'0' || c > L'9') return fallback;
        v = v * 10 + (c - L'0');
    }
    return v;
}

/// CacheWeaponSkillsFromUI: each element keyed by its own weaponType (the list is not in enum
/// order); level from LevelText, percent from the gauge fill.
void cache_weapon_skills(void* controller)
{
    g_weapon_skills.clear();
    void* list = ref::rd<void*>(controller, fDetailsSkillList);
    for (int i = 0, n = unity::list_count(list); i < n && i < 8; ++i) {
        hooks::guarded("FF2.Status.WeaponSkill", [&] {
            void* skill = unity::list_at(list, i);
            void* view = skill ? ref::rd<void*>(skill, fSkillView) : nullptr;
            if (!view) return;
            const int level = parse_int(unity::ui_text(ref::rd<void*>(view, fSkillLevelText)), 1);
            int pct = -1;
            void* gauge = ref::rd<void*>(view, fSkillGauge);
            void* image = gauge ? ref::rd<void*>(gauge, fGaugeImage) : nullptr;
            if (image && mImageFillAmount) {
                pct = static_cast<int>(ref::call<float>(mImageFillAmount, image) * 100);
                pct = pct < 0 ? 0 : (pct > 99 ? 99 : pct);
            }
            const int type = ref::rd<int32_t>(skill, fSkillWeaponType);
            if (type < 0 || type > 9) return;
            g_weapon_skills[type] = {level, pct};
        });
    }
}

/// CacheAccuracyCountFromUI: the "12" of "12x 99%" (the count comes from the equipped weapons).
void cache_accuracy_count(void* controller)
{
    g_accuracy_count = -1;
    void* list = ref::rd<void*>(controller, fDetailsContentList);
    for (int i = 0, n = unity::list_count(list); i < n; ++i) {
        void* c = unity::list_at(list, i);
        if (!c || ref::rd<int32_t>(c, fParamContentType) != kParameterTypeAccuracyRate) continue;
        void* view = ref::rd<void*>(c, fParamContentView);
        if (!view) continue;
        const int v = parse_int(unity::ui_text(ref::rd<void*>(view, fParamMultipliedValueText)), -1);
        if (v >= 0) {
            g_accuracy_count = v;
            return;
        }
    }
}

void invalidate_ui_cache()
{
    g_weapon_skills.clear();
    g_accuracy_count = -1;
}

// --- stat readers ----------------------------------------------------------------------------
std::wstring labeled(const wchar_t* key, const std::wstring& value) { return T(key) + L": " + value; }
std::wstring percent(int v) { return TF(L"{0} percent", {text::num(v)}); }

std::wstring read_name(void* d)
{
    std::wstring name = d ? ref::call_str(mCharName, d) : std::wstring();
    return text::is_blank(name) ? na() : labeled(L"Name", name);
}

std::wstring read_hp(void* d)
{
    void* p = player_param(d);
    if (!p) return na();
    return labeled(L"HP", text::num(ref::rd<int32_t>(p, fCurrentHP)) + L" / " + text::num(pint(mMaxHp, p)));
}

std::wstring read_mp(void* d)
{
    void* p = player_param(d);
    if (!p) return na();
    return labeled(L"MP", text::num(ref::rd<int32_t>(p, fCurrentMP)) + L" / " + text::num(pint(mMaxMp, p)));
}

std::wstring simple(void* d, const wchar_t* key, ref::Method& m)
{
    void* p = player_param(d);
    return p ? labeled(key, text::num(pint(m, p))) : na();
}

std::wstring read_strength(void* d) { return simple(d, L"Strength", mPower); }
std::wstring read_spirit(void* d) { return simple(d, L"Spirit", mSpirit); }
std::wstring read_intellect(void* d) { return simple(d, L"Intellect", mIntelligence); }
std::wstring read_stamina(void* d) { return simple(d, L"Stamina", mVitality); }
std::wstring read_agility(void* d) { return simple(d, L"Agility", mAgility); }
std::wstring read_magic(void* d) { return simple(d, L"Magic", mMagic); }
std::wstring read_attack(void* d) { return simple(d, L"Attack", mAttack); }
std::wstring read_defense(void* d) { return simple(d, L"Defense", mDefense); }

std::wstring read_accuracy(void* d)
{
    void* p = player_param(d);
    if (!p) return labeled(L"Accuracy", na());
    const int rate = pint_b(mAccuracyRate, p, false);
    if (g_accuracy_count > 0) return labeled(L"Accuracy", text::num(g_accuracy_count) + L"x " + percent(rate));
    return labeled(L"Accuracy", percent(rate));
}

std::wstring read_evasion(void* d)
{
    void* p = player_param(d);
    if (!p) return labeled(L"Evasion", na());
    return labeled(L"Evasion", text::num(pint(mEvasionCount, p)) + L"x " + percent(pint_b(mEvasionRate, p, false)));
}

std::wstring read_magic_defense(void* d)
{
    void* p = player_param(d);
    if (!p) return labeled(L"Magic Defense", na());
    // ConfirmedAbilityDefense returns the rate (the game's naming).
    return labeled(L"Magic Defense", text::num(pint(mMagicDefenseCount, p)) + L"x " + percent(pint(mAbilityDefense, p)));
}

std::wstring read_magic_interference(void* d)
{
    void* p = player_param(d);
    return p ? labeled(L"Magic Interference", text::num(pint_b(mAbilityDisturbedRate, p, true))) : na();
}

/// ReadWeaponSkill: from the UI cache only (no computed fallback: wrong data is worse than none).
std::wstring read_weapon_skill(void* d, int type, const wchar_t* key)
{
    const std::wstring name = T(key);
    if (!d) return name + L": " + na();
    auto it = g_weapon_skills.find(type);
    if (it == g_weapon_skills.end() || it->second.first <= 0) return name + L": " + na();
    std::wstring s = name + L": " + TF(L"Level {0}", {text::num(it->second.first)});
    if (it->second.second >= 0) s += L", " + percent(it->second.second);
    return s;
}

std::wstring read_sword(void* d) { return read_weapon_skill(d, 0, L"Sword"); }
std::wstring read_knife(void* d) { return read_weapon_skill(d, 1, L"Knife"); }
std::wstring read_spear(void* d) { return read_weapon_skill(d, 2, L"Spear"); }
std::wstring read_axe(void* d) { return read_weapon_skill(d, 3, L"Axe"); }
std::wstring read_staff(void* d) { return read_weapon_skill(d, 4, L"Staff"); }
std::wstring read_bow(void* d) { return read_weapon_skill(d, 5, L"Bow"); }
std::wstring read_shield(void* d) { return read_weapon_skill(d, 6, L"Shield"); }
std::wstring read_unarmed(void* d) { return read_weapon_skill(d, 7, L"Unarmed"); }

using Reader = std::wstring (*)(void*);
// InitializeStatList: screen order.
const Reader kStats[] = {
    &read_name,                                                                   // 0 character
    &read_hp, &read_mp,                                                           // 1 vitals
    &read_strength, &read_spirit, &read_intellect, &read_stamina, &read_agility, &read_magic,  // 3 attributes
    &read_attack, &read_accuracy, &read_defense, &read_evasion, &read_magic_defense, &read_magic_interference,  // 9 combat
    &read_sword, &read_knife, &read_spear, &read_axe, &read_staff, &read_bow, &read_shield, &read_unarmed,  // 15 skills
};
constexpr int kStatCount = static_cast<int>(sizeof(kStats) / sizeof(kStats[0]));
const int kGroupStarts[] = {0, 1, 3, 9, 15};
constexpr int kGroupCount = 5;

// --- StatusNavigationReader ------------------------------------------------------------------
void read_stat_at(int index)
{
    status::Tracker& t = status::tracker();
    if (index < 0 || index >= kStatCount) return;
    if (!t.current_character_data) {
        ffpr::say(T(L"No character data"));
        return;
    }
    std::wstring value;
    bool ok = hooks::guarded_r("FF2.Status.ReadStat", false, [&] {
        value = kStats[index](t.current_character_data);
        return true;
    });
    if (!ok) {
        LOGF("Error reading stat at index %d", index);
        ffpr::say(T(L"Error reading stat"));
        return;
    }
    // Position within the stat's group (FF1 parity).
    int start = 0, end = kStatCount;
    for (int i = 0; i < kGroupCount && kGroupStarts[i] <= index; ++i) {
        start = kGroupStarts[i];
        end = i + 1 < kGroupCount ? kGroupStarts[i + 1] : kStatCount;
    }
    ffpr::say(ui::menu_position(value, index - start, end - start), true);
}

void read_current_stat()
{
    status::Tracker& t = status::tracker();
    if (!t.validate_state()) {
        ffpr::say(T(L"Navigation not available"));
        return;
    }
    read_stat_at(t.current_stat_index);
}

void step(int (*next)(int))
{
    status::Tracker& t = status::tracker();
    if (!t.is_navigation_active) return;
    t.current_stat_index = next(t.current_stat_index);
    read_current_stat();
}

int next_stat(int i) { return (i + 1) % kStatCount; }
int prev_stat(int i) { return i - 1 < 0 ? kStatCount - 1 : i - 1; }
int top_stat(int) { return 0; }
int bottom_stat(int) { return kStatCount - 1; }
int next_group(int i)
{
    for (int g : kGroupStarts)
        if (g > i) return g;
    return kGroupStarts[0];
}
int prev_group(int i)
{
    for (int k = kGroupCount - 1; k >= 0; --k)
        if (kGroupStarts[k] < i) return kGroupStarts[k];
    return kGroupStarts[kGroupCount - 1];
}

// --- StatusDetailsPatches --------------------------------------------------------------------
/// GetCharacterDataFromController: statusController.targetData.
void* character_from_controller(void* controller)
{
    void* status = ref::rd<void*>(controller, fDetailsStatusController);
    return status ? ref::rd<void*>(status, fCharaStatusTarget) : nullptr;
}

/// DelayedStatusInit (one frame after InitDisplay).
void delayed_status_init(void* controller)
{
    if (!kDetails.is(controller) || !unity::alive(controller) || !unity::active_in_hierarchy(controller)) return;
    void* data = character_from_controller(controller);
    if (!data) return;

    status::Tracker& t = status::tracker();
    t.is_navigation_active = true;
    t.current_stat_index = 0;
    t.current_character_data = data;
    t.active_controller = controller;
    g_character = data;

    invalidate_ui_cache();
    hooks::guarded("FF2.Status.CacheSkills", [&] { cache_weapon_skills(controller); });
    hooks::guarded("FF2.Status.CacheAccuracy", [&] { cache_accuracy_count(controller); });

    std::wstring info = read_status_details();
    if (!info.empty()) ffpr::say(info, true);
}

using Void_t = void (*)(void* self, const void* mi);
Void_t o_InitDisplay, o_ExitDisplay;

void d_InitDisplay(void* self, const void* mi)
{
    o_InitDisplay(self, mi);
    hooks::guarded("FF2.StatusDetails.InitDisplay", [&] {
        if (!kDetails.is(self)) return;
        // As the core's FF1 screen: status details owns the cursor while it is open.
        system::clear_other_menu_states("StatusDetails");
        menu_state::set_active(menu_state::STATUS_DETAILS, true);
        void* controller = self;
        frame::next_frame([controller] {
            hooks::guarded("FF2.StatusDetails.DelayedInit", [&] { delayed_status_init(controller); });
        }, "FF2.StatusDetails.Init");
    });
}

void d_ExitDisplay(void* self, const void* mi)
{
    o_ExitDisplay(self, mi);
    hooks::guarded("FF2.StatusDetails.ExitDisplay", [&] {
        if (!kDetails.is(self)) return;   // folded body: 10 other methods share it
        status::Tracker& t = status::tracker();
        if (!t.is_navigation_active) return;
        g_character = nullptr;
        invalidate_ui_cache();
        t.reset();
        menu_state::reset(menu_state::STATUS_DETAILS);
        if (actions::defined("StatusMenuState.ResetState")) actions::run("StatusMenuState.ResetState");
    });
}

int install()
{
    // FF2's stat list replaces FF1's behind the same action names (input binds the keys).
    actions::define("StatusNavigationReader.NavigateNext", [] { step(&next_stat); });
    actions::define("StatusNavigationReader.NavigatePrevious", [] { step(&prev_stat); });
    actions::define("StatusNavigationReader.JumpToNextGroup", [] { step(&next_group); });
    actions::define("StatusNavigationReader.JumpToPreviousGroup", [] { step(&prev_group); });
    actions::define("StatusNavigationReader.JumpToTop", [] { step(&top_stat); });
    actions::define("StatusNavigationReader.JumpToBottom", [] { step(&bottom_stat); });
    actions::define("StatusNavigationReader.ReadCurrentStat", [] { read_current_stat(); });

    int n = 0;
    n += hooks::attach(mInitDisplay, &d_InitDisplay, &o_InitDisplay) ? 1 : 0;
    n += hooks::attach(mExitDisplay, &d_ExitDisplay, &o_ExitDisplay) ? 1 : 0;
    LOGF("[FF2 StatusDetails] %d of 2 hooks queued", n);
    return n;
}

core::Install s_install{"FF2.StatusDetails", &install, 540, game::kFF2};

} // namespace
} // namespace ffpr::ff2
