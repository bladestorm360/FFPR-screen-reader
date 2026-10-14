// check-sources: field/entity_filter.cpp field/npc_names.cpp field/field_refs.cpp text.cpp
// Actual scanner/capture/publication/input flow, with fake native owners only.
#include <windows.h>
#include <thread>
#include <mutex>
#include <chrono>
#include <cstdio>
#include <cstring>
#include "../src/core/field/entity_scanner.cpp"
#define contains ff2_placeholder_contains
#include "../src/games/ff2/module.cpp"
#undef contains
namespace fixture {
const DWORD owner = GetCurrentThreadId();
int wrong_owner = 0, map = 1, next_task = 0;
bool ready = true;
struct TalkProperty { int script = 65; std::wstring name = L"飛竜", message; } talk;
struct Actor { void* property = &talk; } npc, object;
bool can_action = true;
void native();
bool get_can_action(void*, const void*) { native(); return can_action; }

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
bool Class::is(void* p) { fixture::native();
 if(p==&fixture::npc)return std::strcmp(name,"FieldNonPlayer")==0;
 if(p==&fixture::object)return std::strcmp(name,"EventTriggerEntity")==0;
 if(p==&fixture::talk)return std::strcmp(name,"PropertyEvent")==0||std::strcmp(name,"PropertyTalk")==0;
 return false;
}
int32_t Field::get() { fixture::native(); return std::strcmp(name,"fieldPlayer")==0||std::strcmp(name,"<Property>k__BackingField")==0?0:std::strcmp(name,"<ScriptId>k__BackingField")==0?offsetof(fixture::TalkProperty,script):-1; }
il2cpp::Method* Method::get() { fixture::native(); return std::strcmp(name,"get_CanAction")==0?reinterpret_cast<il2cpp::Method*>(1):nullptr; }
void* Method::ptr() { fixture::native(); return std::strcmp(name,"get_CanAction")==0?reinterpret_cast<void*>(&fixture::get_can_action):nullptr; }
std::wstring str(void*) { fixture::native(); return {}; }
std::wstring rd_str(void* p, Field& f) { fixture::native();if(p!=&fixture::talk)return {};return std::strcmp(f.name,"<MessageKey>k__BackingField")==0?fixture::talk.message:fixture::talk.name; }
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
namespace game { const Flags& flags() { static Flags value; static bool configured=(kFF2Module.configure(value),true); return value; } const char* key() { return "ff2"; } }
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
VehicleState current_vehicle() { fixture::native(); return {}; }
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
std::wstring entity_name_from_property(void* p) { fixture::native(); return p==&fixture::npc?L"Wyvern":L""; }
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
std::shared_ptr<ExitSnapshot> begin_exit_capture(bool,bool) { fixture::native(); return {}; }
void capture_exit_obstacle(ExitSnapshot&,void*) { fixture::native(); }
void finish_exit_capture(ExitSnapshot&) { fixture::native(); }
bool exit_snapshot_world(const ExitSnapshot&) { fixture::native(); return false; }
int exit_snapshot_vehicle(const ExitSnapshot&) { fixture::native(); return 0; }
std::vector<int> exit_distances(ExitSnapshot&,const std::vector<ExitTarget>&,std::function<bool()>) { return {}; }
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
    Context talk; talk.fe=&fixture::npc; talk.go_name=L"Wyvern"; talk.go_lower=L"wyvern";
    for(int script:{65,189}){fixture::talk.script=script;auto result=npc_detector(talk);expect(result.result==Result::Detected&&result.entity->kind==Kind::NPC&&result.entity->name==L"Wyvern","native-actionable Deist Wyvern dialogue scripts are NPC targets despite placeholder name");}
    fixture::talk.script=0;fixture::talk.message=L"VisibleDialogueKey";
    expect(npc_detector(talk).result==Result::Detected,"message-only animal NPC bypasses the placeholder name filter");
    fixture::talk.message.clear();expect(npc_detector(talk).result==Result::Skip,"scriptless/message-less cutscene or vehicle NPC remains excluded");
    fixture::talk.script=65;fixture::can_action=false;expect(npc_detector(talk).result==Result::Skip,"native CanAction still owns interactability");fixture::can_action=true;
    for(const auto* name:{L"太陽の炎トリガー用",L"女神のベルトリガー用"}){fixture::talk.name=name;Context object;object.fe=&fixture::object;object.is_event_trigger=true;object.go_name=name;object.go_lower=text::lower(name);expect(npc_detector(object).result==Result::NotHandled,"keyword objects are not reclassified as NPCs by dialogue");auto event=event_trigger_detector(object);expect(event.result==Result::Detected&&event.entity->kind==Kind::Event,"keyword objects retain the event interaction category");}
    RemoteInput remote;RemoteRow row;row.object.type=baked::ObjType::NPC;row.object.map_id=180;row.object.raw_name=L"飛竜";row.object.scripted=true;row.name=L"Wyvern";row.floor=L"Castle Deist: Wyvern's Room";remote.rows.push_back(row);
    auto built=build_remote(remote);expect(built.size()==1&&built[0]->kind==Kind::NPC&&built[0]->name==L"Wyvern","scripted Wyvern remains an NPC target across floors");
    RemoteBuilder scripted({row.object});scripted.step();expect(scripted.input.rows.size()==1,"remote name capture accepts scripted Wyvern despite placeholder filter");row.object.scripted=false;RemoteBuilder marker({row.object});marker.step();expect(marker.input.rows.empty(),"remote cutscene Wyvern without interaction script remains excluded");
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
    g_navigation_filtered=true;g_view_vehicle=0;
    fixture::pose.x+=1;spoken=false;
    const auto queued = fixture::tasks.size();
    const auto selected_started=frame::now();
    scanner::when_filter_ready([&]{spoken=true;},true);
    const auto selected_us=(frame::now()-selected_started)*1e6;
    expect(spoken && g_views_ready && fixture::tasks.size()==queued,
        "directions to a live selected target after movement do not wait for world-list refiltering");
    apply_filter();fixture::pose.x+=16;spoken=false;
    scanner::when_filter_ready([&]{spoken=true;},true);
    expect(spoken && g_views_ready && !g_filter_pending,"selected route retires an obsolete destination flood without waiting for its worker");
    spoken=false;
    scanner::when_filter_ready([&]{spoken=true;});
    expect(!spoken&&!g_views_ready&&g_filter_pending,"moving before grouped-exit browse waits for the new nearest-exit generation");
    for(int i=0;!spoken&&i<100;++i){fixture::tick();fixture::work();}
    expect(spoken,"grouped-exit browse resumes after owned worker publication");
    scanner::invalidate_map();
    spoken = false; scanner::when_filter_ready([&] { spoken = true; },true);
    expect(!spoken, "map invalidation requires a new ready generation");
    ++fixture::map; fixture::tick(); fixture::work(); fixture::tick();
    expect(!spoken && !g_views_ready, "changed map retires pending input and cannot publish stale categories");
    std::printf("Scanner: 2,000 rows prepared in %d capture/publication ticks; warm category %.3f us; %d failures\n",
        frames, category_us, failures);
    std::printf("Selected destination after movement: %.3f us native-fixture dispatch, zero list-readiness ticks (offline)\n",selected_us);
    // Retire intentionally blocked fixture work before C++ static registries
    // tear down. Production workers/queues live for the entire game process.
    scanner::invalidate_map();
    std::fflush(stdout);
    return failures ? 1 : 0;
}
