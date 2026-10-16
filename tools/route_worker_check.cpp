// check-sources: field/baked_map.cpp field/baked_router.cpp field/map_state_program.cpp field/field_refs.cpp field/entity_filter.cpp field/npc_names.cpp json.cpp text.cpp
// Actual baked planner snapshot on a worker. Every fake native API flags worker use.
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <thread>
#include <fstream>
#include <chrono>
#include <mutex>
#include "../src/core/field/baked_nav.cpp"
#include "../src/core/field/entity_filter.h"
namespace fixture {
const DWORD native_owner = GetCurrentThreadId();
std::atomic<int> wrong_owner{0};
int native_reads = 0;
bool capture_ready=false;
int current_map=0;
unity::Vec3 current_position;
std::vector<int> auto_groups;
std::vector<std::function<void()>> queued,completed;
std::vector<std::string> scheduled;
std::mutex completion_lock;
game::Id game_id = game::Id::FF2;
void native() { if (GetCurrentThreadId() != native_owner) ++wrong_owner; else ++native_reads; }
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
std::wstring rd_str(void*,Field&) {fixture::native();return {};}
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
void* transform(void* p) { fixture::native(); return fixture::capture_ready?p:nullptr; }
Vec3 local_position(void*) { fixture::native(); return fixture::current_position; }
int list_count(void*) { fixture::native(); return 0; }
void* list_at(void*, int) { fixture::native(); return nullptr; }
int array_length(void*) { fixture::native(); return 0; }
}
namespace log_ { void write(const char*, ...) {} }
namespace hooks { void note_fault(const char*) { std::abort(); } }
namespace frame {
double now() { return GetTickCount64() / 1000.0; }
void post(Task task) { std::lock_guard lock(fixture::completion_lock);fixture::completed.push_back(std::move(task)); }
int every_frame(Step, const char* label, int) { fixture::native();fixture::scheduled.push_back(label); return 0; }
void cancel_label(const char*) { fixture::native(); }
}
namespace background { bool post(std::function<void()> work, Lane) { fixture::native();if(!fixture::capture_ready)return false;fixture::queued.push_back(std::move(work));return true; } }
namespace ffpr { void say(std::wstring_view, bool) { fixture::native(); } }
namespace ffpr { void say_sequence(std::vector<std::wstring>, bool) { fixture::native(); } }
namespace settings { bool get_bool(const char*, bool value) { return value; } }
namespace ffpr::system { bool is_field_navigation_ready() { fixture::native(); return fixture::capture_ready; } }
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
int current_map_id() { fixture::native(); return fixture::current_map; }
void* map_handle() { fixture::native(); return nullptr; }
void* player() { fixture::native(); return fixture::capture_ready?reinterpret_cast<void*>(1):nullptr; }
unity::Vec3 player_position() { fixture::native(); return {}; }
VehicleState current_vehicle() { fixture::native(); return {}; }
std::vector<int> owned_auto_vehicle_groups() { fixture::native(); return fixture::auto_groups; }
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
    int retired = 0;
    std::vector<worker_job::Owner*> owners{&g_route_owner,&g_follow_owner,&g_quick_owner,&g_browse_owner};
    std::vector<uint64_t> epochs;
    for (auto* owner : owners) { epochs.push_back(owner->epoch()->load()); owner->retain([&] { ++retired; }); }
    const int reads_before_interrupt = fixture::native_reads;
    interrupt_requests();
    bool interrupted = true;
    for (size_t i=0;i<owners.size();++i) interrupted &= owners[i]->epoch()->load() == epochs[i]+1;
    expect(interrupted && retired == 0 && fixture::native_reads == reads_before_interrupt,
        "battle entry cancels all four real route worker epochs without native APIs, callback destruction or frame-queue cleanup");
    cancel_requests(); cancel_quick_work(); cancel_browse_route();
    expect(retired == 4, "ordinary lifecycle cleanup retires all four interrupted owners once");
    const int blocked_reads=fixture::native_reads;
    bool blocked_publish=false;
    request_route({},field::TargetKind::Infer,L"",[&](field::PathInfo){blocked_publish=true;});
    expect(!blocked_publish&&fixture::native_reads==blocked_reads+2&&!request_pending(),
        "transition-blocked direction request only cancels its owned label and checks readiness, with no map/entity capture");
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
    {
        ExitSnapshot landing(false, field::map_state::Resolver(field::map_state::Flags{}, fixture::records));
        landing.route.submap = shop; landing.route.map_id = shop->first_map_id; landing.route.layer = 0;
        landing.route.position = snapshot.position;
        const auto target = world_of_cell(*shop, {0, 8, 10});
        bool clear = false, occupied = true, invalid = true;
        std::thread clearance([&] {
            clear = teleport_landing_safe(landing, target);
            NativeObstacle npc; npc.npc = true; npc.cell = {0, 8, 10};
            landing.route.obstacles.push_back(npc);
            occupied = teleport_landing_safe(landing, target);
            invalid = teleport_landing_safe(landing, {1.e30f, 0, 0});
        }); clearance.join();
        expect(clear && !occupied && !invalid, "real Fynn landing accepts clear terrain, rejects copied NPC occupancy and oversized coordinates");
        expect(fixture::wrong_owner == 0, "production landing clearance never reads native objects on its worker");
    }
    const auto* world = field::baked::submap_by_name(L"Map_10010");
    expect(world != nullptr, "world bake exists");
    if (world) {
        RouteSnapshot long_route(field::map_state::Resolver(field::map_state::Flags{}, fixture::records));
        long_route.submap = world; long_route.map_id = world->first_map_id; long_route.layer = 1;
        long_route.position = world_of_cell(*world, {1, 124, 36});
        long_route.auto_groups = {2}; // Semitt Falls requires the owned canoe
        long_route.goal.map_id = world->first_map_id; long_route.goal.cell = {1, 107, 43};
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
        if(result.active&&!result.active->phases.empty()&&result.active->phases[0].cells.size()>2) {
            g_published_route=result.active;g_active=g_published_route.get();
            fixture::capture_ready=true;fixture::current_map=world->first_map_id;fixture::auto_groups={2};fixture::scheduled.clear();
            const auto& cells=result.active->phases[0].cells;
            fixture::current_position=world_of_cell(*world,cells[1]);
            int published=0;field::PathInfo answer;
            const auto began=std::chrono::steady_clock::now();
            request_route(long_route.goal,long_route.kind,L"",[&](field::PathInfo path){++published;answer=std::move(path);});
            fixture::current_position=world_of_cell(*world,cells[2]);
            request_route(long_route.goal,long_route.kind,L"",[&](field::PathInfo path){++published;answer=std::move(path);});
            const double dispatch_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();
            expect(fixture::queued.size()==2&&fixture::scheduled.empty(),"small real direction requests submit their bounded capture immediately without waiting a frame");
            auto jobs=std::move(fixture::queued);fixture::queued.clear();
            std::thread worker([&]{for(auto& job:jobs)job();});worker.join();
            auto publications=std::move(fixture::completed);fixture::completed.clear();for(auto& publication:publications)publication();
            expect(published==1&&answer.success&&answer.step_count==result.path.step_count-2&&!request_pending(),
                "rapid repeated actual requests cancel stale work and publish only the latest moved-player route");
            std::printf("Actual warm direction pipeline: two requests, latest-only answer, native dispatch %.3f ms; total %.3f ms (fake native owners, worker launch/join, immediate owner delivery; excludes game-frame/Prism timing)\n",
                dispatch_ms,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count());
            fixture::capture_ready=false;fixture::current_map=0;fixture::auto_groups.clear();
            g_published_route.reset();g_active=nullptr;clear_pending(true);
        }
        {
            SnapshotScope scope(long_route);
            Phase approach,ride,landing;
            for(int y=32;y>=16;--y)approach.cells.push_back({1,16,y});
            ride.cells={{1,16,16},{1,16,15}};for(int x=15;x>=1;--x)ride.cells.push_back({1,x,15});
            ride.action_before=L"board the Ship North";ride.vehicle_before=field::route_progress::VehicleAction::WalkOn;
            ride.vehicle_direction=0;ride.action_cell=ride.cells.front();
            landing.cells={{1,1,15},{1,1,16}};landing.action_before=L"land South";
            landing.vehicle_before=field::route_progress::VehicleAction::WalkOff;landing.vehicle_direction=2;landing.action_cell=landing.cells.front();
            expect(describe({approach,ride,landing},true)==L"16 North, board the Ship North, 15 West, land South",
                "vehicle action speech includes shore headings without counting the boarding/landing step twice");
        }
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
                    expect(ride_phase.action_before.empty() && walk_phase.action_before==L"land "+std::wstring(direction_key(walk_phase.vehicle_direction)),
                        "already-boarded route speaks the exact shore heading without boarding again");
                    expect(ride_phase.cells.back()==walk_phase.cells.front(), "ship and walking phases meet at the same legal coastal stop");
                    for(const auto& cell:ride_phase.cells) expect(field::baked::vehicle_open(field::baked::grids(*world),4,cell.x,cell.y), "riding leg remains on the ship grid");
                    const auto& shore=walk_phase.cells[1];
                    const auto heading=field::baked_route::legs({ride_phase.cells.back(),shore},world->width,world->height);
                    expect(heading.size()==1&&heading[0].dir==walk_phase.vehicle_direction,
                        "spoken landing heading matches the actual planned water-to-shore step, including wrapping");
                    expect(field::baked::group_lands(ship->landing_group,field::baked::attribute(field::baked::grids(*world),shore.x,shore.y)), "walking leg starts through the native ship landing policy");
                    double worst=0;
                    const auto saved=std::make_shared<ActiveRoute>(*result.active);
                    const auto& cells=saved->phases[0].cells;
                    for(size_t step=1;step<std::min<size_t>(cells.size(),21);++step) {
                        auto warm=onboard;warm.previous_active=saved;warm.position=world_of_cell(*world,cells[step]);
                        const auto begin=std::chrono::steady_clock::now();
                        const auto answer=plan_snapshot(warm);
                        worst=std::max(worst,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());
                        expect(answer.path.success&&answer.path.step_count==result.path.step_count-static_cast<int>(step),
                            "repeated directions after ship movement reuse the valid remaining route and retain landing/walk phases");
                    }
                    std::printf("FF2 warm ship target (%d,%d): worst %.3f ms over up to20 movement requests (owned offline compute)\n",target.x,target.y,worst);
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
        RouteSnapshot refresh(field::map_state::Resolver(field::map_state::Flags{},fixture::records));
        refresh.submap=world;refresh.map_id=world->first_map_id;refresh.layer=1;
        refresh.position=world_of_cell(*world,{1,126,129});refresh.vehicle={true,4,2,L"MSG_SYSTEM_200"};
        refresh.cancelled=[] {return true;};
        std::thread obsolete([&] {
            SnapshotScope scope(refresh);
            auto walking=walk_request(*world,{1,126,129});world_mode(walking);walking.cancelled=&planning_cancelled;
            std::vector<Phase> ignored;
            const auto begin=std::chrono::steady_clock::now();
            expect(!boarding_route(*world,field::baked::grids(*world),walking,{{1,110,98}},ignored),
                "destination refresh cancellation reaches nested vehicle planning, not only its first flood");
            std::printf("Cancelled FF2 vehicle refresh: %.3f ms tail (offline)\n",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());
        });obsolete.join();
    }
    int vehicle_cases=0, no_landing=0, parked_cases=0;
    int world_exit_cases=0, world_exit_reachable=0, world_exit_pruned=0;
    double worst_exit_ms=0;
    double worst_vehicle_ms=0, worst_vehicle_filter_ms=0;
    for(int number=1; number<=6; ++number) {
        bool parked_checked=false;
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
            if (!parked_checked) for(const auto& transport:field::baked::transports()) {
                if(transport.type==1||transport.type==5||transport.group<=0||transport.group>static_cast<int>(grid.transport.size()))continue;
                Cell shore,parking;bool found=false;
                field::baked_route::Request walking;walking.submap=map;
                for(int y=0;y<grid.height&&!found;++y)for(int x=0;x<grid.width&&!found;++x) {
                    const Cell at{1,x,y};if(!field::baked_route::open(walking,grid,at))continue;
                    for(const auto& side:std::vector<Cell>{{0,0,-1},{0,1,0},{0,0,1},{0,-1,0}}) {
                        Cell beside{1,(x+side.x+grid.width)%grid.width,(y+side.y+grid.height)%grid.height};
                        if(field::baked::vehicle_open(grid,transport.group,beside.x,beside.y)&&
                            field::baked::group_crosses(transport.group,field::baked::attribute(grid,beside.x,beside.y))) {
                            shore=at;parking=beside;found=true;break;
                        }
                    }
                }
                if(!found)continue;
                ExitSnapshot capture(true,field::map_state::Resolver(chest_flags,chest_records));
                auto& parked=capture.route;parked.submap=map;parked.map_id=map->first_map_id;parked.layer=1;
                parked.position=world_of_cell(*map,shore);
                field::VehicleOnMap vehicle;vehicle.transportation_id=transport.id;vehicle.type=transport.type;vehicle.pos=world_of_cell(*map,parking);parked.vehicles={vehicle};
                std::vector<ExitTarget> exits;std::vector<const field::baked::Object*> sources;
                for(const auto& object:grid.objects)if(object.type==field::baked::ObjType::GotoMap) {
                    exits.push_back({world_of_cell(*map,{1,object.tx,object.ty}),0,object.a,{1,object.tx,object.ty}});sources.push_back(&object);
                }
                if(exits.empty())continue;
                std::thread parked_worker([&] {
                    const auto began=std::chrono::steady_clock::now();
                    const auto distances=exit_distances(capture,exits,{});
                    const double bulk_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();
                    SnapshotScope scope(parked);
                    auto req=walk_request(*map,shore);const auto live=live_blockers(*map,grid,{});
                    req.blocked=live.hard();req.directional=live.directional;req.live_hazards=true;req.hazards=live.triggers;req.live_traps=true;req.traps=live.traps;
                    const auto reachable=field::baked_route::flood(req);
                    const auto reference_began=std::chrono::steady_clock::now();
                    for(size_t i=0;i<sources.size();++i) {
                        int reference=-1;
                        for(const auto& object:grid.objects) {
                            if(object.type!=field::baked::ObjType::GotoMap||object.a!=exits[i].destination||
                                !object.contains(sources[i]->tx,sources[i]->ty)||!parked.state.active(*map,object))continue;
                            auto request=req;
                            for(int y=object.ty;y<object.ty+object.th;++y)for(int x=object.tx;x<object.tx+object.tw;++x)
                                if(field::baked_route::transition_enterable(request,grid,x,y))request.goals.push_back({1,x,y});
                            reference=field::baked_route::transition_distance(request,reachable);
                            if(reference<0&&!request.goals.empty()) {
                                std::vector<Phase> route;
                                if(boarding_route(*map,grid,request,request.goals,route)) {
                                    reference=1;
                                    for(const auto& phase:route)if(phase.vehicle_before==field::route_progress::VehicleAction::WalkOn || phase.vehicle_before==field::route_progress::VehicleAction::WalkOff) {
                                        expect(phase.cells.size()>1&&phase.vehicle_direction==field::baked_route::direction_towards(phase.cells[0],phase.cells[1],map->width,map->height)&&
                                            phase.action_before.ends_with(direction_key(phase.vehicle_direction)),
                                            "all-six watercraft route actions speak their exact physical boarding/landing direction");
                                    }
                                }
                            }
                            if(reference>=0)break;
                        }
                        expect((distances[i]>=0)==(reference>=0),"parked-vehicle bulk distances retain exactly the exits usable by full route construction");
                    }
                    const double reference_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-reference_began).count();
                    std::printf("FF%d parked vehicle %d: %zu exits, bulk %.3f ms, previous per-exit route reference %.3f ms (offline)\n",number,transport.id,exits.size(),bulk_ms,reference_ms);
                    int checks=0;parked.cancelled=[&]{return ++checks>500;};req.cancelled=&planning_cancelled;
                    const auto cancel_began=std::chrono::steady_clock::now();
                    const auto cancelled=parked_vehicle_reach(req,reachable);
                    expect(!cancelled.submap&&checks<=510,"parked-vehicle refresh cancellation reaches its inner riding/landing search promptly");
                    std::printf("FF%d parked cancellation: %.3f ms, %d token checks\n",number,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-cancel_began).count(),checks);
                    parked.cancelled={};
                });parked_worker.join();
                parked_checked=true;++parked_cases;break;
            }
            {
                using namespace field::filter_worker;
                FilterInput input; input.world=true; input.exits=true; input.sort=true;
                input.navigation=std::make_shared<ExitSnapshot>(false,field::map_state::Resolver(field::map_state::Flags{},chest_records));
                auto& copied=input.navigation->route;
                copied.submap=map; copied.map_id=map->first_map_id; copied.layer=1;
                bool start=false;
                for (int y=0;y<grid.height&&!start;++y) for (int x=0;x<grid.width&&!start;++x)
                    if (field::baked::vehicle_open(grid,1,x,y)) { copied.position=world_of_cell(*map,{1,x,y}); start=true; }
                input.player=copied.position;
                for (const auto& object:grid.objects) {
                    if (object.type!=field::baked::ObjType::GotoMap) continue;
                    FilterRow row; row.index=input.rows.size(); row.kind=field::Kind::MapExit;
                    row.category=field::Category::MapExits; row.destination=object.a;
                    row.position=row.numbering_position=world_of_cell(*map,{1,object.tx,object.ty});
                    row.cell_x=object.tx; row.cell_y=object.ty; row.cell_layer=1;
                    input.rows.push_back(std::move(row));
                }
                Categories categories;
                std::thread exit_worker([&] {
                    const auto began=std::chrono::steady_clock::now(); categories=build_categories(input);
                    worst_exit_ms=std::max(worst_exit_ms,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count());
                }); exit_worker.join();
                auto movement=walk_request(*map,cell_of(*map,copied.position,1));
                std::map<int,int> minimum;
                for (const auto& row:input.rows) {
                    ++world_exit_cases;
                    if(row.route_distance<0) { ++world_exit_pruned; continue; }
                    ++world_exit_reachable;
                    expect(field::baked_route::transition_enterable(movement,grid,row.cell_x,row.cell_y),"every retained world exit is on enterable terrain");
                    auto found=minimum.find(row.destination);
                    if(found==minimum.end()||row.route_distance<found->second) minimum[row.destination]=row.route_distance;
                }
                std::unordered_set<int> seen;
                for(size_t index:categories.views[0].order) {
                    const auto& row=input.rows[index];
                    expect(row.route_distance>=0&&minimum[row.destination]==row.route_distance&&seen.insert(row.destination).second,
                        "all-six world grouping keeps exactly the nearest reachable trigger per destination");
                    RouteSnapshot route(field::map_state::Resolver(field::map_state::Flags{},chest_records));
                    route.submap=map;route.map_id=map->first_map_id;route.layer=1;route.position=copied.position;
                    route.goal.map_id=route.map_id;route.goal.cell={1,row.cell_x,row.cell_y};route.kind=field::TargetKind::Transition;route.locale=L"en";
                    std::thread route_worker([&]{result=plan_snapshot(route);});route_worker.join();
                    expect(result.path.success,"retained grouped world exit is usable by the production route planner");
                }
                expect(seen.size()==minimum.size(),"group filter preserves every reachable world destination");
                input.exits=false; const auto ungrouped=build(input);
                expect(ungrouped.order.size()==static_cast<size_t>(std::count_if(input.rows.begin(),input.rows.end(),[](const auto& row){return row.route_distance>=0;})),
                    "grouping off retains each reachable trigger and still prunes invalid world tiles");
                if(!categories.views[0].order.empty()) {
                    const auto index=categories.views[0].order.front(); const auto& row=input.rows[index];
                    NativeObstacle gate;gate.type=19;gate.cell={-1,row.cell_x,row.cell_y};gate.collider=true;gate.collision=1;
                    copied.obstacles.push_back(gate);
                    std::thread gated([&]{categories=build_categories(input);});gated.join();
                    expect(input.rows[index].route_distance<0,"copied native story collider vetoes a world transition before grouping");
                }
            }
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
                // Exercise the actual exit-filter path while already aboard,
                // rather than inferring its behavior from a successful route.
                for(bool with_vehicles : {false,true}) {
                    ExitSnapshot filtered(with_vehicles,field::map_state::Resolver(field::map_state::Flags{},fixture::records));
                    filtered.route.submap=map;filtered.route.map_id=onboard.map_id;filtered.route.layer=1;
                    filtered.route.position=onboard.position;filtered.route.vehicle=onboard.vehicle;filtered.route.auto_groups=onboard.auto_groups;
                    std::vector<ExitTarget> exits;
                    for(const auto& object:grid.objects) if(object.type==field::baked::ObjType::GotoMap)
                        exits.push_back({world_of_cell(*map,{1,object.tx,object.ty}),0,object.a,{1,object.tx,object.ty}});
                    std::vector<int> distances;
                    std::thread filter([&]{distances=exit_distances(filtered,exits,{});});filter.join();
                    if(number==6 && (row.type==3||row.type==8))
                        expect(std::any_of(distances.begin(),distances.end(),[](int d){return d>=0;}),
                            "both FF6 floor and vehicles filters retain destinations reachable from an already flying airship");
                }
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
    expect(parked_cases==6,"all six games exercise walking plus a parked vehicle during destination refresh");
    std::printf("All-six boarded vehicles: %d world/transport cases; %d without eligible start/land destination; worst route %.3f ms, filter %.3f ms (offline)\n",vehicle_cases,no_landing,worst_vehicle_ms,worst_vehicle_filter_ms);
    std::printf("All-six world exits: %d candidates, %d reachable, %d pruned from representative starts; worst six-category preparation %.3f ms (offline)\n",world_exit_cases,world_exit_reachable,world_exit_pruned,worst_exit_ms);
    expect(fixture::wrong_owner == 0, "world search/cancellation also avoid all native APIs");
    std::printf("Route worker check: %d failures\n", failures); return failures ? 1 : 0;
}
