// check-sources: field/baked_map.cpp field/baked_router.cpp field/map_state_program.cpp field/field_refs.cpp field/entity_filter.cpp field/npc_names.cpp json.cpp text.cpp
#define main existing_route_worker_main
#include "route_worker_check.cpp"
#undef main

int main() {
    using namespace field;using namespace field::baked_nav;
    int failures=0;
    const auto expect=[&](bool ok,const char* why){if(!ok){++failures;std::printf("FAIL %s\n",why);}};
    std::vector<std::string> storage;storage.reserve(6);
    for(int n=1;n<=6;++n) {
        fixture::game_id=static_cast<game::Id>(n);
        std::ifstream file("data/ff"+std::to_string(n)+"/maps.bin",std::ios::binary);
        storage.emplace_back(std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>());
        expect(baked::load(storage.back().data(),storage.back().size()),"real game bake loads");
        for(int i=0;i<baked::submap_count();++i)baked::grids(*baked::submap_at(i));
        g_loaded=true;g_prepared=true;
        std::ifstream states("data/ff"+std::to_string(n)+"/mapstate.json");
        const std::string state_bytes((std::istreambuf_iterator<char>(states)),{});json::parse(state_bytes,fixture::records);
        map_state::Flags flags;for(auto& v:flags.scenario)v=0;for(auto& v:flags.treasure)v=0;for(auto& v:flags.global)v=0;
        bool checked=false;
        std::thread worker([&]{
            for(int i=0;i<baked::submap_count()&&!checked;++i) {
                const auto* source=baked::submap_at(i);if(source->world||source->first_map_id<=0)continue;
                const auto& grid=baked::grids(*source);
                for(const auto& edge:edges_of(grid,source->first_map_id)) {
                    const auto* dest=baked::submap_by_map_id(edge.dest_map_id);
                    if(!dest||dest==source||dest->world||baked::is_trap(*edge.object))continue;
                    map_state::Resolver state(flags,fixture::records);
                    const auto* arrival=point_in(*dest,edge.dest_point,state);if(!arrival)continue;
                    RouteSnapshot input(state);input.submap=source;input.map_id=source->first_map_id;
                    input.goal.map_id=edge.dest_map_id;input.goal.cell={arrival->layer,arrival->tx,arrival->ty};
                    input.kind=TargetKind::Spot;input.locale=L"en";
                    bool start=false;
                    for(const auto& d:std::vector<Cell>{{0,0,-1},{0,1,0},{0,0,1},{0,-1,0}}) {
                        Cell at{edge.object->layer,edge.object->tx+d.x,edge.object->ty+d.y};
                        auto movement=walk_request(*source,at);
                        if(baked_route::open(movement,grid,at)){input.layer=at.layer;input.position=world_of_cell(*source,at);start=true;break;}
                    }
                    if(!start)continue;
                    const auto first=plan_snapshot(input);
                    if(!first.path.success||!first.pending.incoming_transition||first.pending.incoming_trap)continue;
                    input.pending=first.pending;
                    const auto planned=*input.pending.incoming_transition;
                    {SnapshotScope scope(input);
                        note_trap_start(input.map_id,{planned.entry.layer,planned.entry.x+8,planned.entry.y+8});
                        record_map_arrival(planned.destination,planned.arrival);
                        expect(input.pending.completed_transitions.empty(),"unobserved/distant transition cannot falsely consume an edge");
                        note_trap_start(input.map_id,planned.entry);
                        record_map_arrival(planned.destination,planned.arrival);
                    }
                    expect(input.pending.completed_transitions.size()==1,"observed ordinary transition records the exact directed edge");
                    input.active={};input.previous_active.reset();
                    // Make every outgoing edge already completed for this exact goal:
                    // stale graph state must stop, never issue another cycling leg.
                    for(const auto& e:edges_of(grid,input.map_id))input.pending.completed_transitions.push_back({source->first_map_id,e.object->entity_id(),e.dest_map_id,e.object->tx,e.object->ty});
                    const auto rejected=plan_snapshot(input);
                    expect(!rejected.path.success&&rejected.path.transition_repeat_blocked&&!rejected.pending.valid&&
                        !rejected.pending.completed_transitions.empty(),"repeated directed crossings stop with bounded receipts retained");
                    auto changed=flags;changed.scenario[0]=1;
                    RouteSnapshot changed_input(map_state::Resolver(changed,fixture::records));
                    changed_input.submap=source;changed_input.map_id=input.map_id;changed_input.layer=input.layer;
                    changed_input.position=input.position;changed_input.goal=input.goal;changed_input.kind=input.kind;
                    changed_input.locale=input.locale;changed_input.pending=rejected.pending;
                    const auto renewed=plan_snapshot(changed_input);
                    expect(!renewed.path.transition_repeat_blocked,"changed native progression state retires ordinary crossing history");
                    std::printf("FF%d: real directed-edge observation/repeat/state-change checks passed on map%d\n",n,input.map_id);
                    checked=true;break;
                }
            }
        });worker.join();
        expect(checked,"each game supplies a real non-trap cross-floor case");
    }
    expect(fixture::wrong_owner==0,"transition planning/receipt checks use no worker-native pointers");
    std::printf("Transition route check: %d failures\n",failures);return failures?1:0;
}
