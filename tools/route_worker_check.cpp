// check-sources: field/baked_map.cpp field/baked_router.cpp field/map_state_program.cpp field/field_refs.cpp json.cpp text.cpp
// Actual baked planner snapshot on a worker. Every fake native API flags worker use.
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <thread>
#include <fstream>
#include <chrono>
#include "../src/core/field/baked_nav.cpp"
namespace fixture {
const DWORD native_owner = GetCurrentThreadId();
std::atomic<int> wrong_owner{0};
game::Id game_id = game::Id::FF2;
void native() { if (GetCurrentThreadId() != native_owner) ++wrong_owner; }
json::Value records;
}
namespace ref {
Class::Class(const char* n, const char* c, uint8_t g, const char* a) : ns(n), name(c), games(g), assembly(a) {}
Field::Field(Class& c, const char* n, const char* a) : cls(c), name(n), alt(a) {}
Method::Method(Class& c, const char* n, int a) : cls(c), name(n), argc(a) {}
void* Method::ptr() { fixture::native(); return nullptr; }
il2cpp::Method* Method::get() { fixture::native(); return nullptr; }
il2cpp::Class* Class::get() { fixture::native(); return nullptr; }
bool Class::is(void*) { fixture::native(); return false; }
int32_t Field::get() { fixture::native(); return -1; }
std::wstring str(void*) { fixture::native(); return {}; }
void call_failed_note(const char*) {}
void* new_str(const std::wstring&) { fixture::native(); return nullptr; }
void* instance(Class&) { fixture::native(); return nullptr; }
}
namespace il2cpp {
Method* find_method_overload_param(Class*, const char*, int, Class*, int) { fixture::native(); return nullptr; }
void* method_pointer(Method*) { fixture::native(); return nullptr; }
Method* virtual_method(void*, Method*) { fixture::native(); return nullptr; }
Class* object_class(void*) { fixture::native(); return nullptr; }
}
namespace unity {
bool alive(void*) { fixture::native(); return false; }
void* game_object(void*) { fixture::native(); return nullptr; }
void* transform(void*) { fixture::native(); return nullptr; }
Vec3 local_position(void*) { fixture::native(); return {}; }
int list_count(void*) { fixture::native(); return 0; }
void* list_at(void*, int) { fixture::native(); return nullptr; }
int array_length(void*) { fixture::native(); return 0; }
}
namespace log_ { void write(const char*, ...) {} }
namespace hooks { void note_fault(const char*) { std::abort(); } }
namespace frame {
double now() { return GetTickCount64() / 1000.0; }
void post(Task) { fixture::native(); }
int every_frame(Step, const char*, int) { fixture::native(); return 0; }
void cancel_label(const char*) { fixture::native(); }
}
namespace background { bool post(std::function<void()>, Lane) { fixture::native(); return false; } }
namespace ffpr { void say(std::wstring_view, bool) { fixture::native(); } }
namespace ffpr::system { bool is_field_navigation_ready() { fixture::native(); return false; } }
namespace game { Id current() { return fixture::game_id; } const Flags& flags() { static Flags f; return f; } const char* key() { return "ff2"; } }
namespace modtext {
LocaleScope::LocaleScope(std::wstring) {}
LocaleScope::~LocaleScope() {}
const std::wstring& lang() { fixture::native(); static std::wstring value = L"en"; return value; }
std::wstring T(std::wstring_view value) { return std::wstring(value); }
std::wstring TF(std::wstring_view value, std::initializer_list<std::wstring> args) {
    auto out = std::wstring(value); int i = 0;
    for (const auto& arg : args) { const auto token = L"{" + std::to_wstring(i++) + L"}"; const auto at = out.find(token); if (at != out.npos) out.replace(at, token.size(), arg); }
    return out;
}
}
namespace data {
void prepare_game_json() {}
std::string game_file(const char*) { return {}; }
}
namespace field {
void prepare_entity_translations() {}
int current_map_id() { fixture::native(); return 0; }
void* map_handle() { fixture::native(); return nullptr; }
void* player() { fixture::native(); return nullptr; }
unity::Vec3 player_position() { fixture::native(); return {}; }
VehicleState current_vehicle() { fixture::native(); return {}; }
std::vector<int> owned_auto_vehicle_groups() { fixture::native(); return {}; }
std::vector<VehicleOnMap> vehicles_on_map() { fixture::native(); return {}; }
std::wstring vehicle_display_name(int, int, const std::wstring&) { fixture::native(); return {}; }
bool EntityCursor::next(void*&) { fixture::native(); return false; }
bool native_position_walkable(void*, unity::Vec3) { fixture::native(); return false; }
bool entity_is_active(void*) { fixture::native(); return false; }
bool entity_is_scenery(void*) { fixture::native(); return false; }
int object_type_of(void*) { fixture::native(); return 0; }
std::wstring resolve_map_name_by_id(int) { fixture::native(); return {}; }
}
namespace field::map_state {
Resolver::Resolver() : Resolver(Flags{}, fixture::records) { fixture::native(); }
}
int main() {
    using namespace field::baked_nav;
    int failures = 0;
    auto expect = [&](bool ok, const char* line) { if (!ok) { ++failures; std::printf("FAIL %s\n", line); } };
    std::ifstream maps("data/ff2/maps.bin", std::ios::binary);
    static std::string bytes((std::istreambuf_iterator<char>(maps)), {});
    expect(field::baked::load(bytes.data(), bytes.size()), "real FF2 bake loads");
    for (int i = 0; i < field::baked::submap_count(); ++i) field::baked::grids(*field::baked::submap_at(i));
    g_loaded = true; g_prepared = true;
    const auto* shop = field::baked::submap_by_name(L"Map_20011_11");
    expect(shop != nullptr, "known Fynn shop bake exists"); if (!shop) return 1;
    RouteSnapshot snapshot(field::map_state::Resolver(field::map_state::Flags{}, fixture::records));
    snapshot.submap = shop; snapshot.map_id = shop->first_map_id; snapshot.layer = 0;
    snapshot.position = world_of_cell(*shop, {0, 8, 15});
    snapshot.goal.map_id = shop->first_map_id; snapshot.goal.cell = {0, 8, 8};
    snapshot.kind = field::TargetKind::Spot; snapshot.locale = L"en";
    Planned result;
    std::thread worker([&] { result = plan_snapshot(snapshot); }); worker.join();
    expect(fixture::wrong_owner == 0, "planner never resolves/reads engine objects or APIs on worker");
    expect(result.path.success && result.path.step_count == 7, "owned snapshot preserves Fynn shop seven-cell route");
    expect(result.path.description == L"7 North", "worker formats concise directions");
    expect(result.active && result.active->valid && result.path.walk_phases.size() == 1, "owned active plan and autowalk phases returned");
    const auto* world = field::baked::submap_by_name(L"Map_10010");
    expect(world != nullptr, "world bake exists");
    if (world) {
        RouteSnapshot long_route(field::map_state::Resolver(field::map_state::Flags{}, fixture::records));
        long_route.submap = world; long_route.map_id = world->first_map_id; long_route.layer = 1;
        long_route.position = world_of_cell(*world, {1, 123, 34});
        long_route.auto_groups = {2}; // Semitt Falls requires the owned canoe
        long_route.goal.map_id = world->first_map_id; long_route.goal.cell = {1, 107, 44};
        long_route.kind = field::TargetKind::Transition; long_route.locale = L"en";
        double route_ms = 0, flood_ms = 0;
        std::thread full([&] {
            const auto started = std::chrono::steady_clock::now();
            result = plan_snapshot(long_route);
            route_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
            field::baked_route::Request reach;
            reach.submap = world; reach.start = {1, 123, 34}; reach.auto_groups = {2}; reach.ignore_hazards = true;
            const auto flood_started = std::chrono::steady_clock::now();
            auto grid = field::baked_route::flood(reach);
            flood_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - flood_started).count();
            expect(grid.reachable({1, 107, 44}), "world flood preserves canoe reachability");
        }); full.join();
        std::printf("FF2 world worker: route %.3f ms, reachability %.3f ms (offline computation)\n", route_ms, flood_ms);
        expect(result.path.success && result.path.step_count >= 26, "worker routes real world map without native distance cap");
        // User's actual FF2 onboard failures: map 1, ship at (126,129),
        // land entrances (110,98) and (218,43). Existing bakes contain group 4.
        const auto* ship = field::baked::transport(4);
        expect(ship && ship->type==2 && ship->group==4, "actual FF2 ship transportation rules are baked");
        for (auto target : {Cell{1,110,98}, Cell{1,218,43}}) {
            RouteSnapshot onboard(field::map_state::Resolver(field::map_state::Flags{}, fixture::records));
            onboard.submap=world; onboard.map_id=world->first_map_id; onboard.layer=1;
            onboard.position=world_of_cell(*world,{1,126,129}); onboard.vehicle={true,4,2,L"MSG_SYSTEM_200"};
            onboard.auto_groups={2}; onboard.goal.map_id=onboard.map_id; onboard.goal.cell=target;
            onboard.kind=field::TargetKind::Transition; onboard.locale=L"en";
            std::thread ship_worker([&] {
                field::baked_route::Request riding;
                riding.submap=world; riding.start={1,126,129}; riding.vehicle_group=4;
                riding.vehicle_type=2; riding.landing_group=ship->landing_group; riding.auto_groups={2}; riding.ignore_hazards=true;
                auto foot=riding; foot.vehicle_group=0;
                auto goals=field::baked_route::goals_for(foot,target,field::baked_route::GoalKind::Transition);
                riding.goals=goals.cells;
                expect(!field::baked_route::find(riding).found, "old ship-only search reproduces the logged no-path failure");
                auto reach=field::baked_route::flood(riding);
                auto land=land_after_ride(riding,reach);
                if (!any_reachable(land,goals.cells)) {
                    int riding_count=0, land_count=0;
                    for(const auto distance:reach.dist[0]) if(distance!=0xFFFF) ++riding_count;
                    for(const auto distance:land.dist[0]) if(distance!=0xFFFF) ++land_count;
                    std::printf("FF2 ship target (%d,%d): ride cells %d, land cells %d, target attribute %d, %zu goals\n",target.x,target.y,riding_count,land_count,field::baked::attribute(field::baked::grids(*world),target.x,target.y),goals.cells.size());
                    for(const auto& cell:goals.cells) std::printf("goal (%d,%d) foot-open %d, land-distance %d\n",cell.x,cell.y,field::baked::vehicle_open(field::baked::grids(*world),1,cell.x,cell.y),land.distance(cell));
                }
                expect(any_reachable(land,goals.cells), "onboard filter includes land destinations through legal shores");
                const auto started=std::chrono::steady_clock::now();
                result=plan_snapshot(onboard);
                const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
                std::printf("FF2 onboard route (%d,%d): %.3f ms, %zu phases\n",target.x,target.y,elapsed,result.path.walk_phases.size());
                expect(result.path.success && result.path.walk_phases.size()==2, "actual ship position reaches the reported land entrance using ride and walk phases");
                if(result.path.walk_phases.size()==2) {
                    const auto& ride_phase=result.path.walk_phases[0]; const auto& walk_phase=result.path.walk_phases[1];
                    expect(ride_phase.action_before.empty() && walk_phase.action_before==L"land", "already-boarded route requests landing without boarding again");
                    expect(ride_phase.cells.back()==walk_phase.cells.front(), "ship and walking phases meet at the same legal coastal stop");
                    for(const auto& cell:ride_phase.cells) expect(field::baked::vehicle_open(field::baked::grids(*world),4,cell.x,cell.y), "riding leg remains on the ship grid");
                    const auto& shore=walk_phase.cells[1];
                    expect(field::baked::group_lands(ship->landing_group,field::baked::attribute(field::baked::grids(*world),shore.x,shore.y)), "walking leg starts through the native ship landing policy");
                }
                riding.cancelled=[] { return true; };
                expect(!land_after_ride(riding,reach).submap, "cancelled onboard reachability never publishes land results");
            }); ship_worker.join();
        }
        // A one-cell wrapped ship step must retain native cardinal direction.
        field::baked_route::Request edge;
        edge.submap=world; edge.vehicle_group=4; edge.ignore_hazards=true; edge.goals_open=false;
        const auto& grid=field::baked::grids(*world);
        bool wrap_case=false;
        for(int y=0; y<grid.height; ++y) if(field::baked::vehicle_open(grid,4,0,y) && field::baked::vehicle_open(grid,4,grid.width-1,y)) {
            edge.start={1,0,y}; edge.goals={{1,grid.width-1,y}};
            auto wrapped=field::baked_route::find(edge);
            auto directions=field::baked_route::legs(wrapped.cells,grid.width,grid.height);
            expect(wrapped.found && wrapped.steps()==1 && directions.size()==1 && directions[0].dir==3, "world left edge wraps west in A* and spoken directions");
            Phase phase; phase.cells=wrapped.cells; Corner corner;
            corner_of(*world,{phase},edge.start,corner);
            expect(corner.point.x < world_of_cell(*world,edge.start).x && corner.steps_to_corner==1, "wrapped beacon corner points along the westward route leg");
            wrap_case=true; break;
        }
        expect(wrap_case, "actual ship grids supply a wrapped-edge regression");
        field::baked_route::Request cancelled;
        cancelled.submap = world; cancelled.start = {1, 123, 34}; cancelled.goals = {{1, 107, 44}};
        cancelled.cancelled = [] { return true; };
        expect(!field::baked_route::find(cancelled).found && field::baked_route::flood(cancelled).submap == nullptr,
               "retired contexts cancel A* and reachability computation");
    }
    int vehicle_cases=0, no_landing=0;
    double worst_vehicle_ms=0, worst_vehicle_filter_ms=0;
    for(int number=1; number<=6; ++number) {
        fixture::game_id=static_cast<game::Id>(number);
        std::ifstream source("data/ff"+std::to_string(number)+"/maps.bin",std::ios::binary);
        std::string vehicle_bytes((std::istreambuf_iterator<char>(source)),{});
        expect(field::baked::load(vehicle_bytes.data(),vehicle_bytes.size()), "each game's actual vehicle grids load");
        std::ifstream state_file("data/ff"+std::to_string(number)+"/mapstate.json",std::ios::binary);
        std::string state_bytes((std::istreambuf_iterator<char>(state_file)),{});
        json::Value chest_records; expect(json::parse(state_bytes,chest_records), "all-six actual saved chest records load");
        field::map_state::Flags chest_flags;
        for(auto& v:chest_flags.scenario) v=0;
        for(auto& v:chest_flags.treasure) v=0;
        for(auto& v:chest_flags.global) v=0;
        bool remote_checked=false;
        for(int index=0;index<field::baked::submap_count()&&!remote_checked;++index){
            const auto* here=field::baked::submap_at(index);if(here->world||here->first_map_id<=0)continue;
            std::vector<RemoteObject> closed;
            field::map_state::Resolver closed_state(chest_flags,chest_records);
            std::thread projection([&]{closed=connected_objects_snapshot(here->first_map_id,closed_state);});projection.join();
            for(const auto& target:closed){
                if(target.type!=field::baked::ObjType::TreasureBox||!target.opened.has_value())continue;
                const auto* floor=field::baked::submap_by_map_id(target.map_id);if(!floor)continue;
                const auto* record=chest_records.get(field::baked::name_of(*floor));
                const auto* records=record?record->get(L"treasures"):nullptr;if(!records)continue;
                int bit=-1;
                for(const auto& object:field::baked::grids(*floor).objects){
                    if(object.type!=target.type||object.tx!=target.cell.x||object.ty!=target.cell.y||field::baked::str(object.group)!=target.group)continue;
                    for(const auto& row:records->arr){const auto& r=row.arr;if(r.size()>=3&&static_cast<int>(r[0].n)==object.entity_id()&&r[1].s==target.group)bit=static_cast<int>(r[2].n);}
                    break;
                }
                if(bit<0||bit>=1024)continue;
                auto updated=chest_flags;updated.treasure[static_cast<size_t>(bit)]=1;
                field::map_state::Resolver open_state(updated,chest_records);
                std::vector<RemoteObject> opened;
                std::thread reprojection([&]{opened=connected_objects_snapshot(here->first_map_id,open_state);});reprojection.join();
                bool retained=false;
                for(const auto& row:opened)if(row.map_id==target.map_id&&row.cell==target.cell&&row.type==target.type&&row.group==target.group){retained=row.opened==std::optional<bool>(true);break;}
                expect(!*target.opened&&retained,"Current map retains opened other-floor chests and copies the saved state on its worker");
                remote_checked=true;break;
            }
        }
        expect(remote_checked,"each game exercises an actual connected other-floor chest");
        for(int index=0; index<field::baked::submap_count(); ++index) {
            const auto* map=field::baked::submap_at(index);
            if(!map->world) continue;
            const auto& grid=field::baked::grids(*map);
            for(const auto& row:field::baked::transports()) {
                if(row.type==0 || row.type==1 || row.type==5 || row.group<=0 || row.group>static_cast<int>(grid.transport.size())) continue;
                Cell start; bool found=false;
                for(int y=0; y<grid.height && !found; ++y) for(int x=0; x<grid.width; ++x)
                    if(field::baked::vehicle_open(grid,row.group,x,y)) { start={1,x,y}; found=true; break; }
                if(!found) { ++no_landing; continue; }
                field::baked_route::Request riding;
                riding.submap=map; riding.start=start; riding.vehicle_group=row.group;
                riding.vehicle_type=row.type; riding.landing_group=row.landing_group; riding.ignore_hazards=true;
                for(const auto& owned:field::baked::transports()) if(owned.type==5) riding.auto_groups.push_back(owned.group);
                field::baked_route::Reach ride, landed;
                std::thread reach_worker([&] {
                    const auto started=std::chrono::steady_clock::now();
                    ride=field::baked_route::flood(riding); landed=land_after_ride(riding,ride);
                    worst_vehicle_filter_ms=std::max(worst_vehicle_filter_ms,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());
                }); reach_worker.join();
                auto foot=riding; foot.vehicle_group=0;
                Cell target; bool destination=false;
                for(const auto& object:grid.objects) {
                    if(object.type!=field::baked::ObjType::GotoMap) continue;
                    Cell cell{1,object.tx,object.ty};
                    const auto goals=field::baked_route::goals_for(foot,cell,field::baked_route::GoalKind::Transition);
                    if(any_reachable(landed,goals.cells)) { target=cell; destination=true; break; }
                }
                if(!destination) { ++no_landing; continue; }
                RouteSnapshot onboard(field::map_state::Resolver(field::map_state::Flags{},fixture::records));
                onboard.submap=map; onboard.map_id=map->first_map_id; onboard.layer=1; onboard.position=world_of_cell(*map,start);
                onboard.vehicle={true,row.id,row.type,field::baked::str(row.message)}; onboard.auto_groups=riding.auto_groups;
                onboard.goal.map_id=onboard.map_id; onboard.goal.cell=target; onboard.kind=field::TargetKind::Transition; onboard.locale=L"en";
                std::thread vehicle_worker([&] {
                    const auto started=std::chrono::steady_clock::now(); result=plan_snapshot(onboard);
                    worst_vehicle_ms=std::max(worst_vehicle_ms,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());
                }); vehicle_worker.join();
                if(!result.path.success) std::printf("FAIL FF%d world %d vehicle %d type %d group %d: onboard destination (%d,%d)\n",number,index,row.id,row.type,row.group,target.x,target.y);
                expect(result.path.success, "representative boarded vehicles reach their world-map land destination"); ++vehicle_cases;
                for(const auto& phase:result.path.walk_phases) expect(phase.action_before.find(L"board")==std::wstring::npos, "already-boarded plans never require boarding again");
            }
        }
    }
    expect(vehicle_cases>=20, "all-six boarded-vehicle regression has representative coverage");
    std::printf("All-six boarded vehicles: %d world/transport cases; %d without eligible start/land destination; worst route %.3f ms, filter %.3f ms (offline)\n",vehicle_cases,no_landing,worst_vehicle_ms,worst_vehicle_filter_ms);
    expect(fixture::wrong_owner == 0, "world search/cancellation also avoid all native APIs");
    std::printf("Route worker check: %d failures\n", failures); return failures ? 1 : 0;
}
