// check-sources: field/entity_filter.cpp field/npc_names.cpp field/baked_map.cpp field/baked_router.cpp field/map_state_program.cpp json.cpp text.cpp
#include "../src/core/field/entity_filter.h"
#include <cstdio>
#include <thread>
#include <chrono>
#include <fstream>
namespace field::baked_nav {
std::vector<int> exit_distances(ExitSnapshot&,const std::vector<ExitTarget>&,std::function<bool()>) { return {}; }
}
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
    input.rows[5].route_distance=-1; input.rows[6].route_distance=20;
    result=build(input);
    expect(std::find(result.order.begin(),result.order.end(),6)!=result.order.end() &&
        std::find(result.order.begin(),result.order.end(),5)==result.order.end(),"blocked nearest tile cannot prune a reachable sibling exit");
    input.rows[5].route_distance=50; input.rows[6].route_distance=20; input.sort=false;
    result=build(input);
    expect(std::find(result.order.begin(),result.order.end(),6)!=result.order.end(),"grouping chooses shortest usable route independently of sort order");
    input.world=true; input.exits=false; input.rows[5].route_distance=-1;
    result=build(input);
    expect(std::find(result.order.begin(),result.order.end(),5)==result.order.end() && result.exits.size()==1,
        "unreachable world trigger is pruned from list and audio exit cache with grouping off");
    input.world=false; input.exits=true; input.sort=true; input.rows[5].route_distance=input.rows[6].route_distance=-1;
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
    const auto categories = build_categories(input);
    for (int category = 0; category < 6; ++category) {
        input.category = static_cast<Category>(category);
        const auto expected = build(input);
        const auto& actual = categories.views[category];
        expect(actual.order == expected.order && actual.restored == expected.restored && actual.continued == expected.continued,
            "prepared category matches the independent native-policy filter and selection");
    }
    {
        FilterInput variants;variants.current_map=50;variants.selected_has=true;variants.selected_map=50;
        variants.selected_entity_id=32;variants.selected_category=Category::Events;variants.selected_raw=L"original";
        variants.continued_goal=true;variants.continued_map=50;variants.continued_entity_id=32;variants.continued_raw=L"original";
        FilterRow first;first.index=0;first.kind=Kind::Event;first.category=Category::Events;first.entity_id=32;
        first.remote_map=50;first.raw_name=L"replacement";
        auto original=first;original.index=1;original.raw_name=L"original";
        variants.rows={first,original};
        auto view=build(variants);
        expect(view.restored>=0&&view.order[view.restored]==1&&view.continued>=0&&view.order[view.continued]==1,
            "collocated story variants retain their exact developer identity across floor restoration");
        variants.rows.pop_back();view=build(variants);
        expect(view.restored==-1&&view.continued==-1,"a missing objective never rebinds to a collocated replacement story variant");
    }
    // Large owned lists are prepared once, not recaptured/renamed for each key.
    FilterInput large;
    large.sort = true;
    for (size_t i = 0; i < 10000; ++i) {
        FilterRow value; value.index = i; value.kind = Kind::MapExit;
        value.category = Category::MapExits; value.name = L"Exit";
        value.position = {static_cast<float>(i % 256), static_cast<float>(i / 256), 0};
        large.rows.push_back(std::move(value));
    }
    const auto started = std::chrono::steady_clock::now();
    const auto prepared = build_categories(large);
    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    expect(prepared.views[0].order.size() == 10000 && prepared.views[3].order.size() == 10000,
        "large-list categories retain every entity without a per-frame entry cap");
    std::printf("Prepared 10,000 rows / six categories: %.3f ms (offline computation)\n", elapsed);
    int checks=0;
    large.cancelled=[&] { return ++checks>12000; };
    const auto retired=build_categories(large);
    expect(retired.views[0].order.empty()&&checks==12001,
        "obsolete destination refresh exits during row preparation/sort without finishing all six views");
    {
        const auto read=[](const char* path){std::ifstream f(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});};
        const auto bytes=read("data/ff1/maps.bin");expect(baked::load(bytes.data(),bytes.size()),"real FF1 entry-guard bake loads");
        json::Value records;expect(json::parse(read("data/ff1/mapstate.json"),records),"real FF1 entry-guard records load");
        const auto* sm=baked::submap_by_map_id(1);expect(sm!=nullptr,"real world event floor exists");
        if(sm) {
            const baked::Object* trigger=nullptr;for(const auto& object:baked::grids(*sm).objects)
                if(object.entity_id()==89&&object.type==baked::ObjType::Event)trigger=&object;
            expect(trigger!=nullptr,"native conditional one-use event exists");
            if(trigger)for(int profile=0;profile<3;++profile) {
                map_state::Flags flags;if(profile<2){for(auto& v:flags.scenario)v=profile;for(auto& v:flags.treasure)v=profile;for(auto& v:flags.global)v=profile;}
                FilterInput stateful;stateful.current_map=1;stateful.state=std::make_shared<map_state::Resolver>(flags,records);
                FilterRow row;row.index=0;row.kind=Kind::Event;row.category=Category::Events;row.entity_id=89;row.raw_name=baked::object_identifier(*trigger);
                row.position={(trigger->tx-sm->width*.5f)*16+8,(sm->height*.5f-trigger->ty)*16-8,0};stateful.rows={row};
                const auto view=build_categories(stateful);
                expect(view.views[0].order.empty()==(profile==1),"actual list worker retires the native used-event branch while preserving unused/unknown states");
            }
        }
    }
    std::printf("Entity filter: owned worker sorting, stable NPC groups, inclusion guards, selection: %d failures\n", failures);
    return failures ? 1 : 0;
}
