// check-sources: field/entity_filter.cpp field/npc_names.cpp field/field_refs.cpp text.cpp
// Actual scanner/capture/publication/input flow, with fake native owners only.
#include <windows.h>
#include <thread>
#include <mutex>
#include <chrono>
#include <cstdio>
#include <cstring>
#include "../src/core/field/entity_scanner.cpp"
namespace fixture {
const DWORD owner = GetCurrentThreadId();
int wrong_owner = 0, map = 1, next_task = 0;
bool ready = true;
unity::Vec3 pose{};
struct Controller { void* player = reinterpret_cast<void*>(1); } controller;
struct Task { frame::Step step; std::string label; };
std::map<int, Task> tasks;
std::vector<frame::Task> navigation, compute, posted;
std::mutex post_lock;
void native() { if (GetCurrentThreadId() != owner) ++wrong_owner; }
void tick() {
    std::vector<frame::Task> completed;
    { std::lock_guard lock(post_lock); completed.swap(posted); }
    for (auto& task : completed) task();
    std::vector<int> ids;
    for (const auto& [id, task] : tasks) ids.push_back(id);
    for (int id : ids) {
        auto it = tasks.find(id); if (it == tasks.end()) continue;
        auto task = std::move(it->second); tasks.erase(it);
        if (task.step()) tasks.emplace(id, std::move(task));
    }
}
void work() {
    auto jobs = std::move(navigation); navigation.clear();
    std::thread worker([&] { for (auto& job : jobs) job(); }); worker.join();
}
}
namespace ref {
Class::Class(const char* n, const char* c, uint8_t g, const char* a) : ns(n), name(c), games(g), assembly(a) {}
Field::Field(Class& c, const char* n, const char* a) : cls(c), name(n), alt(a) {}
Method::Method(Class& c, const char* n, int a) : cls(c), name(n), argc(a) {}
il2cpp::Class* Class::get() { fixture::native(); return nullptr; }
bool Class::is(void*) { fixture::native(); return false; }
int32_t Field::get() { fixture::native(); return std::strcmp(name, "fieldPlayer") == 0 ? 0 : -1; }
il2cpp::Method* Method::get() { fixture::native(); return nullptr; }
void* Method::ptr() { fixture::native(); return nullptr; }
std::wstring str(void*) { fixture::native(); return {}; }
std::wstring rd_str(void*, Field&) { fixture::native(); return {}; }
void* instance(Class&) { fixture::native(); return nullptr; }
void call_failed_note(const char*) {}
void* new_str(const std::wstring&) { fixture::native(); return nullptr; }
}
namespace il2cpp {
Class* object_class(void*) { fixture::native(); return nullptr; }
const char* object_class_name(void*) { fixture::native(); return "FieldEntity"; }
Method* find_method_overload_param(Class*, const char*, int, Class*, int) { fixture::native(); return nullptr; }
void* method_pointer(Method*) { fixture::native(); return nullptr; }
Method* virtual_method(void*, Method*) { fixture::native(); return nullptr; }
}
namespace unity {
bool alive(void* p) { fixture::native(); return p != nullptr; }
void* game_object(void* p) { fixture::native(); return p; }
void* transform(void* p) { fixture::native(); return p; }
Vec3 local_position(void*) { fixture::native(); return fixture::pose; }
std::wstring name(void*) { fixture::native(); return L"Entity"; }
bool active_in_hierarchy(void*) { fixture::native(); return true; }
}
namespace frame {
double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
void post(Task task) { std::lock_guard lock(fixture::post_lock); fixture::posted.push_back(std::move(task)); }
int every_frame(Step step, const char* label, int) {
    fixture::native(); const int id = ++fixture::next_task;
    fixture::tasks.emplace(id, fixture::Task{std::move(step), label ? label : ""}); return id;
}
void cancel_label(const char* label) {
    fixture::native();
    for (auto it = fixture::tasks.begin(); it != fixture::tasks.end();)
        if (it->second.label.starts_with(label)) it = fixture::tasks.erase(it); else ++it;
}
}
namespace background {
bool post(std::function<void()> task, Lane lane) {
    fixture::native();
    (lane == Lane::Navigation ? fixture::navigation : fixture::compute).push_back(std::move(task)); return true;
}
}
namespace hooks { void note_fault(const char*) { std::abort(); } }
namespace log_ { void write(const char*, ...) {} }
namespace game { const Flags& flags() { static Flags value; return value; } const char* key() { return "ff2"; } }
namespace settings {
int get_int(const char*, int fallback) { return fallback; }
bool get_bool(const char*, bool fallback) { return fallback; }
}
namespace ffpr { void say(std::wstring_view, bool) { fixture::native(); } }
namespace ffpr::system { bool is_field_navigation_ready() { fixture::native(); return fixture::ready; } }
namespace ffpr::fieldnav::map_transition { bool is_screen_fading() { fixture::native(); return false; } }
namespace modtext {
LocaleScope::LocaleScope(std::wstring) {}
LocaleScope::~LocaleScope() {}
const std::wstring& lang() { fixture::native(); static std::wstring value = L"en"; return value; }
std::wstring T(std::wstring_view value) { return std::wstring(value); }
std::wstring TF(std::wstring_view value, std::initializer_list<std::wstring>) { return std::wstring(value); }
}
namespace field::cache {
void* get(ref::Class& type) { fixture::native(); return std::strcmp(type.name, "FieldPlayerController") == 0 ? &fixture::controller : nullptr; }
void* refresh(ref::Class& type) { return get(type); }
}
namespace field {
int current_map_id_or_user_data() { fixture::native(); return fixture::map; }
int current_map_id() { return current_map_id_or_user_data(); }
void* player() { fixture::native(); return nullptr; }
void* map_handle() { fixture::native(); return nullptr; }
bool entity_is_active(void*) { fixture::native(); return true; }
bool entity_is_hidden_by_vehicle(void*) { fixture::native(); return false; }
bool entity_is_scenery(void*) { fixture::native(); return false; }
bool entity_on_current_map(void*, const std::wstring&) { fixture::native(); return true; }
int object_type_of(void*) { fixture::native(); return 0; }
int goto_map_destination_id(void*) { fixture::native(); return 0; }
bool vehicle_type_of(void*, int&, int&, std::wstring&) { fixture::native(); return false; }
std::wstring vehicle_display_name(int, int, const std::wstring&) { fixture::native(); return {}; }
std::wstring vehicle_name_from_property(const std::wstring&, const std::wstring&) { fixture::native(); return {}; }
int vehicle_type_from_name(const std::wstring&) { fixture::native(); return 0; }
bool treasure_opened(void*) { fixture::native(); return false; }
std::wstring treasure_contents(void*) { fixture::native(); return {}; }
std::wstring entity_name_from_property(void*) { fixture::native(); return {}; }
std::wstring clean_object_name(const std::wstring& name, const std::wstring&) { return name; }
std::wstring safe_event_label(void*, const std::wstring& name) { fixture::native(); return name; }
std::wstring resolve_map_name(int) { fixture::native(); return {}; }
std::wstring resolve_item_name(int) { fixture::native(); return {}; }
std::wstring current_map_asset_name() { fixture::native(); return {}; }
std::wstring translate_entity_name(const std::wstring& name) { fixture::native(); return name; }
npc_names::Label npc_display_label(const std::wstring& name) { fixture::native(); return npc_names::parse(name); }
bool is_decimal_digit(wchar_t c) { return c >= L'0' && c <= L'9'; }
bool EntityCursor::next(void*&) { fixture::native(); return false; }
bool Entity::is_alive() const { fixture::native(); return game != reinterpret_cast<void*>(2); }
Category Entity::category() const { return kind == Kind::NPC ? Category::NPCs : Category::MapExits; }
unity::Vec3 Entity::position() const { fixture::native(); return pos; }
PathInfo find_path_to_quick(unity::Vec3, unity::Vec3, void*, void*, TargetKind, bool) { fixture::native(); return {}; }
}
namespace field::baked_nav {
bool available(void*) { fixture::native(); return true; }
bool prepared() { fixture::native(); return true; }
bool cell_at(void*, unity::Vec3, baked_route::Cell&) { fixture::native(); return false; }
bool pending_goal(MapGoal&, TargetKind&, std::wstring&) { fixture::native(); return false; }
void request_reachability(bool, std::function<void()> ready) { fixture::native(); ready(); }
std::vector<RemoteObject> connected_objects_snapshot(int, map_state::Resolver&) { return {}; }
}
namespace field::map_state { Resolver::Resolver() : Resolver(Flags{}, json::Value{}) { fixture::native(); } }
int main() {
    using namespace field;
    int failures = 0;
    auto expect = [&](bool ok, const char* reason) { if (!ok) { ++failures; std::printf("FAIL %s\n", reason); } };
    g_last_scanned_map_id = fixture::map;
    for (int i = 0; i < 2000; ++i) {
        auto entity = std::make_shared<Entity>(); entity->kind = i % 2 ? Kind::NPC : Kind::MapExit;
        entity->name = entity->converted_name = i % 2 ? L"Soldier" : L"Exit";
        entity->pos = {static_cast<float>(i), 0, 0}; g_entities.push_back(std::move(entity));
    }
    apply_filter();
    int frames = 0;
    while (g_filter_pending && frames++ < 100) { fixture::tick(); fixture::work(); }
    expect(g_views_ready && scanner::count() == 2000, "initial owner capture and worker publication prepare the actual list");
    expect(frames < 100, "2,000-row preparation has no four-entry-per-frame delay");
    expect(fixture::wrong_owner == 0, "worker build makes zero native calls");
    const auto started = frame::now();
    const auto task_count = fixture::tasks.size();
    for (int i = 0; i < 1000; ++i) { scanner::set_category(Category::NPCs); scanner::set_category(Category::MapExits); }
    expect(scanner::count() == 1000 && fixture::tasks.size() == task_count && fixture::navigation.empty(),
        "warm category switches enqueue no capture, name publication or worker job");
    const auto category_us = (frame::now() - started) * 1e6 / 2000;
    fixture::pose = {1000, 0, 0};
    scanner::set_category(Category::NPCs);
    expect(!g_views_ready && g_filter_pending, "movement refreshes category distance ordering without a full entity scan");
    for (int i = 0; g_filter_pending && i < 100; ++i) { fixture::tick(); fixture::work(); }
    expect(g_views_ready && filtered_view()[0]->pos.x == 999, "new category starts at the nearest target from the current position");
    scanner::set_category(Category::MapExits);
    g_sel = filtered_view()[5]; g_sel_has = true; g_index = 5;
    apply_filter(); // leave the refresh worker/capture blocked
    scanner::g_scan_running = true;
    bool spoken = false;
    scanner::when_filter_ready([&] { spoken = true; });
    expect(spoken, "browse readiness completes immediately while both live scan and filter refresh are pending");
    g_sel = filtered_view()[9]; g_index = 9; // input changes while refresh is pending
    for (int i = 0; g_filter_pending && i < 100; ++i) { fixture::tick(); fixture::work(); }
    expect(scanner::current_entity() == g_sel, "refresh publication keeps the most recent input selection");
    filtered_view()[10]->game = reinterpret_cast<void*>(2);
    scanner::next_entity();
    expect(scanner::current_index() == 11, "warm browsing skips a despawned native handle even without reachability filtering");
    scanner::invalidate_map();
    spoken = false; scanner::when_filter_ready([&] { spoken = true; });
    expect(!spoken, "map invalidation requires a new ready generation");
    ++fixture::map; fixture::tick(); fixture::work(); fixture::tick();
    expect(!spoken && !g_views_ready, "changed map retires pending input and cannot publish stale categories");
    std::printf("Scanner: 2,000 rows prepared in %d capture/publication ticks; warm category %.3f us; %d failures\n",
        frames, category_us, failures);
    // Retire intentionally blocked fixture work before C++ static registries
    // tear down. Production workers/queues live for the entire game process.
    scanner::invalidate_map();
    std::fflush(stdout);
    return failures ? 1 : 0;
}
