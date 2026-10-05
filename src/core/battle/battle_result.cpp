// Ports FF1 Patches/BattleResultPatches.cs: the victory screen (gil, EXP per character, item drops,
// level-ups with stat gains) and the EXP counter tick (sound::play_exp_counter / top_up / stop,
// setting "ExpCounter").
//
// Last.UI.KeyInput.ResultMenuController and its phase-init methods exist in all five games.
// Folded body: EndWaitInit shares its RVA with another class's Close() override in every game (x2),
// so its detour class-checks self. The other phase inits are unique.
// The EXP monitor follows ResultMenuController.pointController -> characterListConteroller ->
// contentList / perormanceEndCount. Those two classes are per game (Serial.FFn.UI.KeyInput, FF4's
// is Serial.FF0.UI.KeyInput), so their fields are looked up by name on the live objects' classes.
#include <set>
#include <string>
#include <vector>
#include "battle_internal.h"
#include "battle_util.h"
#include "../audio/sound.h"
#include "../core.h"
#include "../frame.h"
#include "../hooks.h"
#include "../log.h"
#include "../modtext.h"
#include "../say.h"
#include "../settings.h"
#include "../text.h"
#include "../unity.h"

namespace ffpr::battle {
namespace {

ref::Class kResultMenu{"Last.UI.KeyInput", "ResultMenuController"};
ref::Method mShowPointsInit{kResultMenu, "ShowPointsInit", 0};
ref::Method mShowGetItemsInit{kResultMenu, "ShowGetItemsInit", 0};
ref::Method mShowStatusUpInit{kResultMenu, "ShowStatusUpInit", 0};
ref::Method mShowGetAbilitysInit{kResultMenu, "ShowGetAbilitysInit", 0};
ref::Method mShowLevelUpAbilitysInit{kResultMenu, "ShowLevelUpAbilitysInit", 0};
ref::Method mEndWaitInit{kResultMenu, "EndWaitInit", 0};
ref::Field fTargetData{kResultMenu, "targetData"};
ref::Field fPointController{kResultMenu, "pointController"};

ref::Class kResultData{"Last.Data", "BattleResultData"};
ref::Field fGetGil{kResultData, "<GetGil>k__BackingField"};
ref::Field fItemList{kResultData, "<ItemList>k__BackingField"};
ref::Field fCharacterList{kResultData, "<CharacterList>k__BackingField"};

ref::Class kResultChar{"Last.Data", "BattleResultData.BattleResultCharacterData"};
ref::Field fBeforData{kResultChar, "<BeforData>k__BackingField"};
ref::Field fAfterData{kResultChar, "<AfterData>k__BackingField"};
ref::Field fIsLevelUp{kResultChar, "<IsLevelUp>k__BackingField"};
ref::Method mGetExp{kResultChar, "get_GetExp", 0};

ref::Class kDropItem{"Last.Data", "DropItemData"};
ref::Field fContentId{kDropItem, "<ContentId>k__BackingField"};
ref::Field fDropValue{kDropItem, "<DropValue>k__BackingField"};

bool g_announced_points = false;
bool g_announced_items = false;
// Level-ups already announced this result screen, by "<name>_levelup" (the C# HashSet).
std::set<std::wstring> g_announced_level_ups;

// True only while the EXP counter tone is actually playing.
bool g_exp_counter_playing = false;
bool g_monitor_logged_once = false;

constexpr const char* kMonitorLabel = "BattleResult.ExpMonitor";
constexpr double kMonitorInterval = 0.1;   // WaitForSeconds(0.1f): the allowed EXP-tick audio loop

/// StopExpCounterIfPlaying: safe from any phase init or ResetState; fires once.
void stop_exp_counter_if_playing()
{
    if (!g_exp_counter_playing) return;
    g_exp_counter_playing = false;
    sound::stop_exp_counter();
    LOGF("[Battle Result] EXP counter stopped");
}

/// One monitor poll: keep the counter stream fed, stop the tone when the tally finished.
void monitor_tick(void* char_list_ctrl, int32_t end_count_off, int content_count)
{
    // Keep the Counter stream fed so the loop never drains between ticks.
    sound::top_up_exp_counter();

    // The controller went away: bail out; the phase-init safety nets stop the tone.
    if (!unity::alive(char_list_ctrl)) return;
    int end_count = il2cpp::read_field<int32_t>(char_list_ctrl, end_count_off);

    if (!g_monitor_logged_once) {
        LOGF("[Battle Result] MonitorExp: first poll endCount=%d/%d", end_count, content_count);
        g_monitor_logged_once = true;
    }
    if (end_count >= content_count) {
        LOGF("[Battle Result] MonitorExp: animation done (endCount=%d >= contentCount=%d)", end_count, content_count);
        stop_exp_counter_if_playing();
        return;
    }
    if (!g_exp_counter_playing) return;   // stopped by a safety net
    frame::after_seconds(kMonitorInterval, [char_list_ctrl, end_count_off, content_count] {
        monitor_tick(char_list_ctrl, end_count_off, content_count);
    }, kMonitorLabel);
}

/// MonitorExpCounterAnimation: done when perormanceEndCount >= contentList.Count (Count > 0). If
/// the chain can't be read, return; the phase-init safety nets stop the tone instead.
void start_exp_monitor(void* instance)
{
    frame::cancel_label(kMonitorLabel);
    if (!instance) {
        LOGF("[Battle Result] MonitorExp: instance is null");
        return;
    }
    void* point_controller = ref::rd<void*>(instance, fPointController);
    if (!point_controller) {
        LOGF("[Battle Result] MonitorExp: pointController is null");
        return;
    }
    int32_t list_ctrl_off = il2cpp::field_offset(il2cpp::object_class(point_controller), "characterListConteroller");
    void* char_list_ctrl = il2cpp::read_field<void*>(point_controller, list_ctrl_off);
    if (!char_list_ctrl) {
        LOGF("[Battle Result] MonitorExp: characterListController is null");
        return;
    }
    il2cpp::Class* list_ctrl_class = il2cpp::object_class(char_list_ctrl);
    void* content_list = il2cpp::read_field<void*>(char_list_ctrl, il2cpp::field_offset(list_ctrl_class, "contentList"));
    if (!content_list) {
        LOGF("[Battle Result] MonitorExp: contentList is null");
        return;
    }
    int content_count = unity::list_count(content_list);
    if (content_count <= 0) {
        LOGF("[Battle Result] MonitorExp: contentCount=%d, aborting (safety nets will stop the tone)", content_count);
        return;
    }
    int32_t end_count_off = il2cpp::field_offset(list_ctrl_class, "perormanceEndCount");
    if (end_count_off < 0) {
        LOGF("[Battle Result] MonitorExp: perormanceEndCount not found");
        return;
    }
    LOGF("[Battle Result] MonitorExp: chain OK. contentCount=%d", content_count);
    frame::after_seconds(kMonitorInterval, [char_list_ctrl, end_count_off, content_count] {
        monitor_tick(char_list_ctrl, end_count_off, content_count);
    }, kMonitorLabel);
}

/// CalculateStatGains: "HP +12, Strength +1, ..." (Confirmed*, falling back to Base*).
std::wstring stat_gains(void* before, void* after)
{
    struct Row { Stat stat; const wchar_t* key; };
    static const Row rows[] = {
        {Stat::MaxHp, L"HP +{0}"},
        {Stat::Power, L"Strength +{0}"},
        {Stat::Agility, L"Agility +{0}"},
        {Stat::Vitality, L"Stamina +{0}"},        // shown in game as "Stamina"
        {Stat::Intelligence, L"Intellect +{0}"},
        {Stat::Luck, L"Luck +{0}"},
        {Stat::AccuracyRate, L"Accuracy +{0}"},   // ConfirmedAccuracyRate(false)
        {Stat::EvasionRate, L"Evasion +{0}"},     // ConfirmedEvasionRate(false)
    };
    std::vector<std::wstring> gains;
    for (const Row& r : rows) {
        int b = param_confirmed_or_base(before, r.stat);
        int a = param_confirmed_or_base(after, r.stat);
        if (a > b) gains.push_back(TF(r.key, {text::num(a - b)}));
    }
    return gains.empty() ? std::wstring() : text::join(gains, L", ");
}

using Phase_t = void (*)(void* self, const void* mi);

// ShowPointsInit: gil and EXP; clears the battle flags; starts the EXP counter tone.
Phase_t o_ShowPointsInit;
void d_ShowPointsInit(void* self, const void* mi)
{
    o_ShowPointsInit(self, mi);
    hooks::guarded("ResultMenuController.ShowPointsInit", [&] {
        if (g_announced_points) return;
        g_announced_points = true;

        result_clear_all_battle_states();

        void* data = ref::rd<void*>(self, fTargetData);
        if (!data) {
            LOGF("[Battle Result] Could not get result data");
            return;
        }

        int gil = ref::rd<int32_t>(data, fGetGil);
        ffpr::say(TF(L"Gained {0} gil", {n0(gil)}), true);

        // Per-character EXP; the party total decides whether the counter tone starts.
        int total_exp = 0;
        for (void* char_result : list_items(ref::rd<void*>(data, fCharacterList))) {
            if (!char_result) continue;
            void* after = ref::rd<void*>(char_result, fAfterData);
            if (!after) continue;
            std::wstring name = character_name(after);
            if (name.empty()) continue;
            int exp = ref::call<int32_t>(mGetExp, char_result);
            if (exp > 0) {
                total_exp += exp;
                ffpr::say(TF(L"{0} gained {1} XP", {name, n0(exp)}), false);
            }
        }

        if (settings::get_bool("ExpCounter", true) && total_exp > 0) {
            sound::play_exp_counter();
            g_exp_counter_playing = true;
            g_monitor_logged_once = false;
            start_exp_monitor(self);
        }
    });
}

// ShowGetItemsInit: item drops.
Phase_t o_ShowGetItemsInit;
void d_ShowGetItemsInit(void* self, const void* mi)
{
    o_ShowGetItemsInit(self, mi);
    hooks::guarded("ResultMenuController.ShowGetItemsInit", [&] {
        // Safety net: the EXP tally is finished by the time this phase begins.
        stop_exp_counter_if_playing();

        if (g_announced_items) return;
        g_announced_items = true;

        void* data = ref::rd<void*>(self, fTargetData);
        if (!data) return;
        void* items = ref::rd<void*>(data, fItemList);
        if (!items || unity::list_count(items) == 0) return;

        for (void* drop : list_items(items)) {
            if (!drop) continue;
            std::wstring name = item_name_by_content(ref::rd<int32_t>(drop, fContentId));
            if (name.empty()) continue;
            name = strip_icon(name);
            if (name.empty()) continue;
            int drop_count = ref::rd<int32_t>(drop, fDropValue);
            std::wstring announcement = drop_count > 1 ? TF(L"Found {0} x{1}", {name, text::num(drop_count)})
                                                       : TF(L"Found {0}", {name});
            ffpr::say(announcement, false);
        }
    });
}

// ShowStatusUpInit: level-ups with stat gains.
Phase_t o_ShowStatusUpInit;
void d_ShowStatusUpInit(void* self, const void* mi)
{
    o_ShowStatusUpInit(self, mi);
    hooks::guarded("ResultMenuController.ShowStatusUpInit", [&] {
        stop_exp_counter_if_playing();

        void* data = ref::rd<void*>(self, fTargetData);
        if (!data) return;
        for (void* char_result : list_items(ref::rd<void*>(data, fCharacterList))) {
            if (!char_result) continue;
            if (!ref::rd<bool>(char_result, fIsLevelUp)) continue;
            void* after = ref::rd<void*>(char_result, fAfterData);
            if (!after) continue;
            std::wstring name = character_name(after);
            if (name.empty()) continue;

            // Prevent duplicate announcements.
            std::wstring key = name + L"_levelup";
            if (g_announced_level_ups.count(key)) continue;
            g_announced_level_ups.insert(key);

            int new_level = 0;
            void* after_param = character_parameter(after);
            if (after_param) new_level = param_confirmed_or_base(after_param, Stat::Level);

            std::vector<std::wstring> parts;
            parts.push_back(TF(L"{0} leveled up to {1}", {name, text::num(new_level)}));

            void* before = ref::rd<void*>(char_result, fBeforData);   // "Befor": the game's spelling
            void* before_param = before ? character_parameter(before) : nullptr;
            if (before_param && after_param) {
                std::wstring gains = stat_gains(before_param, after_param);
                if (!gains.empty()) parts.push_back(gains);
            }
            ffpr::say(text::join(parts, L", "), false);
        }
    });
}

// Post-points phases: safety nets that stop the EXP counter tone.
Phase_t o_ShowGetAbilitysInit;
void d_ShowGetAbilitysInit(void* self, const void* mi)
{
    o_ShowGetAbilitysInit(self, mi);
    hooks::guarded("ResultMenuController.ShowGetAbilitysInit", [] { stop_exp_counter_if_playing(); });
}

Phase_t o_ShowLevelUpAbilitysInit;
void d_ShowLevelUpAbilitysInit(void* self, const void* mi)
{
    o_ShowLevelUpAbilitysInit(self, mi);
    hooks::guarded("ResultMenuController.ShowLevelUpAbilitysInit", [] { stop_exp_counter_if_playing(); });
}

// EndWaitInit: the terminal phase (folded body: class-checked).
Phase_t o_EndWaitInit;
void d_EndWaitInit(void* self, const void* mi)
{
    o_EndWaitInit(self, mi);
    hooks::guarded("ResultMenuController.EndWaitInit", [&] {
        if (!kResultMenu.is(self)) return;
        stop_exp_counter_if_playing();
    });
}

int install()
{
    actions::define("BattleResultPatches.ClearAllBattleMenuFlags", [] { result_clear_all_battle_states(); });
    int n = 0;
    n += hooks::attach(mShowPointsInit, &d_ShowPointsInit, &o_ShowPointsInit) ? 1 : 0;
    n += hooks::attach(mShowGetItemsInit, &d_ShowGetItemsInit, &o_ShowGetItemsInit) ? 1 : 0;
    n += hooks::attach(mShowStatusUpInit, &d_ShowStatusUpInit, &o_ShowStatusUpInit) ? 1 : 0;
    n += hooks::attach(mShowGetAbilitysInit, &d_ShowGetAbilitysInit, &o_ShowGetAbilitysInit) ? 1 : 0;
    n += hooks::attach(mShowLevelUpAbilitysInit, &d_ShowLevelUpAbilitysInit, &o_ShowLevelUpAbilitysInit) ? 1 : 0;
    n += hooks::attach(mEndWaitInit, &d_EndWaitInit, &o_EndWaitInit) ? 1 : 0;
    return n;
}
core::Install s_install{"Battle.Result", &install, 110};

} // namespace

void result_clear_all_battle_states()
{
    state_set(menu_state::BATTLE_COMMAND, false);
    state_set(menu_state::BATTLE_TARGET, false);
    state_set(menu_state::BATTLE_ITEM, false);
    state_set(menu_state::BATTLE_MAGIC, false);
    command_reset_state();
    magic_reset_state();
}

void result_reset_state()
{
    g_announced_points = false;
    g_announced_items = false;
    g_announced_level_ups.clear();
    // Hard stop any tone left over from an abnormally-ended result screen.
    stop_exp_counter_if_playing();
}

} // namespace ffpr::battle
