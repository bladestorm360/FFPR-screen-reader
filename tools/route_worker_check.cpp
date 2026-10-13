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
namespace game { const Flags& flags() { static Flags f; return f; } const char* key() { return "ff2"; } }
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
        field::baked_route::Request cancelled;
        cancelled.submap = world; cancelled.start = {1, 123, 34}; cancelled.goals = {{1, 107, 44}};
        cancelled.cancelled = [] { return true; };
        expect(!field::baked_route::find(cancelled).found && field::baked_route::flood(cancelled).submap == nullptr,
               "retired contexts cancel A* and reachability computation");
    }
    expect(fixture::wrong_owner == 0, "world search/cancellation also avoid all native APIs");
    std::printf("Route worker check: %d failures\n", failures); return failures ? 1 : 0;
}
