// check-sources: field/baked_map.cpp field/baked_router.cpp field/map_state_program.cpp field/field_refs.cpp field/entity_filter.cpp field/npc_names.cpp json.cpp text.cpp
// Production browse worker against real bakes; fake native boundaries flag worker reads.
#define main existing_route_worker_main
#include "route_worker_check.cpp"
#undef main

int main() {
    using namespace field::baked_nav;
    int failures=0, direct_games=0, detours=0;
    double worst_ms=0;
    auto expect=[&](bool ok,const char* reason){if(!ok){++failures;std::printf("FAIL %s\n",reason);}};
    std::vector<std::string> storage;storage.reserve(6);
    for(int number=1;number<=6;++number) {
        fixture::game_id=static_cast<game::Id>(number);
        std::ifstream input("data/ff"+std::to_string(number)+"/maps.bin",std::ios::binary);
        storage.emplace_back(std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>());
        expect(field::baked::load(storage.back().data(),storage.back().size()),"real game bake loads");
        for(int i=0;i<field::baked::submap_count();++i)field::baked::grids(*field::baked::submap_at(i));
        g_loaded=true;g_prepared=true;
        bool checked=false;
        for(int i=0;i<field::baked::submap_count()&&!checked;++i) {
            const auto* sm=field::baked::submap_at(i);if(sm->world||sm->first_map_id<=0)continue;
            field::baked_route::Request movement;movement.submap=sm;
            for(int layer=0;layer<3&&!checked;++layer)for(int y=1;y<sm->height-1&&!checked;++y)for(int x=1;x<sm->width-2&&!checked;++x) {
                const Cell from{layer,x,y},to{layer,x+1,y};
                if(!field::baked_route::open(movement,field::baked::grids(*sm),from)||!field::baked_route::open(movement,field::baked::grids(*sm),to))continue;
                RouteSnapshot snapshot(field::map_state::Resolver(field::map_state::Flags{},fixture::records));
                snapshot.submap=sm;snapshot.map_id=sm->first_map_id;snapshot.layer=layer;snapshot.position=world_of_cell(*sm,from);
                snapshot.goal.map_id=sm->first_map_id;snapshot.goal.cell=to;snapshot.kind=field::TargetKind::Spot;snapshot.locale=L"en";
                BrowsePlanned result;
                std::thread worker([&] {
                    ActiveRoute active;active.valid=true;active.map_id=987;
                    Pending pending;pending.valid=true;pending.goal.map_id=876;
                    g_active=&active;g_pending=&pending;
                    const auto began=std::chrono::steady_clock::now();result=plan_browse_snapshot(snapshot);
                    worst_ms=std::max(worst_ms,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count());
                    expect(g_active==&active&&active.map_id==987&&g_pending==&pending&&pending.goal.map_id==876,
                        "browsing preserves active route and remembered destination on its worker");
                    g_active=nullptr;g_pending=nullptr;
                });worker.join();
                if(result.route.direct){checked=true;++direct_games;}
            }
        }
        expect(checked,"each game supplies a direct reachable browsing fixture");
        if(number!=2)continue;
        const auto* shop=field::baked::submap_by_name(L"Map_20011_11");
        expect(shop!=nullptr,"Fynn shop exists");
        if(shop) {
            RouteSnapshot snapshot(field::map_state::Resolver(field::map_state::Flags{},fixture::records));
            snapshot.submap=shop;snapshot.map_id=shop->first_map_id;snapshot.layer=0;snapshot.position=world_of_cell(*shop,{0,8,15});
            snapshot.goal.map_id=shop->first_map_id;snapshot.goal.cell={0,8,8};snapshot.kind=field::TargetKind::Spot;snapshot.locale=L"en";
            for(int x=0;x<shop->width;++x){NativeObstacle event;event.type=2;event.trigger=true;event.cell={-1,x,12};snapshot.obstacles.push_back(event);}
            BrowsePlanned result;std::thread worker([&]{result=plan_browse_snapshot(snapshot);});worker.join();
            expect(!result.route.direct&&!result.route.via_map_id,"live event barrier suppresses steps without inventing a floor");
            snapshot.obstacles.clear();snapshot.epoch=std::make_shared<std::atomic<uint64_t>>(1);
            std::thread cancelled([&]{result=plan_browse_snapshot(snapshot);});cancelled.join();
            expect(!result.route.direct,"cancelled browse cannot claim a direct route");
        }
        // Find a disconnected Deist section using its actual entry points and
        // objects, rather than assuming every same-floor target is reachable.
        for(int floor_id=183;floor_id<=187&&!detours;++floor_id) {
            const auto* floor=field::baked::submap_by_map_id(floor_id);if(!floor)continue;
            const auto& grid=field::baked::grids(*floor);
            for(const auto& start:grid.objects) {
                if(start.type!=field::baked::ObjType::PointIn||detours)continue;
                for(const auto& object:grid.objects) {
                    if(detours)break;
                    if(object.type!=field::baked::ObjType::TreasureBox&&object.type!=field::baked::ObjType::Entity&&object.type!=field::baked::ObjType::GotoMap)continue;
                    RouteSnapshot snapshot(field::map_state::Resolver(field::map_state::Flags{},fixture::records));
                    snapshot.submap=floor;snapshot.map_id=floor_id;snapshot.layer=start.layer;snapshot.position=world_of_cell(*floor,{start.layer,start.tx,start.ty});
                    snapshot.goal.map_id=floor_id;snapshot.goal.cell={object.layer,object.tx,object.ty};snapshot.locale=L"en";
                    snapshot.kind=object.type==field::baked::ObjType::TreasureBox?field::TargetKind::Interact:field::TargetKind::Infer;
                    BrowsePlanned result;std::thread worker([&]{result=plan_browse_snapshot(snapshot);});worker.join();
                    if(result.route.via_map_id>0){expect(!result.route.direct,"real same-floor detour cannot announce steps");++detours;
                        std::printf("Deist map %d object (%d,%d), entry (%d,%d): first required map %d\n",floor_id,object.tx,object.ty,start.tx,start.ty,result.route.via_map_id);}
                }
            }
        }
    }
    expect(direct_games==6&&detours>0,"all-six direct and real Deist cross-floor detour cases exercised");
    expect(fixture::wrong_owner==0,"browse compute never resolves or reads native APIs");
    std::printf("Browse routes: %d direct games, %d real detours; worst direct compute %.3f ms; %d failures\n",direct_games,detours,worst_ms,failures);
    return failures?1:0;
}
