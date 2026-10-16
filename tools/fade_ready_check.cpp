// Actual shared transition helper; native alpha-zero semantics verified separately
// by tools/re/trap_fade_check.py against all six matching GameAssembly binaries.
#include <cstdio>
#include <cstdlib>
#include "../src/core/fieldnav/map_transition.cpp"
namespace fixture { float alpha=0, image_alpha=1; int calls=0; bool used_ready=false; int manager=0,map_id=0,scans=0,invalidations=0,arrivals=0; bool same_map_drop=false; }
bool ready_native(void*,const void*) { ++fixture::calls; return fixture::alpha==0; }
bool old_finish(void*,const void*) { ++fixture::calls; return fixture::alpha==0 && fixture::alpha!=fixture::image_alpha; }
namespace core { Install::Install(const char* n,int(*f)(),int o,uint8_t g):name(n),fn(f),order(o),games(g){} }
namespace ref {
Class::Class(const char*n,const char*c,uint8_t g,const char*a):ns(n),name(c),games(g),assembly(a){}
Method::Method(Class&c,const char*n,int a):cls(c),name(n),argc(a){}
il2cpp::Class* Class::get(){return nullptr;} il2cpp::Method* Method::get(){return nullptr;}
void* Method::ptr(){fixture::used_ready=std::string(name)=="IsStateFadeIn"; return fixture::used_ready ? reinterpret_cast<void*>(&ready_native):reinterpret_cast<void*>(&old_finish);}
void* instance(Class&){return &fixture::manager;}
}
namespace hooks {bool attach_raw(ref::Method&,void*,void**){return false;} void note_fault(const char*){std::abort();}}
namespace log_ {void write(const char*,...){} }
namespace frame {double now(){return 0;} }
namespace menu_state {std::vector<std::string> active_states(){return{};} void reset(const char*){} }
namespace actions {bool defined(const char*){return false;} bool run(const char*){return false;} }
namespace queries {void define(const char*,std::function<bool()>){ } void define_text(const char*,std::function<std::wstring()>){ } std::wstring text(const char*){return{};} }
namespace events {void fire(Kind kind){if(kind==Kind::MapChanged)++fixture::arrivals;} void on(Kind,Handler){} }
namespace field {int current_map_id(){return fixture::map_id;} void request_rescan(){++fixture::scans;} }
namespace field {void* map_handle(){return nullptr;} void* player(){return nullptr;}}
namespace field::baked_nav {bool player_cell(void*,void*,baked_route::Cell&){return true;} bool note_trap_start(int,const baked_route::Cell&){return fixture::same_map_drop;}}
namespace field::scanner {bool needs_scan(int){return false;} void invalidate_map(){++fixture::invalidations;} }
namespace ffpr {void say(std::wstring_view,bool){} }
namespace modtext {std::wstring T(std::wstring_view s){return std::wstring(s);} std::wstring TF(std::wstring_view s,std::initializer_list<std::wstring>){return std::wstring(s);} }
namespace text {std::wstring trim(std::wstring_view s){return std::wstring(s);} bool is_blank(std::wstring_view s){return s.empty();}}
bool outer_blocked=false;
void native_change(void*,int32_t,const void*){outer_blocked=ffpr::fieldnav::map_transition::is_state_changing();}
int main(){
    int failures=0;
    auto expect=[&](bool v,const char* m){if(!v){++failures;std::printf("FAIL %s\n",m);}};
    using ffpr::fieldnav::map_transition::is_screen_fading;
    fixture::alpha=0; fixture::image_alpha=0;
    expect(!old_finish(nullptr,nullptr),"native old completion reproduces transparent pitfall latch");
    expect(!is_screen_fading() && fixture::used_ready,"settled transparent pitfall resumes shared scanner/input readiness");
    fixture::image_alpha=1;
    expect(!is_screen_fading(),"ordinary completed fade resumes");
    for(float alpha:{0.01f,0.5f,1.0f}){fixture::alpha=alpha;expect(is_screen_fading(),"active fade stays blocked");}
    using namespace ffpr::fieldnav::map_transition;
    fixture::alpha=0;fixture::map_id=407;o_ChangeState=&native_change;
    d_ChangeState(nullptr,3,nullptr);
    expect(outer_blocked && !is_state_changing(),"outer transition detour gates its entire prefix/original/postfix without leaving a stale gate");
    const int initial_scans=fixture::scans, initial_arrivals=fixture::arrivals;
    d_ChangeState(nullptr,12,nullptr); d_ChangeState(nullptr,3,nullptr);
    expect(fixture::scans==initial_scans+1 && fixture::arrivals==initial_arrivals,"scripted same-map return refreshes without inventing a floor change");
    fixture::same_map_drop=true;
    d_ChangeState(nullptr,12,nullptr);
    expect(fixture::invalidations==1 && fixture::arrivals==initial_arrivals,"planned same-map drop invalidates before native script and waits for return");
    d_ChangeState(nullptr,3,nullptr);
    expect(fixture::arrivals==initial_arrivals+1 && fixture::scans==initial_scans+2,"planned same-map drop resumes route lifecycle after native return");
    std::printf("Fade readiness check: %d failures\n",failures);return failures?1:0;
}
