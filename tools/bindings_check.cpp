// check-sources: text.cpp
// Production binding catalog/dispatch/capture/persistence with physical boundaries only fake.
#include <map>
#include <cstdio>
#include <chrono>
#include "../src/core/input/bindings.cpp"
namespace fixture {
bool focus=true,pad_available=true,keys[int(input::Key::Count)]{},edges[int(input::Key::Count)]{},buttons[input::kButtonCount]{},button_edges[input::kButtonCount]{};
float lx=0,ly=0,rx=0,ry=0,lt=0,rt=0;
int keymap=0;
std::map<std::string,int> saved;
std::vector<std::wstring> speech;
void neutral(){for(bool&k:keys)k=false;for(bool&k:edges)k=false;for(bool&b:buttons)b=false;for(bool&b:button_edges)b=false;lx=ly=rx=ry=lt=rt=0;input::bindings::begin_frame();}
}
namespace input {
bool game_window_focused(){return fixture::focus;}bool gamepad_available(){return fixture::pad_available;}
bool key_pressed(Key k){return fixture::edges[int(k)];}bool key_held(Key k){return fixture::keys[int(k)];}bool key_in_use(Key k){return key_held(k)||key_pressed(k);}
bool shift_held(){return key_held(Key::LeftShift);}bool ctrl_held(){return key_held(Key::LeftControl);}bool alt_held(){return key_held(Key::LeftAlt);}
bool button_pressed(int i){return fixture::button_edges[i];}bool button_held(int i){return fixture::buttons[i];}
float left_stick_x(){return fixture::lx;}float left_stick_y(){return fixture::ly;}float right_stick_x(){return fixture::rx;}float right_stick_y(){return fixture::ry;}float left_trigger(){return fixture::lt;}float right_trigger(){return fixture::rt;}
std::wstring button_label(int i){return L"Button "+std::to_wstring(i);}const char*key_name(Key){return "Key";}
namespace gamepad {
bool lstick_up_pressed(){return false;}bool lstick_down_pressed(){return false;}bool lstick_left_pressed(){return false;}bool lstick_right_pressed(){return false;}
bool rstick_up_pressed(){return false;}bool rstick_down_pressed(){return false;}bool rstick_left_pressed(){return false;}bool rstick_right_pressed(){return false;}
}
}
namespace settings {
int get_int(const char*k,int d){if(std::string(k)==ui::prefs::kNavigationKeymap)return fixture::keymap;auto it=fixture::saved.find(k);return it==fixture::saved.end()?d:it->second;}
void set_int(const char*k,int v){fixture::saved[k]=v;}
}
namespace modtext {std::wstring T(std::wstring_view x){return std::wstring(x);}std::wstring TF(std::wstring_view x,std::initializer_list<std::wstring>a){return text::format(x,a);}}
namespace ffpr {void say_menu(std::wstring_view x,bool){fixture::speech.emplace_back(x);}}
int main(){
    using K=input::Key;namespace b=input::bindings;
    int failures=0;auto expect=[&](bool x,const char*why){if(!x){++failures;std::printf("FAIL %s\n",why);}};
    const int route=b::add("key.route","Navigate to current entity / stop autowalk",false,1,int(K::P),0,0);
    const int brackets=b::add("key.brackets","Bracket directions",false,1,int(K::Backslash),0,1);
    std::wstring conflict;
    fixture::edges[int(K::P)]=true;
    expect(b::pressed(route)&&!b::pressed(brackets),"default Letters layout remains independently selectable");
    expect(b::set(route,int(K::F12),6,conflict),"extended function key with Ctrl+Alt can be assigned");
    expect(!b::pressed(route),"old physical key stops firing after reassignment");
    fixture::edges[int(K::F12)]=fixture::keys[int(K::LeftControl)]=fixture::keys[int(K::LeftAlt)]=true;
    expect(b::pressed(route),"effective key and exact modifiers fire the command");
    fixture::keys[int(K::LeftShift)]=true;expect(!b::pressed(route),"extra modifier cannot fire a bare/simpler command");fixture::neutral();
    const int global=b::add("key.global","Global command",false,0,int(K::G));
    expect(!b::set(route,int(K::G),0,conflict)&&conflict==L"Global command","overlapping context conflict identifies the command");
    expect(b::set(route,-1,0,conflict),"command can be unbound");fixture::edges[int(K::F12)]=true;expect(!b::pressed(route),"unbound command never dispatches");
    b::reset(route);fixture::neutral();fixture::edges[int(K::P)]=true;expect(b::pressed(route),"individual reset restores its saved default");
    fixture::keymap=1;fixture::edges[int(K::Backslash)]=true;expect(!b::pressed(route)&&b::pressed(brackets),"Brackets layout remains available after remapping Letters");fixture::keymap=0;fixture::neutral();
    b::context(input::Context::Field);b::set(route,int(K::F14),0,conflict);fixture::keys[int(K::F14)]=true;expect(b::remapped_keyboard_in_use(),"changed keyboard shortcut reserves its native input");fixture::neutral();
    const int pad=b::add("pad.field.route","Navigate to selected destination",true,-11,b::kLeftTrigger,-1);
    fixture::lt=1;b::begin_frame();expect(b::pad("pad.field.route")&&b::consumed(b::kLeftTrigger),"default LT request owns its physical trigger");fixture::neutral();
    const int mode=b::add("pad.mode.toggle","Toggle mod mode",true,-10,4,-1);
    expect(!b::set(pad,4,9,conflict),"global mode button cannot be hidden inside a navigation chord");
    expect(b::set(mode,15,-1,conflict),"global mode button accepts a free replacement");
    b::set(pad,0,9,conflict);fixture::lt=1;b::begin_frame();expect(!b::pad("pad.field.route"),"old controller shortcut is released after remapping");fixture::neutral();
    fixture::buttons[9]=true;b::begin_frame();expect(!b::pad("pad.field.route"),"modifier alone cannot execute a chord");
    fixture::buttons[0]=true;b::begin_frame();expect(b::pad("pad.field.route")&&b::consumed(0)&&b::consumed(9)&&b::navigation_request_pressed(),"remapped route chord owns both inputs and is recognized by autowalk");
    b::begin_frame();expect(!b::navigation_request_pressed()&&b::navigation_controller_input(0)&&b::navigation_controller_input(9),"holding a remapped route chord is not a new press or unrelated manual input");
    fixture::buttons[9]=false;b::begin_frame();expect(b::pad("pad.field.route",true),"releasing either chord component releases the command");fixture::neutral();
    fixture::focus=false;fixture::buttons[0]=fixture::buttons[9]=true;b::begin_frame();expect(!b::pad("pad.field.route"),"background input cannot dispatch");
    fixture::focus=true;b::begin_frame();expect(!b::pad("pad.field.route"),"held chord on refocus is seeded without a press");fixture::neutral();
    fixture::button_edges[0]=true;fixture::buttons[9]=true;b::begin_frame();expect(b::pad("pad.field.route"),"a short button tap retained by the worker is not lost");fixture::neutral();
    b::set(pad,b::kRightTrigger,-1,conflict);fixture::rt=1;b::begin_frame();b::begin_frame();expect(b::navigation_controller_input(b::kRightTrigger),"held remapped trigger does not immediately cancel the route");fixture::neutral();
    b::set(pad,b::kRightUp,-1,conflict);fixture::ry=1;b::begin_frame();b::begin_frame();expect(b::navigation_controller_input(b::kRightUp),"held remapped stick direction retains command ownership");fixture::neutral();
    for (int stick=0;stick<2;++stick) for (int direction=0;direction<4;++direction) {
        const int input=b::kLeftUp+4*stick+direction;
        expect(b::set(pad,input,-1,conflict),"stick direction can be assigned to navigation");
        float& x=stick?fixture::rx:fixture::lx;float& y=stick?fixture::ry:fixture::ly;
        x=direction==2?-1.f:direction==3?1.f:0.f;
        y=direction==0?1.f:direction==1?-1.f:0.f;
        b::begin_frame();
        expect(b::pad("pad.field.route")&&b::current[input],"production-sign stick press fires the assigned direction");
        for(int held=0;held<5;++held) {
            b::begin_frame();
            expect(!b::pad("pad.field.route")&&b::navigation_controller_input(input),"held stick remains owned without another press");
            for(int other=b::kLeftUp;other<b::kPadInputs;++other)
                expect(b::current[other]==(other==input),"held direction never becomes its opposite or another stick");
        }
        x=y=0;b::begin_frame();expect(b::pad("pad.field.route",true),"stick neutral releases the assigned command");
        fixture::neutral();
    }
    b::capture(route);b::update_capture();fixture::keys[int(K::LeftControl)]=true;fixture::edges[int(K::F16)]=true;b::update_capture();expect(!b::capturing()&&b::entries()[route].primary==int(K::F16)&&b::entries()[route].modifier==2,"keyboard capture stores key and held modifiers");fixture::neutral();
    b::capture(route);b::update_capture();fixture::edges[int(K::Escape)]=true;b::update_capture();expect(!b::capturing()&&b::entries()[route].primary==int(K::F16),"Escape cancels capture without mutating the binding");fixture::neutral();
    b::capture(pad);b::update_capture();fixture::buttons[9]=true;b::begin_frame();b::update_capture();
    expect(b::capturing(),"controller capture waits to distinguish a single button from a chord");
    fixture::buttons[1]=true;b::begin_frame();b::update_capture();expect(!b::capturing()&&b::entries()[pad].primary==1&&b::entries()[pad].modifier==9,"controller capture stores held modifier plus second input");fixture::neutral();
    b::set(pad,-1,-1,conflict);
    const auto persisted=fixture::saved;b::catalog().clear();fixture::saved=persisted;
    const int restored=b::add("key.route","Directions",false,1,int(K::P));expect(b::entries()[restored].primary==int(K::F16)&&b::entries()[restored].modifier==2,"per-game settings restore the effective binding after fresh registration");
    const int restored_pad=b::add("pad.field.route","Route",true,-11,b::kLeftTrigger,-1);expect(b::entries()[restored_pad].primary==-1,"unbound controller command persists with no modifier across registration");
    b::reset_all();expect(b::entries()[restored].primary==int(K::P),"reset all restores defaults without changing other settings");
    std::vector<std::string> ids;
    for(int i=0;i<240;++i) {ids.push_back("pad.benchmark."+std::to_string(i));b::add(ids.back().c_str(),"Owned fixture command",true,-11,i%b::kPadInputs,-1);}
    const auto started=std::chrono::steady_clock::now();
    for(int frame=0;frame<10000;++frame) {b::begin_frame();for(int i=0;i<24;++i)b::pad(ids[size_t(i)*10].c_str());}
    const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-started).count()/10000;
    std::printf("Owned dispatch fixture: 242 entries, poll plus 24 command checks, %.3f us/frame average; fake physical APIs, no actual game-frame/audible measurement\n",us);
    std::printf("Binding persistence, layouts, conflicts, input ownership, capture and refocus: %d failures\n",failures);return failures?1:0;
}
