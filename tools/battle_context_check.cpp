// Real battle-state transition and native ChangeState detour, without a game runtime.
#include <cstdio>
#include <cstdlib>
#include "../src/core/battle/battle_state.cpp"
#include "../src/core/system/game_state.cpp"
namespace fixture { int calls = 0, scans = 0, starts = 0, ends = 0, resets = 0, state = 3; bool prefix_ready = false, fade = false, input_blocked = false, dialogue = false; }
int native_state(void*, const void*) { return fixture::state; }
namespace core { Install::Install(const char* n, int (*f)(), int o, uint8_t g) : name(n), fn(f), order(o), games(g) {} }
namespace ref {
Class::Class(const char* n, const char* c, uint8_t g, const char* a) : ns(n), name(c), games(g), assembly(a) {}
Method::Method(Class& c, const char* n, int a) : cls(c), name(n), argc(a) {}
bool Class::is(void* object) { return object != nullptr; }
il2cpp::Class* Class::get() { return nullptr; }
il2cpp::Method* Method::get() { return nullptr; }
void* Method::ptr() { if (std::string(name) == "GetCurrentState") return reinterpret_cast<void*>(&native_state); ++fixture::scans; return nullptr; }
void call_failed_note(const char*) { std::abort(); }
}
namespace hooks { bool attach_raw(ref::Method&, void*, void**) { return false; } void note_fault(const char*) { std::abort(); } }
namespace log_ { void write(const char*, ...) {} }
namespace unity { void* find_object(ref::Class&) { ++fixture::scans; return nullptr; } }
namespace fixture { std::wstring scene = L"Title"; }
namespace field { const std::wstring& scene_name() { return fixture::scene; } }
namespace actions { bool defined(const char*) { return false; } bool run(const char*) { return false; } void define(const char*, Fn) {} }
namespace queries { bool defined(const char*) { return false; } bool is(const char*) { return fixture::dialogue; } void define(const char*, std::function<bool()>) {} }
namespace ffpr::fieldnav::map_transition { bool is_screen_fading() { return fixture::fade; } }
namespace events {
void on(Kind, Handler) {}
void fire(Kind kind) { if (kind == Kind::BattleStart) ++fixture::starts; if (kind == Kind::BattleEnd) ++fixture::ends; }
}
namespace menu_state {
void reset_all() {} void reset(const char*) {} void set_active(const char*, bool) {} bool is_active(const char*) { return false; }
void register_reset(const char*, std::function<void()>) {} std::vector<std::string> active_states() { return {}; }
}
namespace ffpr::battle { void pause_reset() {} void clear_cached_target_controller() {} void result_reset_state() { ++fixture::resets; } void removal_reset_state() {} }
namespace game { Id current() { return Id::FF2; } }
void native_transition(void*, int state, const void*) {
    ++fixture::calls;
    fixture::state = state;
    fixture::input_blocked = ffpr::system::input_transition_blocked();
    fixture::prefix_ready = state == 13 ? ffpr::battle::in_battle() : !ffpr::battle::in_battle();
}
int main() {
    int failures = 0;
    auto expect = [&](bool ok, const char* message) { if (!ok) { ++failures; std::printf("FAIL %s\n", message); } };
    ffpr::system::o_ChangeState = &native_transition;
    void* manager = &failures;
    ffpr::system::d_ChangeState(manager, 13, nullptr);
    expect(fixture::prefix_ready && fixture::calls == 1 && fixture::starts == 1, "battle flag gates features before native transition continues");
    expect(fixture::input_blocked, "mod input is blocked inside native battle entry even before fading begins");
    for (int i = 0; i < 100; ++i) ffpr::battle::main_game_transition(13);
    expect(fixture::starts == 1 && fixture::resets == 1, "steady battle detection performs no repeated cleanup");
    ffpr::battle::try_clear_on_battle_end();
    expect(ffpr::battle::in_battle() && fixture::ends == 0, "fade/results callbacks cannot clear the native battle context early");
    ffpr::system::d_ChangeState(manager, 1, nullptr);
    expect(fixture::prefix_ready && fixture::calls == 2 && fixture::ends == 1, "native return releases context exactly once");
    expect(fixture::input_blocked, "native map/return loading blocks mod input before its original body returns");
    for (int state : {0, 1, 2, 12, 14}) {
        fixture::state = state;
        expect(ffpr::system::input_transition_blocked(), "native loading/encounter/title transition state blocks hotkeys");
    }
    for (int state : {3, 4, 5, 8, 9, 10, 13}) {
        fixture::state = state;
        expect(!ffpr::system::input_transition_blocked(), "stable field/vehicle/menu/dialogue/battle keeps mod controls");
        fixture::fade = true;
        expect(ffpr::system::input_transition_blocked(), "fade blocks controls even if native state still reports the old ready screen");
        fixture::fade = false;
    }
    fixture::state = 12; fixture::dialogue = true;
    expect(!ffpr::system::input_transition_blocked(), "visible scripted dialogue remains manually reviewable");
    ffpr::system::reset_main_game_state(); fixture::scene=L"Boot";
    expect(ffpr::system::input_transition_blocked(),"boot loading blocks input before the main-game manager exists");
    fixture::scene=L"Title";
    expect(!ffpr::system::input_transition_blocked(),"stable title keeps the direct settings exception");
    expect(fixture::scans == 0, "automatic transition detection has no scene scan, native query or shutdown polling");
    std::printf("%s: native context gates, one cleanup per transition, no game queries/scans; %d failures\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
