// check-sources: field/baked_map.cpp field/baked_router.cpp field/map_state_program.cpp field/field_refs.cpp field/entity_filter.cpp field/npc_names.cpp json.cpp text.cpp
// All baked master-map roots and native state projections; real logged FFII route replay.
#define main existing_route_worker_main
#include "route_worker_check.cpp"
#undef main
#include <set>
#include <tuple>
#include <cstring>

int main() {
    using namespace field;using namespace field::baked_nav;
    int failures=0,total_events=0,total_roots=0;
    auto expect=[&](bool ok,const char* reason){if(!ok){++failures;std::printf("FAIL %s\n",reason);}};
    std::vector<std::string> storage;storage.reserve(6);
    for(int number=1;number<=6;++number) {
        fixture::game_id=static_cast<game::Id>(number);
        std::ifstream file("data/ff"+std::to_string(number)+"/maps.bin",std::ios::binary);
        storage.emplace_back(std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>());
        auto& bytes=storage.back();expect(baked::load(bytes.data(),bytes.size()),"actual game bake loads");
        int baked_rows=0,unmapped=0,cancel_map=0;
        for(int i=0;i<baked::submap_count();++i) {
            const auto* sm=baked::submap_at(i);baked_rows+=static_cast<int>(baked::grids(*sm).objects.size());
            if(sm->first_map_id<=0)++unmapped;
            else if(!sm->world&&!cancel_map)cancel_map=sm->first_map_id;
        }
        g_loaded=true;g_prepared=true;
        std::ifstream state_file("data/ff"+std::to_string(number)+"/mapstate.json",std::ios::binary);
        std::string state_bytes((std::istreambuf_iterator<char>(state_file)),{});
        json::Value records;expect(json::parse(state_bytes,records),"actual initializer programs load");
        uint32_t offset=0,count=0;std::memcpy(&offset,bytes.data()+32,4);std::memcpy(&count,bytes.data()+36,4);
        using Identity=std::tuple<int,int,int,int,int,int,std::wstring>;
        std::set<Identity> checked_events;
        int roots=0,projections=0,rows=0,largest=0;double worst=0;
        auto identity=[](const RemoteObject& r){return Identity{r.map_id,static_cast<int>(r.type),r.cell.layer,r.cell.x,r.cell.y,r.entity_id,r.group};};
        std::thread worker([&] {
            for(int state=0;state<3;++state)for(uint32_t i=0;i<count;++i) {
                int map=0;std::memcpy(&map,bytes.data()+offset+i*8,4);
                const auto* source=baked::submap_by_map_id(map);if(!source||source->world)continue;
                if(!state)++roots;
                map_state::Flags flags;
                if(state<2){for(auto& v:flags.scenario)v=state;for(auto& v:flags.treasure)v=state;for(auto& v:flags.global)v=state;}
                map_state::Resolver resolver(flags,records);
                const auto started=std::chrono::steady_clock::now();
                const auto objects=connected_objects_snapshot(map,resolver);
                worst=std::max(worst,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());
                ++projections;largest=std::max(largest,static_cast<int>(objects.size()));
                std::set<Identity> actual;
                for(const auto& object:objects)if(!object.event_type.empty()) {
                    ++rows;actual.insert(identity(object));
                    expect(object.type!=baked::ObjType::AnimEntity&&object.type!=baked::ObjType::ToLayer&&object.type!=baked::ObjType::TelepoPoint,
                        "animations, layer markers and native non-entity Teleport-cache markers never become remote objectives");
                    const auto* floor=baked::submap_by_map_id(object.map_id);
                    expect(floor&&floor!=source,"every remote event retains its real different-floor map");
                    if(checked_events.insert(identity(object)).second) {
                        filter_worker::FilterInput input;input.current_map=map;input.category=Category::Events;
                        input.selected_has=true;input.selected_map=object.map_id;input.selected_category=Category::Events;
                        input.selected_entity_id=object.entity_id;input.selected_x=object.cell.x;input.selected_y=object.cell.y;
                        input.continued_goal=true;input.continued_map=object.map_id;input.continued_entity_id=object.entity_id;
                        input.continued_x=object.cell.x;input.continued_y=object.cell.y;
                        filter_worker::FilterRow target;target.kind=Kind::Event;target.category=Category::Events;
                        target.remote_map=object.map_id;target.entity_id=object.entity_id;target.cell_x=object.cell.x;target.cell_y=object.cell.y;
                        auto wrong=target;wrong.entity_id=object.entity_id+100000;wrong.index=0;target.index=1;
                        input.rows={wrong,target};const auto filtered=filter_worker::build(input);
                        expect(filtered.restored==1&&filtered.continued==1,"every projected event survives selection and continuation without jumping to a collocated event");
                    }
                }
                // Check every active event row on every connected floor, not just
                // one named event or the floors reached in a single play session.
                for(const auto& floor:connected_floors(source,map,resolver))for(const auto& object:baked::grids(*floor.submap).objects) {
                    const bool event=(object.type==baked::ObjType::Entity&&(object.d&12))||
                        ((object.type==baked::ObjType::Event||object.type==baked::ObjType::RandomEvent||object.type==baked::ObjType::SwitchEvent||object.type==baked::ObjType::TransportationEventAction)&&(object.a||object.b));
                    if(!event||!resolver.active(*floor.submap,object)||resolver.consumed(*floor.submap,object))continue;
                    const auto& grid=baked::grids(*floor.submap);
                    if(object.tx>=grid.width||object.ty>=grid.height||object.tx+object.tw<=0||object.ty+object.th<=0)continue;
                    expect(actual.contains(Identity{floor.map_id,static_cast<int>(object.type),object.layer,object.tx,object.ty,object.entity_id(),baked::str(object.group)}),
                        "all active scripted/actionable connected-floor events reach the projection");
                }
            }
        });worker.join();
        map_state::Resolver cancelled(map_state::Flags{},records);int checks=0;
        const auto empty=connected_objects_snapshot(cancel_map,cancelled,[&]{++checks;return true;});
        expect(empty.empty()&&checks==1,"cancelled mapped-floor projection returns no partial list and stops at its first cancellation check");
        expect(!checked_events.empty(),"each game's actual bakes exercise other-floor event tracking");
        total_events+=static_cast<int>(checked_events.size());total_roots+=roots;
        std::printf("FF%d event audit: %d bake objects, %d unmapped submaps; %d master roots, %d state projections, %zu unique projected event identities, %d event rows; largest list %d; worst projection %.3f ms\n",number,baked_rows,unmapped,roots,projections,checked_events.size(),rows,largest,worst);
        if(number!=2)continue;
        map_state::Flags flags;for(auto& v:flags.scenario)v=0;for(auto& v:flags.treasure)v=0;for(auto& v:flags.global)v=0;
        std::thread replay([&] {
            MapGoal goal;goal.map_id=184;goal.cell={1,35,18};goal.entity_id=32;
            Pending pending;
            const int maps[]{184,183,184};const Cell starts[]{{0,8,32},{0,8,29},{0,33,16}};
            const int next[]{183,184,0};
            for(int leg=0;leg<3;++leg) {
                const auto* sm=baked::submap_by_map_id(maps[leg]);
                RouteSnapshot snapshot(map_state::Resolver(flags,records));snapshot.submap=sm;snapshot.map_id=maps[leg];snapshot.layer=0;
                snapshot.position=world_of_cell(*sm,starts[leg]);snapshot.goal=goal;snapshot.kind=TargetKind::Infer;snapshot.locale=L"en";
                snapshot.name=L"Conversation (Deist Cavern B2)";snapshot.pending=pending;
                for(const auto& object:baked::grids(*sm).objects)if(object.type==baked::ObjType::TreasureBox||object.entity_id()==32) {
                    NativeObstacle observed;observed.cell={object.layer,object.tx,object.ty};observed.type=static_cast<int>(object.type);
                    observed.id=object.entity_id();observed.chest=object.type==baked::ObjType::TreasureBox;snapshot.obstacles.push_back(observed);
                }
                auto planned=plan_snapshot(snapshot);
                expect(planned.path.success&&planned.path.next_map_id==next[leg],"logged B2-B1-B2 route reaches the original event instead of planning a loop");
                if(leg<2){expect(planned.pending.valid&&planned.pending.goal.entity_id==32&&planned.pending.goal.map_id==184,"each leg retains the original event identity/floor");pending=planned.pending;goal=pending.goal;}
                else expect(!planned.pending.valid&&planned.path.step_count<=5,"return through the upper B2 entrance finishes beside the event");
                std::printf("Conversation replay leg %d: map%d (%d,%d), %d steps, next map%d, success%d\n",leg+1,maps[leg],starts[leg].x,starts[leg].y,planned.path.step_count,planned.path.next_map_id,planned.path.success);
            }
            map_state::Resolver active(flags,records);const auto objects=connected_objects_snapshot(183,active);
            expect(std::any_of(objects.begin(),objects.end(),[](const auto& row){return row.entity_id==32&&row.map_id==184&&row.event_type==L"Interactive Object";}),"Conversation remains browsable on B1 with B2 as its floor");
            flags.scenario[43]=1;map_state::Resolver retired(flags,records);
            const auto after=connected_objects_snapshot(183,retired);
            expect(std::none_of(after.begin(),after.end(),[](const auto& row){return row.entity_id==32&&row.map_id==184;}),"completed native story state retires the event from projection");
            RouteSnapshot gone(map_state::Resolver(flags,records));gone.submap=baked::submap_by_map_id(183);gone.map_id=183;gone.layer=0;
            gone.position=world_of_cell(*gone.submap,{0,8,29});gone.goal=goal;gone.kind=TargetKind::Infer;gone.locale=L"en";gone.pending=pending;
            const auto unavailable=plan_snapshot(gone);
            expect(!unavailable.path.success&&!unavailable.pending.valid&&unavailable.path.reason==L"Destination is no longer available","completed event stops tracking instead of routing to stale coordinates");
        });replay.join();
    }
    expect(fixture::wrong_owner==0,"projection, matching and route replay make zero native API reads on workers");
    std::printf("Event tracking: %d master roots, %d projected event identities; %d failures\n",total_roots,total_events,failures);
    return failures?1:0;
}
