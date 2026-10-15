// check-sources: field/field_refs.cpp text.cpp
// Actual category/browse/route producers with manually delayed readiness; no game.
#include <cstdio>
#include <cstring>
#include "../src/core/field/entity_nav.cpp"
namespace fixture {
struct Controller { void* player = reinterpret_cast<void*>(1); } controller;
struct Output { std::vector<std::wstring> parts; bool interrupt; };
std::vector<Output> output;
std::vector<std::function<void()>> pending;
std::function<void(field::PathInfo)> route;
field::EntityPtr entity = std::make_shared<field::Entity>();
field::Category category = field::Category::All;
int next = 0, previous = 0, cancelled = 0;
bool selectable = true, transition = false, field_ready = true, battle = false;
bool soundscape=true,beacons=false,combined=false;int beacon_requests=0;
int scans = 0, cache_reads = 0;
void finish() { auto callbacks = std::move(pending); pending.clear(); for (auto& callback : callbacks) callback(); }
}
namespace core { Install::Install(const char* n, int (*f)(), int o, uint8_t g) : name(n), fn(f), order(o), games(g) {} }
namespace ref {
Class::Class(const char* n, const char* c, uint8_t g, const char* a) : ns(n), name(c), games(g), assembly(a) {}
Field::Field(Class& c, const char* n, const char* a) : cls(c), name(n), alt(a) {}
Method::Method(Class& c, const char* n, int a) : cls(c), name(n), argc(a) {}
il2cpp::Class* Class::get() { return nullptr; }
bool Class::is(void*) { return false; }
int32_t Field::get() { return std::strcmp(name, "fieldPlayer") == 0 ? 0 : -1; }
il2cpp::Method* Method::get() { return nullptr; }
void* Method::ptr() { return nullptr; }
std::wstring str(void*) { return {}; }
std::wstring rd_str(void*, Field&) { return {}; }
void* instance(Class&) { return nullptr; }
void* new_str(const std::wstring&) { return nullptr; }
void call_failed_note(const char*) {}
}
namespace il2cpp {
Class* object_class(void*) { return nullptr; }
Method* find_method_overload_param(Class*, const char*, int, Class*, int) { return nullptr; }
void* method_pointer(Method*) { return nullptr; }
Method* virtual_method(void*, Method*) { return nullptr; }
}
namespace unity {
bool alive(void* p) { return p != nullptr; }
void* game_object(void* p) { return p; }
void* transform(void* p) { return p; }
bool active_in_hierarchy(void* p) { return p != nullptr; }
}
namespace hooks { bool attach_raw(ref::Method&, void*, void**) { return false; } void note_fault(const char*) { std::abort(); } }
namespace log_ { void write(const char*, ...) {} }
namespace ffpr {
void say(std::wstring_view text, bool interrupt) { fixture::output.push_back({{std::wstring(text)}, interrupt}); }
void say_sequence(std::vector<std::wstring> parts, bool interrupt) { fixture::output.push_back({std::move(parts), interrupt}); }
void say(std::wstring_view text, bool interrupt, speech::Timing) { say(text, interrupt); }
void say_sequence(std::vector<std::wstring> parts, bool interrupt, speech::Timing) { say_sequence(std::move(parts), interrupt); }
}
namespace modtext {
std::wstring T(std::wstring_view value) { return std::wstring(value); }
std::wstring TF(std::wstring_view value, std::initializer_list<std::wstring> args) {
    auto result = std::wstring(value); int i = 0;
    for (const auto& arg : args) { const auto token = L"{" + std::to_wstring(i++) + L"}"; const auto at = result.find(token); if (at != result.npos) result.replace(at, token.size(), arg); }
    return result;
}
}
namespace actions { bool defined(const char*) { return false; } bool run(const char* name) { if(std::strcmp(name,"Mod.RestartEntityBeacon")==0)++fixture::beacon_requests;return true; } void define(const char*, Fn) {} }
namespace queries {
bool is(const char* name) {
    if (std::strcmp(name, "GameStatePatches.IsInputTransitionBlocked") == 0) return fixture::transition;
    if (std::strcmp(name, "GameStatePatches.IsFieldNavigationReady") == 0) return fixture::field_ready;
    if (std::strcmp(name, "BattleStateHelper.IsInBattle") == 0) return fixture::battle;
    return true;
}
void define_text(const char*, std::function<std::wstring()>) {}
}
namespace menu_state { bool any_active() { return false; } }
namespace events { void on(Kind, Handler) {} void fire(Kind) {} }
namespace frame { double now() { return GetTickCount64()/1000.0; } void after_seconds(double, Task, const char*) {} void cancel_label(const char*) {} int every_frame(Step, const char*, int) { return 0; } }
namespace fixture { bool warnings = true; }
namespace settings { bool get_bool(const char* key, bool def) {
    if(std::strcmp(key,"ModWarnings")==0)return fixture::warnings;
    if(std::strcmp(key,"Soundscape")==0)return fixture::soundscape;
    if(std::strcmp(key,"AudioBeacons")==0)return fixture::beacons;
    if(std::strcmp(key,"BeaconWithDirections")==0)return fixture::combined;
    return def;
} }
namespace ffpr::fieldnav::autowalk { bool active() { return false; } bool stop_requested() { return false; } void request(field::baked_nav::MapGoal, field::TargetKind, std::wstring) {} }
namespace ffpr::fieldnav::audio_loops { void restart_beacon() { ++fixture::beacon_requests; } }
namespace field::cache {
void* get_or_refresh(ref::Class& type) { ++fixture::cache_reads; return std::strcmp(type.name, "FieldPlayerController") == 0 ? &fixture::controller : reinterpret_cast<void*>(1); }
void* refresh(ref::Class& type) { return get_or_refresh(type); }
void clear(ref::Class&) {}
}
namespace field {
int current_map_id() { return 1; }
std::wstring current_map_name() { return L"Map"; }
bool Entity::is_alive() const { return true; }
Category Entity::category() const { return Category::Chests; }
unity::Vec3 Entity::position() const { return pos; }
std::wstring Entity::display_name() const { return L"Chest"; }
std::wstring Entity::format_description(unity::Vec3) const { return L"Chest, north"; }
FilterContext::FilterContext() { field_player = map_handle = reinterpret_cast<void*>(1); }
}
namespace field::scanner {
void when_filter_ready(std::function<void()> ready, bool) { fixture::pending.push_back(std::move(ready)); }
void scan_entities() { ++fixture::scans; } void force_rescan() { ++fixture::scans; } void invalidate_map() {} void schedule_scan() {} void cancel_scan() {}
void reapply_filter() {} void set_filter_to_layer(bool) {}
void next_entity() { ++fixture::next; } void previous_entity() { ++fixture::previous; }
bool no_reachable_entities() { return false; } bool select_first_reachable() { return fixture::selectable; }
void set_category(Category c) { fixture::category = c; } Category category() { return fixture::category; }
EntityPtr current_entity() { return fixture::entity; } int current_index() { return 0; } int count() { return 1; }
double captured_at() { return frame::now(); }
}
namespace field::baked_nav {
void clear_pending(bool) {} void continue_pending() {} void invalidate() {}
void cancel_requests() { ++fixture::cancelled; }
bool cell_at(void*, unity::Vec3, baked_route::Cell&) { return true; }
void request_route(MapGoal, TargetKind, std::wstring, std::function<void(PathInfo)> publish, bool) { fixture::route = std::move(publish); }
}
int main() {
    int failures = 0;
    auto expect = [&](bool ok, const char* reason) { if (!ok) { ++failures; std::printf("FAIL %s\n", reason); } };
    for (int condition = 0; condition < 3; ++condition) {
        fixture::transition = condition == 0;
        fixture::field_ready = condition != 1;
        fixture::battle = condition == 2;
        field::cycle(true); field::cycle(false); field::category_next(); field::category_previous();
        field::category_reset_to_all(); field::announce_current_entity(); field::navigate_to_entity();
        field::announce_entity_only(); field::force_entity_rescan();
        expect(fixture::scans == 0 && fixture::cache_reads == 0 && fixture::pending.empty() && fixture::output.empty(),
            "every navigation entry rejects fades/loading/battle before cache reads, scans, speech or queued work");
    }
    fixture::transition = fixture::battle = false; fixture::field_ready = true;
    fixture::entity->kind = field::Kind::TreasureChest;
    field::cycle(true); // old category input, still waiting for readiness
    field::category_next();
    auto callbacks = std::move(fixture::pending); fixture::pending.clear();
    callbacks[1](); callbacks[0](); // new category is ready before the old browse callback
    expect(fixture::next == 0 && fixture::output.size() == 1,
        "new category retires an older delayed entity-cycle announcement");
    expect(fixture::cancelled >= 2, "new browse/category input cancels obsolete route computation without waiting");
    expect(fixture::output.back().parts == std::vector<std::wstring>{L"Category: Treasure Chests", L"Chest, north (1 of 1)"},
        "category and initially focused destination are one correctly ordered speech event");
    fixture::output.clear();
    field::cycle(true); field::cycle(true); fixture::finish();
    expect(fixture::next == 2 && fixture::output.size() == 2,
        "distinct queued browse inputs in the same category both execute");
    fixture::output.clear();
    field::category_next(); // old callback for NPCs
    for (int i = 0; i < 6; ++i) field::category_next(); // return to NPCs with new event identity
    fixture::finish();
    expect(fixture::output.size() == 1 && fixture::output.back().parts.front() == L"Category: NPCs",
        "returning to the same category enum cannot revive an obsolete callback");
    fixture::output.clear(); fixture::selectable = false;
    field::category_next(); fixture::finish();
    expect(fixture::output.size() == 1 && fixture::output.back().parts.size() == 1,
        "empty category announces only its current category headline");
    fixture::output.clear(); fixture::selectable = true;
    field::announce_current_entity();
    expect(!fixture::route,"directions wait for current grouped-exit representative before planning");
    fixture::finish();
    expect(static_cast<bool>(fixture::route), "actual route producer waits for asynchronous plan");
    field::category_next(); fixture::finish();
    auto before = fixture::output.size(); field::PathInfo path; path.success = true; path.description = L"1 north";
    fixture::route(path);
    expect(fixture::output.size() == before, "old asynchronous route cannot interrupt a newer category even if entity handle matches");
    fixture::route={};
    field::announce_current_entity();field::category_next();fixture::finish();
    expect(!fixture::route,"new category retires directions still waiting for nearest-exit readiness");
    fixture::output.clear(); fixture::route = {};
    field::announce_current_entity(); fixture::transition = true; fixture::finish(); fixture::transition = false;
    expect(!fixture::route && fixture::output.empty(), "directions waiting for entities cannot start a route during a fade");
    field::announce_current_entity(); fixture::finish();
    fixture::transition = true; fixture::route(path); fixture::transition = false;
    expect(fixture::output.empty(), "a completed route cannot speak during a transition");
    path.warning=L"Warning: a trap is required to reach this chest.";
    field::announce_current_entity(); fixture::finish(); fixture::route(path);
    expect(fixture::output.size()==1 && fixture::output.back().parts==std::vector<std::wstring>{path.warning,path.description},
        "enabled warnings precede directions in one correctly ordered speech event");
    fixture::output.clear(); fixture::warnings=false;
    field::announce_current_entity(); fixture::finish(); fixture::route(path);
    expect(fixture::output.size()==1 && fixture::output.back().parts==std::vector<std::wstring>{path.description},
        "disabling mod warnings preserves route directions");
    fixture::output.clear();fixture::route={};
    field::navigate_to_entity();fixture::finish();
    expect(bool(fixture::route),"soundscape leaves turn-by-turn route producer available");
    fixture::route(path);expect(!fixture::output.empty(),"soundscape leaves actual directions audible");
    fixture::beacons=true;fixture::route={};fixture::output.clear();
    field::navigate_to_entity();fixture::finish();
    expect(fixture::beacon_requests==1&&!fixture::route,"soundscape leaves beacon-only navigation available");
    fixture::combined=true;field::navigate_to_entity();fixture::finish();
    expect(fixture::beacon_requests==2&&bool(fixture::route),"soundscape permits simultaneous beacon and directions");
    fixture::route(path);expect(!fixture::output.empty()&&fixture::soundscape,"combined navigation never changes soundscape preference");
    std::printf("Navigation speech: ordered category/focus, stale-event rejection, FIFO browse, async route and concurrent soundscape/beacon/directions: %d failures\n", failures);
    return failures ? 1 : 0;
}
