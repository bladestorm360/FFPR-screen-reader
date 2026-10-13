// check-sources: text.cpp field/object_cache.cpp
// Actual title producer, speech ingress and Prism adapter; fake native owners/SDK.
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <map>
#include <thread>
#include <chrono>
#include "../src/core/say.cpp"
#include "../src/core/speech.cpp"
#include "../src/core/dialogue/title.cpp"

namespace fixture {
struct Object {
    const char* type;
    bool alive = true, active = true, rendered = true;
    void* command = nullptr;
    void* view = nullptr;
    void* start = nullptr;
    void* cursor = nullptr;
    std::wstring text;
};
Object line{"Text"}, view{"TitleWindowView"}, command{"TitleMenuCommandController"}, title{"TitleWindowController"};
Object other{"OtherWindow"};
game::Flags flags;
std::wstring scene = L"Title";
std::map<std::string, void*> detours;
events::Handler scene_loaded;
bool ready = false, select_in_update = false;
int native_calls = 0, text_reads = 0, focus_reads = 0;
struct Output { std::string text; bool interrupt; };
std::vector<Output> output;
using Void = void (*)(void*, const void*);
void call(const char* method, void* owner = &title) {
    reinterpret_cast<Void>(detours.at(method))(owner, nullptr);
}
void init_none(void*, const void*) { ++native_calls; line.active = false; }
void init_shortcut(void*, const void*) { ++native_calls; line.active = true; }
void init_select(void*, const void*) { ++native_calls; line.active = false; }
void native_update(void*, const void*) {
    ++native_calls;
    if (ready) line.active = true;
    if (select_in_update) call("InitSelect");
}
void native_noop(void*, const void*) { ++native_calls; }
void native_menu(void*, bool enable, const void*) { ++native_calls; if (enable) line.active = false; }
PrismConfig config() { return {}; }
PrismContext* init(PrismConfig*) { return reinterpret_cast<PrismContext*>(1); }
void shutdown(PrismContext*) {}
PrismBackend* acquire(PrismContext*) { return reinterpret_cast<PrismBackend*>(2); }
PrismError initialize(PrismBackend*) { return PRISM_OK; }
const char* name(PrismBackend*) { return "Fixture"; }
uint64_t features(PrismBackend*) { return PRISM_BACKEND_SUPPORTS_SPEAK | PRISM_BACKEND_SUPPORTS_OUTPUT; }
PrismError speak(PrismBackend*, const char* text, bool interrupt) { output.push_back({text, interrupt}); return PRISM_OK; }
PrismError braille(PrismBackend*, const char*) { return PRISM_OK; }
PrismError stop(PrismBackend*) { return PRISM_OK; }
void free(PrismBackend*) {}
const char* error(PrismError) { return "fixture"; }
const char* version() { return "0.18.3"; }
void drain() {
    std::thread worker([] {
        speech::prism::Api api;
        api.config_init=config; api.init=init; api.shutdown=shutdown; api.registry_acquire_best=acquire;
        api.backend_initialize=initialize; api.backend_name=name; api.backend_get_features=features;
        api.backend_output=speak; api.backend_speak=speak; api.backend_braille=braille;
        api.backend_stop=stop; api.backend_free=free;
        api.error_string=error; api.version_string=version;
        g_backend.load(api); speech::drain_backend(); g_backend.close();
    });
    worker.join();
}
}
namespace core { Install::Install(const char* n, int (*f)(), int o, uint8_t g) : name(n), fn(f), order(o), games(g) {} }
namespace ref {
Class::Class(const char* n, const char* c, uint8_t g, const char* a) : ns(n), name(c), games(g), assembly(a) {}
Field::Field(Class& c, const char* n, const char* a) : cls(c), name(n), alt(a) {}
Method::Method(Class& c, const char* n, int a) : cls(c), name(n), argc(a) {}
il2cpp::Class* Class::get() {
    static std::map<std::string, int> keys;
    return reinterpret_cast<il2cpp::Class*>(&keys[name]);
}
bool Class::is(void* p) { return p && std::strcmp(static_cast<fixture::Object*>(p)->type, name) == 0; }
int32_t Field::get() {
    if (std::strcmp(name,"commandController")==0) return offsetof(fixture::Object,command);
    if (std::strcmp(name,"view")==0) return offsetof(fixture::Object,view);
    if (std::strcmp(name,"startText")==0) return offsetof(fixture::Object,start);
    if (std::strcmp(name,"selectCursor")==0) return offsetof(fixture::Object,cursor);
    return -1;
}
}
namespace unity {
bool alive(void* p) { return p && static_cast<fixture::Object*>(p)->alive; }
void* game_object(void* p) { return alive(p) ? p : nullptr; }
bool active_in_hierarchy(void* p) { return alive(p) && static_cast<fixture::Object*>(p)->active; }
int list_count(void*) { return 0; }
}
namespace displayed {
std::wstring text(void* p) {
    ++fixture::text_reads;
    return unity::active_in_hierarchy(p) && static_cast<fixture::Object*>(p)->rendered
        ? static_cast<fixture::Object*>(p)->text : std::wstring();
}
}
namespace hooks {
bool attach_raw(ref::Method& m, void* detour, void** original) {
    fixture::detours[m.name] = detour;
    if (std::strcmp(m.name,"InitNone")==0) *original=reinterpret_cast<void*>(&fixture::init_none);
    else if (std::strcmp(m.name,"InitShortcutCommand")==0) *original=reinterpret_cast<void*>(&fixture::init_shortcut);
    else if (std::strcmp(m.name,"InitSelect")==0) *original=reinterpret_cast<void*>(&fixture::init_select);
    else if (std::strncmp(m.name,"Update",6)==0) *original=reinterpret_cast<void*>(&fixture::native_update);
    else if (std::strcmp(m.name,"SetEnableMainMenu")==0) *original=reinterpret_cast<void*>(&fixture::native_menu);
    else *original=reinterpret_cast<void*>(&fixture::native_noop);
    return true;
}
void note_fault(const char*) { std::abort(); }
}
namespace events { void on(Kind, Handler h) { fixture::scene_loaded=std::move(h); } }
namespace actions { void define(const char*, Fn) {} bool run(const char*) { return true; } }
namespace menu_state { void reset_all() {} }
namespace field { const std::wstring& scene_name() { return fixture::scene; } }
namespace ffpr::system { void wait_and_read_cursor(void*) { ++fixture::focus_reads; ffpr::say(L"Load Game",true); } }
namespace game { const Flags& flags() { return fixture::flags; } }
namespace modtext { std::wstring T(std::wstring_view v) { return std::wstring(v); } }
namespace log_ { void write(const char*, ...) {} void writeW(const wchar_t*, const wchar_t*) {} }
namespace settings { void save_pending() {} }

int main() {
    using namespace fixture;
    title.command=&command; title.view=&view; view.start=&line; command.cursor=&command;
    line.text=L"Press any button";
    int failures=0;
    auto expect=[&](bool ok,const char* why) { if(!ok) { ++failures; std::printf("FAIL %s\n",why); } };
    speech::init(); ffpr::dialogue::install();
    double worst_capture_us=0;
    for(int game=1;game<=6;++game) {
        flags.title_idle_cinematic=game>=5;
        output.clear(); ready=false; line.rendered=true;
        call("InitNone"); call("UpdateNone"); drain();
        expect(output.empty(),"native preload/hidden prompt emits nothing");
        ready=true;
        auto start=std::chrono::steady_clock::now();
        call("UpdateNone");
        worst_capture_us=std::max(worst_capture_us,std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());
        expect(ffpr::last_spoken().find(L"Press any button")==0,"ready native activation publishes before its callback returns");
        drain();
        expect(output.size()==1 && output[0].interrupt && output[0].text.find("Press any button")==0,
            "actual speech ingress/adapter submit the prompt as one interrupting event");
        expect(!output.empty() && (output[0].text.find("intro cinematic")!=std::string::npos)==(game>=5),"only FF5/FF6 append their existing intro instruction");
        int reads=text_reads;
        for(int i=0;i<100;++i) call("UpdateNone");
        drain(); expect(text_reads==reads && output.size()==1,"completed phase does no further text reads or speech");
        output.clear(); call("InitNone"); call("UpdateNone"); drain();
        expect(output.size()==1,"a new native title visit repeats identical prompt wording");
    }
    flags.title_idle_cinematic=false; output.clear(); ready=false;
    call("InitNone"); ready=true; line.rendered=false;
    for(int i=0;i<120;++i) call("UpdateNone");
    drain(); expect(output.empty(),"renderer-hidden prompt stays absent beyond the previous 60-frame deadline");
    line.rendered=true; call("UpdateNone"); drain();
    expect(output.size()==1,"native ready update recovers after delayed rendering");
    output.clear(); ready=false; call("InitNone"); ready=true; select_in_update=true;
    call("UpdateNone"); select_in_update=false; drain();
    expect(output.size()==1 && output[0].text=="Load Game","same-frame input/menu entry cancels a transient prompt and preserves initial focus");
    output.clear(); call("InitNone"); call("UpdateNone"); call("InitSelect"); drain();
    expect(output.size()==1 && output[0].text=="Load Game","busy speech worker discards obsolete prompt after native menu focus interrupts");
    output.clear(); line.rendered=false; call("InitShortcutCommand"); drain();
    expect(output.empty(),"shortcut respects rendered visibility");
    line.rendered=true; call("UpdateShortcutCommand"); drain();
    expect(output.size()==1,"shortcut update owns first ready prompt");
    output.clear(); field::cache::clear_all(); scene=L"TitleScreen"; scene_loaded(); call("UpdateNone"); drain();
    expect(output.size()==1,"native title update recovers scene entry without a cached controller");
    output.clear(); call("InitNone"); scene=L"CommonUI"; scene_loaded(); call("UpdateNone"); drain();
    expect(output.size()==1,"additive preload scene cannot cancel the title's native phase");
    output.clear(); call("InitNone"); call("InitSelect"); drain(); output.clear();
    scene=L"WorldMap"; scene_loaded(); call("UpdateNone"); drain();
    expect(output.empty(),"native menu entry retires prompt through later scene loads");
    scene=L"Title"; call("InitNone"); int reads=text_reads; call("UpdateNone",&other); drain();
    expect(text_reads==reads && output.empty(),"unrelated owner cannot supply a title prompt");
    title.alive=false; call("UpdateNone"); drain(); expect(output.empty(),"destroyed owner cannot speak");
    std::printf("Title prompt: all-six native event/visibility/phase cancellation and actual worker/adapter; worst fake-native capture %.3f us; %d failures\n",worst_capture_us,failures);
    speech::shutdown();
    return failures?1:0;
}
