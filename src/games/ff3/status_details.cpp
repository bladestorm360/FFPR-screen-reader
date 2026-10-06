// Ports FF3 Patches/StatusMenuPatches.cs (StatusDetailsPatches, StatusDetailsHelpers) and the stat
// list of FF3 Menus/StatusDetailsReader.cs (StatusNavigationReader).
//
// The status details screen (Serial.FF3.UI.KeyInput.StatusDetailsController). The core's details
// hooks and stat list are FF1's (src/core/status); FF3 uses the core's tracker
// (ffpr::status::tracker, the Status key context) and summary line, with its own controller
// hooks and its own 26-stat list, and replaces the StatusNavigationReader actions for FF3:
//   Name, Job, Level, Experience, Next Level, Job Level | HP, MP LV1-8 |
//   Strength, Agility, Stamina, Intellect, Spirit |
//   Attack, Accuracy, Defense, Evasion, Magic Defense, Magic Evasion.
// "(X of Y)" is the position inside the stat's group (FF1 parity, as FF3 already did).
//
// Not ported: FF3's "Attack: hits x power" (the hit count was read from the attack row's view by
// walking the parameter controllers); Attack reads FF1's "Attack: {0}" until that is ported.
// InitDisplay (0x6C9E30) is unique; ExitDisplay (0x2BCA70) is shared by 10 methods: class-checked.
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "../../core/core.h"
#include "../../core/frame.h"
#include "../../core/hooks.h"
#include "../../core/il2cpp.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/status/status.h"
#include "../../core/ui/navigation_buffer.h"

namespace ffpr::ff3 {
namespace {

namespace st = ffpr::status;

ref::Class kDetails{"Serial.FF3.UI.KeyInput", "StatusDetailsController", game::kFF3};
ref::Field fStatusController{kDetails, "statusController"};
ref::Method mInitDisplay{kDetails, "InitDisplay", 0};
ref::Method mExitDisplay{kDetails, "ExitDisplay", 0};
ref::Class kCharaStatus{"Serial.FF3.UI.KeyInput", "AbilityCharaStatusController", game::kFF3};
ref::Field fTargetData{kCharaStatus, "targetData"};

ref::Class kPlayerParam{"Last.Data", "PlayerCharacterParameter", game::kFF3};
ref::Method mSpirit{kPlayerParam, "ConfirmedSpirit", 0};
ref::Method mAbilityDefense{kPlayerParam, "ConfirmedAbilityDefense", 0};
ref::Method mAbilityEvasionRate{kPlayerParam, "ConfirmedAbilityEvasionRate", 1};

ref::Class kBattleUtility{"Last.Battle", "BattleUtility", game::kFF3};
ref::Method mGetJobLevel{kBattleUtility, "GetJobLevel", 1};
ref::Class kOwnedCharacter{"Last.Data.User", "OwnedCharacterData", game::kFF3};
ref::Method mOwnedJob{kOwnedCharacter, "get_OwnedJob", 0};
ref::Class kOwnedJob{"Last.Data.User", "OwnedJobData", game::kFF3};
ref::Method mOwnedJobLevel{kOwnedJob, "get_Level", 0};

ui::NavigationBuffer g_buffer;
bool g_built = false;

std::wstring na() { return T(L"N/A"); }
void* data() { return st::tracker().current_character_data; }
void* param_of(void* d) { return d ? st::character_parameter(d) : nullptr; }
void* player_param(void* d)
{
    void* p = param_of(d);
    return (p && kPlayerParam.is(p)) ? p : nullptr;
}
std::wstring fmt(const wchar_t* key, int v) { return TF(key, {text::num(v)}); }

std::wstring read_name()
{
    const std::wstring name = data() ? st::character_name(data()) : std::wstring();
    return text::is_blank(name) ? na() : TF(L"Name: {0}", {name});
}
std::wstring read_job()
{
    if (!data()) return na();
    const std::wstring job = st::current_job_name(data());
    return TF(L"Job: {0}", {text::is_blank(job) ? L"ID " + text::num(st::character_job_id(data())) : job});
}
std::wstring read_level() { void* p = param_of(data()); return p ? fmt(L"Level: {0}", st::param_confirmed_level(p)) : na(); }
std::wstring read_exp() { return data() ? fmt(L"Experience: {0}", st::character_current_exp(data())) : na(); }
std::wstring read_next() { return data() ? fmt(L"Next Level: {0}", st::character_next_exp(data())) : na(); }
std::wstring read_job_level()
{
    void* d = data();
    if (!d) return na();
    const int level = ref::call_static<int32_t>(mGetJobLevel, d);   // the game's own calculation
    if (level > 0) return fmt(L"Job Level: {0}", level);
    if (void* job = ref::call<void*>(mOwnedJob, d)) {
        const int l = ref::call<int32_t>(mOwnedJobLevel, job);
        return fmt(L"Job Level: {0}", l > 0 ? l : 1);
    }
    return TF(L"Job Level: {0}", {na()});
}
std::wstring read_hp()
{
    void* p = param_of(data());
    return p ? TF(L"HP: {0} / {1}", {text::num(st::param_current_hp(p)), text::num(st::param_max_hp(p))}) : na();
}
std::wstring read_mp(int level)
{
    void* p = param_of(data());
    return p ? TF(L"MP LV{0}: {1}", {text::num(level), text::num(st::param_mp_count(p, level))}) : na();
}
std::wstring read_param(const wchar_t* key, int (*value)(void*))
{
    void* p = param_of(data());
    return p ? fmt(key, value(p)) : na();
}
std::wstring read_method(const wchar_t* key, ref::Method& m)
{
    void* p = player_param(data());
    return p ? fmt(key, ref::call<int32_t>(m, p)) : na();
}
std::wstring read_magic_evasion()
{
    void* p = player_param(data());
    return p ? fmt(L"Magic Evasion: {0}%", ref::call<int32_t>(mAbilityEvasionRate, p, false)) : na();
}

void build_buffer()
{
    std::vector<ui::NavigationBuffer::Entry> e = {
        read_name, read_job, read_level, read_exp, read_next, read_job_level,
        read_hp,
        [] { return read_mp(1); }, [] { return read_mp(2); }, [] { return read_mp(3); }, [] { return read_mp(4); },
        [] { return read_mp(5); }, [] { return read_mp(6); }, [] { return read_mp(7); }, [] { return read_mp(8); },
        [] { return read_param(L"Strength: {0}", &st::param_power); },
        [] { return read_param(L"Agility: {0}", &st::param_agility); },
        [] { return read_param(L"Stamina: {0}", &st::param_vitality); },
        [] { return read_param(L"Intellect: {0}", &st::param_intelligence); },
        [] { return read_method(L"Spirit: {0}", mSpirit); },
        [] { return read_param(L"Attack: {0}", &st::param_attack); },
        [] { return read_param(L"Accuracy: {0}%", &st::param_accuracy_rate); },
        [] { return read_param(L"Defense: {0}", &st::param_defense); },
        [] { return read_param(L"Evasion: {0}%", &st::param_evasion_rate); },
        [] { return read_method(L"Magic Defense: {0}", mAbilityDefense); },
        read_magic_evasion,
    };
    g_buffer = ui::NavigationBuffer(std::move(e), {0, 6, 15, 20});
    g_built = true;
}

/// StatusNavigationReader.Move: validate, sync the tracker's index, step, speak with the
/// position inside the group.
template <typename Op>
void move(Op op)
{
    if (!g_built) build_buffer();
    st::Tracker& t = st::tracker();
    if (!t.is_navigation_active) return;
    if (!t.validate_state()) {
        ffpr::say(T(L"Navigation not available"), true);
        return;
    }
    if (!t.current_character_data) {
        ffpr::say(T(L"No character data"), true);
        return;
    }
    g_buffer.set_index(t.current_stat_index);
    const std::wstring value = hooks::guarded_r("FF3.Status.Stat", std::wstring(L"\x01"), [&] { return op(); });
    if (value == L"\x01") {
        ffpr::say(T(L"Error reading stat"), true);
        return;
    }
    t.current_stat_index = g_buffer.index();
    if (value.empty()) return;
    const std::pair<int, int> pos = g_buffer.current_group_position();
    ffpr::say(ui::menu_position(value, pos.first, pos.second), true);
}

/// GetCharacterDataFromController: statusController.targetData.
void* character_from(void* controller)
{
    void* status = ref::rd<void*>(controller, fStatusController);
    return status ? ref::rd<void*>(status, fTargetData) : nullptr;
}

// DelayedStatusInit, one frame after InitDisplay.
void delayed_init(void* controller)
{
    if (!unity::alive(controller) || !unity::active_in_hierarchy(controller)) return;
    menu_state::reset_all_except(menu_state::STATUS_DETAILS);
    void* character = character_from(controller);
    if (!character) return;
    st::Tracker& t = st::tracker();
    t.is_navigation_active = true;
    t.current_stat_index = 0;
    t.active_controller = controller;
    t.current_character_data = character;
    st::set_current_character_data(character);
    build_buffer();
    menu_state::set_active(menu_state::STATUS_DETAILS, true);
    const std::wstring summary = st::read_status_details(controller);
    if (!text::is_blank(summary)) ffpr::say(summary, true);
}

using Void_t = void (*)(void* self, const void* mi);
Void_t o_InitDisplay;
Void_t o_ExitDisplay;

void d_InitDisplay(void* self, const void* mi)
{
    o_InitDisplay(self, mi);
    hooks::guarded("FF3.StatusDetails.InitDisplay", [&] {
        if (!kDetails.is(self)) return;
        void* controller = self;
        frame::next_frame([controller] {
            hooks::guarded("FF3.StatusDetails.DelayedInit", [&] { delayed_init(controller); });
        }, "FF3.StatusDetails.Init");
    });
}

void d_ExitDisplay(void* self, const void* mi)
{
    o_ExitDisplay(self, mi);
    hooks::guarded("FF3.StatusDetails.ExitDisplay", [&] {
        if (!kDetails.is(self)) return;   // folded body: 9 other methods share it
        st::clear_current_character_data();
        st::tracker().reset();
        menu_state::set_active(menu_state::STATUS_DETAILS, false);
        menu_state::set_active(menu_state::STATUS_MENU, false);
    });
}

int install()
{
    int n = 0;
    n += hooks::attach(mInitDisplay, &d_InitDisplay, &o_InitDisplay) ? 1 : 0;
    n += hooks::attach(mExitDisplay, &d_ExitDisplay, &o_ExitDisplay) ? 1 : 0;
    if (n != 2) LOGF("[FF3 StatusDetails] only %d of 2 hooks", n);

    // FF3's stat list replaces FF1's for these actions (the Status context keys run them).
    actions::define("StatusNavigationReader.NavigateNext", [] { move([] { return g_buffer.next(); }); });
    actions::define("StatusNavigationReader.NavigatePrevious", [] { move([] { return g_buffer.previous(); }); });
    actions::define("StatusNavigationReader.JumpToNextGroup", [] { move([] { return g_buffer.next_group(); }); });
    actions::define("StatusNavigationReader.JumpToPreviousGroup", [] { move([] { return g_buffer.previous_group(); }); });
    actions::define("StatusNavigationReader.JumpToTop", [] { move([] { return g_buffer.jump_top(); }); });
    actions::define("StatusNavigationReader.JumpToBottom", [] { move([] { return g_buffer.jump_bottom(); }); });
    actions::define("StatusNavigationReader.ReadCurrentStat", [] { move([] { return g_buffer.current(); }); });
    return n;
}

core::Install s_install{"FF3.StatusDetails", &install, 507, game::kFF3};

} // namespace
} // namespace ffpr::ff3
