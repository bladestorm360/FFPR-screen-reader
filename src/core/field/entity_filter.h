#pragma once
#include "field_internal.h"
#include "npc_names.h"
#include "baked_nav.h"
#include <array>
namespace field::filter_worker {
using Vec3 = unity::Vec3;
struct FilterRow {
    size_t index = 0;
    Kind kind = Kind::Event;
    Category category = Category::All;
    Vec3 position, numbering_position;
    int remote_map = 0, cell_x = 0, cell_y = 0, cell_layer = 1, destination = 0;
    std::wstring name, pad, event_type;
    npc_names::Label label;
    bool selected = false;
    bool cell_valid = false;
    int route_distance = -1;
};
struct FilterInput {
    std::vector<FilterRow> rows;
    Vec3 player, selected_position;
    bool sort = false, pads = false, exits = false, layer = false, selected_has = false;
    int selected_remote = 0, selected_x = 0, selected_y = 0;
    Category category = Category::All, selected_category = Category::All;
    std::wstring selected_name;
    bool continued_goal = false;
    int continued_map = 0, continued_x = 0, continued_y = 0, current_map = 0;
    bool world = false;
    std::shared_ptr<baked_nav::ExitSnapshot> navigation;
    std::function<bool()> cancelled;
};
struct Filtered {
    std::vector<size_t> order;
    std::vector<std::wstring> names;
    std::vector<Vec3> exits;
    int restored = -1;
    int continued = -1;
};
Filtered build(FilterInput& input);
// One worker pass prepares every category. Only view 0 owns names/exits; the
// other views share those source rows and contain their own selection indices.
struct Categories { std::array<Filtered, 6> views; };
Categories build_categories(FilterInput& input);
}
