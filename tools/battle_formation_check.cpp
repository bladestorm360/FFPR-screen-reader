// Actual shared native enemy-panel detour; no game runtime, bestiary or synthetic scene scan.
#include "../src/core/battle/battle_formation.cpp"
#define install opening_install
#define s_install opening_registration
#include "../src/core/battle/battle_start.cpp"
#undef install
#undef s_install
#include <cstdio>
#include <map>
#include <cstring>

namespace fixture {
struct Row { bool active = true; std::wstring name, count; };
struct Panel { std::vector<Row*>* rows; };
bool battle = false;
int originals = 0, reads = 0;
std::wstring opening_text;
std::vector<std::wstring> ordered;
std::vector<frame::Task> deferred;
void opening(void*,const void*) { if(!opening_text.empty())ffpr::battle::message_line::game_message(opening_text,false); }
std::vector<std::vector<std::wstring>> speech;
std::map<events::Kind, std::vector<events::Handler>> handlers;
DWORD owner = GetCurrentThreadId();
void native() { if (owner != GetCurrentThreadId()) std::abort(); ++reads; }
void update(void*, void*, void*, const void*) { ++originals; }
void begin() { battle = true; events::fire(events::Kind::BattleEntering); }
void end() { battle = false; events::fire(events::Kind::BattleEnd); }
}
namespace core { Install::Install(const char* n, int (*f)(), int o, uint8_t g) : name(n), fn(f), order(o), games(g) {} }
namespace ref {
Class::Class(const char* n, const char* c, uint8_t g, const char* a) : ns(n), name(c), games(g), assembly(a) {}
Field::Field(Class& c, const char* n, const char* a) : cls(c), name(n), alt(a) {}
Method::Method(Class& c, const char* n, int a) : cls(c), name(n), argc(a) {}
int32_t Field::get() { fixture::native(); return 0; }
bool Class::is(void* p) { fixture::native(); return p != nullptr; }
}
namespace hooks { bool attach_raw(ref::Method&, void*, void**) { return false; } void note_fault(const char*) { std::abort(); } }
namespace events {
void on(Kind kind, Handler handler) { fixture::handlers[kind].push_back(std::move(handler)); }
void fire(Kind kind) { for (const auto& handler : fixture::handlers[kind]) handler(); }
}
namespace ffpr::battle { bool in_battle() { return fixture::battle; } void on_battle_start() { events::fire(events::Kind::BattleStart); } }
namespace frame { void after_frames(int count, Task task, const char*) { if(count!=2)std::abort();fixture::deferred.push_back(std::move(task)); } }
namespace ffpr { void say_parts(std::vector<std::wstring> parts, bool interrupt) { if (interrupt) std::abort(); for(const auto& part:parts)fixture::ordered.push_back(part); fixture::speech.push_back(std::move(parts)); } }
namespace ffpr { void say(std::wstring_view text,bool interrupt) { if(interrupt)std::abort();fixture::ordered.emplace_back(text); } }
namespace modtext { std::wstring T(std::wstring_view value) { return std::wstring(value); } }
namespace displayed {
void* field(void* p, const char* name) {
    fixture::native(); if (!p) return nullptr;
    auto* row = static_cast<fixture::Row*>(p);
    if (std::strcmp(name, "view") == 0) return p;
    if (std::strcmp(name, "nameText") == 0) return &row->name;
    if (std::strcmp(name, "countText") == 0) return &row->count;
    std::abort();
}
}
namespace unity {
bool alive(void* p) { fixture::native(); return p != nullptr; }
bool active_self(void* p) { fixture::native(); return static_cast<fixture::Row*>(p)->active; }
int list_count(void* p) { fixture::native(); return static_cast<int>(static_cast<std::vector<fixture::Row*>*>(p)->size()); }
void* list_at(void* p, int i) { fixture::native(); return (*static_cast<std::vector<fixture::Row*>*>(p))[i]; }
std::wstring ui_text(void* p) { fixture::native(); return p ? *static_cast<std::wstring*>(p) : std::wstring(); }
}
int main()
{
    using namespace ffpr::battle;
    int failures = 0;
    auto expect = [&](bool ok, const char* label) { if (!ok) { ++failures; std::printf("FAIL %s\n", label); } };
    install(); o_update = &fixture::update;
    fixture::Row goblin{true, L"Goblin", L"3"}, hornet{true, L"Hornet", L"2"}, hidden{false, L"Future boss", L"1"};
    std::vector<fixture::Row*> rows{&goblin, &hornet, &hidden}; fixture::Panel panel{&rows};
    const auto capture = [&] { d_update(&panel, nullptr, nullptr, nullptr); };
    capture();
    expect(fixture::reads == 0 && fixture::speech.empty() && fixture::originals == 1, "preloaded battle UI calls only its native original");
    for (int game = 1; game <= 6; ++game) {
        const auto before = fixture::speech.size();
        const int reads = fixture::reads;
        fixture::begin();
        expect(fixture::reads == reads, "battle admission uses flags only, without native reads or cleanup");
        capture();
        goblin.name = L"Changed after snapshot";
        expect(fixture::speech.size() == before, "opening list can arrive during fade but speech waits for native ready event");
        events::fire(events::Kind::BattleStart);
        expect(fixture::speech.size() == before+1 && fixture::speech.back() == std::vector<std::wstring>{L"Enemy formation", L"3 Goblin", L"2 Hornet"},
            "ready formation copies native count/name/order and excludes inactive pooled future enemies");
        expect(formation_first_turn() && !formation_first_turn(), "first turn queues once behind roster; later turns keep interruption");
        const int published_reads = fixture::reads;
        for (int i = 0; i < 100; ++i) { capture(); events::fire(events::Kind::BattleStart); }
        expect(fixture::speech.size() == before+1 && fixture::reads == published_reads,
            "per-battle event guard avoids repeated announcement and further extraction during actions/spawns");
        fixture::end(); goblin.name = L"Goblin";
        fixture::begin(); events::fire(events::Kind::BattleStart);
        expect(fixture::speech.size() == before+1, "next encounter cannot reuse previous formation before its own producer");
        capture();
        expect(fixture::speech.size() == before+2, "late native ready list publishes without a polling timer or Autodetail dependency");
        fixture::end();
    }
    o_StartPreeMptiveMes=&fixture::opening;
    for(int game=1;game<=6;++game)for(const auto* condition:{L"Ambushed!",L"Pre-emptive strike!",L"Back attack!",L""}) {
        fixture::ordered.clear();fixture::begin();capture();fixture::opening_text=condition;
        d_StartPreeMptiveMes(&panel,nullptr);
        std::vector<std::wstring> expected;if(*condition)expected.emplace_back(condition);
        expected.insert(expected.end(),{L"Enemy formation",L"3 Goblin",L"2 Hornet"});
        expect(fixture::ordered==expected&&fixture::deferred.empty(),"native opening message precedes roster without action-pair delay in all six games");
        expect(!message_line::opening_depth,"native producer tag ends before battle ready/commands");
        fixture::end();
    }
    fixture::ordered.clear();
    message_line::action_started(L"Firion",L"Fire");message_line::game_message(L"Fire",false);
    for(auto& task:fixture::deferred)task();fixture::deferred.clear();
    expect(fixture::ordered==std::vector<std::wstring>{L"Firion: Fire"},"ordinary action pairing retains actor plus native text once");
    fixture::ordered.clear();message_line::game_message(L"Blizzard",false);message_line::action_started(L"Maria",L"Blizzard");
    for(auto& task:fixture::deferred)task();fixture::deferred.clear();
    expect(fixture::ordered==std::vector<std::wstring>{L"Maria: Blizzard"},"message-first ordinary action pairing is unchanged");
    fixture::ordered.clear();message_line::game_message(L"Couldn't escape",false);
    expect(fixture::ordered.empty(),"non-opening system text retains existing action-pair event guard");
    for(auto& task:fixture::deferred)task();fixture::deferred.clear();
    expect(fixture::ordered==std::vector<std::wstring>{L"Couldn't escape"},"unpaired ordinary system message still publishes");
    try { message_line::OpeningPresentation opening;throw 1; } catch(...) {}
    expect(!message_line::opening_depth,"native exception unwinding cannot misclassify later battle messages");
    goblin.name = hornet.name = L"???"; goblin.count = L"1";
    fixture::begin(); capture(); events::fire(events::Kind::BattleStart);
    expect(fixture::speech.back() == std::vector<std::wstring>{L"Enemy formation",L"1 ???",L"2 ???"},
        "native same-name groups/disguises remain separate without text deduplication or hidden master names");
    fixture::end();
    const auto before = fixture::speech.size();
    fixture::begin(); hornet.count.clear(); capture(); events::fire(events::Kind::BattleStart);
    expect(fixture::speech.size() == before, "incomplete count never becomes a fabricated one-enemy formation");
    hornet.count = L"2"; capture();
    expect(fixture::speech.size() == before+1, "next native complete update recovers missing row text");
    fixture::end();
    rows.assign(20,&hidden); rows[0]=&goblin;
    fixture::begin(); capture(); events::fire(events::Kind::BattleStart);
    expect(fixture::speech.back() == std::vector<std::wstring>{L"Enemy formation",L"1 ???"},
        "a reserved pool larger than nine still announces its actual active formation");
    fixture::end();
    const auto oversized_before=fixture::speech.size();
    rows.assign(10,&goblin); fixture::begin(); capture(); events::fire(events::Kind::BattleStart);
    expect(fixture::speech.size() == oversized_before, "unexpected oversized formation is rejected without a truncated roster");
    fixture::end(); rows.clear(); fixture::begin(); capture(); events::fire(events::Kind::BattleStart);
    expect(fixture::speech.size() == oversized_before, "empty encounter keeps pending native readiness without inventing enemies");
    fixture::end();
    std::printf("%s: actual opening producer/phase ownership, all-six shared path, counts/order, pools, repeated/late/empty events; %d failures\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
