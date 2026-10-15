// check-sources: field/baked_map.cpp field/baked_router.cpp field/map_state_program.cpp field/field_refs.cpp field/entity_filter.cpp field/npc_names.cpp json.cpp text.cpp
// Actual owned planner and all-six extracted maps. No game process or live capture.
#define main existing_route_worker_main
#include "route_worker_check.cpp"
#undef main

int main() {
    using namespace field::baked_nav;
    int failures = 0;
    auto expect = [&](bool value, const char* message) { if (!value) { ++failures; std::printf("FAIL %s\n", message); } };
    std::vector<std::string> storage;
    storage.reserve(6);
    int traps = 0;
    for (int number=1; number<=6; ++number) {
        fixture::game_id = static_cast<game::Id>(number);
        std::ifstream file("data/ff" + std::to_string(number) + "/maps.bin", std::ios::binary);
        storage.emplace_back(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        expect(field::baked::load(storage.back().data(),storage.back().size()), "real bake loads");
        int count = 0;
        for (int index=0; index<field::baked::submap_count(); ++index) {
            const auto* map = field::baked::submap_at(index);
            const auto& grid = field::baked::grids(*map);
            for (const auto& object : grid.objects) if (field::baked::is_trap(object)) {
                ++count;
                field::baked_route::Request request;
                request.submap=map; request.ignore_hazards=true;
                const Cell at{object.layer,object.tx,object.ty};
                expect(field::baked_route::trap_cell(request,grid,at), "trap mask includes its exact native layer");
                expect(!field::baked_route::trap_cell(request,grid,{(object.layer+1)%3,object.tx,object.ty}) ||
                    std::any_of(grid.objects.begin(),grid.objects.end(),[&](const auto& other) { return other.layer==(object.layer+1)%3 && field::baked::is_trap(other) && other.contains(at.x,at.y); }),
                    "trap mask does not bleed across layers");
            }
        }
        traps += count;
        std::printf("FF%d: %d trap objects, including original events and baked transitions\n",number,count);
        std::ifstream overrides("data/ff"+std::to_string(number)+"/warp_arrival_overrides.json");
        if(overrides) {
            const std::string data((std::istreambuf_iterator<char>(overrides)),{});
            json::Value rows; expect(json::parse(data,rows),"reviewed arrival fixture JSON parses");
            for(const auto& row:rows.arr) {
                const auto* source=field::baked::submap_by_name(row.get(L"submap")->s);
                const auto* dest=field::baked::submap_by_map_id(static_cast<int>(row.get(L"destination_map")->n));
                const auto* self=row.get(L"same_map");
                const bool same=self && self->b;
                const int id=static_cast<int>(row.get(L"entity_id")->n);
                const field::baked::Object* edge=nullptr;
                if(source) for(const auto& object:field::baked::grids(*source).objects)
                    if(object.type==field::baked::ObjType::Warp && object.entity_id()==id &&
                        object.a==(same ? -1 : dest->first_map_id)) edge=&object;
                expect(source && dest && edge,"reviewed trap has a graph transition");
                if(!dest || !edge) continue;
                const field::baked::Object* arrival=nullptr;
                for(const auto& point:field::baked::grids(*dest).objects)
                    if(point.type==field::baked::ObjType::PointIn && point.a==edge->b) arrival=&point;
                expect(arrival!=nullptr,"reviewed graph transition has exact final arrival");
                if(!arrival) continue;
                if(const auto* cell=row.get(L"arrival_baked_cell"))
                    expect(arrival->tx==cell->arr[0].n && arrival->ty==cell->arr[1].n && arrival->layer==cell->arr[2].n,"baked trap arrival equals reviewed script position and final layer");
            }
            std::printf("FF%d: %zu reviewed trap landing/relocation arcs decoded\n",number,rows.arr.size());
        }
        if(number==6) {
            g_loaded=true;g_prepared=true;
            const auto* room=field::baked::submap_by_map_id(407);
            expect(room!=nullptr,"FF6 same-map falling room exists");
            if(room) {
                const auto& grid=field::baked::grids(*room);
                const field::baked::Object* drop=nullptr;
                Cell start;
                field::baked_route::Request movement;movement.submap=room;
                for(const auto& object:grid.objects) if(object.type==field::baked::ObjType::Warp && object.a==-1) {
                    for(const auto& side:std::vector<Cell>{{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}) {
                        Cell candidate{object.layer,object.tx+side.x,object.ty+side.y};
                        if(field::baked_route::open(movement,grid,candidate) && !field::baked_route::trap_cell(movement,grid,candidate) &&
                           field::baked_route::edge_allowed(movement,grid,candidate,{object.layer,object.tx,object.ty})) { drop=&object; start=candidate; break; }
                    }
                    if(drop) break;
                }
                expect(drop!=nullptr,"real same-map pitfall has a walkable approach");
                if(drop) {
                    RouteSnapshot snapshot(field::map_state::Resolver(field::map_state::Flags{},fixture::records));
                    snapshot.submap=room;snapshot.map_id=407;snapshot.layer=start.layer;snapshot.position=world_of_cell(*room,start);
                    snapshot.kind=field::TargetKind::Spot;snapshot.locale=L"en";
                    snapshot.goal.map_id=407;snapshot.goal.cell={1,7,8};
                    for(const auto& object:grid.objects) if(object.type==field::baked::ObjType::Event && field::baked::is_trap(object)) {
                        NativeObstacle trap;trap.cell={object.layer,object.tx,object.ty};trap.type=2;trap.id=object.entity_id();trap.trigger=true;
                        snapshot.obstacles.push_back(trap);
                    }
                    for(int dx=-1;dx<=1;++dx) for(int dy=-1;dy<=1;++dy) {
                        if(!dx && !dy) continue;
                        const Cell cell{start.layer,start.x+dx,start.y+dy};
                        if(drop->contains(cell.x,cell.y)) continue;
                        NativeObstacle gate;gate.cell=cell;gate.type=19;gate.collider=true;snapshot.obstacles.push_back(gate);
                    }
                    std::thread worker{[&] {
                        const auto result=plan_snapshot(snapshot);
                        expect(result.path.success && result.path.required_trap && result.path.next_map_id==407,"same-map drop is a real forward route leg when alternatives close");
                        snapshot.pending=result.pending;
                        { SnapshotScope owner(snapshot);
                          expect(note_trap_start(407,{drop->layer,drop->tx,drop->ty}),"same-map start requests transition recovery");
                          record_map_arrival(407,start);
                          expect(snapshot.pending.completed_traps.empty(),"same map ID alone cannot falsely complete a fall");
                          record_map_arrival(407,{1,7,8}); }
                        expect(snapshot.pending.completed_traps.size()==1,"same-map actual landing completes the fall once");
                        snapshot.active={};
                        const auto repeat=plan_snapshot(snapshot);
                        expect(!repeat.path.success && repeat.path.trap_repeat_blocked,"route back to same-map falling area cannot cause a repeated drop");
                    }};worker.join();
                }
            }
        }
        if (number != 2) continue;
        g_loaded = true; g_prepared = true;
        const auto* b3=field::baked::submap_by_map_id(185);
        const auto* b4=field::baked::submap_by_map_id(186);
        const auto* b5=field::baked::submap_by_map_id(187);
        expect(b3 && b4 && b5,"Deist floors exist");
        if (!b3 || !b4 || !b5) continue;
        const auto& grid=field::baked::grids(*b3);
        const auto* trap=static_cast<const field::baked::Object*>(nullptr);
        for (const auto& o:grid.objects) if (o.type==field::baked::ObjType::Warp && field::baked::is_trap(o)) trap=&o;
        expect(trap && trap->b==5,"Deist trap graph uses scripted final landing PointIn5");
        RouteSnapshot snapshot(field::map_state::Resolver(field::map_state::Flags{},fixture::records));
        snapshot.submap=b3; snapshot.map_id=185; snapshot.layer=0; snapshot.position=world_of_cell(*b3,{0,15,27});
        snapshot.locale=L"en"; snapshot.kind=field::TargetKind::Interact;
        NativeObstacle observed; observed.cell={0,14,27}; observed.type=2; observed.id=9; observed.trigger=true;
        snapshot.obstacles.push_back(observed);
        int safe=0, required=0;
        std::thread worker{[&] {
            for (const auto* floor : {b3,b4,b5}) for (const auto& chest:field::baked::grids(*floor).objects) {
                if(chest.type!=field::baked::ObjType::TreasureBox) continue;
                snapshot.goal={}; snapshot.goal.map_id=floor->first_map_id; snapshot.goal.cell={chest.layer,chest.tx,chest.ty};
                snapshot.pending={}; snapshot.active={};
                auto result=plan_snapshot(snapshot);
                if (!result.path.success) continue;
                if (result.path.required_trap) {
                    ++required;
                    expect(result.path.warning==L"Warning: a trap is required to reach this chest.","required chest trap warning is separate from directions");
                    snapshot.pending=result.pending;
                    snapshot.pending.completed_traps={{185,9,186,14,27}};
                    snapshot.active={};
                    auto repeated=plan_snapshot(snapshot);
                    expect(!repeated.path.success,"same destination cannot plan the same drop again");
                } else ++safe;
                std::printf("Deist chest map%d (%d,%d): %s, first destination%d\n",floor->first_map_id,chest.tx,chest.ty,result.path.required_trap ? "required trap" : "safe stairs",result.path.next_map_id);
            }
            const std::vector<Cell> cells{{0,15,27},{0,14,27}};
            expect(!event_on_route(cells,{{0,14,27}},{{0,14,27}}),"selected event produces no incidental-event warning");
            expect(event_on_route(cells,{{0,14,27}},{{0,13,27}}),"another event still produces warning");
            snapshot.kind=field::TargetKind::Spot; snapshot.goal={};
            snapshot.goal.map_id=187; snapshot.goal.cell={1,9,8}; snapshot.pending={}; snapshot.active={};
            auto guide=plan_snapshot(snapshot);
            std::printf("Deist B5 guidance fixture: success%d, required%d, next%d\n",guide.path.success,guide.path.required_trap,guide.path.next_map_id);
            expect(guide.path.success && !guide.path.required_trap,"B5 guide follows the safe stairs");
            // Simulate closed alternatives using the real current-map collision
            // snapshot contract; retain the actual map and pitfall destination.
            for(const auto& exit:grid.objects) if(exit.type==field::baked::ObjType::GotoMap) {
                NativeObstacle gate; gate.type=19; gate.collider=true;
                for(int y=exit.ty;y<exit.ty+exit.th;++y) for(int x=exit.tx;x<exit.tx+exit.tw;++x) {
                    gate.cell={exit.layer,x,y}; snapshot.obstacles.push_back(gate);
                }
            }
            snapshot.goal.map_id=186; snapshot.goal.cell={0,9,32}; snapshot.pending={}; snapshot.active={};
            auto drop=plan_snapshot(snapshot);
            snapshot.keep_destination=false;
            auto preview=plan_snapshot(snapshot);
            expect(preview.pending.valid && preview.pending.incoming_trap.has_value(),"guide/follower preview retains its own trap receipt");
            snapshot.keep_destination=true;
            expect(drop.path.success && drop.path.required_trap && drop.path.next_map_id==186,"closed stairs allow the only required trap leg");
            expect(drop.path.warning==L"Warning: a trap is required on this route.","required trap warning is a separate optional speech part");
            expect(drop.path.walk_phases.size()==1 && drop.path.walk_phases.front().cells.back()==Cell{0,14,27},"trap leg stops at trigger rather than crossing it");
            expect(drop.pending.incoming_trap.has_value(),"planned trap arrival is retained with final destination");
            snapshot.pending=drop.pending;
            { SnapshotScope owner(snapshot); note_trap_start(185,{0,14,27}); record_map_arrival(186,{0,9,32}); }
            expect(snapshot.pending.completed_traps.size()==1 && !snapshot.pending.incoming_trap,"matching native map arrival records the completed drop once");
            snapshot.active={};
            auto repeat=plan_snapshot(snapshot);
            expect(!repeat.path.success && repeat.path.trap_repeat_blocked,"same-goal route cannot repeat the completed drop");
            expect(repeat.pending.completed_traps.size()==1,"repeat failure preserves protection until a different goal is chosen");
            snapshot.pending=repeat.pending; snapshot.active={};
            repeat=plan_snapshot(snapshot);
            expect(!repeat.path.success && repeat.path.trap_repeat_blocked,"another directions request does not erase repeat protection");
            field::PathInfo warning; warning.required_trap=true;
            MapGoal chest_goal; chest_goal.map_id=187; chest_goal.cell={1,35,29};
            expect(route_warning(warning,chest_goal)==L"Warning: a trap is required to reach this chest.","actual chest identity selects concise required-trap chest warning");
            snapshot.pending={}; snapshot.active={}; snapshot.kind=field::TargetKind::Infer;
            snapshot.goal.map_id=185; snapshot.goal.cell={0,14,27};
            const auto selected=plan_snapshot(snapshot);
            expect(selected.path.success && selected.path.warning.empty(),"directions to the selected trap itself omit event-along-route warning");
        }}; worker.join();
        // Publication keeps the latest guide/follower receipt apart from the
        // destination chosen manually; neither client shares mutable worker data.
        Pending manual;manual.valid=true;manual.goal.map_id=42;manual.goal.cell={1,3,4};
        auto* prior=g_pending;auto* prior_preview=g_preview_pending;g_pending=&manual;
        Planned preview;preview.keep_destination=false;preview.pending.valid=true;
        preview.pending.goal.map_id=185;preview.pending.goal.cell={0,14,27};
        commit_plan(preview);
        expect(manual.valid && manual.goal.map_id==42,"guide publication preserves the manual destination");
        expect(g_preview_pending && g_preview_pending->valid && g_preview_pending->goal.map_id==185,"guide publication owns a separate receipt");
        clear_preview_destination(g_preview_pending->goal);
        expect(!g_preview_pending->valid && manual.valid,"guide completion retires only its own receipt");
        g_pending=prior;g_preview_pending=prior_preview;
        expect(safe>0,"Deist has safe cross-floor chest routes");
        std::printf("Deist: %d safe, %d required-trap chest routes from B3 fixture\n",safe,required);
    }
    expect(traps>0,"actual all-six inventory contains environmental traps");
    expect(fixture::wrong_owner==0,"trap planning never reads engine APIs on worker");
    std::printf("Trap route check: %d failures\n",failures);
    return failures ? 1 : 0;
}
