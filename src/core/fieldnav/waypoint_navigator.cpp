// Ports FF1 Core/WaypointNavigator.cs: waypoint cycling and category filtering (separate from
// the entity navigator; waypoints have their own categories).
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>
#include "fieldnav_util.h"
#include "waypoints.h"
#include "../log.h"
#include "../modtext.h"

namespace ffpr::fieldnav::waypoints::nav {
namespace {

std::vector<EntityPtr> g_list;
int g_index = -1;
int g_category = All;

unity::Vec3 player_position()
{
    unity::Vec3 p{};
    player_local_position(p);   // zero when there is no player, as in C#
    return p;
}

/// LINQ OrderBy(distance): a stable sort on a key computed once per element.
void order_by_distance(unity::Vec3 player_pos)
{
    std::vector<std::pair<float, EntityPtr>> keyed;
    keyed.reserve(g_list.size());
    for (const EntityPtr& w : g_list) keyed.emplace_back(distance(player_pos, w->position), w);
    std::stable_sort(keyed.begin(), keyed.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (size_t i = 0; i < keyed.size(); ++i) g_list[i] = std::move(keyed[i].second);
}

void sort_by_distance()
{
    if (g_list.size() <= 1) return;

    EntityPtr current_selection = selected();
    order_by_distance(player_position());

    if (current_selection) {
        auto it = std::find(g_list.begin(), g_list.end(), current_selection);
        if (it != g_list.end()) g_index = static_cast<int>(it - g_list.begin());
    }
}

void sort_by_distance_preserving_selection(const std::wstring& waypoint_id_to_preserve)
{
    if (g_list.size() <= 1) return;

    order_by_distance(player_position());

    if (!waypoint_id_to_preserve.empty()) {
        for (size_t i = 0; i < g_list.size(); ++i) {
            if (g_list[i]->id == waypoint_id_to_preserve) {
                g_index = static_cast<int>(i);
                return;
            }
        }
    }
    g_index = 0;
}

} // namespace

EntityPtr selected()
{
    return (g_index >= 0 && g_index < static_cast<int>(g_list.size())) ? g_list[g_index] : nullptr;
}

int count() { return static_cast<int>(g_list.size()); }

int current_category() { return g_category; }

void refresh_list(const std::wstring& map_id)
{
    try {
        EntityPtr prev = selected();
        std::wstring previous_selection_id = prev ? prev->id : std::wstring();

        if (g_category == All)
            g_list = store::for_map(map_id);
        else
            g_list = store::for_category(map_id, g_category);

        sort_by_distance_preserving_selection(previous_selection_id);

        int n = count();
        if (n == 0)
            g_index = -1;
        else if (g_index >= n)
            g_index = n - 1;
        else if (g_index < 0)
            g_index = 0;
    } catch (...) {
        LOGF("[Waypoints] Error refreshing list");
        g_list.clear();
        g_index = -1;
    }
}

EntityPtr cycle_next()
{
    if (g_list.empty()) return nullptr;
    sort_by_distance();
    g_index = (g_index + 1) % count();
    return selected();
}

EntityPtr cycle_previous()
{
    if (g_list.empty()) return nullptr;
    sort_by_distance();
    g_index = (g_index - 1 + count()) % count();
    return selected();
}

std::wstring cycle_next_category(const std::wstring& map_id)
{
    g_category = (g_category + 1) % kCategoryCount;
    refresh_list(map_id);
    return category_name(g_category);
}

std::wstring cycle_previous_category(const std::wstring& map_id)
{
    g_category = (g_category - 1 + kCategoryCount) % kCategoryCount;
    refresh_list(map_id);
    return category_name(g_category);
}

std::wstring format_current_waypoint()
{
    EntityPtr waypoint = selected();
    if (!waypoint) return T(L"No waypoints");

    std::wstring description = waypoint->format_description(player_position());

    if (count() > 1)
        description += L", " + TF(L"{0} of {1}", {std::to_wstring(g_index + 1), std::to_wstring(count())});

    return description;
}

std::wstring category_announcement()
{
    std::wstring name = category_name(g_category);
    int n = count();
    std::wstring plural = n == 1 ? T(L"waypoint") : T(L"waypoints");
    return name + L": " + std::to_wstring(n) + L" " + plural;
}

void clear_selection()
{
    g_index = g_list.empty() ? -1 : 0;
}

} // namespace ffpr::fieldnav::waypoints::nav
