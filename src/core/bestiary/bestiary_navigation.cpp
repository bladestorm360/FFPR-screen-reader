// Ports FF1 Menus/BestiaryNavigationReader.cs: the detail view's stats as a virtual buffer with
// dynamic group boundaries and group names; all wrap and group logic is ui::NavigationBuffer's.
// The input area binds Up/Down (+Shift, +Ctrl) to the actions below, gated on
// "BestiaryNavigationTracker.IsNavigationActive" + "ValidateState" (KeyContext.BestiaryDetail).
#include <optional>
#include <deque>
#include <climits>
#include "bestiary.h"
#include "../core.h"
#include "../frame.h"
#include "../modtext.h"
#include "../say.h"
#include "../ui_util.h"
#include "../ui/navigation_buffer.h"
#include "../worker_job.h"

namespace ffpr::bestiary {
namespace {

std::optional<ui::NavigationBuffer> g_buffer;   // C# null when not set
worker_job::Owner g_work;
bool g_build_pending = false;
bool g_replaying = false;
uint64_t g_generation = 0;
using Op = std::wstring (ui::NavigationBuffer::*)();
std::deque<Op> g_pending_ops;

// GetGroupDisplayName
std::wstring group_display_name(StatGroup group)
{
    switch (group) {
    case StatGroup::MonsterData: return T(L"Monster Data");
    case StatGroup::Status: return T(L"Status");
    case StatGroup::Options: return T(L"Rewards");
    case StatGroup::Items: return T(L"Items");
    case StatGroup::Properties: return T(L"Properties");
    default: return T(L"Other");
    }
}

// Move: gate, run the op, speak with the position within the group.
void perform(Op op)
{
    std::wstring value = ((*g_buffer).*op)();
    if (!value.empty()) {
        auto [local_index, group_count] = g_buffer->current_group_position();
        ffpr::say(ui::menu_position(value, local_index, group_count), true);
    }
}

void move(Op op)
{
    if (!nav_is_active()) return;
    if (g_build_pending || g_replaying) {
        g_pending_ops.push_back(op);
        return;
    }
    perform(op);
}

void start_replay(uint64_t generation)
{
    if (g_pending_ops.empty()) { g_replaying = false; return; }
    g_replaying = true;
    frame::every_frame([generation]() {
        if (generation != g_generation || !tracker().is_navigation_active || !g_buffer) {
            if (generation == g_generation) {
                g_pending_ops.clear();
                g_replaying = false;
            }
            return false;
        }
        for (int i = 0; i < 4 && !g_pending_ops.empty(); ++i) {
            const auto op = g_pending_ops.front();
            g_pending_ops.pop_front();
            perform(op);
        }
        if (g_pending_ops.empty()) {
            g_replaying = false;
            return false;
        }
        return true;
    }, "BestiaryNavigation.Replay", INT_MAX);
}

int install()
{
    actions::define("BestiaryNavigationReader.NavigateNext", [] { navigate_next(); });
    actions::define("BestiaryNavigationReader.NavigatePrevious", [] { navigate_previous(); });
    actions::define("BestiaryNavigationReader.JumpToNextGroup", [] { jump_to_next_group(); });
    actions::define("BestiaryNavigationReader.JumpToPreviousGroup", [] { jump_to_previous_group(); });
    actions::define("BestiaryNavigationReader.JumpToTop", [] { jump_to_top(); });
    actions::define("BestiaryNavigationReader.JumpToBottom", [] { jump_to_bottom(); });
    actions::define("BestiaryNavigationReader.ReadCurrentStat", [] { read_current_stat(); });
    queries::define("BestiaryNavigationReader.IsActive", [] { return nav_is_active(); });
    return 0;
}

core::Install s_install{"BestiaryNavigation", &install, 131};

} // namespace

void nav_initialize(const std::vector<StatEntry>& entries)
{
    g_work.cancel();
    frame::cancel_label("BestiaryNavigation.Replay");
    const auto generation = ++g_generation;
    g_buffer.reset();
    g_pending_ops.clear();
    g_replaying = false;
    g_build_pending = true;

    struct Input {
        std::vector<StatEntry> entries;
        std::wstring locale;
    };
    struct Built {
        ui::NavigationBuffer buffer;
    };
    worker_job::start(g_work, Input{entries, modtext::lang()},
        [](Input& input) {
            modtext::LocaleScope locale(input.locale);
            std::vector<ui::NavigationBuffer::Entry> funcs;
            std::vector<int> group_starts;
            std::vector<std::wstring> group_names;
            if (!input.entries.empty()) {
                StatGroup last_group = input.entries[0].group;
                group_starts.push_back(0);
                group_names.push_back(group_display_name(input.entries[0].group));
                for (size_t i = 0; i < input.entries.size(); ++i) {
                    std::wstring line = input.entries[i].to_string();
                    funcs.push_back([line] { return line; });
                    if (i > 0 && input.entries[i].group != last_group) {
                        group_starts.push_back(static_cast<int>(i));
                        group_names.push_back(group_display_name(input.entries[i].group));
                        last_group = input.entries[i].group;
                    }
                }
            }
            return Built{ui::NavigationBuffer(std::move(funcs), std::move(group_starts), std::move(group_names))};
        }, [generation](Built built) {
            if (generation != g_generation) return;
            g_build_pending = false;
            if (!tracker().is_navigation_active) {
                g_pending_ops.clear();
                return;
            }
            if (built.buffer.empty()) {
                g_pending_ops.clear();
                return;
            }
            g_buffer.emplace(std::move(built.buffer));
            start_replay(generation);
        });
}

void nav_reset()
{
    g_work.cancel();
    frame::cancel_label("BestiaryNavigation.Replay");
    ++g_generation;
    g_build_pending = false;
    g_replaying = false;
    g_pending_ops.clear();
    g_buffer.reset();
}

bool nav_is_active() { return tracker().is_navigation_active && (g_build_pending || (g_buffer && !g_buffer->empty())); }

void navigate_next() { move(&ui::NavigationBuffer::next); }
void navigate_previous() { move(&ui::NavigationBuffer::previous); }
void jump_to_next_group() { move(&ui::NavigationBuffer::next_group); }
void jump_to_previous_group() { move(&ui::NavigationBuffer::previous_group); }
void jump_to_top() { move(&ui::NavigationBuffer::jump_top); }
void jump_to_bottom() { move(&ui::NavigationBuffer::jump_bottom); }
void read_current_stat() { move(&ui::NavigationBuffer::current); }

} // namespace ffpr::bestiary
