// check-sources: field/entity_filter.cpp field/npc_names.cpp field/field_refs.cpp field/baked_map.cpp field/baked_router.cpp field/map_state_program.cpp json.cpp text.cpp
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
int wrong_owner = 0, map = 1, next_task = 0, native_reads=0;
bool ready = true, transition = false, route_pending = false;
struct TalkProperty { int script = 65; std::wstring name = L"飛竜", message; } talk;
struct Actor { void* property = &talk; } npc, object;
bool can_action = true;
bool capture_cells = false;
std::vector<void*> candidates;
int action_reads=0;
void native();
bool get_can_action(void*, const void*) { native();++action_reads; return can_action; }

unity::Vec3 pose{};
struct Controller { void* player = reinterpret_cast<void*>(1); } controller;
struct Task { frame::Step step; std::string label; };
std::map<int, Task> tasks;
std::vector<frame::Task> navigation, compute, posted;
std::mutex post_lock;
void native() { ++native_reads; if (GetCurrentThreadId() != owner) ++wrong_owner; }
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
    auto other=std::move(compute);compute.clear();for(auto& job:other)jobs.push_back(std::move(job));
    std::thread worker([&] { for (auto& job : jobs) job(); }); worker.join();
}
}
namespace ref {
Class::Class(const char* n, const char* c, uint8_t g, const char* a) : ns(n), name(c), games(g), assembly(a) {}
Field::Field(Class& c, const char* n, const char* a) : cls(c), name(n), alt(a) {}
Method::Method(Class& c, const char* n, int a) : cls(c), name(n), argc(a) {}
il2cpp::Class* Class::get() { fixture::native(); return nullptr; }
bool Class::is(void* p) { fixture::native();
 if(p==&fixture::npc)return std::strcmp(name,"FieldNonPlayer")==0||std::strcmp(name,"FieldEntity")==0;
 if(p==&fixture::object)return std::strcmp(name,"EventTriggerEntity")==0||std::strcmp(name,"FieldEntity")==0;
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
namespace hooks { void note_fault(const char* label) { std::printf("FAIL native fixture fault: %s\n",label);std::fflush(stdout);std::abort(); } }
namespace log_ { void write(const char*, ...) {} }
namespace game { const Flags& flags() { static Flags value; static bool configured=(kFF2Module.configure(value),true); return value; } const char* key() { return "ff2"; } }
namespace settings {
int get_int(const char*, int fallback) { return fallback; }
bool get_bool(const char*, bool fallback) { return fallback; }
}
namespace ffpr { void say(std::wstring_view, bool) { fixture::native(); } }
namespace ffpr::system {
bool is_field_navigation_ready() { fixture::native(); return fixture::ready; }
bool input_transition_blocked() { fixture::native(); return fixture::transition; }
}
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
std::wstring safe_event_label(const std::wstring&, const std::wstring&) { return L"Conversation"; }
std::wstring resolve_map_name(int) { fixture::native(); return {}; }
std::wstring resolve_item_name(int) { fixture::native(); return {}; }
std::wstring current_map_asset_name() { fixture::native(); return {}; }
std::wstring translate_entity_name(const std::wstring& name) { fixture::native(); return name; }
npc_names::Label npc_display_label(const std::wstring& name) { fixture::native(); return npc_names::parse(name); }
bool is_decimal_digit(wchar_t c) { return c >= L'0' && c <= L'9'; }
bool EntityCursor::next(void*& entity) { fixture::native();entity=nullptr;if(index>=static_cast<int>(fixture::candidates.size()))return false;entity=fixture::candidates[index++];seen.insert(entity);return true; }
bool Entity::is_alive() const { fixture::native(); return game != reinterpret_cast<void*>(2); }
bool Entity::is_opened() const { fixture::native(); return opened; }
Category Entity::category() const { return kind == Kind::NPC ? Category::NPCs : kind == Kind::MapExit ? Category::MapExits : Category::Events; }
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
bool cell_at(void*, unity::Vec3 position, baked_route::Cell& cell) { fixture::native();if(!fixture::capture_cells)return false;cell={0,static_cast<int>(position.x),static_cast<int>(position.y)};return true; }
bool pending_goal(MapGoal&, TargetKind&, std::wstring&) { fixture::native(); return false; }
bool request_pending() { fixture::native(); return fixture::route_pending; }
void request_reachability(bool, std::function<void()> ready) { fixture::native(); ready(); }
std::vector<RemoteObject> connected_objects_snapshot(int, map_state::Resolver&,std::function<bool()>) { return {}; }
}
namespace field::map_state { Resolver::Resolver() : Resolver(Flags{}, []()->const json::Value&{static const json::Value empty;return empty;}()) { fixture::native(); } }
int main() {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    using namespace field;
    int failures = 0;
    auto expect = [&](bool ok, const char* reason) { if (!ok) { ++failures; std::printf("FAIL %s\n", reason); } };
    fixture::transition = true;
    bool transition_callback = false;
    scanner::when_filter_ready([&] { transition_callback = true; }, true);
    expect(!transition_callback && fixture::tasks.empty() && fixture::navigation.empty(),
        "transition input cannot inspect a map or enqueue scanner readiness work");
    fixture::transition = false;
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
    {
        RemoteInput events;
        for(int id:{32,33}) { RemoteRow event;event.object.type=baked::ObjType::Entity;event.object.event_type=L"Interactive Object";
            event.object.raw_name=L"ペンダント";event.object.entity_id=id;event.object.map_id=184;event.object.cell={0,35,18};
            event.floor=L"Deist Cavern B2";events.rows.push_back(event); }
        std::vector<EntityPtr> objects;
        std::thread worker([&]{objects=build_remote(events);});worker.join();
        expect(objects.size()==2&&objects[0]->kind==Kind::Event&&objects[0]->name==L"Conversation"&&objects[0]->floor_name==L"Deist Cavern B2"&&objects[0]->remote_map_id==184,
            "remote events retain safe labels/category/floor and distinct native identities at the same cell");
        events.cancelled=[]{return true;};expect(build_remote(events).empty(),"retired event-label worker returns no partial destinations");
        auto event=events.rows.front().object;event.raw_name=L"飛竜";RemoteBuilder placeholder_event({event});placeholder_event.step();
        expect(placeholder_event.input.rows.empty(),"remote events preserve the game's existing placeholder exclusions");
    }
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
    // Production adapter over the existing published pathfinder registry. Its
    // capacity is prepared on the audio worker before native acquisition.
    auto measure_capture=[&](const char* label) {
        sound::scape::Snapshot snapshot; snapshot.rows.reserve(sound::scape::max_rows);
        size_t next=0;uint64_t revision=0;int calls=0,result=0;double peak=0,total=0;
        const double start=frame::now();
        while(result==0 && calls<100) {
            const double begin=frame::now(); result=field::capture_soundscape(snapshot,next,revision);
            const double cost=frame::now()-begin; peak=std::max(peak,cost);total+=cost;++calls;
        }
        expect(result==1&&snapshot.rows.size()==g_category_views[0].size(),"soundscape captures every published row across bounded native slices");
        expect(calls>1&&calls>=int((next+511)/512),"soundscape enforces the per-slice count bound");
        expect(!fixture::wrong_owner,"soundscape never accesses a native pointer off its owner");
        ++g_audio_revision;
        expect(field::capture_soundscape(snapshot,next,revision)==-1,"changed pathfinder registry invalidates an unfinished snapshot");
        std::printf("Soundscape %s %zu: %d native slices, peak %.3f ms, aggregate %.3f ms, copy %.3f ms plus frame scheduling\n",
            label,next,calls,peak*1000,total*1000,(frame::now()-start)*1000);
    };
    measure_capture("normal");
    // Repeated production soundscape captures must never consume navigation
    // selection/category/readiness or retire pending native-owner input work.
    scanner::set_category(Category::NPCs);scanner::next_entity();
    const auto audio_selected=scanner::current_entity();const int audio_index=scanner::current_index();
    scanner::when_filter_ready([]{},true); // one coalesced post-directions refresh is expected
    const auto audio_tasks=fixture::tasks.size();bool audio_browse=false,audio_directions=false;
    for(int repeat=0;repeat<20;++repeat) {
        sound::scape::Snapshot audio;audio.rows.reserve(sound::scape::max_rows);
        size_t next=0;uint64_t revision=0;
        for(int step=0;step<100 && capture_soundscape(audio,next,revision)==0;++step) {}
        scanner::when_filter_ready([&]{audio_browse=true;});
        scanner::when_filter_ready([&]{audio_directions=true;},true);
    }
    expect(audio_browse&&audio_directions&&g_views_ready&&scanner::current_entity()==audio_selected&&
        scanner::current_index()==audio_index&&scanner::category()==Category::NPCs&&fixture::tasks.size()==audio_tasks,
        "continuous production soundscape capture preserves actual scanner browse/directions readiness, selection and category");
    scanner::cancel_navigation_work(); // synthetic rows below have no native collection
    scanner::set_category(Category::All);
    auto saved_audio_view=g_category_views[0];
    g_category_views[0].resize(sound::scape::max_rows,saved_audio_view.front());
    for(auto& e:g_category_views[0]) e->audio_hint=std::wstring(512,L'x');
    measure_capture("busy");
    g_category_views[0].push_back(saved_audio_view.front());
    sound::scape::Snapshot invalid;size_t invalid_next=0;uint64_t invalid_revision=0;
    expect(field::capture_soundscape(invalid,invalid_next,invalid_revision)==-1&&invalid.rows.empty(),"over-bound registry fails closed before traversal/allocation");
    g_category_views[0]=std::move(saved_audio_view);
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
    const auto selected_started=frame::now();
    scanner::when_filter_ready([&]{spoken=true;},true);
    const auto selected_us=(frame::now()-selected_started)*1e6;
    const auto restarts=std::count_if(fixture::tasks.begin(),fixture::tasks.end(),[](const auto& entry){return entry.second.label=="EntityScanner.RefreshAfterDirections";});
    expect(spoken && g_views_ready && restarts==1,
        "directions to a live selected target after movement do not wait for world-list refiltering");
    apply_filter();spoken=false;fixture::route_pending=true;
    const auto previous_filter=g_filter_work.epoch()->load(),previous_scan=scanner::g_scan_work.epoch()->load();
    scanner::when_filter_ready([&]{spoken=true;},true);
    expect(spoken && g_views_ready && !g_filter_pending &&
        previous_filter!=g_filter_work.epoch()->load() && previous_scan!=scanner::g_scan_work.epoch()->load(),
        "selected route cancels both refresh owners even without movement and keeps its published destination");
    fixture::tick();
    expect(!g_filter_pending&&!scanner::g_scan_running,"no replacement refresh queues ahead of the pending interactive route");
    fixture::route_pending=false;fixture::tick();
    expect(scanner::g_scan_running,"refresh restarts from current native collections after the route completes");
    const auto scan_epoch_before=scanner::g_scan_work.epoch()->load(),filter_epoch_before=g_filter_work.epoch()->load();
    const int reads_before_interrupt=fixture::native_reads;
    const auto tasks_before_interrupt=fixture::tasks.size();
    int callbacks_retired=0;
    scanner::g_scan_work.retain([&]{++callbacks_retired;});
    g_filter_work.retain([&]{++callbacks_retired;});
    scanner::interrupt_navigation_work();
    expect(scanner::g_scan_work.epoch()->load()==scan_epoch_before+1 && g_filter_work.epoch()->load()==filter_epoch_before+1 &&
        !scanner::g_scan_running && !g_filter_pending && fixture::native_reads==reads_before_interrupt &&
        fixture::tasks.size()==tasks_before_interrupt && callbacks_retired==0,
        "battle prefix retires scanner generations without native reads, scanning task queues or destroying owner callbacks");
    scanner::cancel_navigation_work();fixture::pose.x+=16;
    expect(callbacks_retired==2,"ready-phase scanner cleanup releases both interrupted owner registries once");
    spoken=false;
    scanner::when_filter_ready([&]{spoken=true;});
    expect(!spoken&&!g_views_ready&&g_filter_pending,"moving before grouped-exit browse waits for the new nearest-exit generation");
    for(int i=0;!spoken&&i<100;++i){fixture::tick();fixture::work();}
    expect(spoken,"grouped-exit browse resumes after owned worker publication");
    g_views_ready = false;
    spoken=false;scanner::when_filter_ready([&]{spoken=true;},true);
    expect(spoken&&!g_filter_pending,"stale-origin list publication cannot block a still-valid selected destination");
    g_sel.reset(); // cold/unselected context still requires list readiness
    spoken = false;
    scanner::when_filter_ready([&] { spoken = true; }, true);
    fixture::transition = true; fixture::tick(); fixture::transition = false;
    g_views_ready = true; fixture::tick();
    expect(!spoken, "a loading transition retires pending input rather than replaying it on return");
    scanner::invalidate_map();
    spoken = false; scanner::when_filter_ready([&] { spoken = true; },true);
    expect(!spoken, "map invalidation requires a new ready generation");
    ++fixture::map; fixture::tick(); fixture::work(); fixture::tick();
    expect(!spoken && !g_views_ready, "changed map retires pending input and cannot publish stale categories");
    for(int id:{0,32}) {
        scanner::invalidate_map();fixture::map=183;g_last_scanned_map_id=183;g_category=Category::Events;fixture::capture_cells=true;
        auto make_event=[&](int map,int identity){auto e=std::make_shared<Entity>();e->kind=Kind::Event;e->name=L"Conversation";
            e->event_type=L"Interactive Object";e->entity_id=identity;e->remote_map_id=map;e->cell_x=35;e->cell_y=18;e->floor_name=L"B2";return e;};
        auto target=make_event(184,id),other=make_event(184,id+100),wrong_floor=make_event(185,id);
        g_entities={other,wrong_floor,target};g_sel.reset();g_sel_has=true;g_sel_map=184;g_sel_entity_id=id;
        g_sel_cx=35;g_sel_cy=18;g_sel_category=Category::Events;
        apply_filter();for(int tick=0;g_filter_pending&&tick<100;++tick){fixture::tick();fixture::work();}
        expect(scanner::current_entity()==target&&g_sel==target,"cross-floor publication restores the exact event and binds its new owner-side handle, including native ID zero");
        fixture::map=184;g_last_scanned_map_id=184;
        auto native=make_event(0,id);native->game=reinterpret_cast<void*>(1);native->pos={35,18,0};
        other=make_event(0,id+100);other->game=reinterpret_cast<void*>(1);other->pos=native->pos;
        g_entities={other,native};apply_filter();for(int tick=0;g_filter_pending&&tick<100;++tick){fixture::tick();fixture::work();}
        expect(scanner::current_entity()==native&&g_sel==native,"returning to the destination floor rebinds the projected event to the exact live event");
        g_entities={other};apply_filter();for(int tick=0;g_filter_pending&&tick<100;++tick){fixture::tick();fixture::work();}
        expect(scanner::current_entity()==nullptr&&g_index==-1,"missing event never silently selects an unrelated or collocated destination");
        scanner::previous_entity();expect(scanner::current_entity()==other,"explicit browsing safely resumes after a target disappears");
    }
    expect(fixture::wrong_owner==0,"event capture/publication and identity matching preserve native thread ownership");
    {
        scanner::invalidate_map();fixture::candidates={&fixture::npc};fixture::can_action=true;fixture::action_reads=0;
        std::printf("Checking native action revalidation\n");
        auto old=std::make_shared<Entity>();old->game=&fixture::npc;old->kind=Kind::NPC;old->name=L"Wyvern";
        g_entity_map={{&fixture::npc,old}};g_entities={old};scanner::g_converted_revision=scanner::g_state_revision;
        g_last_scanned_map_id=fixture::map;g_remote_map_id=fixture::map;scanner::schedule_scan();
        for(int i=0;scanner::g_scan_running&&i<100;++i){fixture::tick();fixture::work();}
        expect(g_entity_map.size()==1&&g_entity_map[0].second==old&&fixture::action_reads==0,
            "ordinary refresh retains a valid known native action without reconverting it on each key");
        scanner::refresh_state();
        for(int i=0;(scanner::g_scan_running||g_filter_pending)&&i<100;++i){fixture::tick();fixture::work();}
        expect(fixture::action_reads>0&&g_entity_map.size()==1&&g_entity_map[0].second->game==&fixture::npc,
            "returning from a reusable native interaction keeps the objective available");
        fixture::action_reads=0;
        fixture::can_action=false;scanner::refresh_state();
        for(int i=0;(scanner::g_scan_running||g_filter_pending)&&i<100;++i){fixture::tick();fixture::work();}
        expect(fixture::action_reads>0&&g_entity_map.empty()&&g_entities.empty(),
            "script return rechecks known native actions and removes consumed entries instead of retaining the cached conversion");
        expect(g_remote_map_id==fixture::map,"script return also rebuilds the other-floor projection from new story/chest flags");
        fixture::candidates.clear();fixture::can_action=true;
    }
    apply_filter();
    const auto filter_epoch=g_filter_work.epoch()->load(),scan_epoch=scanner::g_scan_work.epoch()->load();
    scanner::cancel_navigation_work();
    expect(g_filter_work.epoch()->load()!=filter_epoch && scanner::g_scan_work.epoch()->load()!=scan_epoch &&
        !g_filter_pending && !scanner::g_scan_running,"transition cancellation retires both scan and filter worker epochs before native loading");
    std::printf("Scanner: 2,000 rows prepared in %d capture/publication ticks; warm category %.3f us; %d failures\n",
        frames, category_us, failures);
    std::printf("Selected destination after movement: %.3f us native-fixture dispatch, zero list-readiness ticks (offline)\n",selected_us);
    // Retire intentionally blocked fixture work before C++ static registries
    // tear down. Production workers/queues live for the entire game process.
    scanner::invalidate_map();
    std::fflush(stdout);
    return failures ? 1 : 0;
}
