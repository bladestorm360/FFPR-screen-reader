// Ports FF6 Patches/StatusDetailsPatches.cs (StatusNavigationTracker, StatusDetailsHelpers and the
// StatusDetailsController InitDisplay / SetNextPlayer / SetPrevPlayer / ExitDisplay postfixes)
// and Menus/StatusDetailsReader.cs (ReadStatusDetails and FF6's StatusNavigationReader stats).
//
// Mod-side behaviour is the core's FF1 status details screen (src/core/status/status_menu.cpp and
// status_navigation.cpp), as for FF2-FF5; only FF6's game content differs. FF6's controller is
// Serial.FF6.UI.KeyInput.StatusDetailsController. This file fills the core's
// StatusNavigationTracker (status::tracker(): the core's Status key context and its
// "StatusNavigationTracker.*" queries work unchanged) and replaces the "StatusNavigationReader.*"
// actions with FF6's stat list (install order 520, after the core's 121):
//   - keys, wrap, group jumps without a group name, R re-reads, the position within the group:
//     the core's ui::NavigationBuffer, as FF1;
//   - wording: FF1's keys ("Name: {0}", "Job: {0}", "Level: {0}", "Experience: {0}",
//     "Next Level: {0}", "HP: {0} / {1}", "Evasion: {0}%" ...), MP and Magic Evasion as FF4/FF5;
//   - opening line: FF1's "Name. Level N. HP: a / b", plus MP (FF6 has MP);
//   - a player switch to the character already shown is skipped by FF1's existing
//     "Status.Character" check, recorded on open and cleared by the tracker's reset.
// FF6 content: the stats Magic, Magic Defense, Magic Evasion, and the Commands, Magicite and
// At Level Up rows read from the screen. Not ported (C# conventions that differ from FF1): the
// group-name prefix on group jumps, "Next Level in:", Evasion from ConfirmedDefenseCount (FF6's
// screen row is the evasion rate, as FF1/FF4/FF5), the C# Experience/Next/Commands/Magicite/Bonus
// in the opening line (they are stats in the list), and the SetParameter postfix (FF1-FF5 track
// the character through InitDisplay and the player switch only).
//
// Like the core's FF1 screen, opening it sets STATUS_DETAILS so the generic cursor reader stays
// silent (the FF6 C# skipped every cursor under a "status" object); ExitDisplay resets it.
//
// Folded body: ExitDisplay (RVA 0x341440) is shared by 9 methods in FF6, among them this class's
// ExitChangeCommand and the Touch controller's ExitDisplay; the detour class-checks self (the C#
// postfix, which took no __instance, ran for all 9). InitDisplay, SetNextPlayer and SetPrevPlayer
// are unique.
#include <cstdint>
#include <iterator>
#include <string>
#include <utility>
#include <vector>
#include "../../core/core.h"
#include "../../core/dedup.h"
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
#include "../../core/ui/navigation_buffer.h"

namespace ffpr::ff6 {
namespace {

constexpr uint8_t k6 = game::kFF6;

ref::Class  kDetails{"Serial.FF6.UI.KeyInput", "StatusDetailsController", k6};
ref::Field  fDetailsView{kDetails, "view"};
ref::Field  fDetailsStatusController{kDetails, "statusController"};
ref::Method mInitDisplay{kDetails, "InitDisplay", 0};
ref::Method mExitDisplay{kDetails, "ExitDisplay", 0};
ref::Method mSetNextPlayer{kDetails, "SetNextPlayer", 0};
ref::Method mSetPrevPlayer{kDetails, "SetPrevPlayer", 0};

ref::Class  kDetailsView{"Serial.FF6.UI.KeyInput", "StatusDetailsView", k6};
ref::Field  fBattleCommandTextList{kDetailsView, "battleCommandTextList"};
ref::Field  fExpText{kDetailsView, "expText"};
ref::Field  fNextExpText{kDetailsView, "nextExpText"};
ref::Field  fMagicalStoneText{kDetailsView, "magicalStoneText"};
ref::Field  fLevelUpBonusText{kDetailsView, "levelUpBonusText"};

ref::Class  kCharaStatus{"Serial.FF6.UI.KeyInput", "AbilityCharaStatusController", k6};
ref::Field  fCharaStatusView{kCharaStatus, "view"};
ref::Field  fCharaTargetData{kCharaStatus, "targetData"};
ref::Class  kCharaStatusView{"Serial.FF6.UI.KeyInput", "AbilityCharaStatusView", k6};

ref::Class  kParamBase{"Last.Data", "CharacterParameterBase", k6};
ref::Field  fParamCurrentMp{kParamBase, "currentMP"};
ref::Class  kPlayerParam{"Last.Data", "PlayerCharacterParameter", k6};
ref::Method mConfirmedMaxMp{kPlayerParam, "ConfirmedMaxMp", 0};
ref::Method mConfirmedMagic{kPlayerParam, "ConfirmedMagic", 0};
ref::Method mConfirmedAbilityDefense{kPlayerParam, "ConfirmedAbilityDefense", 0};
ref::Method mConfirmedAbilityEvasionRate{kPlayerParam, "ConfirmedAbilityEvasionRate", 1};

// The core's FF1 check (status_menu.cpp kCtxCharacter, DEDUP_INVENTORY.md row 32): the same
// context, so status::Tracker::reset() clears it here too.
constexpr const char* kCtxCharacter = "Status.Character";

// --- views -------------------------------------------------------------------------------------

/// StatusDetailsReader.GetTextSafe: the Text's string trimmed, empty for null or blank.
std::wstring text_safe(void* text_component)
{
    if (!text_component) return {};
    std::wstring s = text::trim(unity::ui_text(text_component));
    return text::is_blank(s) ? std::wstring() : s;
}

/// controller.statusController.view (AbilityCharaStatusView).
void* chara_status_view(void* controller)
{
    void* sc = controller ? ref::rd<void*>(controller, fDetailsStatusController) : nullptr;
    void* v = sc ? ref::rd<void*>(sc, fCharaStatusView) : nullptr;
    return v && kCharaStatusView.is(v) ? v : nullptr;
}

/// controller.view (StatusDetailsView) of the tracked controller.
void* active_details_view()
{
    void* c = status::tracker().active_controller;
    return c ? ref::rd<void*>(c, fDetailsView) : nullptr;
}

bool is_placeholder(const std::wstring& s) { return text::trim(s) == L"---"; }

// --- StatusDetailsReader.ReadStatusDetails (FF1's opening line, with MP) ------------------------

std::wstring mp_value(void* p, bool& ok)
{
    ok = p != nullptr;
    return ok ? TF(L"MP: {0} / {1}", {text::num(ref::rd<int32_t>(p, fParamCurrentMp)),
                                      text::num(ref::call<int32_t>(mConfirmedMaxMp, p))})
              : std::wstring();
}

std::wstring read_status_details()
{
    std::vector<std::wstring> parts;
    if (void* data = status::get_current_character_data()) {
        std::wstring name = status::character_name(data);
        if (!text::is_blank(name)) parts.push_back(name);
        if (void* p = status::character_parameter(data)) {
            parts.push_back(TF(L"Level {0}", {text::num(status::param_confirmed_level(p))}));
            parts.push_back(TF(L"HP: {0} / {1}", {text::num(status::param_current_hp(p)), text::num(status::param_max_hp(p))}));
            bool ok = false;
            parts.push_back(mp_value(p, ok));
        }
    }
    return text::join(parts, L". ");
}

// --- StatusNavigationReader: the stats ---------------------------------------------------------

std::wstring na() { return T(L"N/A"); }

/// FindClassTitleInHierarchy: the first Text on a descendant whose name holds "job" or "class"
/// (the title the screen shows under the name).
std::wstring find_class_title(void* root, int depth)
{
    if (!root || depth > 32) return {};
    int n = unity::child_count(root);
    for (int i = 0; i < n; ++i) {
        void* child = unity::child(root, i);
        if (!child) continue;
        std::wstring name = text::lower(unity::name(child));
        if (text::contains(name, L"job") || text::contains(name, L"class")) {
            std::wstring s = text_safe(unity::get_component(child, unity::text_class()));
            if (!s.empty()) return s;
        }
        std::wstring found = find_class_title(child, depth + 1);
        if (!found.empty()) return found;
    }
    return {};
}

std::wstring read_name(void* data)
{
    if (!data) return na();
    std::wstring name = status::character_name(data);
    return !text::is_blank(name) ? TF(L"Name: {0}", {name}) : na();
}

/// FF1's Job row; FF6's job is the title the screen shows (game text), else the Job master name.
std::wstring read_job(void* data)
{
    if (!data) return na();
    void* sv = chara_status_view(status::tracker().active_controller);
    std::wstring job = sv ? find_class_title(unity::transform(sv), 0) : std::wstring();
    if (job.empty()) job = status::current_job_name(data);
    if (!text::is_blank(job)) return TF(L"Job: {0}", {job});
    return TF(L"Job: ID {0}", {text::num(status::character_job_id(data))});
}

std::wstring read_level(void* data)
{
    void* p = status::character_parameter(data);
    if (!p) return na();
    return TF(L"Level: {0}", {text::num(status::param_confirmed_level(p))});
}

/// The screen's text for a row, else the character data (FF5's order: game text first).
std::wstring screen_or_data(ref::Field& f, const wchar_t* key, int (*value)(void*), void* data)
{
    if (!data) return na();
    void* dv = active_details_view();
    std::wstring s = dv ? text_safe(ref::rd<void*>(dv, f)) : std::wstring();
    return TF(key, {s.empty() ? text::num(value(data)) : s});
}

std::wstring read_experience(void* d) { return screen_or_data(fExpText, L"Experience: {0}", &status::character_current_exp, d); }
std::wstring read_next_level(void* d) { return screen_or_data(fNextExpText, L"Next Level: {0}", &status::character_next_exp, d); }

std::wstring read_hp(void* data)
{
    void* p = status::character_parameter(data);
    if (!p) return na();
    return TF(L"HP: {0} / {1}", {text::num(status::param_current_hp(p)), text::num(status::param_max_hp(p))});
}

std::wstring read_mp(void* data)
{
    bool ok = false;
    std::wstring s = mp_value(status::character_parameter(data), ok);
    return ok ? s : na();
}

std::wstring read_param(void* data, const wchar_t* key, int (*value)(void*))
{
    void* p = status::character_parameter(data);
    if (!p) return na();
    return TF(key, {text::num(value(p))});
}

int magic(void* p) { return ref::call<int32_t>(mConfirmedMagic, p); }
int ability_defense(void* p) { return ref::call<int32_t>(mConfirmedAbilityDefense, p); }
int ability_evasion_rate(void* p) { return ref::call<int32_t>(mConfirmedAbilityEvasionRate, p, false); }

std::wstring read_strength(void* d) { return read_param(d, L"Strength: {0}", &status::param_power); }
std::wstring read_agility(void* d) { return read_param(d, L"Agility: {0}", &status::param_agility); }
std::wstring read_stamina(void* d) { return read_param(d, L"Stamina: {0}", &status::param_vitality); }
std::wstring read_magic(void* d) { return read_param(d, L"Magic: {0}", &magic); }
std::wstring read_attack(void* d) { return read_param(d, L"Attack: {0}", &status::param_attack); }
std::wstring read_defense(void* d) { return read_param(d, L"Defense: {0}", &status::param_defense); }
std::wstring read_evasion(void* d) { return read_param(d, L"Evasion: {0}%", &status::param_evasion_rate); }
std::wstring read_magic_defense(void* d) { return read_param(d, L"Magic Defense: {0}", &ability_defense); }
std::wstring read_magic_evasion(void* d) { return read_param(d, L"Magic Evasion: {0}%", &ability_evasion_rate); }

/// FF6's Commands row: the battle commands the screen lists.
std::wstring read_commands(void*)
{
    void* dv = active_details_view();
    if (!dv) return na();
    void* list = ref::rd<void*>(dv, fBattleCommandTextList);
    int n = list ? unity::list_count(list) : 0;
    std::vector<std::wstring> commands;
    for (int i = 0; i < n; ++i) {
        std::wstring s = text_safe(unity::list_at(list, i));
        if (!s.empty()) commands.push_back(s);
    }
    return TF(L"Commands: {0}", {commands.empty() ? T(L"none") : text::join(commands, L", ")});
}

/// FF6's Magicite / At Level Up rows: the screen's text, "none" where it shows "---".
std::wstring read_optional(ref::Field& f, const wchar_t* key)
{
    void* dv = active_details_view();
    void* t = dv ? ref::rd<void*>(dv, f) : nullptr;
    if (!t) return na();
    std::wstring s = text_safe(t);
    return TF(key, {!s.empty() && !is_placeholder(s) ? s : T(L"none")});
}

std::wstring read_magicite(void*) { return read_optional(fMagicalStoneText, L"Magicite: {0}"); }
std::wstring read_level_up_bonus(void*) { return read_optional(fLevelUpBonusText, L"At Level Up: {0}"); }

using Reader = std::wstring (*)(void*);
// FF1's layout with FF6's stats: Character Info 0-4, Vitals 5-6, Attributes 7-10,
// Combat Stats 11-15, Details (FF6) 16-18.
const Reader kStats[] = {
    &read_name, &read_job, &read_level, &read_experience, &read_next_level,
    &read_hp, &read_mp,
    &read_strength, &read_agility, &read_stamina, &read_magic,
    &read_attack, &read_defense, &read_evasion, &read_magic_defense, &read_magic_evasion,
    &read_commands, &read_magicite, &read_level_up_bonus,
};
const int kGroupStarts[] = {0, 5, 7, 11, 16};

ui::NavigationBuffer g_buffer;

void build_buffer()
{
    std::vector<ui::NavigationBuffer::Entry> entries;
    for (Reader r : kStats)
        entries.push_back([r] { return r(status::tracker().current_character_data); });
    g_buffer = ui::NavigationBuffer(std::move(entries), std::vector<int>(std::begin(kGroupStarts), std::end(kGroupStarts)));
}

using Op = std::wstring (ui::NavigationBuffer::*)();

/// The core's navigation step (status_navigation.cpp move): validate, sync the tracker's index,
/// run the op, speak with the position in the stat's group.
void move(Op op)
{
    status::Tracker& t = status::tracker();
    if (!t.is_navigation_active) return;
    if (!t.validate_state()) {
        ffpr::say(T(L"Navigation not available"));
        return;
    }
    if (!t.current_character_data) {
        ffpr::say(T(L"No character data"));
        return;
    }
    g_buffer.set_index(t.current_stat_index);
    std::wstring value;
    bool ok = hooks::guarded_r("FF6.Status.ReadStat", false, [&] {
        value = (g_buffer.*op)();
        return true;
    });
    if (!ok) {
        LOGF("Error reading stat at index %d", g_buffer.index());
        ffpr::say(T(L"Error reading stat"));
        return;
    }
    t.current_stat_index = g_buffer.index();
    if (!value.empty()) {
        auto [local_index, group_count] = g_buffer.current_group_position();
        ffpr::say(ui::menu_position(value, local_index, group_count), true);
    }
}

// --- StatusDetailsPatches (the core's FF1 flow) -------------------------------------------------

/// StatusDetailsHelpers.GetCharacterDataFromController: statusController.targetData.
void* character_from_controller(void* controller)
{
    void* sc = controller ? ref::rd<void*>(controller, fDetailsStatusController) : nullptr;
    return sc ? ref::rd<void*>(sc, fCharaTargetData) : nullptr;
}

/// DelayedStatusInit: one frame after InitDisplay, when the UI has updated.
void delayed_status_init(void* controller)
{
    if (!unity::alive(controller) || !unity::active_in_hierarchy(controller)) return;
    void* data = character_from_controller(controller);
    if (!data) {
        LOGF("[FF6 Status Details] Could not get character data");
        return;
    }
    status::Tracker& t = status::tracker();
    t.is_navigation_active = true;
    t.current_stat_index = 0;
    t.active_controller = controller;
    t.current_character_data = data;
    dedup::reset(kCtxCharacter);
    dedup::text(kCtxCharacter, status::character_name(data), dedup::Scope::Session);
    status::set_current_character_data(data);

    std::wstring info = read_status_details();
    if (!text::is_blank(info)) ffpr::say(info);
}

/// DelayedPlayerSwitchInit: one frame after SetNextPlayer / SetPrevPlayer.
void delayed_player_switch(void* controller)
{
    if (!unity::alive(controller) || !status::status_details_active()) return;
    if (!unity::active_in_hierarchy(controller)) return;
    void* data = character_from_controller(controller);
    if (!data) {
        LOGF("[FF6 Status Details] Could not get character data after switch");
        return;
    }
    // Skip if it is the character already shown (redundant calls).
    status::Tracker& t = status::tracker();
    if (t.current_character_data) {
        if (!dedup::text(kCtxCharacter, status::character_name(data), dedup::Scope::Session)) return;
    } else {
        dedup::reset(kCtxCharacter);
        dedup::text(kCtxCharacter, status::character_name(data), dedup::Scope::Session);
    }
    t.current_character_data = data;
    t.current_stat_index = 0;
    t.active_controller = controller;
    status::set_current_character_data(data);

    std::wstring info = read_status_details();
    if (!text::is_blank(info)) ffpr::say(info, true);
}

using Void_t = void (*)(void* self, const void* mi);

Void_t o_InitDisplay;
void d_InitDisplay(void* self, const void* mi)
{
    o_InitDisplay(self, mi);
    hooks::guarded("FF6.StatusDetailsController.InitDisplay", [&] {
        if (!self || !kDetails.is(self)) return;
        // Status details active immediately: clear the other states and set this one.
        ffpr::system::clear_other_menu_states("StatusDetails");
        menu_state::set_active(menu_state::STATUS_DETAILS, true);
        void* controller = self;
        frame::next_frame([controller] {
            hooks::guarded("FF6.StatusDetails.DelayedStatusInit", [&] { delayed_status_init(controller); });
        }, "FF6.StatusDetails.Init");
    });
}

Void_t o_ExitDisplay;
void d_ExitDisplay(void* self, const void* mi)
{
    o_ExitDisplay(self, mi);
    hooks::guarded("FF6.StatusDetailsController.ExitDisplay", [&] {
        if (!self || !kDetails.is(self)) return;   // folded body: 9 methods share it
        status::clear_current_character_data();
        status::tracker().reset();
        menu_state::reset(menu_state::STATUS_DETAILS);
    });
}

void handle_player_switch(void* self)
{
    if (!status::status_details_active()) return;
    if (!status::tracker().is_navigation_active) return;
    if (!self || !kDetails.is(self)) return;
    void* controller = self;
    frame::next_frame([controller] {
        hooks::guarded("FF6.StatusDetails.DelayedPlayerSwitch", [&] { delayed_player_switch(controller); });
    }, "FF6.StatusDetails.Switch");
}

Void_t o_SetNextPlayer;
void d_SetNextPlayer(void* self, const void* mi)
{
    o_SetNextPlayer(self, mi);
    hooks::guarded("FF6.StatusDetailsController.SetNextPlayer", [&] { handle_player_switch(self); });
}

Void_t o_SetPrevPlayer;
void d_SetPrevPlayer(void* self, const void* mi)
{
    o_SetPrevPlayer(self, mi);
    hooks::guarded("FF6.StatusDetailsController.SetPrevPlayer", [&] { handle_player_switch(self); });
}

int install()
{
    build_buffer();
    int n = 0;
    n += hooks::attach(mInitDisplay, &d_InitDisplay, &o_InitDisplay) ? 1 : 0;
    n += hooks::attach(mExitDisplay, &d_ExitDisplay, &o_ExitDisplay) ? 1 : 0;
    n += hooks::attach(mSetNextPlayer, &d_SetNextPlayer, &o_SetNextPlayer) ? 1 : 0;
    n += hooks::attach(mSetPrevPlayer, &d_SetPrevPlayer, &o_SetPrevPlayer) ? 1 : 0;

    // FF6's stat list replaces the core's (FF1) behind the same action names; the Status key
    // context, its bindings and the StatusNavigationTracker queries stay the core's.
    actions::define("StatusNavigationReader.NavigateNext", [] { move(&ui::NavigationBuffer::next); });
    actions::define("StatusNavigationReader.NavigatePrevious", [] { move(&ui::NavigationBuffer::previous); });
    actions::define("StatusNavigationReader.JumpToNextGroup", [] { move(&ui::NavigationBuffer::next_group); });
    actions::define("StatusNavigationReader.JumpToPreviousGroup", [] { move(&ui::NavigationBuffer::previous_group); });
    actions::define("StatusNavigationReader.JumpToTop", [] { move(&ui::NavigationBuffer::jump_top); });
    actions::define("StatusNavigationReader.JumpToBottom", [] { move(&ui::NavigationBuffer::jump_bottom); });
    actions::define("StatusNavigationReader.ReadCurrentStat", [] { move(&ui::NavigationBuffer::current); });
    return n;
}

core::Install s_install{"FF6.StatusDetails", &install, 520, game::kFF6};

} // namespace
} // namespace ffpr::ff6
