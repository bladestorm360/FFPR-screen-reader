// check-sources: text.cpp
// Actual movement/input detours with bounded native owners; no game is launched.
#include <map>
#include <cstdio>
#include <cstdlib>
#include "../src/core/text.h"
#include "../src/core/fieldnav/autowalk.cpp"
#include "../src/core/input/input_passthrough.cpp"
namespace fixture {
struct Node { void* player=nullptr; void* controller=nullptr; bool enabled=true, airship=false, moving=false, up=false, down=false; float angle=0; unity::Vec2 cached{}; };
Node player, controller, map, field_controller;
uint64_t tick=10; double seconds=1;
bool ready=true, focus=true, menu=false, manual=false;
game::Id game=game::Id::FF1;
field::VehicleState vehicle{true,1,1,L""};
field::baked_route::Cell cell{1,1,1};
unity::Vec2 native{};
unity::Vec2 moved{};int calls=0, airship_calls=0, key_calls=0; bool shoulders=false;
void step() { ++tick; seconds+=.016; }
bool moving(void* o,const void*) { return static_cast<Node*>(o)->moving; }
float angle(void* o,const void*) { return static_cast<Node*>(o)->angle; }
void on_key(void* o,int k,const void*) { ++key_calls; if(k==2)static_cast<Node*>(o)->up=true; if(k==3)static_cast<Node*>(o)->down=true; }
void on_key_up(void* o,int k,const void*) { ++key_calls; if(k==2)static_cast<Node*>(o)->up=false; if(k==3)static_cast<Node*>(o)->down=false; }
void touch(void* o,unity::Vec2 value,const void*) {static_cast<Node*>(o)->cached=value;}
void flight(void* o,const void*) { ++airship_calls;auto& n=*static_cast<Node*>(o);shoulders=n.up&&n.down;
 const float radians=field_controller.angle*.017453292519943295f;
 moved={n.cached.x*std::cos(radians)-n.cached.y*std::sin(radians),n.cached.x*std::sin(radians)+n.cached.y*std::cos(radians)};
 // Recovered native phase order: consume stored axis, then observe next input.
 n.cached=ffpr::fieldnav::autowalk::d_InputAxis(nullptr,nullptr);
}
unity::Vec2 native_axis(void*,const void*) { ++calls;return native; }
bool native_key(void*,int,const void*) { ++calls;return false; }
bool listener(void*,void*,int,bool,bool,const void*) { ++calls;return false; }
}
namespace core { Install::Install(const char* n,int(*f)(),int o,uint8_t g):name(n),fn(f),order(o),games(g){} }
namespace ref {
Class::Class(const char*n,const char*c,uint8_t g,const char*a):ns(n),name(c),games(g),assembly(a){}
Method::Method(Class&c,const char*n,int a):cls(c),name(n),argc(a){}
Field::Field(Class&c,const char*n,const char*a):cls(c),name(n),alt(a){}
bool Class::is(void* p){if(!p)return false;auto*s=static_cast<fixture::Node*>(p);return std::string(name)=="FieldPlayerKeyAirshipController"?s->airship:std::string(name)=="FieldPlayerKeyController"?!s->airship:true;}
int32_t Field::get(){std::string n=name;if(n=="fieldPlayer")return offsetof(fixture::Node,player);if(n=="fieldController")return offsetof(fixture::Node,controller);if(n=="<InputEnable>k__BackingField")return offsetof(fixture::Node,enabled);if(n=="isDownShiftUp")return offsetof(fixture::Node,up);if(n=="isDownShiftDown")return offsetof(fixture::Node,down);return -1;}
void* Method::ptr(){std::string n=name;if(n=="get_IsMoving")return reinterpret_cast<void*>(&fixture::moving);if(n=="GetZAxisRotateBirdCamera")return reinterpret_cast<void*>(&fixture::angle);if(n=="OnTouchPadCallback")return reinterpret_cast<void*>(&fixture::touch);if(n=="OnKey")return reinterpret_cast<void*>(&fixture::on_key);if(n=="OnKeyUp")return reinterpret_cast<void*>(&fixture::on_key_up);return nullptr;}
il2cpp::Method* Method::get(){return reinterpret_cast<il2cpp::Method*>(ptr());}
std::wstring rd_str(void*,Field&){return L"Action";}
void call_failed_note(const char*){std::abort();}
}
namespace unity {bool alive(void*p){return p!=nullptr;}bool active_in_hierarchy(void*p){return p!=nullptr;}void*transform(void*p){return p;}Vec3 local_position(void*){return {};}}
namespace log_ {void write(const char*,...){} }
namespace hooks {bool attach_raw(ref::Method&,void*,void**){return true;}void note_fault(const char*){std::abort();}}
namespace game {Id current(){return fixture::game;}}
namespace frame {uint64_t count(){return fixture::tick;}double now(){return fixture::seconds;}void cancel_label(const char*){}int every_frame(Step,const char*,int){return 1;}}
namespace actions {void define(const char*,Fn){} }
namespace events {void on(Kind,Handler){} }
namespace queries {void define(const char*,std::function<bool()>){}bool is(const char*n){return std::string(n)=="GameStatePatches.IsFieldNavigationReady"?fixture::ready:false;} }
namespace settings {bool get_bool(const char*k,bool d){return std::string(k)=="Autowalk"?true:d;} }
namespace menu_state {bool any_active(){return fixture::menu;} }
namespace field::cache {void*get(ref::Class&t){return std::string(t.name)=="FieldMap"?&fixture::map:&fixture::controller;}void*get_or_refresh(ref::Class&t){return get(t);} }
namespace field {
void*player(){return &fixture::player;}void*map_handle(){return &fixture::map;}int current_map_id(){return 1;}bool face_player(void*,int){return true;}Target selected(){return {};}
VehicleState current_vehicle(){return fixture::vehicle;}const std::wstring&scene_name(){static std::wstring s=L"Field";return s;}
namespace baked_nav {bool player_cell(void*,void*,baked_route::Cell&c){c=fixture::cell;return true;}bool cell_at(void*,unity::Vec3,baked_route::Cell&c){c=fixture::cell;return true;}bool walk_edge(void*,void*,const baked_route::Cell&,const baked_route::Cell&,bool,route_progress::VehicleAction,int){return true;}
void remember_destination(const MapGoal&,TargetKind,const std::wstring&){}void clear_pending(){}void cancel_requests(){}void request_route(MapGoal,TargetKind,std::wstring,std::function<void(PathInfo)>,bool){} }
}
namespace modtext {std::wstring T(std::wstring_view s){return std::wstring(s);}std::wstring TF(std::wstring_view s,std::initializer_list<std::wstring>a){return text::format(s,a);} }
namespace ffpr {void say(std::wstring_view,bool){} }
namespace input {
bool game_window_focused(){return fixture::focus;}bool suppress_game_input(){return false;}bool field_active(){return true;}bool any_key_pressed(){return fixture::manual;}bool key_pressed(Key){return false;}bool key_in_use(Key){return false;}
bool shift_held(){return false;}bool ctrl_held(){return false;}bool alt_held(){return false;}float left_trigger(){return 0;}float right_trigger(){return 0;}float left_stick_x(){return 0;}float left_stick_y(){return 0;}float right_stick_x(){return 0;}float right_stick_y(){return 0;}
bool button_pressed(int){return false;}bool button_held(int){return false;}bool button_released(int){return false;}bool gamepad_available(){return false;}
namespace router {bool is_button_consumed(int){return false;}bool is_stick_pulse_down(int){return false;}bool is_stick_pulse_up(int){return false;}}
namespace gamepad {bool lstick_up_pressed(){return false;}bool lstick_down_pressed(){return false;}bool lstick_left_pressed(){return false;}bool lstick_right_pressed(){return false;}bool lstick_up_held(){return false;}bool lstick_down_held(){return false;}bool lstick_left_held(){return false;}bool lstick_right_held(){return false;}}
}
int main(){
 namespace a=ffpr::fieldnav::autowalk;int failures=0;auto expect=[&](bool b,const char*s){if(!b){++failures;std::printf("FAIL %s\n",s);}};
 fixture::controller.player=&fixture::player;fixture::map.controller=&fixture::field_controller;a::g_hooked=true;a::o_InputAxis=&fixture::native_axis;a::o_airship_update=&fixture::flight;
 input::o_GetKeyDown=input::o_GetKey=input::o_GetKeyUp=&fixture::native_key;input::o_listener_down=&fixture::listener;
 field::PathInfo p;p.success=true;p.map_width=p.map_height=8;
 a::Phase walk;walk.cells={{1,1,1}};a::Phase fly;fly.cells={{1,2,1},{1,3,1},{1,4,1}};fly.action_before=L"board";fly.vehicle_before=a::VehicleAction::ConfirmBoard;fly.transportation_id=3;fly.vehicle_direction=1;fly.action_cell=a::Cell{1,1,1};
 a::Phase land;land.cells={{1,4,1},{1,4,2}};land.action_before=L"land";land.vehicle_before=a::VehicleAction::ConfirmLand;land.transportation_id=3;land.action_cell=a::Cell{1,4,1};p.walk_phases={walk,fly,land};
 a::start(p);expect(a::active(),"valid route starts");auto axis=a::d_InputAxis(nullptr,nullptr);expect(axis.x==0&&axis.y==0&&input::d_GetKeyDown(nullptr,0,nullptr),"planned boarding queues native confirm without movement");
 expect(!input::d_GetKeyDown(nullptr,1,nullptr)&&input::d_listener_down(nullptr,nullptr,3,false,false,nullptr),"one pulse reaches manager and listener only for correct action");
 fixture::step();fixture::ready=false;fixture::controller.enabled=false;a::input_updated();expect(a::active()&&a::d_InputAxis(nullptr,nullptr).x==0,"boarding animation releases movement and preserves route");
 fixture::ready=true;fixture::controller.enabled=true;fixture::vehicle={true,3,3,L""};fixture::cell={1,2,1};fixture::step();axis=a::d_InputAxis(nullptr,nullptr);expect(axis.x==1&&a::active(),"matching boarded owner resumes riding leg");
 fixture::player.moving=true;expect(a::d_InputAxis(nullptr,nullptr).x==0,"native in-progress movement cannot advance route");fixture::player.moving=false;fixture::cell={1,3,1};fixture::step();expect(a::d_InputAxis(nullptr,nullptr).x==1,"next completed tile advances route");
 fixture::cell={1,4,1};fixture::step();a::d_InputAxis(nullptr,nullptr);expect(input::d_GetKeyDown(nullptr,0,nullptr),"landing uses native confirm");fixture::vehicle={true,1,1,L""};fixture::step();axis=a::d_InputAxis(nullptr,nullptr);expect(axis.y==-1&&a::active(),"native walking state resumes last leg");
 fixture::cell={1,4,2};fixture::step();a::d_InputAxis(nullptr,nullptr);expect(!a::active()&&!a::virtual_key(0),"arrival releases all synthetic input");
 for(int game_number:{3,6}) {
  fixture::game=static_cast<game::Id>(game_number);fixture::cell={1,1,1};fixture::vehicle={true,1,1,L""};
  a::Phase manual_board=fly;manual_board.vehicle_before=a::VehicleAction::None;p.walk_phases={walk,manual_board};a::start(p);fixture::step();a::d_InputAxis(nullptr,nullptr);
  expect(!a::active()&&!a::virtual_key(0),"manual airship boundary stops the walker without a confirm pulse in FF3/FF6");
 }
 fixture::game=game::Id::FF6;fixture::controller.airship=true;fixture::vehicle={true,3,3,L""};fixture::cell={1,2,1};fixture::field_controller.angle=90;fly.action_before.clear();fly.vehicle_before=a::VehicleAction::None;land.vehicle_before=a::VehicleAction::BackLand;p.walk_phases={fly,land};a::start(p);
 axis=a::d_InputAxis(nullptr,nullptr);expect(std::fabs(axis.x)<.00001&&std::fabs(axis.y+1)<.00001,"FF6 native camera rotation receives inverse-transformed cardinal axis");a::d_airship_update(&fixture::controller,nullptr);expect(fixture::shoulders&&!fixture::controller.up&&!fixture::controller.down,"FF6 holds both native shoulder inputs during flight and restores them afterwards");
 fixture::controller.up=true;a::d_airship_update(&fixture::controller,nullptr);expect(fixture::controller.up&&!fixture::controller.down,"native pre-existing shoulder hold survives synthetic flight");fixture::controller.up=false;
 fixture::cell={1,3,1};fixture::step();a::d_InputAxis(nullptr,nullptr);fixture::cell={1,4,1};fixture::step();a::d_InputAxis(nullptr,nullptr);expect(input::d_GetKeyDown(nullptr,1,nullptr)&&!input::d_GetKeyDown(nullptr,0,nullptr),"FF6 lands with Back rather than Confirm");
 a::cancel();fixture::cell={1,2,1};fixture::controller.cached={.75f,.75f};
 a::Phase corner;corner.cells={{1,2,1},{1,3,1},{1,3,2}};p.walk_phases={corner};a::start(p);fixture::step();a::d_airship_update(&fixture::controller,nullptr);
 expect(std::fabs(fixture::moved.x-1)<.00001&&std::fabs(fixture::moved.y)<.00001,"flight movement consumes current route axis rather than prior stored input");
 fixture::cell={1,3,1};fixture::step();a::d_airship_update(&fixture::controller,nullptr);
 expect(std::fabs(fixture::moved.x)<.00001&&std::fabs(fixture::moved.y+1)<.00001,"native stored-axis pipeline turns at the route corner without an extra straight move");
 fixture::cell={1,3,2};fixture::step();a::d_airship_update(&fixture::controller,nullptr);
 expect(!a::active()&&std::fabs(fixture::moved.x)<.00001&&std::fabs(fixture::moved.y)<.00001,"arrival releases cached native flight input before movement");
 fixture::cell={1,2,1};a::start(p);fixture::step();a::d_airship_update(&fixture::controller,nullptr);a::cancel();fixture::step();a::d_airship_update(&fixture::controller,nullptr);
 expect(std::fabs(fixture::moved.x)<.00001&&std::fabs(fixture::moved.y)<.00001,"explicit cancel releases the owned stored axis on the next native observation");
 a::start(p);fixture::native={.5f,0};fixture::step();a::d_airship_update(&fixture::controller,nullptr);expect(!a::active()&&fixture::controller.cached.x==.5f,"native manual axis takes priority and is preserved");fixture::native={};
 fixture::manual=true;a::input_updated();expect(!a::active()&&!a::virtual_key(1),"manual input cancels pending landing and releases pulse");fixture::manual=false;
 std::printf("Autowalk native input/vehicle waits/FF6 flight: %d failures\n",failures);return failures?1:0;
}
