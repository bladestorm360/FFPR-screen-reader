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
ref::Method mShow{kMenu, "Show", 2}, mClose{kMenu, "Close", 0}, mUpdateMenu{kMenu, "UpdateController", 0};
ref::Method mPoints{kMenu, "ShowPointsInit", 0}, mStatus{kMenu, "ShowStatusUpInit", 0},
            mItems{kMenu, "ShowGetItemsInit", 0}, mLearning{kMenu, "ShowGetAbilitysInit", 0},
            mAbilityLevels{kMenu, "ShowLevelUpAbilitysInit", 0}, mEnd{kMenu, "EndWaitInit", 0};
ref::Class kSkills{"Last.UI.KeyInput", "ResultSkillController", game::kFF2 | game::kFF4 | game::kFF5 | game::kFF6};
ref::Method mSkillPage{kSkills, "UpdateContentPage", 0};
bool g_announced_points = false, g_announced_items = false;
unsigned g_generation = 0;
using Phase = void (*)(void*, const void*);
Phase o_points, o_status, o_items, o_learning, o_ability, o_end, o_skill_page, o_close, o_update_menu;
using Show = void (*)(void*, void*, bool, const void*); Show o_show;
std::wstring g_skill_headline;
void* g_menu = nullptr;
void* g_point_owner = nullptr;
void* g_status_owner = nullptr;
enum class ResultPhase { None, Points, Status, Items, Learning, Ability, Weapon };
ResultPhase g_phase = ResultPhase::None;
bool g_point_animation_pending = false;
// True only while the EXP counter tone is actually playing.
bool g_exp_counter_playing = false;
bool g_monitor_logged_once = false;
std::shared_ptr<ui::review::ResultNotificationSession> g_notification_session;

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
    if (!displayed::container_visible(controller)) return lines;
    void* list = displayed::field(controller, list_field);
    const int count = unity::list_count(list);
    if (count < 0 || count > 128) return lines;
    for (int i = 0; i < count; ++i) {
        void* content = unity::list_at(list, i);
        if (!displayed::container_visible(content)) continue;
        auto line = compare ? displayed::comparison(displayed::field(content, "view")) : displayed::skill_row(content);
        if (!line.empty()) lines.push_back(std::move(line));
    }
    return lines;
}
using ReadLines = std::function<std::vector<std::wstring>()>;
using Configure = std::function<void(ui::review::Options&)>;
struct PendingPage {
    void* owner;
    std::function<std::wstring()> title;
    ReadLines read, summary, notifications;
    Configure configure;
    unsigned generation;
    uint64_t earliest_frame;
};
std::optional<PendingPage> g_pending_page;
void read_pending_page()
{
    if (!g_pending_page) return;
    if (g_pending_page->generation != g_generation || g_phase == ResultPhase::None ||
        !unity::alive(g_pending_page->owner)) { g_pending_page.reset(); return; }
    if (frame::count() < g_pending_page->earliest_frame ||
        !displayed::container_visible(g_pending_page->owner)) return;
    auto page = *g_pending_page;
    auto summary = page.summary ? page.summary() : std::vector<std::wstring>{};
    auto notifications = page.notifications ? page.notifications() : std::vector<std::wstring>{};
    // Read a small rendered total first while a reward panel is fading. Do not
    // traverse its full table (or all descendant text) on every native update.
    if ((g_phase == ResultPhase::Points || g_phase == ResultPhase::Weapon) && summary.empty() && notifications.empty()) return;
    auto lines = page.read();
    ui::review::Options options;
    options.include_headline = true;
    options.defer_details = page.owner == g_point_owner && g_point_animation_pending;
    options.summary = std::move(summary);
    options.notifications = std::move(notifications);
    if (ui::prefs::result_notifications_once_enabled()) {
        if (!g_notification_session) g_notification_session = std::make_shared<ui::review::ResultNotificationSession>();
        options.notification_session = g_notification_session;
    }
    if (page.configure) page.configure(options);
    const auto heading = page.title();
    g_pending_page.reset();
    LOGF("[Battle Result] displayed phase %d: %zu detail rows, %zu totals",
         static_cast<int>(g_phase), lines.size(), options.summary.size());
    ui::review::show(page.owner, heading, std::move(lines), std::move(options));
}
void later(void* owner, std::function<std::wstring()> title, ReadLines read,
           ReadLines summary = {}, Configure configure = {}, ReadLines notifications = {})
{
    const unsigned generation = ++g_generation;
    ui::review::clear();
    frame::cancel_label("BattleResult.Read");
    g_pending_page = PendingPage{owner, std::move(title), std::move(read), std::move(summary), std::move(notifications),
                                std::move(configure), generation, frame::count() + 1};
    // Only the result controller's post-update event can read this page. Its
    // outer init queues an inner state transition: a global next-frame callback
    // can run before that transition has populated the new views.
    // Fades never exhaust an arbitrary page deadline.
}
std::vector<std::wstring> point_totals(void* self)
{
    std::vector<std::wstring> lines;
    void* view = displayed::field(displayed::field(self, "getPointController"), "view");
    for (auto [label, value] : {std::pair{"getExpFixText", "getExpValueText"},
                              std::pair{"getGilFixText", "getGilValueText"},
                              std::pair{"getABPFixText", "getABPValueText"},
                              std::pair{"getMpFixText", "getMpValueText"}}) {
        // FF2 has gil, weapon/spell levels and use-based stats, never character EXP.
        if (game::is(game::Id::FF2) && std::string(label) != "getGilFixText") continue;
        auto line = displayed::pair(view, label, value);
        if (!line.empty()) lines.push_back(std::move(line));
    }
    return lines;
}
void append_skill_details(std::vector<std::wstring>& lines)
{
    // Actual learning/level-up messages are essential, independent of Autodetail.
    void* skill = displayed::field(g_menu, "skillController");
    auto learned = row_lines(skill, "contentList");
    auto levels = row_lines(skill, "levelUpContentList");
    lines.insert(lines.end(), learned.begin(), learned.end());
    lines.insert(lines.end(), levels.begin(), levels.end());
}
std::vector<std::wstring> skill_notifications()
{
    std::vector<std::wstring> lines;
    append_skill_details(lines);
    return lines;
}
std::vector<std::wstring> point_details(void* self)
{
    auto lines = game::is(game::Id::FF2) ? std::vector<std::wstring>{} :
        row_lines(displayed::field(self, "characterListConteroller"), "contentList");
    return lines;
}
void begin_phase(void* menu, ResultPhase phase)
{
    g_menu = menu; g_phase = phase;
    LOGF("[Battle Result] native phase entry %d", static_cast<int>(phase));
    g_pending_page.reset();
    ++g_generation;
    frame::cancel_label("BattleResult.Read");
    frame::cancel_label("BattleResult.FinalPoints");
    frame::cancel_label("BattleResult.LearningPage");
    stop_exp_counter_if_playing();
    g_point_animation_pending = false;
    ui::review::clear();
}
void point_rows(void* self)
{
    // Called by the inner PointsInit, after the native controller fills its visible views.
    if (!unity::alive(g_menu) || g_phase != ResultPhase::Points || g_announced_points) return;
    g_announced_points = true;
    g_point_owner = self;
    g_phase = ResultPhase::Points;
    g_point_animation_pending = !game::is(game::Id::FF2) &&
        unity::list_count(displayed::field(displayed::field(self, "characterListConteroller"), "performanceList")) > 0;
    later(self, [] { return T(L"Battle results"); }, [self] { return point_details(self); },
          [self] { return point_totals(self); }, {}, [] { return skill_notifications(); });
    // Counter audio follows native animation completion, never changes the game values.
    if (!game::is(game::Id::FF2) && settings::get_bool("ExpCounter", true)) {
        void* data = displayed::field(self, "targetData");
        if (data) {
            sound::play_exp_counter(); g_exp_counter_playing = true; g_monitor_logged_once = false;
            start_exp_monitor(self);
        }
    }
}
std::vector<std::wstring> growth_rows(void* self, bool changed_only)
{
    std::vector<std::wstring> lines;
    void* list = displayed::field(self, "contentList");
    const int count = unity::list_count(list);
    if (count < 0 || count > 64) return lines;
    for (int i = 0; i < count; ++i) {
        void* content = unity::list_at(list, i);
        if (!displayed::container_visible(content)) continue;
        void* view = displayed::field(content, "view");
        // Native job-mastery messages replace this row's numbers. They are
        // essential messages, not an optional numeric comparison.
        if (!displayed::text_field(view, "upperJobMasterText").empty() ||
            !displayed::text_field(view, "lowerJobMasterText").empty()) continue;
        auto reading = displayed::comparison_reading(view);
        if (reading.line.empty()) continue;
        if (!changed_only || reading.changed) lines.push_back(std::move(reading.line));
    }
    return lines;
}
std::vector<std::wstring> growth_messages(void* self)
{
    std::vector<std::wstring> lines;
    void* list = displayed::field(self, "contentList");
    const int count = unity::list_count(list);
    if (count < 0 || count > 64) return lines;
    for (int i = 0; i < count; ++i) {
        void* row = unity::list_at(list, i);
        if (!displayed::container_visible(row)) continue;
        void* view = displayed::field(row, "view");
        for (const char* field : {"upperJobMasterText", "lowerJobMasterText"}) {
            auto message = displayed::text_field(view, field);
            if (!message.empty()) lines.push_back(std::move(message));
        }
    }
    append_skill_details(lines);
    return lines;
}
void status_rows(void* self)
{
    // SetData also runs during native UI construction. Only the game's actual
    // growth phase owns per-character tables; skipped phases build nothing.
    if (!unity::alive(g_menu) || g_phase != ResultPhase::Status) return;
    g_status_owner = self;
    g_phase = ResultPhase::Status;
    stop_exp_counter_if_playing();
    later(self, [self] { return displayed::text_field(displayed::field(self, "view"), "nameText"); }, [self] {
        auto lines = growth_rows(self, false);
        auto bonus = displayed::pair(displayed::field(self, "view"), "magicStoneBonusText", "magicStoneBonusValueText");
        if (!bonus.empty()) lines.push_back(std::move(bonus));
        return lines;
    }, {}, [self](ui::review::Options& options) {
        options.brief = growth_rows(self, true);
        options.brief_details = ui::prefs::stat_delta_mode() == 1;
        options.announce_brief = ui::prefs::announce_result_stat_changes_enabled();
    }, [self] { return growth_messages(self); });
}
std::vector<std::wstring> ff2_weapon_rows(void* controller, bool changed_only = false)
{
    std::vector<std::wstring> lines;
    if (!game::is(game::Id::FF2) || !displayed::container_visible(controller)) return lines;
    void* characters = displayed::field(controller, "contentList");
    for (int i = 0; i < unity::list_count(characters); ++i) {
        void* character = unity::list_at(characters, i);
        if (!displayed::container_visible(character)) continue;
        const auto name = displayed::text_field(displayed::field(character, "view"), "nameText");
        void* skills = displayed::field(character, "contentList");
        for (int j = 0; j < unity::list_count(skills); ++j) {
            void* skill = unity::list_at(skills, j);
            if (!displayed::container_visible(skill)) continue;
            const auto rendered = displayed::skill_row(skill);
            if (rendered.empty()) continue;
            auto line = changed_only || ui::prefs::stat_delta_mode() == 1 ? displayed::skill_change(skill) : rendered;
            if (!changed_only && line.empty()) line = ui::prefs::stat_delta_mode() == 1 ?
                displayed::skill_name(skill) + L": " + T(L"No change") : rendered;
            // Native FF2 SetData populates slot 0 from right-hand equipment and
            // slot 1 from left-hand equipment. Retain the displayed column labels.
            const auto hand = displayed::text_field(displayed::field(controller, "view"),
                j == 0 ? "fixedRightHandText" : j == 1 ? "fixedLeftHandText" : "");
            if (!line.empty()) lines.push_back(name + (name.empty() ? L"" : L": ") +
                hand + (hand.empty() ? L"" : L" ") + line);
        }
    }
    return lines;
}
void skills(void* self)
{
    later(self, [] { return g_skill_headline; }, [] { return std::vector<std::wstring>{}; }, {}, {}, [self] {
        auto lines = row_lines(self, "contentList");
        auto levels = row_lines(self, "levelUpContentList");
        lines.insert(lines.end(), levels.begin(), levels.end());
        return lines;
    });
}
void weapon_page(void* point)
{
    // FF2's outer ShowLevelUpAbilitys phase combines gil, equipped weapon gauges
    // and any spell-level rows. The spell panel can legitimately be absent.
    // Own the page through the point controller, never through that optional panel.
    g_point_owner = point;
    g_phase = ResultPhase::Weapon;
    stop_exp_counter_if_playing();
    later(point, [] { return T(L"Battle results"); }, [point] {
        auto lines = ff2_weapon_rows(displayed::field(point, "skillLevelController"));
        return lines;
    }, [point] {
        // This is the actual native weapon phase, not the preceding member panel.
        if (!displayed::container_visible(displayed::field(point, "skillLevelController")))
            return std::vector<std::wstring>{};
        return point_totals(point);
    }, [point](ui::review::Options& options) {
        void* controller = displayed::field(point, "skillLevelController");
        options.brief = ff2_weapon_rows(controller, true);
        options.brief_details = ui::prefs::stat_delta_mode() == 1;
        options.announce_brief = ui::prefs::announce_result_stat_changes_enabled();
        options.announce_brief_with_details = !options.brief_details;
    }, [point] {
        return displayed::container_visible(displayed::field(point, "skillLevelController")) ?
            skill_notifications() : std::vector<std::wstring>{};
    });
}

void points_finished(void* list)
{
    if (!g_point_animation_pending || !displayed::container_visible(g_point_owner) ||
        displayed::field(g_point_owner, "characterListConteroller") != list) return;
    g_point_animation_pending = false;
    stop_exp_counter_if_playing();
    const unsigned generation = g_generation;
    // If completion precedes the initial buffer read, that read already announces
    // final details. Only an existing deferred page needs a completion announcement.
    const bool had_deferred_page = ui::review::owns(g_point_owner);
    frame::next_frame([generation, had_deferred_page] {
        if (generation != g_generation || !ui::review::owns(g_point_owner) || !displayed::container_visible(g_point_owner)) return;
        auto lines = point_details(g_point_owner);
        const auto announcement = text::join(lines, L". ");
        ui::review::Options options;
        options.announce = false;
        options.restore_index = ui::review::index();
        // Keep current totals as review lines; replace only the now-final detailed rows.
        ui::review::replace_lines(std::move(lines), options.restore_index);
        if (had_deferred_page && ui::prefs::auto_detail_enabled() && !announcement.empty()) ffpr::say(announcement, false);
    }, "BattleResult.FinalPoints");
}
void animation_step(void* list)
{
    if (!g_point_animation_pending || !unity::alive(list)) return;
    const auto offset = il2cpp::field_offset(il2cpp::object_class(list), "perormanceEndCount");
    if (offset < 0) return;
    const int count = unity::list_count(displayed::field(list, "performanceList"));
    if (count > 0 && il2cpp::read_field<int32_t>(list, offset) >= count) points_finished(list);
}
void d_show(void* self, void* data, bool reverse, const void* mi) {
    hooks::guarded("Results.Open", [self] {
        result_reset_state(); begin_phase(self, ResultPhase::Points); result_clear_all_battle_states();
    });
    o_show(self, data, reverse, mi);
}
void d_update_menu(void* self, const void* mi) {
    o_update_menu(self, mi);
    hooks::guarded("Results.NativeReadiness", [self] { if (self == g_menu) read_pending_page(); });
}
void d_close(void* self, const void* mi) {
    hooks::guarded("Results.Close", [self] { if (self == g_menu) result_reset_state(); });
    o_close(self, mi);
}
void d_points(void* self, const void* mi) {
    hooks::guarded("Results.Points", [self] { begin_phase(self, ResultPhase::Points); result_clear_all_battle_states(); });
    o_points(self, mi);
}
void d_status(void* self, const void* mi) {
    hooks::guarded("Results.StatusPhase", [self] { begin_phase(self, ResultPhase::Status); });
    o_status(self, mi);
}
void d_items(void* self, const void* mi) {
    hooks::guarded("Results.ItemsPhase", [self] { begin_phase(self, ResultPhase::Items); });
    o_items(self, mi);
    hooks::guarded("Results.Items", [self] {
        if (g_announced_items) return;
        g_announced_items = true;
        void* controller = displayed::field(self, "itemControlle");
        later(controller, [] { return T(L"Items obtained"); }, [controller] { return row_lines(controller, "itemContentList"); });
    });
}
void d_learning(void* self, const void* mi) {
    hooks::guarded("Results.LearningPhase", [self] { begin_phase(self, ResultPhase::Learning); });
    o_learning(self, mi);
    hooks::guarded("Results.Learned", [self] {
        stop_exp_counter_if_playing(); g_skill_headline = T(L"Learned abilities");
        skills(displayed::field(self, "skillController"));
    });
}
void d_ability(void* self, const void* mi) {
    hooks::guarded("Results.AbilityPhase", [self] {
        begin_phase(self, game::is(game::Id::FF2) ? ResultPhase::Weapon : ResultPhase::Ability);
    });
    o_ability(self, mi);
    hooks::guarded("Results.AbilityLevels", [self] {
        stop_exp_counter_if_playing(); g_skill_headline = T(L"Ability and job level gains");
        if (game::is(game::Id::FF2)) weapon_page(displayed::field(self, "pointController"));
        else skills(displayed::field(self, "skillController"));
    });
}
void d_skill_page(void* self, const void* mi) {
    o_skill_page(self, mi);
    hooks::guarded("Results.AbilityPage", [self] {
        if (!unity::alive(g_menu)) return;
        if (g_phase == ResultPhase::Learning || g_phase == ResultPhase::Ability) skills(self);
        // A combined points/learning panel must keep the native points page's buffer.
        // Initialization is covered by point_rows; this branch handles later page changes.
        else if (g_phase == ResultPhase::Points && ui::review::owns(g_point_owner)) {
            void* owner = g_point_owner;
            later(owner, [] { return T(L"Battle results"); }, [owner] { return point_details(owner); },
                  [owner] { return point_totals(owner); }, {}, [] { return skill_notifications(); });
        } else if (g_phase == ResultPhase::Weapon && game::is(game::Id::FF2)) {
            weapon_page(g_point_owner);
        } else if (g_phase == ResultPhase::Status && ui::review::owns(g_status_owner)) {
            status_rows(g_status_owner);
        }
    });
}
void d_end(void* self, const void* mi) {
    o_end(self, mi);
    hooks::guarded("Results.End", [self] { if (self == g_menu) result_reset_state(); });
}

// Per-game inner phase and per-member page hooks. These bodies are unique in every dump.
#define RESULT_TYPES(N, NS, MASK) \
ref::Class kPoint##N{NS, "ResultPointController", MASK}, kStatus##N{NS, "ResultStatusUpController", MASK}; \
ref::Class kCharacterList##N{NS, "ResultCharacterListController", MASK}; \
ref::Method mStep##N{kCharacterList##N, "<PlayPerformance>b__6_0", 0}, mForced##N{kCharacterList##N, "ForcedEndPerformance", 0}; \
Phase oStep##N, oForced##N; \
void dStep##N(void* s, const void* m) { oStep##N(s, m); hooks::guarded("Results.AnimationStep", [s] { if (kCharacterList##N.is(s)) animation_step(s); }); } \
void dForced##N(void* s, const void* m) { oForced##N(s, m); hooks::guarded("Results.AnimationForced", [s] { if (kCharacterList##N.is(s)) points_finished(s); }); } \
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
        if (unity::alive(g_menu) && g_phase == ResultPhase::Weapon) weapon_page(self);
    });
}
int install() {
    int n = 0;
#define ATTACH(M, D, O) n += hooks::attach(M, &D, &O) ? 1 : 0
    ATTACH(mShow, d_show, o_show); ATTACH(mClose, d_close, o_close); ATTACH(mUpdateMenu, d_update_menu, o_update_menu);
    ATTACH(mPoints, d_points, o_points); ATTACH(mStatus, d_status, o_status);
    ATTACH(mItems, d_items, o_items); ATTACH(mLearning, d_learning, o_learning);
    ATTACH(mAbilityLevels, d_ability, o_ability); ATTACH(mEnd, d_end, o_end);
    ATTACH(mSkillPage, d_skill_page, o_skill_page); ATTACH(mSkillLevels, d_skill_levels, o_skill_levels);
#define ATTACH_GAME(N) ATTACH(mPoint##N, dPoint##N, oPoint##N); ATTACH(mStatus##N, dStatus##N, oStatus##N); ATTACH(mStep##N, dStep##N, oStep##N); ATTACH(mForced##N, dForced##N, oForced##N)
    ATTACH_GAME(1); ATTACH_GAME(2); ATTACH_GAME(3); ATTACH_GAME(4); ATTACH_GAME(5); ATTACH_GAME(6);
#undef ATTACH_GAME
#undef ATTACH
    actions::define("BattleResultPatches.ClearAllBattleMenuFlags", [] { result_clear_all_battle_states(); });
    actions::define("BattleResult.RebuildStatDeltas", [] {
        if (g_phase == ResultPhase::Status && displayed::container_visible(g_status_owner)) status_rows(g_status_owner);
        else if (g_phase == ResultPhase::Weapon && displayed::container_visible(g_point_owner)) weapon_page(g_point_owner);
        else return;
        if (g_pending_page) {
            auto configure = g_pending_page->configure;
            g_pending_page->configure = [configure](ui::review::Options& options) {
                if (configure) configure(options);
                options.announce = false;
            };
        }
    });
    // Native session ownership starts before any phase content/animation is ready.
    queries::define("ReviewBuffer.IsResults", [] { return g_phase != ResultPhase::None && unity::alive(g_menu); });
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
    { timing::Scope part("Result notification reset"); g_notification_session.reset(); }
    { timing::Scope part("Result skill cache reset"); displayed::clear_skill_changes(); }
    { timing::Scope part("Result pending page reset"); g_pending_page.reset(); }
    g_phase = ResultPhase::None; g_menu = nullptr; g_skill_headline.clear();
    g_announced_points = g_announced_items = false; ++g_generation;
    g_point_animation_pending = false; g_point_owner = nullptr; g_status_owner = nullptr;
    { timing::Scope part("Result counter reset"); stop_exp_counter_if_playing(); }
    { timing::Scope part("Result frame cancellation"); frame::cancel_label(kMonitorLabel); frame::cancel_label("BattleResult."); }
    { timing::Scope part("Result review reset"); ui::review::clear(); }
}
}
