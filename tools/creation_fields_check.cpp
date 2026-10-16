// check-sources: text.cpp
// Production FF1 creation reader; fake only native UI/metadata and speech boundaries.
#include "../src/games/ff1/new_game.cpp"
#include <map>
#include <cstdio>
#include <cstdlib>
#include <thread>
namespace fixture {
struct Node {
    const char* type = "Other";
    bool visible = true;
    void* list = nullptr;
    void* data = nullptr;
    int id = 0;
    std::wstring text;
};
using List = std::vector<void*>;
DWORD owner=GetCurrentThreadId();int reads=0,invokes=0;
std::vector<std::wstring> speech;
std::vector<frame::Task> tasks;
std::map<std::string,long long> indices;
void native() { if(GetCurrentThreadId()!=owner)std::abort();++reads; }
void select(void*,void*,const void*) {}
void drain() { auto pending=std::move(tasks);tasks.clear();for(auto& task:pending)task(); }
}
namespace core { Install::Install(const char*n,int(*f)(),int o,uint8_t g):name(n),fn(f),order(o),games(g){} }
namespace ref {
Class::Class(const char*n,const char*c,uint8_t g,const char*a):ns(n),name(c),games(g),assembly(a){}
Field::Field(Class&c,const char*n,const char*a):cls(c),name(n),alt(a){}
Method::Method(Class&c,const char*n,int a):cls(c),name(n),argc(a){}
il2cpp::Class* Class::get(){++fixture::invokes;return nullptr;}
bool Class::is(void*p){fixture::native();return p&&std::strcmp(static_cast<fixture::Node*>(p)->type,name)==0;}
int32_t Field::get(){fixture::native();
    const std::string n=name;
    if(n=="contentList"||n=="<CommandList>k__BackingField")return offsetof(fixture::Node,list);
    if(n=="<Data>k__BackingField"||n=="<CurrentData>k__BackingField")return offsetof(fixture::Node,data);
    if(n=="<Id>k__BackingField")return offsetof(fixture::Node,id);
    return -1;
}
void* Method::ptr(){++fixture::invokes;return nullptr;}il2cpp::Method* Method::get(){++fixture::invokes;return nullptr;}
std::wstring str(void*p){fixture::native();return p?*static_cast<std::wstring*>(p):L"";}
std::wstring rd_str(void*p,Field&){fixture::native();return p?static_cast<fixture::Node*>(p)->text:L"";}
void call_failed_note(const char*){std::abort();}
}
namespace hooks {bool attach_raw(ref::Method&,void*,void**){return false;}void note_fault(const char*){std::abort();}}
namespace unity {
bool alive(void*p){fixture::native();return p!=nullptr;}
bool active_in_hierarchy(void*p){fixture::native();return p&&static_cast<fixture::Node*>(p)->visible;}
int list_count(void*p){fixture::native();return p?int(static_cast<fixture::List*>(p)->size()):0;}
void* list_at(void*p,int i){fixture::native();auto&v=*static_cast<fixture::List*>(p);return i>=0&&i<int(v.size())?v[i]:nullptr;}
}
namespace displayed {
bool container_visible(void*p){return unity::active_in_hierarchy(p);}
void* field(void*,const char*){return nullptr;}
std::wstring text_field(void*p,const char*){fixture::native();return container_visible(p)?static_cast<fixture::Node*>(p)->text:L"";}
}
namespace ui {int cursor_index(void*p){return static_cast<fixture::Node*>(p)->id;}}
namespace frame {void next_frame(Task task,const char*){fixture::tasks.push_back(std::move(task));}uint64_t count(){return 1;}}
namespace ffpr {void say(std::wstring_view s,bool){fixture::speech.emplace_back(s);}void say_sequence(std::vector<std::wstring> parts,bool){for(auto& part:parts)fixture::speech.push_back(std::move(part));}}
namespace modtext {std::wstring T(std::wstring_view s){return std::wstring(s);}std::wstring TF(std::wstring_view s,std::initializer_list<std::wstring>a){return text::format(s,a);}}
namespace dedup {
bool index(const char*c,long long i,Scope){auto it=fixture::indices.find(c);if(it!=fixture::indices.end()&&it->second==i)return false;fixture::indices[c]=i;return true;}
bool text(const char*,std::wstring_view,Scope){return true;}void reset(const char*c){fixture::indices.erase(c);}
}
namespace log_ {void write(const char*,...){} }
namespace queries {void define(const char*,std::function<bool()>){}}
namespace actions {void define(const char*,Fn){}}
namespace ffpr::dialogue {int change_name_popup_opens(){return 0;}void read_popup_message(void*,const char*){}}
int main() {
    namespace n=ffpr::ff1;using fixture::Node;using fixture::List;
    int failures=0;auto expect=[&](bool ok,const char*why){if(!ok){++failures;std::printf("FAIL %s\n",why);}};
    Node window{"NewGameWindowController"},list{"CharacterContentListController"},cursor;
    Node data[4],name[4],job[4],slot[4];List commands[4],slots;
    const wchar_t* values[]={L"Warrior",L"Thief",L"White Mage",L"Black Mage"};
    for(int i=0;i<4;++i){data[i].type="CharacterSelectCommandData";data[i].id=1;
        name[i].type=job[i].type="CharacterSelectCommandContentView";name[i].text=L"Name";
        job[i].data=&data[i];job[i].text=values[i];commands[i]={&name[i],&job[i]};
        slot[i].type="CharacterContentController";slot[i].list=&commands[i];slot[i].text=L"Player";
        slots.push_back(&slot[i]);}
    list.list=&slots;n::o_CharSelectContent=&fixture::select;
    for(int i=0;i<4;++i){n::on_init_select(&window);fixture::speech.clear();cursor.id=2*i+1;
        n::d_CharSelectContent(&list,&cursor,nullptr);expect(fixture::speech.empty(),"existing native one-frame field readiness remains");fixture::drain();
        expect(fixture::speech==std::vector<std::wstring>{L"Light Warrior "+std::to_wstring(i+1)+L": Class: "+values[i]},"each character's displayed class is read from its own slot");}
    for(const auto* value:{L"Monk",L"Red Mage",L"Krieger",L"白魔道士"}){
        job[0].text=value;n::on_init_select(&window);fixture::speech.clear();cursor.id=1;n::d_CharSelectContent(&list,&cursor,nullptr);fixture::drain();
        expect(fixture::speech.size()==1&&fixture::speech[0]==std::wstring(L"Light Warrior 1: Class: ")+value,"changed and localized displayed class wins without a master lookup or English fallback");}
    const auto unchanged=fixture::speech.size();n::d_CharSelectContent(&list,&cursor,nullptr);fixture::drain();
    expect(fixture::speech.size()==unchanged,"existing slot/field event guard remains unchanged");
    data[0].id=2;expect(n::job_name(&list,0).empty(),"non-class command cannot substitute its text");data[0].id=1;
    job[0].visible=false;expect(n::job_name(&list,0).empty(),"hidden class value stays absent");job[0].visible=true;
    slot[0].visible=false;expect(n::job_name(&list,0).empty(),"hidden slot stays absent");slot[0].visible=true;
    slot[0].data=nullptr;expect(n::job_name(&list,0)==job[0].text,"no master or NewGameSelectData is required for a displayed class");
    expect(n::job_name(&list,-1).empty()&&n::job_name(&list,4).empty(),"invalid slot cannot read a neighbor");
    commands[0].push_back(&name[0]);expect(n::job_name(&list,0).empty(),"unexpected command layout fails closed");commands[0].pop_back();
    slots.push_back(&slot[0]);expect(n::job_name(&list,0).empty(),"oversized slot pool is bounded");slots.pop_back();
    n::on_init_select(&window);fixture::speech.clear();cursor.id=0;n::d_CharSelectContent(&list,&cursor,nullptr);fixture::drain();
    expect(fixture::speech==std::vector<std::wstring>{L"Light Warrior 1: Name: Player"},"name field remains independent");
    fixture::speech.clear();cursor.id=8;n::d_CharSelectContent(&list,&cursor,nullptr);fixture::drain();expect(fixture::speech==std::vector<std::wstring>{L"Done"},"Done remains independent");
    expect(fixture::invokes==0,"field capture makes no master/native method invocation");
    std::printf("Creation fields: actual reader/detour, four slots/six classes/localization, changed class, bounds/visibility, name/Done, owner-only reads; %d failures\n",failures);
    return failures?1:0;
}
