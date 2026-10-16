// Actual lifecycle hooks and native-owner selection; no game, scene scan or process access.
#include <map>
#include <cstdio>
#include <cstring>
#include "../src/core/field/live_context.cpp"
namespace fixture {
struct Node {void* fieldController=nullptr;void* player=nullptr;void* list=nullptr;bool active=true,enabled=true;};
Node map,root,player,old_player,keyboard,touch,alternate;
std::vector<void*> list;
std::map<std::string,void*> cache;
int reads=0,original_calls=0;
void original(void*,void*,const void*){++original_calls;}
}
namespace core {Install::Install(const char*n,int(*f)(),int o,uint8_t g):name(n),fn(f),order(o),games(g){}}
namespace field::r {
ref::Class kFieldMap{"","FieldMap"},kFieldController{"Last.Map","FieldController"},kFieldPlayerController{"Last.Map","FieldPlayerController"};
ref::Field fFieldController{kFieldMap,"fieldController"},fFieldPlayer{kFieldPlayerController,"fieldPlayer"};
}
namespace ref {
Class::Class(const char*n,const char*c,uint8_t g,const char*a):ns(n),name(c),games(g),assembly(a){}
Field::Field(Class&c,const char*n,const char*a):cls(c),name(n),alt(a){}
Method::Method(Class&c,const char*n,int a):cls(c),name(n),argc(a){}
bool Class::is(void*p){return p!=nullptr;}
int32_t Field::get(){
    ++fixture::reads;
    if(std::strcmp(name,"fieldController")==0)return offsetof(fixture::Node,fieldController);
    if(std::strcmp(name,"player")==0||std::strcmp(name,"fieldPlayer")==0)return offsetof(fixture::Node,player);
    if(std::strcmp(name,"<FieldPlayerControllerList>k__BackingField")==0)return offsetof(fixture::Node,list);
    if(std::strcmp(name,"<InputEnable>k__BackingField")==0)return offsetof(fixture::Node,enabled);
    return -1;
}
void* Method::ptr(){return nullptr;}
}
namespace unity {
bool alive(void*p){return p!=nullptr;}
bool active_in_hierarchy(void*p){return p&&static_cast<fixture::Node*>(p)->active;}
int array_length(void*p){return p?int(static_cast<std::vector<void*>*>(p)->size()):0;}
void* array_at(void*p,int i){++fixture::reads;return static_cast<std::vector<void*>*>(p)->at(i);}
}
namespace field::cache {
void* get(ref::Class&c){return fixture::cache[c.name];}
void register_object(ref::Class&c,void*p){fixture::cache[c.name]=p;}
}
namespace hooks {bool attach_raw(ref::Method&,void*,void**){return true;}void note_fault(const char*){std::abort();}}
int main(){
    int failures=0;auto expect=[&](bool x,const char*n){if(!x){++failures;std::printf("FAIL %s\n",n);}};
    fixture::map.fieldController=&fixture::root;fixture::root.player=&fixture::player;
    fixture::root.list=&fixture::list;
    fixture::keyboard.player=fixture::touch.player=fixture::alternate.player=&fixture::player;
    fixture::alternate.enabled=false;fixture::alternate.active=false;
    fixture::list={&fixture::keyboard,&fixture::touch,&fixture::alternate};fixture::touch.enabled=false;
    fixture::cache["FieldMap"]=&fixture::map;fixture::cache["FieldPlayerController"]=&fixture::alternate;
    expect(field::player_controller()==&fixture::keyboard,"inactive last-initialized controller cannot replace active native owner");
    fixture::keyboard.enabled=false;fixture::touch.enabled=true;
    expect(field::player_controller()==&fixture::touch,"native input-format handoff resolves active touch controller");
    fixture::touch.player=&fixture::old_player;
    expect(!field::player_controller() && field::current_native_player()==&fixture::player,"stale player cannot pass by comparing a cached controller to itself");
    fixture::touch.player=&fixture::player;field::o_switch=&fixture::original;
    field::d_switch(&fixture::map,&fixture::touch,nullptr);
    expect(fixture::original_calls==1 && fixture::cache["FieldPlayerController"]==&fixture::touch,"native switch runs first then captures its actual current controller");
    field::o_init=&fixture::original;field::d_init(&fixture::alternate,&fixture::player,nullptr);
    expect(fixture::cache["FieldPlayerController"]==&fixture::touch,"later initialization of an inactive alternative cannot overwrite the switched owner");
    fixture::touch.enabled=false;fixture::cache["FieldPlayerController"]=nullptr;fixture::list.resize(33,nullptr);fixture::reads=0;
    expect(!field::player_controller() && fixture::reads<10,"oversized owner list refuses bounded fallback without scanning");
    std::printf("Native controller ownership/init/switch/read bound: %d failures\n",failures);return failures?1:0;
}
