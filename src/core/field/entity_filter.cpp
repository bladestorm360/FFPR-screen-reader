#include "entity_filter.h"
#include <algorithm>
#include <cmath>
namespace field::filter_worker {
namespace {
float point_distance(Vec3 a, Vec3 b) {
    const auto x=a.x-b.x, y=a.y-b.y, z=a.z-b.z;
    return static_cast<float>(std::sqrt(static_cast<double>(x*x+y*y+z*z)));
}
void restore_selection(Filtered& result, const FilterInput& input) {
    if (input.continued_goal) for (size_t i = 0; i < result.order.size(); ++i) {
        const auto& row = input.rows[result.order[i]];
        const bool map_matches = row.remote_map ? row.remote_map == input.continued_map
            : input.current_map == input.continued_map;
        if (map_matches && (row.remote_map || row.cell_valid)
            && row.cell_x == input.continued_x && row.cell_y == input.continued_y) {
            result.continued = static_cast<int>(i); break;
        }
    }
    for (size_t i = 0; i < result.order.size(); ++i)
        if (input.rows[result.order[i]].selected) { result.restored = static_cast<int>(i); return; }
    if (input.selected_has) {
        for (size_t i = 0; i < result.order.size(); ++i) {
            const auto& row = input.rows[result.order[i]];
            const bool match = input.selected_remote > 0
                ? row.remote_map == input.selected_remote && row.cell_x == input.selected_x && row.cell_y == input.selected_y
                : !row.remote_map && row.category == input.selected_category && point_distance(row.position, input.selected_position) < 0.5f;
            if (match) { result.restored = static_cast<int>(i); return; }
        }
        if (!input.selected_name.empty() && !input.selected_remote)
            for (size_t i = 0; i < result.order.size(); ++i) {
                const auto& row = input.rows[result.order[i]];
                if (!row.remote_map && row.category == input.selected_category && row.name == input.selected_name) {
                    result.restored = static_cast<int>(i); break;
                }
            }
    }
}
}
Filtered build(FilterInput& input) {
    Filtered result;
    std::vector<npc_names::Entry> names;
    for (auto& row : input.rows) {
        if (row.kind == Kind::NPC)
            names.push_back({&row.name, row.label, row.remote_map, 0,
                row.remote_map ? static_cast<float>(row.cell_x) : row.numbering_position.x,
                row.remote_map ? static_cast<float>(row.cell_y) : -row.numbering_position.y});
    }
    npc_names::renumber(names); // before category/distance/reachability inclusion
    for (const auto& row : input.rows) {
        if (row.kind == Kind::MapExit && !row.remote_map && (!input.world || row.route_distance>=0)) result.exits.push_back(row.numbering_position);
        result.names.push_back(row.name);
        if (input.category == Category::All || input.category == row.category) result.order.push_back(row.index);
    }
    if (input.sort) std::stable_sort(result.order.begin(), result.order.end(), [&](size_t a, size_t b) {
        const auto distance = [&](size_t i) {
            const auto& row = input.rows[i];
            return row.remote_map ? 1.0e9f + static_cast<float>(i) : point_distance(row.position, input.player);
        };
        return distance(a) < distance(b);
    });
    std::unordered_set<std::wstring> pads;
    std::unordered_map<int,size_t> exits;
    if (input.exits) for (size_t index : result.order) {
        const auto& row = input.rows[index];
        if (row.kind != Kind::MapExit || row.destination <= 0) continue;
        const auto found = exits.find(row.destination);
        if (found == exits.end()) { exits.emplace(row.destination,index); continue; }
        const auto& old = input.rows[found->second];
        // Grouping is independent from list sorting. Prefer the shortest usable
        // route; if every indoor exit is blocked, retain one for its explanation.
        const bool better = row.route_distance >= 0 &&
            (old.route_distance < 0 || row.route_distance < old.route_distance);
        const bool fallback = row.route_distance < 0 && old.route_distance < 0 && input.sort &&
            point_distance(row.position,input.player) < point_distance(old.position,input.player);
        if (better || fallback) found->second = index;
    }
    result.order.erase(std::remove_if(result.order.begin(), result.order.end(), [&](size_t index) {
        const auto& row = input.rows[index];
        if (input.world && row.kind == Kind::MapExit && row.route_distance < 0) return true;
        // Preserve only the two existing, user-approved grouping guards.
        if (input.pads && !row.pad.empty() && !pads.insert(row.pad).second) return true;
        if (input.exits && row.kind == Kind::MapExit && row.destination > 0 && exits[row.destination] != index) return true;
        return input.layer && row.kind == Kind::Event && row.event_type == L"ToLayer";
    }), result.order.end());
    restore_selection(result, input);
    return result;
}
Categories build_categories(FilterInput& input) {
    if (input.navigation) {
        std::vector<baked_nav::ExitTarget> targets;
        std::vector<size_t> indices;
        for (size_t i=0; i<input.rows.size(); ++i) {
            const auto& row = input.rows[i];
            if (row.kind != Kind::MapExit) continue;
            targets.push_back({row.position,row.remote_map,row.destination,{row.cell_layer,row.cell_x,row.cell_y}});
            indices.push_back(i);
        }
        const auto distances = baked_nav::exit_distances(*input.navigation,targets,input.cancelled);
        for (size_t i=0; i<distances.size(); ++i) input.rows[indices[i]].route_distance = distances[i];
    }
    input.category = Category::All;
    Categories result;
    auto& all = result.views[0];
    all = build(input);
    for (size_t i = 0; i < all.order.size(); ++i) {
        const auto index = all.order[i];
        auto& view = result.views[static_cast<size_t>(input.rows[index].category)];
        if (&view == &all) continue;
        view.order.push_back(index);
    }
    for (size_t category = 1; category < result.views.size(); ++category)
        restore_selection(result.views[category], input);
    return result;
}
}
