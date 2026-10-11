// Native result phases and displayed rows, shared by FF1-FF6. See docs/AUDIT_2026_10.md.
#include "battle.h"
#include "battle_internal.h"
#include "battle_util.h"
#include "../audio/sound.h"
#include "../core.h"
#include "../displayed_ui.h"
#include "../frame.h"
#include "../hooks.h"
#include "../log.h"
#include "../modtext.h"
#include "../say.h"
#include "../settings.h"
#include "../text.h"
#include "../unity.h"
#include "../ui/review_buffer.h"
#include "../ui/preferences.h"

namespace ffpr::battle {
namespace {
ref::Class kMenu{"Last.UI.KeyInput", "ResultMenuController"};
ref::Method mPoints{kMenu, "ShowPointsInit", 0}, mStatus{kMenu, "ShowStatusUpInit", 0},
            mItems{kMenu, "ShowGetItemsInit", 0}, mLearning{kMenu, "ShowGetAbilitysInit", 0},
            mAbilityLevels{kMenu, "ShowLevelUpAbilitysInit", 0}, mEnd{kMenu, "EndWaitInit", 0};
ref::Class kSkills{"Last.UI.KeyInput", "ResultSkillController", game::kFF2 | game::kFF4 | game::kFF5 | game::kFF6};
ref::Method mSkillPage{kSkills, "UpdateContentPage", 0};
bool g_announced_points = false, g_announced_items = false;
unsigned g_generation = 0;
using Phase = void (*)(void*, const void*);
Phase o_points, o_status, o_items, o_learning, o_ability, o_end, o_skill_page;
std::wstring g_skill_headline;
void* g_menu = nullptr;
void* g_point_owner = nullptr;
enum class ResultPhase { None, Points, Status, Items, Learning, Ability, Weapon };
ResultPhase g_phase = ResultPhase::None;
bool g_point_animation_pending = false;
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
    if (!displayed::visible(char_list_ctrl) || !in_battle()) { stop_exp_counter_if_playing(); return; }
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
    auto stop_on_failure = [] { stop_exp_counter_if_playing(); };
    if (!instance) {
        LOGF("[Battle Result] MonitorExp: instance is null");
        stop_on_failure();
        return;
    }
    void* point_controller = instance;
    if (!point_controller) {
        LOGF("[Battle Result] MonitorExp: pointController is null");
        stop_on_failure();
        return;
    }
    int32_t list_ctrl_off = il2cpp::field_offset(il2cpp::object_class(point_controller), "characterListConteroller");
    void* char_list_ctrl = il2cpp::read_field<void*>(point_controller, list_ctrl_off);
    if (!char_list_ctrl) {
        LOGF("[Battle Result] MonitorExp: characterListController is null");
        stop_on_failure();
        return;
    }
    il2cpp::Class* list_ctrl_class = il2cpp::object_class(char_list_ctrl);
    void* content_list = il2cpp::read_field<void*>(char_list_ctrl, il2cpp::field_offset(list_ctrl_class, "performanceList"));
    if (!content_list) {
        LOGF("[Battle Result] MonitorExp: contentList is null");
        stop_on_failure();
        return;
    }
    int content_count = unity::list_count(content_list);
    if (content_count <= 0) {
        LOGF("[Battle Result] MonitorExp: contentCount=%d, aborting (safety nets will stop the tone)", content_count);
        stop_on_failure();
        return;
    }
    int32_t end_count_off = il2cpp::field_offset(list_ctrl_class, "perormanceEndCount");
    if (end_count_off < 0) {
        LOGF("[Battle Result] MonitorExp: perormanceEndCount not found");
        stop_on_failure();
        return;
    }
    LOGF("[Battle Result] MonitorExp: chain OK. contentCount=%d", content_count);
    frame::after_seconds(kMonitorInterval, [char_list_ctrl, end_count_off, content_count] {
        monitor_tick(char_list_ctrl, end_count_off, content_count);
    }, kMonitorLabel);
}


std::vector<std::wstring> row_lines(void* controller, const char* list_field, bool compare = false)
{
    std::vector<std::wstring> lines;
    void* list = displayed::field(controller, list_field);
    for (int i = 0; i < unity::list_count(list); ++i) {
        void* content = unity::list_at(list, i);
        if (!displayed::visible(content)) continue;
        auto line = compare ? displayed::comparison(displayed::field(content, "view")) : displayed::skill_row(content);
        if (!line.empty()) lines.push_back(std::move(line));
    }
    return lines;
}
void later(void* owner, const std::wstring& title, std::function<std::vector<std::wstring>()> read,
           std::function<std::wstring()> headline = {}, bool headline_has_details = false)
{
    const unsigned generation = ++g_generation;
    ui::review::clear();
    frame::cancel_label("BattleResult.Read");
    frame::every_frame([owner, title, read, headline, headline_has_details, generation] {
        if (generation != g_generation || !in_battle() || !unity::alive(owner)) return false;
        if (!displayed::visible(owner)) return true;
        auto lines = read();
        if (lines.empty() && displayed::texts(owner).empty()) return true;
        ui::review::Options options;
        options.headline_has_details = headline_has_details;
        options.defer_details = owner == g_point_owner && g_point_animation_pending;
        if (headline_has_details && headline) options.entry_headline = headline();
        ui::review::show(owner, !headline_has_details && headline ? headline() : title, std::move(lines), std::move(options));
        return false;
    }, "BattleResult.Read", 30);
}
std::wstring points_headline(void* self)
{
        std::vector<std::wstring> lines{T(L"Battle results")};
        void* view = displayed::field(displayed::field(self, "getPointController"), "view");
        for (auto [label, value] : {std::pair{"getGilFixText", "getGilValueText"},
                                  std::pair{"getExpFixText", "getExpValueText"},
                                  std::pair{"getABPFixText", "getABPValueText"},
                                  std::pair{"getMpFixText", "getMpValueText"}}) {
            auto line = displayed::pair(view, label, value);
            if (!line.empty()) lines.push_back(std::move(line));
        }
        // Some games combine learned abilities/job proficiency with the points page.
        // Only include the simultaneously displayed side panel, never future learning data.
        void* skill = displayed::field(g_menu, "skillController");
        auto learned = row_lines(skill, "contentList");
        auto levels = row_lines(skill, "levelUpContentList");
        lines.insert(lines.end(), learned.begin(), learned.end());
        lines.insert(lines.end(), levels.begin(), levels.end());
        return text::join(lines, L". ");
}
void point_rows(void* self)
{
    // Called by the inner PointsInit, after the native controller fills its visible views.
    if (g_announced_points) return;
    g_announced_points = true;
    g_point_owner = self;
    g_phase = ResultPhase::Points;
    g_point_animation_pending = !game::is(game::Id::FF2);
    later(self, T(L"Battle results"), [self] {
        return row_lines(displayed::field(self, "characterListConteroller"), "contentList");
    }, [self] { return points_headline(self); });
    // Counter audio follows native animation completion, never changes the game values.
    if (!game::is(game::Id::FF2) && settings::get_bool("ExpCounter", true)) {
        void* data = displayed::field(self, "targetData");
        if (data) {
            sound::play_exp_counter(); g_exp_counter_playing = true; g_monitor_logged_once = false;
            start_exp_monitor(self);
        }
    }
}
void status_rows(void* self)
{
    g_phase = ResultPhase::Status;
    stop_exp_counter_if_playing();
    later(self, text::join(displayed::texts(displayed::field(self, "view")), L" "), [self] {
        return row_lines(self, "contentList", true);
    });
}
void skills(void* self)
{
    auto read = [self] {
        auto lines = row_lines(self, "contentList");
        auto levels = row_lines(self, "levelUpContentList");
        lines.insert(lines.end(), levels.begin(), levels.end());
        return lines;
    };
    later(self, g_skill_headline, read, [read] { return g_skill_headline + L". " + text::join(read(), L". "); }, true);
}
void points_finished(void* list)
{
    if (!g_point_animation_pending || !displayed::visible(g_point_owner) ||
        displayed::field(g_point_owner, "characterListConteroller") != list) return;
    g_point_animation_pending = false;
    stop_exp_counter_if_playing();
    const unsigned generation = g_generation;
    frame::next_frame([generation] {
        if (generation != g_generation || !ui::review::owns(g_point_owner) || !displayed::visible(g_point_owner)) return;
        auto lines = row_lines(displayed::field(g_point_owner, "characterListConteroller"), "contentList");
        const auto announcement = text::join(lines, L". ");
        ui::review::Options options;
        options.announce = false;
        options.restore_index = ui::review::index();
        // Keep native totals in the review headline; replace only the now-final member rows.
        ui::review::replace_lines(std::move(lines), options.restore_index);
        if (ui::prefs::auto_detail_enabled() && !announcement.empty()) ffpr::say(announcement, false);
    }, "BattleResult.FinalPoints");
}
void d_points(void* self, const void* mi) {
    g_phase = ResultPhase::Points; g_menu = self;
    o_points(self, mi);
    g_menu = self;
    hooks::guarded("Results.Points", [] { result_clear_all_battle_states(); ui::review::clear(); });
}
void d_status(void* self, const void* mi) {
    g_phase = ResultPhase::Status;
    o_status(self, mi);
    hooks::guarded("Results.StatusPhase", [] { stop_exp_counter_if_playing(); ui::review::clear(); });
}
void d_items(void* self, const void* mi) {
    g_phase = ResultPhase::Items;
    o_items(self, mi);
    hooks::guarded("Results.Items", [self] {
        stop_exp_counter_if_playing();
        if (g_announced_items) return;
        g_announced_items = true;
        void* controller = displayed::field(self, "itemControlle");
        later(controller, T(L"Items obtained"), [controller] { return row_lines(controller, "itemContentList"); });
    });
}
void d_learning(void* self, const void* mi) {
    g_phase = ResultPhase::Learning;
    o_learning(self, mi);
    hooks::guarded("Results.Learned", [self] {
        stop_exp_counter_if_playing(); g_skill_headline = T(L"Learned abilities");
        skills(displayed::field(self, "skillController"));
    });
}
void d_ability(void* self, const void* mi) {
    g_phase = ResultPhase::Ability;
    o_ability(self, mi);
    hooks::guarded("Results.AbilityLevels", [self] {
        stop_exp_counter_if_playing(); g_skill_headline = T(L"Ability and job level gains");
        skills(displayed::field(self, "skillController"));
    });
}
void d_skill_page(void* self, const void* mi) {
    o_skill_page(self, mi);
    hooks::guarded("Results.AbilityPage", [self] {
        if (!in_battle()) return;
        if (g_phase == ResultPhase::Learning || g_phase == ResultPhase::Ability) skills(self);
        // A combined points/learning panel must keep the native points page's buffer.
        // Initialization is covered by point_rows; this branch handles later page changes.
        else if (g_phase == ResultPhase::Points && ui::review::owns(g_point_owner)) {
            const unsigned generation = g_generation;
            frame::next_frame([self, generation] {
                if (generation != g_generation || g_phase != ResultPhase::Points || !displayed::visible(self)) return;
                auto lines = row_lines(self, "contentList");
                auto levels = row_lines(self, "levelUpContentList");
                lines.insert(lines.end(), levels.begin(), levels.end());
                ui::review::Options options;
                options.announce = false;
                options.defer_details = g_point_animation_pending;
                options.restore_index = ui::review::index();
                ui::review::show(g_point_owner, points_headline(g_point_owner),
                    row_lines(displayed::field(g_point_owner, "characterListConteroller"), "contentList"), options);
                if (!lines.empty()) ffpr::say(text::join(lines, L". "), false);
            }, "BattleResult.LearningPage");
        }
    });
}
void d_end(void* self, const void* mi) {
    o_end(self, mi);
    if (kMenu.is(self)) { g_phase = ResultPhase::None; stop_exp_counter_if_playing(); ++g_generation; ui::review::clear(); }
}

// Per-game inner phase and per-member page hooks. These bodies are unique in every dump.
#define RESULT_TYPES(N, NS, MASK) \
ref::Class kPoint##N{NS, "ResultPointController", MASK}, kStatus##N{NS, "ResultStatusUpController", MASK}; \
ref::Class kCharacterList##N{NS, "ResultCharacterListController", MASK}; \
ref::Method mFinished##N{kCharacterList##N, "IsEndPerformance", 0}; \
using Finished##N = bool (*)(void*, const void*); Finished##N oFinished##N; \
bool dFinished##N(void* s, const void* m) { bool done = oFinished##N(s, m); hooks::guarded("Results.AnimationFinished", [s, done] { if (done) points_finished(s); }); return done; } \
ref::Method mPoint##N{kPoint##N, "PointsInit", 0}, mStatus##N{kStatus##N, "SetData", 1}; \
Phase oPoint##N; using Status##N = void (*)(void*, void*, const void*); Status##N oStatus##N; \
void dPoint##N(void* s, const void* m) { oPoint##N(s, m); hooks::guarded("Results.PointRows", [s] { point_rows(s); }); } \
void dStatus##N(void* s, void* d, const void* m) { oStatus##N(s, d, m); hooks::guarded("Results.StatusRows", [s] { status_rows(s); }); }
RESULT_TYPES(1, "Serial.FF1.UI.KeyInput", game::kFF1)
RESULT_TYPES(2, "Serial.FF2.UI.KeyInput", game::kFF2)
RESULT_TYPES(3, "Serial.FF3.UI.KeyInput", game::kFF3)
RESULT_TYPES(4, "Serial.FF0.UI.KeyInput", game::kFF4)
RESULT_TYPES(5, "Serial.FF5.UI.KeyInput", game::kFF5)
RESULT_TYPES(6, "Serial.FF6.UI.KeyInput", game::kFF6)
#undef RESULT_TYPES
ref::Class kFF2Point{"Serial.FF2.UI.KeyInput", "ResultPointController", game::kFF2};
ref::Method mSkillLevels{kFF2Point, "SkilllLevelsInit", 0}; Phase o_skill_levels;
void d_skill_levels(void* self, const void* mi) {
    o_skill_levels(self, mi);
    hooks::guarded("Results.WeaponSkills", [self] {
        g_phase = ResultPhase::Weapon;
        stop_exp_counter_if_playing();
        void* controller = displayed::field(self, "skillLevelController");
        later(controller, T(L"Skill gains"), [controller] { return row_lines(controller, "contentList"); });
    });
}
int install() {
    int n = 0;
#define ATTACH(M, D, O) n += hooks::attach(M, &D, &O) ? 1 : 0
    ATTACH(mPoints, d_points, o_points); ATTACH(mStatus, d_status, o_status);
    ATTACH(mItems, d_items, o_items); ATTACH(mLearning, d_learning, o_learning);
    ATTACH(mAbilityLevels, d_ability, o_ability); ATTACH(mEnd, d_end, o_end);
    ATTACH(mSkillPage, d_skill_page, o_skill_page); ATTACH(mSkillLevels, d_skill_levels, o_skill_levels);
#define ATTACH_GAME(N) ATTACH(mPoint##N, dPoint##N, oPoint##N); ATTACH(mStatus##N, dStatus##N, oStatus##N); ATTACH(mFinished##N, dFinished##N, oFinished##N)
    ATTACH_GAME(1); ATTACH_GAME(2); ATTACH_GAME(3); ATTACH_GAME(4); ATTACH_GAME(5); ATTACH_GAME(6);
#undef ATTACH_GAME
#undef ATTACH
    actions::define("BattleResultPatches.ClearAllBattleMenuFlags", [] { result_clear_all_battle_states(); });
    actions::define("BattleResultNavigator.Open", [] { ui::review::details(); }); // FF5's established L key.
    queries::define("BattleResultDataStore.HasData", [] { return ui::review::active(); });
    return n;
}
core::Install s_install{"Battle.Result", &install, 110};
}
void result_clear_all_battle_states() {
    state_set(menu_state::BATTLE_COMMAND, false); state_set(menu_state::BATTLE_TARGET, false);
    state_set(menu_state::BATTLE_ITEM, false); state_set(menu_state::BATTLE_MAGIC, false);
    command_reset_state(); magic_reset_state();
}
void result_reset_state() {
    g_phase = ResultPhase::None; g_menu = nullptr; g_skill_headline.clear();
    g_announced_points = g_announced_items = false; ++g_generation;
    g_point_animation_pending = false; g_point_owner = nullptr;
    stop_exp_counter_if_playing(); frame::cancel_label(kMonitorLabel); frame::cancel_label("BattleResult.");
    ui::review::clear();
}
}
