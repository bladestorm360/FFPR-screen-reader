// Ports FF1 Menus/BestiaryNavigationReader.cs: the detail view's stats as a virtual buffer with
// dynamic group boundaries and group names; all wrap and group logic is ui::NavigationBuffer's.
// The input area binds Up/Down (+Shift, +Ctrl) to the actions below, gated on
// "BestiaryNavigationTracker.IsNavigationActive" + "ValidateState" (KeyContext.BestiaryDetail).
#include <optional>
#include "bestiary.h"
#include "../core.h"
#include "../modtext.h"
#include "../say.h"
#include "../ui_util.h"
#include "../ui/navigation_buffer.h"

namespace ffpr::bestiary {
namespace {

std::optional<ui::NavigationBuffer> g_buffer;   // C# null when not set

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

using Op = std::wstring (ui::NavigationBuffer::*)();

// Move: gate, run the op, speak with the position within the group.
void move(Op op)
{
    if (!nav_is_active()) return;
    std::wstring value = ((*g_buffer).*op)();
    if (!value.empty()) {
        auto [local_index, group_count] = g_buffer->current_group_position();
        ffpr::say(ui::menu_position(value, local_index, group_count), true);
    }
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
    std::vector<ui::NavigationBuffer::Entry> funcs;
    std::vector<int> group_starts;
    std::vector<std::wstring> group_names;

    if (!entries.empty()) {
        StatGroup last_group = entries[0].group;
        group_starts.push_back(0);
        group_names.push_back(group_display_name(entries[0].group));
        for (size_t i = 0; i < entries.size(); ++i) {
            std::wstring line = entries[i].to_string();   // pre-rendered
            funcs.push_back([line] { return line; });
            if (i > 0 && entries[i].group != last_group) {
                group_starts.push_back(static_cast<int>(i));
                group_names.push_back(group_display_name(entries[i].group));
                last_group = entries[i].group;
            }
        }
    }
    g_buffer.emplace(std::move(funcs), std::move(group_starts), std::move(group_names));
}

void nav_reset() { g_buffer.reset(); }

bool nav_is_active() { return g_buffer && !g_buffer->empty() && tracker().is_navigation_active; }

void navigate_next() { move(&ui::NavigationBuffer::next); }
void navigate_previous() { move(&ui::NavigationBuffer::previous); }
void jump_to_next_group() { move(&ui::NavigationBuffer::next_group); }
void jump_to_previous_group() { move(&ui::NavigationBuffer::previous_group); }
void jump_to_top() { move(&ui::NavigationBuffer::jump_top); }
void jump_to_bottom() { move(&ui::NavigationBuffer::jump_bottom); }
void read_current_stat() { move(&ui::NavigationBuffer::current); }

} // namespace ffpr::bestiary
