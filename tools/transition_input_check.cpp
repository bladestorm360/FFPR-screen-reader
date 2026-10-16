// check-sources: input/bindings.cpp text.cpp
// Actual keyboard binding dispatch and controller router; native/UI boundaries only are fake.
#include <cstdio>
#include <cstring>
#define install keyboard_install
#define s_install keyboard_registration
#include "../src/core/input/input_manager.cpp"
#undef install
#undef s_install
#include "../src/core/input/controller_router.cpp"
namespace fixture {
bool blocked = true, buttons = false, teleport = false,battle=false;int one_button=-1;
bool soundscape=true;int single_key=-1;
struct Controller { void* player=reinterpret_cast<void*>(1); } controller;
int keymap = 0;int modifier=0;
std::string last_action;
float trigger = 1;
int actions = 0, speech = 0, native_reads = 0, modal_calls = 0;
}
namespace core { Install::Install(const char* n, int (*f)(), int o, uint8_t g) : name(n), fn(f), order(o), games(g) {} }
namespace ref {
Class::Class(const char* n, const char* c, uint8_t g, const char* a) : ns(n), name(c), games(g), assembly(a) {}
Field::Field(Class& c, const char* n, const char* a) : cls(c), name(n), alt(a) {}
Method::Method(Class& c, const char* n, int a) : cls(c), name(n), argc(a) {}
il2cpp::Class* Class::get() { ++fixture::native_reads; return nullptr; }
int32_t Field::get() { ++fixture::native_reads; return std::strcmp(name,"fieldPlayer")==0?0:-1; }
il2cpp::Method* Method::get() { ++fixture::native_reads; return nullptr; }
void* Method::ptr() { ++fixture::native_reads; return nullptr; }
void call_failed_note(const char*) {}
}
namespace unity {
bool alive(void*) { ++fixture::native_reads; return false; }
void* get_component(void*, ref::Class&) { ++fixture::native_reads; return nullptr; }
}
namespace field::cache { void* get(ref::Class&) { ++fixture::native_reads; return &fixture::controller; } }
namespace log_ { void write(const char*, ...) {} }
namespace actions {
bool defined(const char*) { return false; }
bool run(const char* action) { fixture::last_action = action; ++fixture::actions; return true; }
void define(const char*, Fn) {}
}
namespace queries {
bool is(const char* name) {
    if (std::strcmp(name, "GameStatePatches.IsInputTransitionBlocked") == 0) return fixture::blocked;
    if (std::strcmp(name, "GameStatePatches.IsFieldNavigationReady") == 0) return !fixture::blocked;
    if(std::strcmp(name,"BattleStateHelper.IsInBattle")==0)return fixture::battle;
    return std::strcmp(name, "NavigationTargetTracker.IsEntity") == 0;
}
bool defined(const char*) { return false; }
void define(const char*, std::function<bool()>) {}
void define_text(const char*, std::function<std::wstring()>) {}
}
namespace menu_state { bool any_active() { return false; } }
namespace frame { void subscribe(void (*)(), const char*) {} }
namespace modtext {
std::wstring T(std::wstring_view value) { return std::wstring(value); }
std::wstring TF(std::wstring_view value, std::initializer_list<std::wstring>) { return std::wstring(value); }
}
namespace ffpr { void say(std::wstring_view, bool) { ++fixture::speech; } void say_menu(std::wstring_view,bool){++fixture::speech;} void silence() { ++fixture::speech; } }
namespace ffpr::fieldnav::autowalk { void input_updated() {} }
namespace ui {
namespace confirmation {bool is_open(){return false;}}
namespace text_input {bool is_open(){return false;}}
bool any_modal_open() { return false; }
bool handle_modal_input() { ++fixture::modal_calls; return false; }
namespace mod_menu {
bool is_open() { return false; }
void open() { ++fixture::actions; } void close() { ++fixture::actions; }
void navigate_previous() { ++fixture::actions; } void navigate_next() { ++fixture::actions; }
void adjust_current_item(int) { ++fixture::actions; } void toggle_current_item() { ++fixture::actions; }
void back() { ++fixture::actions; }
}
}
namespace settings {
int get_int(const char* key, int fallback) { return std::string(key) == ui::prefs::kNavigationKeymap ? fixture::keymap : fallback; }
bool get_bool(const char* key, bool fallback) {
    if(std::string(key)=="Soundscape")return fixture::soundscape;
    return std::string(key) == ui::prefs::kTeleportShortcuts ? fixture::teleport : fallback;
}
void set_int(const char*, int) { ++fixture::actions; }
}
namespace input {
bool key_held(Key){return false;} bool key_in_use(Key k){return fixture::single_key==int(k);}
const char* key_name(Key){return "Key";}
float right_stick_x(){return 0;}float right_stick_y(){return 0;}
bool key_pressed(Key k) { return fixture::single_key<0 || fixture::single_key==int(k); } bool any_key_pressed() { return true; }
bool shift_held() { return (fixture::modifier&1)!=0; } bool ctrl_held() { return (fixture::modifier&2)!=0; } bool alt_held() { return (fixture::modifier&4)!=0; }
bool game_window_focused() { return true; } bool gamepad_available() { return true; }
bool sdl_ready() { return true; }
bool button_pressed(int i) { return fixture::buttons || fixture::one_button==i; } bool button_held(int i) { return fixture::buttons || fixture::one_button==i; }
float left_trigger() { return fixture::trigger; } float right_trigger() { return 0; }
float left_stick_x() { return 0; } float left_stick_y() { return 0; }
std::wstring button_label(int) { return L"Button"; }
void update_native_input_ownership() {}
namespace global_hotkeys { void announce_current_vehicle() { ++fixture::actions; } void handle_item_details_key() { ++fixture::actions; } }
namespace gamepad {
void update() {} void shutdown() {}
bool dpad_up_pressed() { return fixture::buttons; } bool dpad_down_pressed() { return fixture::buttons; }
bool dpad_left_pressed() { return fixture::buttons; } bool dpad_right_pressed() { return fixture::buttons; }
bool rstick_up_pressed() { return fixture::buttons; } bool rstick_down_pressed() { return fixture::buttons; }
bool rstick_left_pressed() { return fixture::buttons; } bool rstick_right_pressed() { return fixture::buttons; }
bool lstick_up_pressed() { return fixture::buttons; } bool lstick_down_pressed() { return fixture::buttons; }
bool lstick_left_pressed() { return fixture::buttons; } bool lstick_right_pressed() { return fixture::buttons; }
}
}
namespace fixture {
bool execute(input::Key key,input::Mod mods,input::Context context) {
    if(blocked)return false;
    const int before=actions,oldkey=single_key,oldmod=modifier;
    single_key=int(key);modifier=int(mods);
    input::dispatch_registered_bindings(context);
    single_key=oldkey;modifier=oldmod;return actions>before;
}
}
int main() {
    int failures = 0;
    auto expect = [&](bool ok, const char* reason) { if (!ok) { ++failures; std::printf("FAIL %s\n", reason); } };
    input::initialize_bindings();input::install();
    for (const auto& entry : input::g_bindings) for (const auto& binding : entry.list)
        expect(!fixture::execute(entry.key, binding.mod, binding.ctx), "every registered keyboard binding rejects a transition");
    fixture::buttons = true; input::g_state = input::RouterState::ModMode; input::g_stick_click_button = input::kRightStick;
    input::bindings::begin_frame();input::router::update(input::Context::Field); input::update();
    expect(fixture::actions == 0 && fixture::speech == 0 && fixture::native_reads == 0 && fixture::modal_calls == 0,
        "loading input performs no mod action, speech, engine/cache lookup or modal callback");
    expect(!input::field_active() && input::g_stick_click_button < 0 && input::g_state == input::RouterState::Normal,
        "loading drops pending controller mod/stick gestures and field readiness");
    fixture::buttons = false; fixture::blocked = false;
    input::bindings::begin_frame();input::router::update(input::Context::Field);
    expect(fixture::actions == 0 && fixture::speech == 0, "LT held through loading cannot request navigation on return");
    fixture::trigger = 0; input::bindings::begin_frame();input::router::update(input::Context::Field);
    fixture::trigger = 1; input::bindings::begin_frame();input::router::update(input::Context::Field);
    expect(fixture::actions == 1, "a fresh LT press requests navigation after native readiness returns");
    expect(fixture::execute(input::Key::P, input::Mod::None, input::Context::Field) && fixture::actions == 2,
        "fresh keyboard directions remain available after loading");
    auto binding = [&](input::Key key, input::Mod mod, const char* action) {
        fixture::last_action.clear();
        const bool ran = fixture::execute(key, mod, input::Context::Field);
        expect(action ? ran && fixture::last_action == action : !ran && fixture::last_action.empty(),
               "only the selected navigation keymap dispatches its exact action");
    };
    using K = input::Key; using M = input::Mod;
    for (int layout = 0; layout < 2; ++layout) {
        fixture::keymap = layout;
        binding(K::P, M::None, layout ? "Mod.AnnounceEntityOnly" : "Mod.NavigateToEntity");
        binding(K::P, M::Shift, layout ? "Mod.ResetToAllCategory" : "Mod.TogglePathfindingFilter");
        binding(K::P, M::Ctrl, layout ? nullptr : "Mod.ToggleToLayerFilter");
        binding(K::K, M::None, layout ? nullptr : "Mod.AnnounceEntityOnly");
        binding(K::K, M::Shift, layout ? nullptr : "Mod.ResetToAllCategory");
        binding(K::J, M::None, layout ? nullptr : "Mod.CyclePrevious");
        binding(K::L, M::Shift, layout ? nullptr : "Mod.CycleNextCategory");
        binding(K::LeftBracket, M::None, layout ? "Mod.CyclePrevious" : nullptr);
        binding(K::RightBracket, M::Shift, layout ? "Mod.CycleNextCategory" : nullptr);
        binding(K::Backslash, M::None, layout ? "Mod.NavigateToEntity" : nullptr);
        binding(K::Backslash, M::Shift, layout ? "Mod.TogglePathfindingFilter" : nullptr);
        binding(K::Backslash, M::Ctrl, layout ? "Mod.ToggleToLayerFilter" : nullptr);
        binding(K::F6, M::None, "Mod.ToggleAudioBeacons");
        binding(K::F6, M::Shift, "Mod.ToggleBeaconMode");
        binding(K::Semicolon,M::Shift,"Soundscape.RangeDown");
        binding(K::Quote,M::Shift,"Soundscape.RangeUp");
        binding(K::Quote,M::Ctrl,"Mod.ToggleSoundscape");
        for (K removed : {K::Alpha9, K::Minus, K::Equals}) binding(removed, M::None, nullptr);
        binding(K::UpArrow, M::Ctrl, nullptr);
        fixture::teleport = true; binding(K::UpArrow, M::Ctrl, "Mod.TeleportNorth"); fixture::teleport = false;
    }
    fixture::keymap=0;fixture::single_key=int(K::Backslash);fixture::last_action.clear();
    const int before_layout=fixture::actions;input::update();
    expect(fixture::actions==before_layout&&fixture::last_action.empty(),"FF2 reported backslash no-op reproduces through full input tick with Letters layout and soundscape On");
    fixture::keymap=1;input::update();
    expect(fixture::actions==before_layout+1&&fixture::last_action=="Mod.NavigateToEntity"&&fixture::soundscape,
        "selecting Brackets dispatches actual full-tick navigation without disabling soundscape");
    namespace b=input::bindings;
    auto index=[](const char*id) {const auto&v=b::entries();for(size_t i=0;i<v.size();++i)if(v[i].id==id)return int(i);return -1;};
    auto pad_frame=[&](int button,input::Context context=input::Context::Field) {
        fixture::one_button=button;b::begin_frame();input::router::update(context);
    };
    fixture::trigger=0;fixture::single_key=-2;fixture::buttons=false;input::router::reset();pad_frame(-1);
    const int mode=index("pad.mode.toggle"),gil=index("pad.mode.field.gil"),hp=index("pad.mode.battle.hp");
    std::wstring conflict;
    expect(mode>=0&&gil>=0&&hp>=0,"mod-mode entry and individual context functions are registered for remapping");
    expect(b::set(mode,15,-1,conflict),"mode entry can move from Back to an unused button");
    pad_frame(input::kBack);expect(input::router_state()==input::RouterState::Normal&&!input::router::is_button_consumed(input::kBack),"old mode button is released to the game after remapping");pad_frame(-1);
    const int before_mode=fixture::actions;pad_frame(15);
    expect(input::router_state()==input::RouterState::ModMode&&input::suppress_game_input()&&input::router::is_button_consumed(15)&&fixture::actions==before_mode,"new mode button enters and owns input without firing an underlying function");
    pad_frame(15);expect(input::router_state()==input::RouterState::ModMode,"holding replacement mode button cannot immediately toggle back");pad_frame(-1);
    expect(b::set(gil,input::kBack,-1,conflict),"freed former mode button can be assigned to a mod-mode function");
    const int before_gil=fixture::actions;
    pad_frame(input::kWest);expect(fixture::actions==before_gil&&input::router_state()==input::RouterState::ModMode,"old mod-mode function button no longer fires");pad_frame(-1);
    pad_frame(input::kBack);expect(fixture::last_action=="Mod.AnnounceGilAmount"&&fixture::actions==before_gil+1&&input::router_state()==input::RouterState::Normal,"remapped mod-mode function runs on the former mode button and returns normally");pad_frame(-1);
    pad_frame(15);pad_frame(-1);pad_frame(15);expect(input::router_state()==input::RouterState::Normal,"replacement mode button also cancels mod mode");pad_frame(-1);
    fixture::battle=true;expect(b::set(hp,16,-1,conflict),"battle mod-mode functions have independent remaps");
    pad_frame(15,input::Context::Battle);pad_frame(-1,input::Context::Battle);const int before_hp=fixture::actions;
    pad_frame(input::kWest,input::Context::Battle);expect(fixture::actions==before_hp,"old battle-mode function binding is inactive");pad_frame(-1,input::Context::Battle);
    pad_frame(16,input::Context::Battle);expect(fixture::last_action=="Mod.AnnounceCharacterStatus"&&fixture::actions==before_hp+1,"replacement battle-mode function dispatches correctly");pad_frame(-1,input::Context::Battle);
    expect(!b::set(mode,input::kBack,-1,conflict)&&conflict==L"Read Gil","restoring a mode button checks functions now assigned to its former input");
    b::reset_all();fixture::battle=false;
    std::printf("Transition input: binding guards, controller retirement/held LT, both layouts and full-tick soundscape/layout regression: %d failures\n", failures);
    return failures ? 1 : 0;
}
