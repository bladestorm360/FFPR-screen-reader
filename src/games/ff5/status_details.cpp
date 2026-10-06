// Ports FF5 Patches/StatusDetailsPatches.cs (StatusNavigationTracker, the InitDisplay / ExitDisplay /
// SetNextPlayer / SetPrevPlayer patches, StatusDetailsHelpers) and Menus/StatusDetailsReader.cs
// (ReadStatusDetails and FF5's StatusNavigationReader with its stat list).
//
// The core's status details screen and stat list are FF1's (Serial.FF1 controller, FF1 stats);
// FF5 has its own Serial.FF5.UI.KeyInput.StatusDetailsController. This file re-defines, for FF5,
// the actions and queries the core's Status key context drives:
//   actions  StatusNavigationReader.NavigateNext / NavigatePrevious / JumpToNextGroup /
//            JumpToPreviousGroup / JumpToTop / JumpToBottom / ReadCurrentStat
//   queries  StatusNavigationTracker.IsNavigationActive / ValidateState / IsActive
// (install order 520 runs after the core's 121, so these definitions replace the core's).
//
// Not ported, kept in the core: the status character list (StatusWindowController.SelectContent,
// FF1's reader in all five games). FF5's StatusMenuFocusPatches (an entry read of that list) is
// not ported: the core reader has no cursor-active gate, so the game's own SelectContent on entry
// already speaks there, and a second entry read would double it.
//
// Folded body: StatusDetailsController.ExitDisplay shares its RVA with 6 other methods in FF5
// (the C# postfix, which took no __instance, ran for all of them): the detour class-checks self.
// Parity: InitDisplay's "user opened the status menu" gate (StatusMenuTracker.IsUserOpened, set by
// FF5's own list reader, which the core replaces) is MenuManager.IsOpen here.
#include <cstdint>
#include <string>
#include <vector>
#include "ff5_internal.h"
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
#include "../../core/battle/battle_util.h"
#include "../../core/menus/menus.h"

namespace ffpr::ff5 {
namespace {

constexpr uint8_t k5 = game::kFF5;

ref::Class  kDetails{"Serial.FF5.UI.KeyInput", "StatusDetailsController", k5};
ref::Field  fDetailsView{kDetails, "view"};
ref::Field  fDetailsStatusController{kDetails, "statusController"};
ref::Method mInitDisplay{kDetails, "InitDisplay", 0};
ref::Method mExitDisplay{kDetails, "ExitDisplay", 0};
ref::Class  kDetailsBase{"Serial.Template.UI", "StatusDetailsControllerBase", k5};
ref::Method mSetNextPlayer{kDetailsBase, "SetNextPlayer", 0};
ref::Method mSetPrevPlayer{kDetailsBase, "SetPrevPlayer", 0};

ref::Class  kDetailsView{"Serial.FF5.UI.KeyInput", "StatusDetailsView", k5};
ref::Field  fFixedBattleCommandText{kDetailsView, "fixedBattleCommandText"};
ref::Field  fEquipAbilityContentList{kDetailsView, "equipAbilityContentList"};
ref::Field  fExpText{kDetailsView, "expText"};
ref::Field  fNextExpText{kDetailsView, "nextExpText"};
ref::Field  fJobLevelText{kDetailsView, "jobLevelText"};
ref::Field  fCurrentAbpText{kDetailsView, "currentAbpText"};
ref::Field  fMaxAbpText{kDetailsView, "maxAbpText"};
ref::Field  fCurrentJobCountText{kDetailsView, "currentJobCountText"};
ref::Field  fMaxJobCountText{kDetailsView, "maxJobCountText"};
ref::Field  fCurrentAbilityCountText{kDetailsView, "currentAbilityCountText"};
ref::Field  fMaxAbilityCountText{kDetailsView, "maxAbilityCountText"};

ref::Class  kCharaStatus{"Serial.FF5.UI.KeyInput", "AbilityCharaStatusController", k5};
ref::Field  fCharaStatusView{kCharaStatus, "view"};
ref::Field  fCharaStatusTargetData{kCharaStatus, "targetData"};
ref::Class  kCharaStatusView{"Serial.FF5.UI.KeyInput", "AbilityCharaStatusView", k5};
ref::Field  fCsvJobNameText{kCharaStatusView, "jobNameText"};
ref::Field  fCsvNameText{kCharaStatusView, "nameText"};
ref::Field  fCsvCurrentHpText{kCharaStatusView, "currentHpText"};
ref::Field  fCsvMaxHpText{kCharaStatusView, "maxHpText"};
ref::Field  fCsvCurrentMpText{kCharaStatusView, "currentMpText"};
ref::Field  fCsvMaxMpText{kCharaStatusView, "maxMpText"};
ref::Field  fCsvCurrentLevelText{kCharaStatusView, "currentLevelText"};

ref::Class  kChangeContent{"Serial.FF5.UI.KeyInput", "AbilityChangeContentController", k5};
ref::Field  fChangeContentView{kChangeContent, "view"};
ref::Class  kChangeContentBase{"Serial.Template.UI", "AbilityChangeContentBaseController", k5};
ref::Field  fChangeContentTargetData{kChangeContentBase, "targetData"};
ref::Class  kChangeContentView{"Serial.FF5.UI.KeyInput", "AbilityChangeContentView", k5};
ref::Field  fChangeContentIconText{kChangeContentView, "iconText"};
ref::Class  kIconTextView{"Last.UI", "IconTextView", k5};
ref::Field  fIconNameText{kIconTextView, "nameText"};

ref::Class  kOwnedChar{"Last.Data.User", "OwnedCharacterData", k5};
ref::Method mCharJobId{kOwnedChar, "get_JobId", 0};
ref::Method mCharCurrentExp{kOwnedChar, "get_CurrentExp", 0};
ref::Method mCharNextExp{kOwnedChar, "GetNextExp", 0};
ref::Method mCharOwnedJob{kOwnedChar, "get_OwnedJob", 0};
ref::Field  fCharOwnedAbilityList{kOwnedChar, "<OwnedAbilityList>k__BackingField"};
ref::Class  kOwnedJobData{"Last.Data.User", "OwnedJobData", k5};
ref::Method mJobCurrentProficiency{kOwnedJobData, "get_CurrentProficiency", 0};

ref::Class  kParam{"Last.Data", "CharacterParameterBase", k5};
ref::Field  fParamCurrentHP{kParam, "currentHP"};
ref::Field  fParamCurrentMP{kParam, "currentMP"};
ref::Method mConfirmedLevel{kParam, "ConfirmedLevel", 0};
ref::Method mConfirmedMaxHp{kParam, "ConfirmedMaxHp", 0};
ref::Method mConfirmedMaxMp{kParam, "ConfirmedMaxMp", 0};
ref::Method mConfirmedPower{kParam, "ConfirmedPower", 0};
ref::Method mConfirmedVitality{kParam, "ConfirmedVitality", 0};
ref::Method mConfirmedAgility{kParam, "ConfirmedAgility", 0};
ref::Method mConfirmedAttack{kParam, "ConfirmedAttack", 0};
ref::Method mConfirmedDefense{kParam, "ConfirmedDefense", 0};
ref::Method mConfirmedAbilityDefense{kParam, "ConfirmedAbilityDefense", 0};
ref::Method mConfirmedMagic{kParam, "ConfirmedMagic", 0};
ref::Method mConfirmedEvasionRate{kParam, "ConfirmedEvasionRate", 1};

// --- StatusNavigationTracker -------------------------------------------------------------------
struct Tracker {
    bool active = false;
    int index = 0;
    void* character = nullptr;   // OwnedCharacterData (party data)
    void* controller = nullptr;  // re-checked with unity::alive on use
    void reset() { *this = Tracker{}; }
    bool validate() const
    {
        return active && character && controller && unity::alive(controller) && unity::active_in_hierarchy(controller);
    }
} g_tracker;

// --- stat list -------------------------------------------------------------------------------
enum class Group { CharacterInfo, Vitals, Attributes, CombatStats, Progression, Commands };

struct Stat {
    Group group;
    std::wstring (*reader)(void* data);
    std::wstring captured;   // Commands rows: the text the screen shows
};
std::vector<Stat> g_stats;
std::vector<int> g_group_starts{0};
std::wstring g_commands_label;

std::wstring na() { return T(L"N/A"); }
std::wstring labeled(const wchar_t* key, const std::wstring& v) { return T(key) + L": " + v; }

void* details_view() { return g_tracker.controller ? ref::rd<void*>(g_tracker.controller, fDetailsView) : nullptr; }
void* chara_status_view()
{
    void* sc = g_tracker.controller ? ref::rd<void*>(g_tracker.controller, fDetailsStatusController) : nullptr;
    return sc ? ref::rd<void*>(sc, fCharaStatusView) : nullptr;
}
void* param_of(void* data) { return data ? battle::character_parameter(data) : nullptr; }

/// `$"{T(key)}: {param.ConfirmedX()}"`, T("N/A") on a missing parameter or a failed call.
std::wstring param_stat(void* data, const wchar_t* key, ref::Method& m)
{
    void* p = param_of(data);
    int v = 0;
    if (!p || !battle::vcall(m, p, v)) return na();
    return labeled(key, text::num(v));
}

/// A details-view Text, or empty.
std::wstring dv_text(ref::Field& f)
{
    void* v = details_view();
    return v ? text_of(ref::rd<void*>(v, f)) : std::wstring();
}

std::wstring read_job_name(void* data)
{
    if (!data) return na();
    void* sv = chara_status_view();
    std::wstring job = sv ? text_of(ref::rd<void*>(sv, fCsvJobNameText)) : std::wstring();
    if (!blank(job)) return labeled(L"Job", job);
    return labeled(L"Job", text::num(ref::call<int32_t>(mCharJobId, data)));
}
std::wstring read_character_level(void* data) { return param_stat(data, L"Level", mConfirmedLevel); }
std::wstring read_job_level(void*)
{
    std::wstring level = dv_text(fJobLevelText);
    return blank(level) ? na() : labeled(L"Job Level", level);
}
std::wstring read_exp(void* data)
{
    if (!data) return na();
    std::wstring s = dv_text(fExpText);
    return labeled(L"EXP", !blank(s) ? s : text::num(ref::call<int32_t>(mCharCurrentExp, data)));
}
std::wstring read_next_exp(void* data)
{
    if (!data) return na();
    std::wstring s = dv_text(fNextExpText);
    return labeled(L"Next", !blank(s) ? s : text::num(ref::call<int32_t>(mCharNextExp, data)));
}
std::wstring read_abp(void* data)
{
    void* job = data ? ref::call<void*>(mCharOwnedJob, data) : nullptr;
    if (!job) return na();
    std::wstring cur = dv_text(fCurrentAbpText), max = dv_text(fMaxAbpText);
    if (!blank(cur) && !blank(max)) return labeled(L"ABP", cur + L" / " + max);
    return labeled(L"ABP", text::num(ref::call<int32_t>(mJobCurrentProficiency, job)));
}
std::wstring read_hp(void* data)
{
    void* p = param_of(data);
    int max = 0;
    if (!p || !battle::vcall(mConfirmedMaxHp, p, max)) return na();
    return labeled(L"HP", text::num(ref::rd<int32_t>(p, fParamCurrentHP)) + L" / " + text::num(max));
}
std::wstring read_mp(void* data)
{
    void* p = param_of(data);
    int max = 0;
    if (!p || !battle::vcall(mConfirmedMaxMp, p, max)) return na();
    return labeled(L"MP", text::num(ref::rd<int32_t>(p, fParamCurrentMP)) + L" / " + text::num(max));
}
std::wstring read_strength(void* data) { return param_stat(data, L"Strength", mConfirmedPower); }
std::wstring read_agility(void* data) { return param_stat(data, L"Agility", mConfirmedAgility); }
std::wstring read_stamina(void* data) { return param_stat(data, L"Stamina", mConfirmedVitality); }
std::wstring read_magic(void* data) { return param_stat(data, L"Magic", mConfirmedMagic); }
std::wstring read_attack(void* data) { return param_stat(data, L"Attack", mConfirmedAttack); }
std::wstring read_defense(void* data) { return param_stat(data, L"Defense", mConfirmedDefense); }
std::wstring read_magic_defense(void* data) { return param_stat(data, L"Magic Defense", mConfirmedAbilityDefense); }
std::wstring read_evasion(void* data)
{
    // The on-screen row is ParameterType.EvasionRate: ConfirmedEvasionRate(false), shown with "%".
    void* p = param_of(data);
    int v = 0;
    if (!p || !battle::vcall(mConfirmedEvasionRate, p, v, false)) return na();
    return labeled(L"Evasion", text::num(v) + L"%");
}
std::wstring read_jobs(void* data)
{
    if (!data) return na();
    std::wstring cur = dv_text(fCurrentJobCountText), max = dv_text(fMaxJobCountText);
    if (!blank(cur) && !blank(max)) return labeled(L"Jobs", cur + L" / " + max);
    return labeled(L"Jobs", na());
}
std::wstring read_abilities(void* data)
{
    void* list = data ? ref::rd<void*>(data, fCharOwnedAbilityList) : nullptr;
    if (!list) return na();
    std::wstring cur = dv_text(fCurrentAbilityCountText), max = dv_text(fMaxAbilityCountText);
    if (!blank(cur) && !blank(max)) return labeled(L"Abilities", cur + L" / " + max);
    return labeled(L"Abilities", text::num(unity::list_count(list)));
}

/// AppendCommandRows: one row per entry rendered in the Commands/Abilities panel.
void append_command_rows()
{
    hooks::guarded("FF5.Status.CommandRows", [] {
        void* view = details_view();
        if (!view) return;
        g_commands_label = text_of(ref::rd<void*>(view, fFixedBattleCommandText));
        void* list = ref::rd<void*>(view, fEquipAbilityContentList);
        int n = list ? unity::list_count(list) : 0;
        for (int i = 0; i < n; ++i) {
            void* content = unity::list_at(list, i);
            if (!content || !unity::active_in_hierarchy(content)) continue;
            if (!ref::rd<void*>(content, fChangeContentTargetData)) continue;
            void* cview = ref::rd<void*>(content, fChangeContentView);
            void* icon = cview ? ref::rd<void*>(cview, fChangeContentIconText) : nullptr;
            if (!icon) continue;
            std::wstring label = text_of(ref::rd<void*>(icon, fIconNameText));
            if (label.empty()) continue;
            label = battle::strip_icon(label);
            if (label.empty()) continue;
            g_stats.push_back(Stat{Group::Commands, nullptr, label});
        }
    });
}

void initialize_stat_list()
{
    g_stats.clear();
    g_stats.push_back({Group::CharacterInfo, &read_job_name, {}});
    g_stats.push_back({Group::CharacterInfo, &read_character_level, {}});
    g_stats.push_back({Group::CharacterInfo, &read_job_level, {}});
    g_stats.push_back({Group::CharacterInfo, &read_exp, {}});
    g_stats.push_back({Group::CharacterInfo, &read_next_exp, {}});
    g_stats.push_back({Group::CharacterInfo, &read_abp, {}});
    g_stats.push_back({Group::Vitals, &read_hp, {}});
    g_stats.push_back({Group::Vitals, &read_mp, {}});
    g_stats.push_back({Group::Attributes, &read_strength, {}});
    g_stats.push_back({Group::Attributes, &read_agility, {}});
    g_stats.push_back({Group::Attributes, &read_stamina, {}});
    g_stats.push_back({Group::Attributes, &read_magic, {}});
    g_stats.push_back({Group::CombatStats, &read_attack, {}});
    g_stats.push_back({Group::CombatStats, &read_defense, {}});
    g_stats.push_back({Group::CombatStats, &read_evasion, {}});
    g_stats.push_back({Group::CombatStats, &read_magic_defense, {}});
    g_stats.push_back({Group::Progression, &read_jobs, {}});
    g_stats.push_back({Group::Progression, &read_abilities, {}});
    append_command_rows();

    g_group_starts.clear();
    for (size_t i = 0; i < g_stats.size(); ++i)
        if (i == 0 || g_stats[i].group != g_stats[i - 1].group) g_group_starts.push_back(static_cast<int>(i));
    if (g_group_starts.empty()) g_group_starts.push_back(0);
}

std::wstring group_name(Group g)
{
    switch (g) {
    case Group::CharacterInfo: return T(L"Character Info");
    case Group::Vitals: return T(L"Vitals");
    case Group::Attributes: return T(L"Attributes");
    case Group::CombatStats: return T(L"Combat Stats");
    case Group::Progression: return T(L"Progression");
    case Group::Commands: return g_commands_label.empty() ? T(L"Commands") : g_commands_label;
    }
    return {};
}

std::wstring stat_value(const Stat& s, void* data) { return s.reader ? s.reader(data) : s.captured; }

/// ReadCurrentStat / ReadStatAtIndex; with_group = ReadCurrentStatWithGroup.
void read_current(bool with_group)
{
    if (!g_tracker.validate()) {
        ffpr::say(T(L"Navigation not available"));
        return;
    }
    if (g_stats.empty()) initialize_stat_list();
    const int index = g_tracker.index;
    if (index < 0 || index >= static_cast<int>(g_stats.size())) {
        LOGF("Invalid stat index: %d", index);
        return;
    }
    if (!g_tracker.character) {
        ffpr::say(T(L"No character data"));
        return;
    }
    bool ok = hooks::guarded_r("FF5.Status.ReadStat", false, [&] {
        const Stat& s = g_stats[static_cast<size_t>(index)];
        std::wstring value = stat_value(s, g_tracker.character);
        if (with_group) value = group_name(s.group) + L". " + value;
        ffpr::say(ui::menu_position(value, index, static_cast<int>(g_stats.size())), true);
        return true;
    });
    if (!ok) {
        LOGF("Error reading stat at index %d", index);
        ffpr::say(T(L"Error reading stat"));
    }
}

void navigate_next()
{
    if (g_stats.empty()) initialize_stat_list();
    if (!g_tracker.active || g_stats.empty()) return;
    g_tracker.index = (g_tracker.index + 1) % static_cast<int>(g_stats.size());
    read_current(false);
}
void navigate_previous()
{
    if (g_stats.empty()) initialize_stat_list();
    if (!g_tracker.active || g_stats.empty()) return;
    if (--g_tracker.index < 0) g_tracker.index = static_cast<int>(g_stats.size()) - 1;
    read_current(false);
}
void jump_next_group()
{
    if (g_stats.empty()) initialize_stat_list();
    if (!g_tracker.active) return;
    int next = -1;
    for (int start : g_group_starts)
        if (start > g_tracker.index) {
            next = start;
            break;
        }
    g_tracker.index = next == -1 ? g_group_starts.front() : next;
    read_current(true);
}
void jump_previous_group()
{
    if (g_stats.empty()) initialize_stat_list();
    if (!g_tracker.active) return;
    int prev = -1;
    for (auto it = g_group_starts.rbegin(); it != g_group_starts.rend(); ++it)
        if (*it < g_tracker.index) {
            prev = *it;
            break;
        }
    g_tracker.index = prev == -1 ? g_group_starts.back() : prev;
    read_current(true);
}
void jump_top()
{
    if (!g_tracker.active) return;
    g_tracker.index = 0;
    read_current(false);
}
void jump_bottom()
{
    if (g_stats.empty()) initialize_stat_list();
    if (!g_tracker.active) return;
    g_tracker.index = static_cast<int>(g_stats.size()) - 1;
    read_current(false);
}

// --- StatusDetailsReader.ReadStatusDetails ------------------------------------------------------
std::wstring read_status_details(void* controller)
{
    if (!controller) return {};
    void* sc = ref::rd<void*>(controller, fDetailsStatusController);
    void* sv = sc ? ref::rd<void*>(sc, fCharaStatusView) : nullptr;
    void* dv = ref::rd<void*>(controller, fDetailsView);
    if (!sv && !dv) return {};
    std::vector<std::wstring> parts;
    if (sv) {
        std::wstring name = text_of(ref::rd<void*>(sv, fCsvNameText));
        std::wstring level = text_of(ref::rd<void*>(sv, fCsvCurrentLevelText));
        if (!blank(name)) parts.push_back(name);
        if (!blank(level)) parts.push_back(T(L"Level") + L" " + level);
        std::wstring hp = text_of(ref::rd<void*>(sv, fCsvCurrentHpText)), max_hp = text_of(ref::rd<void*>(sv, fCsvMaxHpText));
        std::wstring mp = text_of(ref::rd<void*>(sv, fCsvCurrentMpText)), max_mp = text_of(ref::rd<void*>(sv, fCsvMaxMpText));
        if (!blank(hp) && !blank(max_hp)) parts.push_back(T(L"HP") + L": " + hp + L" / " + max_hp);
        if (!blank(mp) && !blank(max_mp)) parts.push_back(T(L"MP") + L": " + mp + L" / " + max_mp);
    }
    if (dv) {
        std::wstring exp = text_of(ref::rd<void*>(dv, fExpText)), next = text_of(ref::rd<void*>(dv, fNextExpText));
        if (!blank(exp)) parts.push_back(T(L"EXP") + L": " + exp);
        if (!blank(next)) parts.push_back(T(L"Next") + L": " + next);
    }
    return text::join(parts, L". ");
}

void* character_from_controller(void* controller)
{
    void* sc = controller ? ref::rd<void*>(controller, fDetailsStatusController) : nullptr;
    return sc ? ref::rd<void*>(sc, fCharaStatusTargetData) : nullptr;
}

/// StatusDetailsHelpers.ShowCharacter.
void show_character(void* controller, void* character)
{
    if (character) {
        g_tracker.active = true;
        g_tracker.index = 0;
        g_tracker.controller = controller;
        g_tracker.character = character;
        initialize_stat_list();
    } else {
        LOGF("[Status] Could not get character data for navigation");
    }
    std::wstring s = read_status_details(controller);
    if (!blank(s)) ffpr::say(s);
}

bool controller_shown(void* c) { return c && unity::alive(c) && kDetails.is(c) && unity::active_in_hierarchy(c); }

// --- detours -------------------------------------------------------------------------------------
using V_t = void (*)(void* self, const void* mi);

V_t o_InitDisplay;
void d_InitDisplay(void* self, const void* mi)
{
    o_InitDisplay(self, mi);
    hooks::guarded("FF5.StatusDetails.InitDisplay", [&] {
        if (!self || !kDetails.is(self)) return;
        frame::next_frame(
            [self] {
                if (!controller_shown(self)) return;
                if (!focus::menu_open()) return;   // InitDisplay also fires during game load
                show_character(self, character_from_controller(self));
            },
            "FF5.StatusDetails.Show");
    });
}

V_t o_ExitDisplay;
void d_ExitDisplay(void* self, const void* mi)
{
    o_ExitDisplay(self, mi);
    if (self && kDetails.is(self)) g_tracker.reset();   // folded body: 7 methods
}

void on_player_switch(void* self)
{
    if (!self || !kDetails.is(self) || !g_tracker.active) return;
    frame::next_frame(
        [self] {
            if (!controller_shown(self)) return;
            void* character = character_from_controller(self);
            if (!character || character == g_tracker.character) return;
            show_character(self, character);
        },
        "FF5.StatusDetails.Switch");
}

V_t o_SetNextPlayer;
void d_SetNextPlayer(void* self, const void* mi)
{
    o_SetNextPlayer(self, mi);
    hooks::guarded("FF5.StatusDetails.SetNextPlayer", [&] { on_player_switch(self); });
}

V_t o_SetPrevPlayer;
void d_SetPrevPlayer(void* self, const void* mi)
{
    o_SetPrevPlayer(self, mi);
    hooks::guarded("FF5.StatusDetails.SetPrevPlayer", [&] { on_player_switch(self); });
}

int install()
{
    int n = 0;
    n += hooks::attach(mInitDisplay, &d_InitDisplay, &o_InitDisplay) ? 1 : 0;
    n += hooks::attach(mExitDisplay, &d_ExitDisplay, &o_ExitDisplay) ? 1 : 0;
    n += hooks::attach(mSetNextPlayer, &d_SetNextPlayer, &o_SetNextPlayer) ? 1 : 0;
    n += hooks::attach(mSetPrevPlayer, &d_SetPrevPlayer, &o_SetPrevPlayer) ? 1 : 0;

    actions::define("StatusNavigationReader.NavigateNext", [] { navigate_next(); });
    actions::define("StatusNavigationReader.NavigatePrevious", [] { navigate_previous(); });
    actions::define("StatusNavigationReader.JumpToNextGroup", [] { jump_next_group(); });
    actions::define("StatusNavigationReader.JumpToPreviousGroup", [] { jump_previous_group(); });
    actions::define("StatusNavigationReader.JumpToTop", [] { jump_top(); });
    actions::define("StatusNavigationReader.JumpToBottom", [] { jump_bottom(); });
    actions::define("StatusNavigationReader.ReadCurrentStat", [] { read_current(false); });
    queries::define("StatusNavigationTracker.IsNavigationActive", [] { return g_tracker.active; });
    queries::define("StatusNavigationTracker.ValidateState", [] { return g_tracker.validate(); });
    queries::define("StatusNavigationTracker.IsActive", [] { return g_tracker.active && g_tracker.validate(); });
    return n;
}

core::Install s_install{"FF5.StatusDetails", &install, 520, game::kFF5};

} // namespace
} // namespace ffpr::ff5
