// check-sources: json.cpp text.cpp
// Actual guide controller, real FF2 guide data and native-phase callback contract.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include "../src/core/field/puzzle_guide.cpp"
namespace fixture {
json::Value puzzles;
bool ready=true,blocked=false;
int map=185,requests=0,retired=0;
field::baked_route::Cell cell{0,15,27};
std::vector<std::wstring> spoken;
std::function<void(field::PathInfo)> route;
frame::Step step;
std::wstring scene=L"Field";
std::map<events::Kind,events::Handler> handlers;
}
namespace core {Install::Install(const char*n,int(*f)(),int o,uint8_t g):name(n),fn(f),order(o),games(g){} }
namespace hooks {void note_fault(const char*){std::abort();} }
namespace log_ {void write(const char*,...){} }
namespace data {bool game_data_ready(){return true;} const json::Value& game_json(const char*){return fixture::puzzles;} }
namespace actions {void define(const char*,Fn){} }
namespace queries {bool is(const char*n){return std::string(n)=="GameStatePatches.IsFieldNavigationReady" ? fixture::ready : std::string(n)=="GameStatePatches.IsInputTransitionBlocked" ? fixture::blocked : false;} void define(const char*,std::function<bool()>){ } }
namespace events {void on(Kind kind,Handler fn){fixture::handlers[kind]=std::move(fn);} }
namespace frame {int every_frame(Step s,const char*,int){fixture::step=std::move(s);return 1;} void cancel_label(const char*){fixture::step={};} }
namespace ffpr {void say(std::wstring_view s,bool){fixture::spoken.emplace_back(s);} void say_sequence(std::vector<std::wstring> parts,bool){for(auto& s:parts)fixture::spoken.push_back(std::move(s));} }
namespace modtext {std::wstring T(std::wstring_view s){return std::wstring(s);} std::wstring TF(std::wstring_view s,std::initializer_list<std::wstring> args){auto out=std::wstring(s);int i=0;for(const auto& a:args){auto token=L"{"+std::to_wstring(i++)+L"}";auto at=out.find(token);if(at!=out.npos)out.replace(at,token.size(),a);}return out;} }
namespace settings {bool get_bool(const char*,bool v){return v;} }
namespace field {int current_map_id(){return fixture::map;} void* map_handle(){return nullptr;} void* player(){return nullptr;} }
namespace field {const std::wstring& scene_name(){return fixture::scene;}}
namespace field::baked_nav {
std::wstring map_name_of(int id){return id==187 ? L"Deist Cavern B5":L"Deist Cavern";}
bool player_cell(void*,void*,baked_route::Cell& out){out=fixture::cell;return true;}
void clear_preview_destination(const MapGoal&){++fixture::retired;}
void request_route(MapGoal,TargetKind,std::wstring,std::function<void(PathInfo)> fn,bool keep){if(keep)std::abort();++fixture::requests;fixture::route=std::move(fn);}
}
int main(){
    using namespace field::puzzle_guide;
    int failures=0;auto expect=[&](bool v,const char*s){if(!v){++failures;std::printf("FAIL %s\n",s);}};
    std::ifstream file("data/ff2/puzzles.json");const std::string bytes((std::istreambuf_iterator<char>(file)),{});
    expect(json::parse(bytes,fixture::puzzles),"real Deist guide JSON loads");
    install();
    fixture::blocked=true;start_or_repeat();
    expect(!active() && !fixture::requests && fixture::spoken.empty(),"guide key cannot load or plan during transition");
    fixture::blocked=false;start_or_repeat();
    expect(active() && fixture::requests==1,"Deist guide starts and owns its route preview");
    field::PathInfo info;info.success=true;info.description=L"3 West";
    fixture::blocked=true;const auto before=fixture::spoken.size();fixture::route(info);
    expect(fixture::spoken.size()==before,"guide callback cannot speak during transition");
    fixture::blocked=false;announce_step(false);auto old=fixture::route;stop();start_or_repeat();
    const auto restarted=fixture::spoken.size();old(info);
    expect(fixture::spoken.size()==restarted,"stopped/restarted same guide cannot revive the old callback");
    fixture::ready=false;on_map_changed();const int requests=fixture::requests;
    expect(fixture::step && fixture::step() && fixture::requests==requests,"guide waits for native readiness without route work");
    fixture::map=187;fixture::cell={1,9,8};fixture::ready=true;
    expect(!fixture::step() && !active() && fixture::retired>=2,"native B5 arrival completes guide and retires its own receipt");
    expect(fixture::spoken.back()==L"Puzzle guide finished","completion is queued after the map announcement");
    fixture::map=185;start_or_repeat();fixture::scene=L"Title";
    fixture::handlers[events::Kind::SceneLoaded]();
    expect(!active(),"return to title retires the guide before another game load");
    std::printf("Puzzle guide check: %d failures\n",failures);return failures?1:0;
}
