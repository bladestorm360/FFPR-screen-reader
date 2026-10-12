// check-sources: field/entity_filter.cpp field/npc_names.cpp
#include "../src/core/field/entity_filter.h"
#include <cstdio>
#include <thread>
int main() {
    using namespace field; using namespace filter_worker;
    int failures = 0;
    auto expect = [&](bool ok, const char* reason) { if (!ok) { ++failures; std::printf("FAIL %s\n", reason); } };
    FilterInput input; input.sort = true; input.player = {};
    auto row = [&](Kind kind, Category category, std::wstring label, float distance, int floor = 0) {
        FilterRow value; value.index = input.rows.size(); value.kind = kind; value.category = category;
        value.name = label; value.label = npc_names::parse(label); value.position = {distance, 0, 0};
        value.numbering_position = value.position; value.remote_map = floor; value.cell_x = static_cast<int>(distance);
        input.rows.push_back(std::move(value));
    };
    row(Kind::NPC, Category::NPCs, L"7: Soldier", 2);
    row(Kind::NPC, Category::NPCs, L"Soldier２", 8);
    row(Kind::NPC, Category::NPCs, L"⑥ Old Man", 5);
    row(Kind::NPC, Category::NPCs, L"Soldier1", 1, 50);
    row(Kind::NPC, Category::NPCs, L"Soldier2", 2, 50);
    row(Kind::MapExit, Category::MapExits, L"Exit", 4); input.rows.back().destination = 100;
    row(Kind::MapExit, Category::MapExits, L"Exit", 6); input.rows.back().destination = 100;
    row(Kind::Event, Category::Events, L"Pad", 7); input.rows.back().pad = L"one";
    row(Kind::Event, Category::Events, L"Pad", 3); input.rows.back().pad = L"one";
    row(Kind::Event, Category::Events, L"Layer", 9); input.rows.back().event_type = L"ToLayer";
    Filtered result;
    std::thread worker([&] { result = build(input); }); worker.join();
    expect(result.names[0] == L"Soldier2" && result.names[1] == L"Soldier1" && result.names[2] == L"Old Man",
        "NPC source ordinals number exact tags before distance filtering");
    expect(result.names[3] == L"Soldier1" && result.names[4] == L"Soldier2", "remote floor numbering is independent");
    expect(result.order.size() == 10 && result.order.back() == 4, "default grouping preserves all rows and remote order");
    expect(result.exits.size() == 2, "unfiltered live exit cache includes both exits");
    input.pads = input.exits = input.layer = true; input.rows[1].selected = true;
    result = build(input);
    expect(result.order.size() == 7 && result.restored >= 0 && result.order[result.restored] == 1,
        "existing pad/exit/layer guards and selected identity survive worker sort");
    expect(result.exits.size() == 2, "navigation filters do not remove wall-tone exit locations");
    input.category = Category::NPCs; result = build(input);
    expect(result.order.size() == 5 && result.names[0] == L"Soldier2", "category filter does not renumber groups");
    input.rows[1].selected = false; input.selected_has = true; input.selected_remote = 50;
    input.selected_x = 2; input.selected_y = 0; result = build(input);
    expect(result.restored >= 0 && result.order[result.restored] == 4, "recreated remote row restores floor/cell selection");
    input.continued_goal = true; input.continued_map = 50; input.continued_x = 1;
    result = build(input);
    expect(result.continued >= 0 && result.order[result.continued] == 3,
        "pending destination is matched on the worker independently of ordinary selection");
    input.continued_map = input.current_map = 12; input.continued_x = 8;
    input.rows[1].cell_valid = true; result = build(input);
    expect(result.continued >= 0 && result.order[result.continued] == 1, "live destination uses copied verified cell");
    input.rows[1].cell_valid = false; result = build(input);
    expect(result.continued == -1, "invalid native cell capture cannot match a live destination");
    std::printf("Entity filter: owned worker sorting, stable NPC groups, inclusion guards, selection: %d failures\n", failures);
    return failures ? 1 : 0;
}
