#include <algorithm>
#include <deque>
#include <climits>
#include "review_buffer.h"
#include "navigation_buffer.h"
#include "../frame.h"
#include "preferences.h"
#include "../core.h"
#include "../displayed_ui.h"
#include "../say.h"
#include "../modtext.h"
#include "../text.h"
#include "../ui_util.h"
#include "../input/input.h"
#include "../worker_job.h"

namespace ui::review {
namespace {
void* g_owner = nullptr;
NavigationBuffer g_buffer;
std::vector<std::wstring> g_lines;
std::wstring g_headline;
Options g_options;
worker_job::Owner g_work;
bool g_build_pending = false;
bool g_replaying = false;
bool g_executing_action = false;
uint64_t g_generation = 0;
std::wstring g_full_details;
std::wstring g_phase_changes;
using Action = void (*)();
std::deque<Action> g_pending_actions;
Domain g_domain = Domain::Results;
void speak(std::wstring line) {
    if (!active() || line.empty()) return;
    ffpr::say(ui::menu_position(line, g_buffer.index(), g_buffer.count()), true);
}
int install() {
    queries::define("ReviewBuffer.IsActive", [] { return active(); });
    queries::define("ReviewBuffer.IsInformation", [] { return active() && domain() == Domain::Information; });
    actions::define("ReviewBuffer.Details", [] { details(); });
    actions::define("ReviewBuffer.Next", [] { next(); });
    actions::define("ReviewBuffer.Previous", [] { previous(); });
    actions::define("ReviewBuffer.First", [] { first(); });
    actions::define("ReviewBuffer.Last", [] { last(); });
    using namespace input;
    add_binding(Key::DownArrow, Mod::None, Context::Results, "ReviewBuffer.Next", "Next displayed result line");
    add_binding(Key::UpArrow, Mod::None, Context::Results, "ReviewBuffer.Previous", "Previous displayed result line");
    add_binding(Key::LeftArrow, Mod::None, Context::Results, "ReviewBuffer.First", "First displayed result line");
    add_binding(Key::RightArrow, Mod::None, Context::Results, "ReviewBuffer.Last", "Last displayed result line");
    add_binding(Key::I, Mod::None, Context::Results, "ReviewBuffer.Details", "Read displayed results");
    add_binding(Key::DownArrow, Mod::None, Context::Information, "ReviewBuffer.Next", "Next displayed comparison line");
    add_binding(Key::UpArrow, Mod::None, Context::Information, "ReviewBuffer.Previous", "Previous displayed comparison line");
    add_binding(Key::LeftArrow, Mod::None, Context::Information, "ReviewBuffer.First", "First displayed comparison line");
    add_binding(Key::RightArrow, Mod::None, Context::Information, "ReviewBuffer.Last", "Last displayed comparison line");
    events::on(events::Kind::BattleStart, [] { clear(); });
    events::on(events::Kind::BattleEnd, [] { clear(); });
    return 0;
}
core::Install s_install{"UI.ReviewBuffer", &install, 90};

void begin_replay(uint64_t generation) {
    if (g_pending_actions.empty()) { g_replaying = false; return; }
    g_replaying = true;
    frame::every_frame([generation]() {
        if (generation != g_generation || !active()) {
            if (generation == g_generation) {
                g_pending_actions.clear();
                g_replaying = false;
            }
            return false;
        }
        for (int i = 0; i < 4 && !g_pending_actions.empty(); ++i) {
            const auto action = g_pending_actions.front();
            g_pending_actions.pop_front();
            g_executing_action = true;
            action();
            g_executing_action = false;
        }
        if (g_pending_actions.empty()) {
            g_replaying = false;
            return false;
        }
        return true;
    }, "ReviewBuffer.Replay", INT_MAX);
}

void show_impl(void* owner, const std::wstring& headline, std::vector<std::wstring> lines,
               Options options, bool preserve_actions) {
    std::deque<Action> saved_actions;
    if (preserve_actions && g_owner == owner) saved_actions = std::move(g_pending_actions);
    clear();
    if (!displayed::container_visible(owner)) return;
    if (preserve_actions) g_pending_actions = std::move(saved_actions);
    g_owner = owner;
    g_domain = options.domain;
    g_headline = headline;
    g_options = options; // available to same-page replacements while this build is pending
    g_build_pending = true; // this displayed context owns review input immediately
    const auto generation = g_generation;
    auto notification_session = options.notification_session;
    // Keep mutable native-owned session state out of the worker snapshot.
    const auto notification_history = notification_session ? notification_session->history : nullptr;
    options.notification_session.reset();
    const bool filter_notifications = notification_session && !preserve_actions &&
        !options.notifications.empty();
    // Essential ready text goes straight to the speech worker. It must not wait
    // behind route computation or full-table assembly on the compute lane.
    if (options.announce && !preserve_actions) {
        std::vector<std::wstring> ready;
        if (!headline.empty()) ready.push_back(headline);
        for (const auto& total : options.summary) if (!total.empty()) ready.push_back(total);
        if (!filter_notifications)
            for (const auto& line : options.notifications) if (!line.empty()) ready.push_back(line);
        if (!ready.empty()) ffpr::say_parts(std::move(ready), true);
    }
    struct Input {
        std::wstring headline; std::vector<std::wstring> lines; Options options; bool autodetail;
        std::shared_ptr<const ResultNotificationHistory> history;
        bool filter_notifications;
    };
    struct Built {
        NavigationBuffer buffer;
        std::vector<std::wstring> lines;
        Options options;
        std::wstring full, headline, details, brief;
        bool autodetail = false;
        std::shared_ptr<const ResultNotificationHistory> notification_history;
    };
    worker_job::start(g_work, Input{headline, std::move(lines), std::move(options), prefs::auto_detail_enabled(),
                                  notification_history, filter_notifications},
        [](Input& input) {
            Built built; auto& options = input.options;
            if (input.filter_notifications) {
                auto selection = select_result_notifications(options.notifications, input.history);
                options.notifications = std::move(selection.lines);
                built.notification_history = std::move(selection.history);
            }
            built.details = text::join(input.lines, L". ");
            built.headline = std::move(input.headline);
            if (options.include_headline && !built.headline.empty()) built.lines.push_back(built.headline);
            for (const auto& line : options.summary) if (!line.empty()) built.lines.push_back(line);
            for (const auto& line : options.notifications) if (!line.empty()) built.lines.push_back(line);
            for (auto& line : input.lines) if (!line.empty()) built.lines.push_back(std::move(line));
            std::vector<NavigationBuffer::Entry> entries;
            for (const auto& line : built.lines) entries.push_back([line] { return line; });
            built.buffer = NavigationBuffer(std::move(entries), options.groups);
            if (!built.lines.empty()) built.buffer.set_index(std::max(0, std::min(options.restore_index, static_cast<int>(built.lines.size()) - 1)));
            built.full = text::join(built.lines, L". ");
            if (!options.include_headline && !built.headline.empty())
                built.full = built.headline + (built.full.empty() ? L"" : L". " + built.full);
            if (options.brief) built.brief = text::join(*options.brief, L". ");
            built.options = std::move(options); built.autodetail = input.autodetail;
            return built;
        }, [owner, generation, notification_session, filter_notifications](Built built) {
            if (generation != g_generation || g_owner != owner || !displayed::container_visible(owner)) return;
            g_build_pending = false;
            g_options = std::move(built.options); g_lines = std::move(built.lines);
            g_options.notification_session = notification_session;
            g_buffer = std::move(built.buffer); g_full_details = std::move(built.full);
            g_phase_changes = std::move(built.brief);
            if (g_options.announce) {
                if (filter_notifications) {
                    if (!g_options.notifications.empty()) ffpr::say_parts(g_options.notifications, false);
                    // A cancelled/hidden build or muted game speech must not count
                    // an unseen notification as announced in a later buffer.
                    if (prefs::speech_enabled()) notification_session->history = std::move(built.notification_history);
                }
                if (built.autodetail && !g_options.defer_details) {
                    const auto& details = g_options.brief_details ? g_phase_changes : built.details;
                    if (!details.empty()) ffpr::say(details, false);
                }
                if ((!built.autodetail || g_options.announce_brief_with_details) &&
                    g_options.announce_brief && !g_options.defer_details && !g_phase_changes.empty()) ffpr::say(g_phase_changes, false);
            }
            begin_replay(generation);
        });
}
}
bool active() { return displayed::container_visible(g_owner) && (g_build_pending || !g_buffer.empty()); }
bool owns(void* owner) { return owner && owner == g_owner; }
Domain domain() { return g_domain; }
int index() { return g_buffer.index(); }
void clear() {
    g_work.cancel(); frame::cancel_label("ReviewBuffer.Replay"); ++g_generation;
    g_build_pending = false; g_replaying = false; g_executing_action = false; g_pending_actions.clear();
    g_owner = nullptr; g_buffer = {}; g_lines.clear(); g_headline.clear(); g_options = {}; g_full_details.clear(); g_phase_changes.clear();
}
void replace_lines(std::vector<std::wstring> lines, int restore_index, std::optional<std::vector<std::wstring>> brief) {
    const auto headline = g_headline;
    Options options = g_options;
    options.announce = g_build_pending && g_options.announce;
    options.restore_index = restore_index;
    if (brief) options.brief = std::move(brief);
    show_impl(g_owner, headline, std::move(lines), options, true);
}
bool pending_action(void (*action)());
void show(void* owner, const std::wstring& headline, std::vector<std::wstring> lines, Options options) {
    show_impl(owner, headline, std::move(lines), std::move(options), false);
}
void begin(void* owner, const std::wstring& headline, Options options) {
    clear();
    if (!displayed::container_visible(owner)) return;
    g_owner = owner; g_domain = options.domain; g_headline = headline; g_options = std::move(options);
    g_build_pending = true;
}
void details() {
    if (!active()) return;
    if (!g_options.details_action.empty()) { actions::run(g_options.details_action.c_str()); return; }
    if (pending_action(&details)) return;
    if (g_options.brief) {
        // Explicit I/West repeats the automatic current-phase summary, regardless
        // of cursor position or automatic-summary/Autodetail preferences.
        ffpr::say_parts({g_headline, g_phase_changes.empty() ? T(L"No stat changes") : g_phase_changes}, true);
        return;
    }
    if (!g_full_details.empty()) ffpr::say(g_full_details, false);
}
bool pending_action(void (*action)()) {
    if (g_executing_action || (!g_build_pending && !g_replaying)) return false;
    g_pending_actions.push_back(action);
    return true;
}
void next() { if (active() && !pending_action(&next)) speak(g_buffer.next()); }
void previous() { if (active() && !pending_action(&previous)) speak(g_buffer.previous()); }
void first() { if (active() && !pending_action(&first)) speak(g_buffer.jump_top()); }
void last() { if (active() && !pending_action(&last)) speak(g_buffer.jump_bottom()); }
void current() { if (active() && !pending_action(&current)) speak(g_buffer.current()); }
void next_group() { if (active() && !pending_action(&next_group)) speak(g_buffer.next_group()); }
void previous_group() { if (active() && !pending_action(&previous_group)) speak(g_buffer.previous_group()); }
}
