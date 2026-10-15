// Production teleport request/capture/publish; native objects and worker scheduling are fixtures.
#include <map>
#include <cstdio>
#include "../src/core/fieldnav/teleport.cpp"
namespace fixture {
bool enabled = false, ready = true, focused = true, modal = false, safe = true, worker = false;
bool riding = false, blocked = false, moving = false;
double time = 0;
int map = 1, next_task = 0, moves = 0, reads_on_worker = 0, captured = 0;
unity::Vec3 position{0, 0, 3};
field::Target selection{true, {32, 32, 0}, L"Test", reinterpret_cast<void*>(2)};
std::map<int, std::pair<std::string, frame::Step>> steps;
std::vector<frame::Task> jobs, posts;
std::vector<std::wstring> spoken;
void native() { if (worker) ++reads_on_worker; }
void move(void*, unity::Vec3 dest, const void*) { native(); ++moves; position = dest; }
bool is_moving(void*, const void*) { native(); return moving; }
void tick(bool run_jobs = true) {
    std::vector<int> ids; for (auto& [id, step] : steps) ids.push_back(id);
    for (int id : ids) { auto i = steps.find(id); if (i == steps.end()) continue; auto fn = i->second.second; if (!fn()) steps.erase(id); }
    if (run_jobs) {
        auto work = std::move(jobs); jobs.clear(); worker = true;
        for (auto& job : work) job(); worker = false;
        auto callbacks = std::move(posts); posts.clear(); for (auto& callback : callbacks) callback();
    }
}
void reset() {
    ffpr::fieldnav::teleport::g_owner.cancel(); steps.clear(); jobs.clear(); posts.clear(); spoken.clear();
    enabled = ready = focused = safe = true; modal = riding = blocked = moving = false;
    time = 0; map = 1; moves = captured = 0; position = {0, 0, 3}; selection.identity = reinterpret_cast<void*>(2);
}
}
namespace core { Install::Install(const char* n, int (*f)(), int o, uint8_t g) : name(n), fn(f), order(o), games(g) {} }
namespace ref {
Class::Class(const char* n, const char* c, uint8_t g, const char* a) : ns(n), name(c), games(g), assembly(a) {}
Method::Method(Class& c, const char* n, int a) : cls(c), name(n), argc(a) {}
void* Method::ptr() { fixture::native(); return std::string(name) == "get_IsMoving" ? reinterpret_cast<void*>(&fixture::is_moving) : reinterpret_cast<void*>(&fixture::move); }
il2cpp::Method* Method::get() { fixture::native(); return nullptr; }
void call_failed_note(const char*) {}
}
namespace settings { bool get_bool(const char*, bool) { fixture::native(); return fixture::enabled; } }
namespace queries {
bool is(const char* name) {
    fixture::native(); const std::string key = name;
    if (key == "GameStatePatches.IsFieldNavigationReady") return fixture::ready;
    if (key == "GameStatePatches.IsInputTransitionBlocked") return fixture::blocked;
    return false;
}
}
namespace menu_state { bool any_active() { fixture::native(); return false; } }
namespace ui { bool any_modal_open() { fixture::native(); return fixture::modal; } }
namespace input { bool game_window_focused() { fixture::native(); return fixture::focused; } bool suppress_game_input() { fixture::native(); return fixture::modal; } }
namespace unity { void* transform(void* p) { fixture::native(); return p; } Vec3 local_position(void*) { fixture::native(); return fixture::position; } }
namespace log_ { void write(const char*, ...) {} }
namespace modtext {
std::wstring T(std::wstring_view s) { return std::wstring(s); }
std::wstring TF(std::wstring_view s, std::initializer_list<std::wstring>) { return std::wstring(s); }
}
namespace ffpr { void say(std::wstring_view s, bool) { fixture::spoken.emplace_back(s); } }
namespace actions { void define(const char*, Fn) {} }
namespace frame {
double now() { return fixture::time; }
void post(Task t) { fixture::posts.push_back(std::move(t)); }
int every_frame(Step s, const char* label, int) { const int id = ++fixture::next_task; fixture::steps.emplace(id, std::pair{std::string(label), std::move(s)}); return id; }
void cancel_label(const char* prefix) { std::erase_if(fixture::steps, [&](const auto& entry) { return entry.second.first.starts_with(prefix); }); }
}
namespace background { bool post(std::function<void()> work, Lane) { fixture::jobs.push_back(std::move(work)); return true; } }
namespace ffpr::fieldnav::autowalk { void cancel(bool) {} }
namespace field {
void* player() { fixture::native(); return reinterpret_cast<void*>(1); }
void* map_handle() { fixture::native(); return reinterpret_cast<void*>(3); }
int current_map_id() { fixture::native(); return fixture::map; }
Target selected() { fixture::native(); return fixture::selection; }
VehicleState current_vehicle() { fixture::native(); VehicleState v; v.valid = true; v.id = 1; v.type = fixture::riding ? 2 : 1; return v; }
bool EntityCursor::next(void*& value) { fixture::native(); value = reinterpret_cast<void*>(4); return index++ < 2; }
namespace baked_nav {
struct ExitSnapshot {};
std::shared_ptr<ExitSnapshot> begin_exit_capture(bool, bool) { fixture::native(); return std::make_shared<ExitSnapshot>(); }
void capture_exit_obstacle(ExitSnapshot&, void*) { fixture::native(); ++fixture::captured; }
void finish_exit_capture(ExitSnapshot&) { fixture::native(); }
bool player_cell(void*, void*, baked_route::Cell& out) { fixture::native(); out = {1, static_cast<int>(fixture::position.x / 16), static_cast<int>(fixture::position.y / 16)}; return true; }
bool teleport_landing_safe(ExitSnapshot&, unity::Vec3) { return fixture::worker && fixture::safe; }
void invalidate() { fixture::native(); }
}
}
int main() {
    int failures = 0;
    auto expect = [&](bool ok, const char* text) { if (!ok) { ++failures; std::printf("FAIL %s\n", text); } };
    auto request = [] { ffpr::fieldnav::teleport::teleport_in_direction(0, 16); };
    fixture::reset(); fixture::enabled = false; request(); fixture::tick();
    expect(fixture::moves == 0 && fixture::steps.empty(), "default-off shortcuts schedule no teleport");
    fixture::reset(); request();
    expect(fixture::moves == 0, "request never moves before owned clearance completes");
    fixture::tick();
    expect(fixture::moves == 1 && fixture::captured == 2 && fixture::position.x == 32 && fixture::position.y == 48 && fixture::position.z == 3,
           "safe result moves once on native owner and preserves depth");
    fixture::reset(); fixture::safe = false; request(); fixture::tick();
    expect(fixture::moves == 0 && fixture::position.x == 0 && fixture::spoken.back() == L"Teleport blocked: no safe landing.", "unsafe result preserves source position and explains refusal");
    for (int change = 0; change < 9; ++change) {
        fixture::reset(); request(); fixture::tick(false);
        switch (change) {
        case 0: fixture::time = 0.6; break;
        case 1: fixture::enabled = false; break;
        case 2: fixture::map = 2; break;
        case 3: fixture::selection.identity = reinterpret_cast<void*>(5); break;
        case 4: fixture::position.x = 16; break;
        case 5: fixture::riding = true; break;
        case 6: fixture::focused = false; break;
        case 7: fixture::blocked = true; break;
        case 8: fixture::moving = true; break;
        }
        fixture::tick(); expect(fixture::moves == 0, "obsolete or stale teleport never writes native position");
    }
    fixture::reset(); request(); request(); fixture::tick();
    expect(fixture::moves == 1, "latest teleport cancels earlier capture/publication");
    fixture::reset(); fixture::riding = true; request(); fixture::tick();
    expect(fixture::moves == 0 && fixture::steps.empty(), "riding rejects unsafe player-only relocation");
    expect(fixture::reads_on_worker == 0, "worker receives only owned data and performs no engine/input/settings reads");
    std::printf("Teleport ownership/cancellation/clearance: %d failures\n", failures);
    return failures ? 1 : 0;
}
