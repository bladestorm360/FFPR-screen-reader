// check-sources: text.cpp
// Actual modal producers, front end, worker queue and Prism adapter; fake SDK only.
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>
#include <cstdarg>
#include "../src/core/say.cpp"
#include "../src/core/speech.cpp"
#include "../src/core/ui/text_input.cpp"
#include "../src/core/ui/confirmation_dialog.cpp"
#include "../src/core/ui/initial_speech.h"
namespace fixture {
struct Output { std::string text; bool interrupt; double at; };
std::vector<Output> output;
struct Trace { std::string feature; double request, queue, age, sdk; int result; };
std::vector<Trace> timing;
uint64_t tick = 0;
int pressed = 0, delayed = 0;
bool game_speech = true;
HANDLE entered = nullptr, release = nullptr;
std::atomic<bool> block{false};
double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
PrismConfig config() { return {}; }
PrismContext* init(PrismConfig*) { return reinterpret_cast<PrismContext*>(1); }
void shutdown(PrismContext*) {}
PrismBackend* acquire(PrismContext*) { return reinterpret_cast<PrismBackend*>(2); }
PrismError initialize(PrismBackend*) { return PRISM_OK; }
const char* name(PrismBackend*) { return "Fixture"; }
uint64_t features(PrismBackend*) { return PRISM_BACKEND_SUPPORTS_SPEAK | PRISM_BACKEND_SUPPORTS_OUTPUT; }
PrismError speak(PrismBackend*, const char* text, bool interrupt) {
    if (block.exchange(false)) { SetEvent(entered); WaitForSingleObject(release, 2000); }
    output.push_back({text, interrupt, now()}); return PRISM_OK;
}
PrismError braille(PrismBackend*, const char*) { return PRISM_OK; }
PrismError stop(PrismBackend*) { return PRISM_OK; }
void free(PrismBackend*) {}
const char* error(PrismError) { return "fixture"; }
const char* version() { return "0.18.3"; }
speech::prism::Api api() {
    speech::prism::Api result;
    result.config_init=config; result.init=init; result.shutdown=shutdown; result.registry_acquire_best=acquire;
    result.backend_initialize=initialize; result.backend_name=name; result.backend_get_features=features;
    result.backend_output=speak; result.backend_speak=speak; result.backend_braille=braille; result.backend_stop=stop;
    result.backend_free=free; result.error_string=error; result.version_string=version; return result;
}
void consume() {
    g_backend.load(api());
    speech::drain_backend();
    g_backend.close();
}
void drain() { std::thread worker(&consume); worker.join(); }
}
namespace core { Install::Install(const char* n, int (*f)(), int o, uint8_t g) : name(n), fn(f), order(o), games(g) {} }
namespace actions { void define(const char*, Fn) {} }
namespace queries { void define(const char*, std::function<bool()>) {} }
namespace log_ {
void write(const char* format, ...) {
    if (std::strncmp(format,"[SpeechTiming]",14)!=0) return;
    va_list args; va_start(args,format);
    fixture::Trace trace; trace.feature=va_arg(args,const char*);
    (void)va_arg(args,unsigned long long);
    trace.request=va_arg(args,double); trace.queue=va_arg(args,double);
    trace.age=va_arg(args,double); trace.sdk=va_arg(args,double); trace.result=va_arg(args,int);
    fixture::timing.push_back(std::move(trace)); va_end(args);
}
void writeW(const wchar_t*, const wchar_t*) {}
}
namespace settings {
bool get_bool(const char* key, bool def) { return std::strcmp(key, "SpeechEnabled") == 0 ? fixture::game_speech : def; }
void save_pending() {}
}
namespace frame { uint64_t count() { return fixture::tick; } void after_seconds(double, Task, const char*) { ++fixture::delayed; } }
namespace ui::modal_keys {
void seed() {} void sample() {} bool pressed(int key) { return key == fixture::pressed; } bool held(int) { return false; }
}
namespace modtext {
std::wstring T(std::wstring_view value) { return std::wstring(value); }
std::wstring TF(std::wstring_view value, std::initializer_list<std::wstring> args) {
    auto result=std::wstring(value); int i=0;
    for (const auto& arg:args) { const auto token=L"{"+std::to_wstring(i++)+L"}"; const auto at=result.find(token); if(at!=result.npos) result.replace(at,token.size(),arg); }
    return result;
}
}
int main() {
    int failures=0;
    auto expect=[&](bool ok,const char* reason) { if(!ok) { ++failures; std::printf("FAIL %s\n",reason); } };
    speech::init();
    double start=fixture::now();
    ui::confirmation::open(L"First question?", [] {
        ui::confirmation::open(L"Second question?", [] {});
    });
    fixture::drain();
    const double cold_ms=(fixture::output[0].at-start)*1000;
    expect(fixture::output.size()==2 && fixture::output[0].text=="First question?" && fixture::output[0].interrupt &&
        fixture::output[1].text=="Yes or No" && !fixture::output[1].interrupt,
        "ready prompt and initial answer choices traverse actual worker/adapter in order");
    fixture::output.clear(); fixture::pressed='Y'; ++fixture::tick; ui::confirmation::handle_input(); fixture::drain();
    expect(fixture::output.size()==3 && fixture::output[0].text=="Yes" && fixture::output[0].interrupt &&
        fixture::output[1].text=="Second question?" && !fixture::output[1].interrupt && !fixture::output[2].interrupt && ui::confirmation::is_open(),
        "chained question cannot interrupt the selected answer");
    fixture::pressed=0; ui::confirmation::close(); fixture::output.clear();
    ui::text_input::open(L"Enter waypoint name", L"Chest", [](const std::wstring&) { ffpr::say(L"Waypoint renamed"); });
    fixture::drain();
    expect(fixture::output.size()==2 && fixture::output[0].text=="Enter waypoint name" && fixture::output[1].text=="Chest" &&
        fixture::output[0].interrupt && !fixture::output[1].interrupt,
        "text-entry prompt precedes the initial text with no settling timer");
    fixture::output.clear(); fixture::pressed=VK_RETURN; ++fixture::tick; ui::text_input::handle_input(); fixture::drain();
    expect(fixture::output.size()==2 && fixture::output[0].text=="Confirmed: Chest" && fixture::output[0].interrupt &&
        fixture::output[1].text=="Waypoint renamed" && !fixture::output[1].interrupt && !ui::text_input::is_open(),
        "completion callback output queues behind confirmation and closes input immediately");
    expect(fixture::delayed==0, "ready modal speech and completion contain no deferred stale announcements");
    expect(ffpr::last_spoken()==L"Waypoint renamed", "repeat state retains callback output");
    fixture::output.clear();
    ui::InitialSpeech entry;
    entry.begin();
    expect(entry.capture(L"Firion") && entry.capture(L"Firion"), "nested native entry keeps every ready focus event without text deduplication");
    entry.ready(L"Select name for character"); fixture::drain();
    expect(fixture::output.size()==3 && fixture::output[0].text=="Select name for character" && fixture::output[0].interrupt &&
        fixture::output[1].text=="Firion" && !fixture::output[1].interrupt && fixture::output[2].text=="Firion" && !fixture::output[2].interrupt,
        "native focus observed during initialization queues after its parent's ready heading");
    expect(!entry.capture(L"Maria"), "later navigation is not captured by an ended native-entry event");
    fixture::pressed=0; fixture::output.clear();
    double worst_warm=0;
    for(int i=0;i<50;++i) {
        start=fixture::now();
        ffpr::say_sequence({L"Category: NPCs",L"Soldier1"},true,{speech::Feature::EntityCategory,start,start}); fixture::drain();
        worst_warm=std::max(worst_warm,(fixture::output[fixture::output.size()-2].at-start)*1000);
    }
    expect(cold_ms<100 && worst_warm<100, "cold/warm request through enqueue, worker and fake SDK stays millisecond-scale");
    expect(fixture::timing.size()==50 && fixture::timing.back().feature=="category" && fixture::timing.back().request>=0 &&
        fixture::timing.back().queue>=0 && fixture::timing.back().result==PRISM_OK,
        "production dispatcher records first SDK submission/queue/return per ordered event");
    fixture::output.clear();
    fixture::timing.clear();
    fixture::entered=CreateEventW(nullptr,TRUE,FALSE,nullptr); fixture::release=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    start=fixture::now();
    ffpr::say_sequence({L"Old category",L"Old destination"},true,{speech::Feature::EntityCategory,start,start}); fixture::block=true;
    std::thread blocked(&fixture::consume);
    expect(WaitForSingleObject(fixture::entered,2000)==WAIT_OBJECT_0,"fake SDK really holds the speech owner");
    const double requested=fixture::now();
    for(int i=0;i<2000;++i) ffpr::say_sequence({L"New category",L"New destination"},true,
        {speech::Feature::EntityCategory,requested,requested-0.010});
    const double publish_ms=(fixture::now()-requested)*1000;
    expect(publish_ms<100,"rapid owner requests do not wait for the blocked backend or full queue");
    SetEvent(fixture::release); blocked.join();
    expect(fixture::output.size()==3 && fixture::output[1].text=="New category" && fixture::output[1].interrupt &&
        fixture::output[2].text=="New destination" && !fixture::output[2].interrupt,
        "busy SDK/rapid category changes discard obsolete tails and retain latest headline/focus");
    const double busy_ms=(fixture::output[1].at-requested)*1000;
    expect(fixture::timing.size()==2 && fixture::timing.back().age>=10 && fixture::timing.back().queue>=0,
        "pressure recovery retains owned snapshot age and excludes obsolete/tail first-output reports");
    fixture::output.clear();
    fixture::game_speech=false;
    const std::wstring last_before_mute=ffpr::last_spoken();
    ffpr::say(L"Muted game line",true);
    ffpr::say_parts({L"Muted game parts"},true);
    ffpr::say_sequence({L"Muted game sequence"},true);
    expect(ffpr::last_spoken()==last_before_mute && fixture::output.empty(),
        "muted say, parts and sequence leave repeat state untouched and publish nothing");
    fixture::drain();
    expect(fixture::output.empty(), "muted gameplay messages do not reach the backend");

    ffpr::say_menu(L"Game speech: Off",true); fixture::drain();
    ffpr::say_menu_sequence({L"Mod menu",L"Game speech: Off"},true); fixture::drain();
    ffpr::say_menu(L"Mod menu closed",true); fixture::drain();
    expect(fixture::output.size()==4 && fixture::output[0].text=="Game speech: Off" &&
        fixture::output[1].text=="Mod menu" && fixture::output[1].interrupt &&
        fixture::output[2].text=="Game speech: Off" && !fixture::output[2].interrupt &&
        fixture::output[3].text=="Mod menu closed" && ffpr::last_spoken()==L"Mod menu closed",
        "explicit menu row, ordered sequence and close remain spoken with gameplay speech off");
    ffpr::say(L"Muted after menu",true);
    ffpr::say_sequence({L"Muted sequence after menu"},true);
    expect(ffpr::last_spoken()==L"Mod menu closed", "menu exemption does not leak to later ordinary speech");
    fixture::drain();
    expect(fixture::output.size()==4, "ordinary speech immediately after menu remains muted");

    fixture::output.clear();
    fixture::game_speech=true;
    ffpr::say_sequence({L"Queued gameplay heading",L"Obsolete gameplay tail"},true);
    fixture::game_speech=false;
    ffpr::say_menu(L"Game speech: Off",true);
    fixture::drain();
    expect(fixture::output.size()==1 && fixture::output[0].text=="Game speech: Off" && fixture::output[0].interrupt,
        "turning the master off with menu confirmation cancels queued gameplay speech");

    fixture::game_speech=true;
    fixture::output.clear();
    ffpr::say(L"Repeated line",true);
    ffpr::say(L"Repeated line",false);
    fixture::drain();
    expect(fixture::output.size()==2 && fixture::output[0].text=="Repeated line" &&
        fixture::output[1].text=="Repeated line" && fixture::output[0].interrupt && !fixture::output[1].interrupt,
        "re-enabling restores gameplay speech and identical queued lines remain distinct");
    std::printf("Speech request to fake SDK: cold %.3f ms, worst warm %.3f ms; 2,000 blocked-SDK publications %.3f ms, latest output %.3f ms; %d failures\n",
        cold_ms,worst_warm,publish_ms,busy_ms,failures);
    CloseHandle(fixture::entered); CloseHandle(fixture::release);
    return failures?1:0;
}
