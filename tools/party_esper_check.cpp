// check-sources: text.cpp ui/navigation_buffer.cpp
// Actual renderer, battle party, Esper readers and review worker; fake native owner only.
#include <map>
#include <array>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include "../src/core/frame.cpp"
#include "../src/core/background.cpp"
#include "../src/core/displayed_ui.cpp"
#include "../src/core/battle/battle_party.cpp"
#include "../src/core/ui/review_buffer.cpp"
#include "../src/games/ff6/ability_espers.cpp"
namespace fixture {
struct Node {std::array<void*,128> fields{};int command=0,kind=0,id=0;bool alive=true,active=true,enabled=true,owned=true;float alpha=1;Node* parent=nullptr;void* sprite=nullptr;std::wstring value;};
using List=std::vector<void*>;
std::map<std::string,int> offsets;std::vector<std::wstring> spoken;std::map<std::string,void*> owners;
std::map<std::string,std::wstring> guards;
DWORD owner=GetCurrentThreadId();int wrong=0;bool autodetail=false;game::Id game=game::Id::FF6;
void native(){if(GetCurrentThreadId()!=owner)++wrong;}
int offset(const char*n){auto [i,inserted]=offsets.emplace(n,static_cast<int>(offsets.size()));return static_cast<int>(offsetof(Node,fields))+i->second*static_cast<int>(sizeof(void*));}
void set(Node&n,const char*k,void*v){*reinterpret_cast<void**>(reinterpret_cast<char*>(&n)+offset(k))=v;}
void* get(Node&n,const char*k){return *reinterpret_cast<void**>(reinterpret_cast<char*>(&n)+offset(k));}
bool enabled(void*p,const void*){native();return static_cast<Node*>(p)->enabled;}
displayed::Color color(void*p,const void*){native();return {1,1,1,static_cast<Node*>(p)->alpha};}
void* renderer(void*,const void*){native();return nullptr;}
void* sprite(void*p,const void*){native();return static_cast<Node*>(p)->sprite;}
void* content(void*p,int index,const void*){native();auto*l=static_cast<List*>(get(*static_cast<Node*>(p),"rows"));return l&&index>=0&&index<static_cast<int>(l->size())?(*l)[index]:nullptr;}
void phase(void*,const void*){}void update(void*,void*,bool,const void*){}void focus(void*,void*,bool,const void*){}
std::function<void()> on_details_show;
void show(void*p,void*,void*,void*,const void*){static_cast<Node*>(p)->active=true;if(on_details_show)on_details_show();}void details_update(void*,void*,void*,const void*){}void list_select(void*,int,int,const void*){}
void tick(){const auto until=GetTickCount64()+5000;do{frame::run_tick();std::this_thread::yield();}while(ui::review::g_build_pending&&GetTickCount64()<until);}
Node make_text(const wchar_t*v){Node n;n.kind=1;n.value=v;return n;}
Node sprite_node;
Node image(){Node n;n.kind=2;n.sprite=&sprite_node;return n;}
bool contains(std::wstring_view s){for(auto&v:spoken)if(v.find(s)!=v.npos)return true;return false;}
}
namespace core {Install::Install(const char*n,int(*f)(),int o,uint8_t g):name(n),fn(f),order(o),games(g){}}
namespace ref {
Class::Class(const char*n,const char*c,uint8_t g,const char*a):ns(n),name(c),games(g),assembly(a){}
Field::Field(Class&c,const char*n,const char*a):cls(c),name(n),alt(a){}Method::Method(Class&c,const char*n,int a):cls(c),name(n),argc(a){}
bool Class::is(void*p){fixture::native();if(!p)return false;auto&n=*static_cast<fixture::Node*>(p);std::string k=name;if(k=="Image")return n.kind==2;if(k=="Graphic")return n.kind==1||n.kind==2;if(k=="Behaviour")return true;return true;}
void*Method::ptr(){fixture::native();std::string n=name;if(n=="get_enabled")return reinterpret_cast<void*>(&fixture::enabled);if(n=="get_color")return reinterpret_cast<void*>(&fixture::color);if(n=="get_canvasRenderer")return reinterpret_cast<void*>(&fixture::renderer);if(n=="get_overrideSprite")return reinterpret_cast<void*>(&fixture::sprite);if(n=="GetContentObject")return reinterpret_cast<void*>(&fixture::content);return nullptr;}
il2cpp::Method*Method::get(){return reinterpret_cast<il2cpp::Method*>(ptr());}
int32_t Field::get(){fixture::native();if(std::string(name)=="callerCommandId")return offsetof(fixture::Node,command);if(std::string(name)=="<ContentId>k__BackingField")return offsetof(fixture::Node,id);if(std::string(name)=="<IsOwned>k__BackingField")return offsetof(fixture::Node,owned);return fixture::offset(name);}
void call_failed_note(const char*){std::abort();}
}
namespace il2cpp {Class*object_class(Object*){fixture::native();return reinterpret_cast<Class*>(1);}int32_t field_offset(Class*,const char*n){fixture::native();return fixture::offset(n);}void*resolve_icall(const char*){return nullptr;}}
namespace unity {
bool alive(void*p){fixture::native();return p&&static_cast<fixture::Node*>(p)->alive;}bool active_in_hierarchy(void*p){return alive(p)&&static_cast<fixture::Node*>(p)->active&&(!static_cast<fixture::Node*>(p)->parent||active_in_hierarchy(static_cast<fixture::Node*>(p)->parent));}
void*transform(void*p){fixture::native();return p;}void*parent(void*p){fixture::native();return p?static_cast<fixture::Node*>(p)->parent:nullptr;}void*get_component(void*p,ref::Class&){fixture::native();return p;}void*get_component_in_parent(void*p,ref::Class&){return parent(p);}
std::wstring ui_text(void*p){fixture::native();return static_cast<fixture::Node*>(p)->value;}
std::vector<void*>texts(void*,bool){return {};}std::vector<void*>components_in_children(void*,ref::Class&,bool){return {};}
int list_count(void*p){fixture::native();return p?static_cast<int>(static_cast<fixture::List*>(p)->size()):0;}void*list_at(void*p,int i){fixture::native();return p&&i>=0&&i<list_count(p)?(*static_cast<fixture::List*>(p))[i]:nullptr;}
}
namespace unity {bool enumerate_bounded(void*p,std::vector<void*>&out,int bound){fixture::native();out.clear();if(!p||list_count(p)>bound)return false;out=*static_cast<fixture::List*>(p);return true;}}
namespace displayed {std::wstring conditions(void*){return {};}}
namespace hooks {bool attach_raw(ref::Method&,void*,void**){return false;}void note_fault(const char*){std::abort();}}
namespace game {Id current(){return fixture::game;}}
namespace field::cache {void*get(ref::Class&c){return fixture::owners[c.name];}void register_object(ref::Class&c,void*p){fixture::owners[c.name]=p;}}
namespace ffpr::battle {ref::Class kBattlePlayerData{"Last.Battle","BattlePlayerData"};bool in_battle(){return true;}void*player_owned_character(void*p){return displayed::field(p,"owned");}int character_id(void*p){return p?static_cast<fixture::Node*>(p)->id:0;}void*current_actor(){return nullptr;}std::wstring unit_name(void*){return {};}}
namespace speech {void request_load(){}}
namespace sound {void initialize(){}}
namespace ffpr::ff6 {bool auto_detail(){return fixture::autodetail;}void set_ability_focus(void*,const std::wstring&,const std::wstring&){} }
namespace ffpr {void say(std::wstring_view v,bool){fixture::spoken.emplace_back(v);}void say_parts(std::vector<std::wstring>v,bool){fixture::spoken.push_back(text::join(v,L". "));}void say_sequence(std::vector<std::wstring>v,bool b){say_parts(std::move(v),b);}}
namespace modtext {std::wstring T(std::wstring_view v){return std::wstring(v);}std::wstring TF(std::wstring_view v,std::initializer_list<std::wstring>a){return text::format(v,a);}}
namespace ui {std::wstring menu_position(const std::wstring&s,int,int){return s;}int cursor_index(void*p){return p?static_cast<fixture::Node*>(p)->id:-1;}}
namespace actions {void define(const char*,Fn){}bool run(const char*){return false;}}
namespace queries {void define(const char*,std::function<bool()>){} }
namespace events {void on(Kind,Handler){} }
namespace menu_state {bool is_active(const char*){return false;}}
namespace input {void add_binding(Key,Mod,Context,const char*,const char*){}}
namespace settings {bool get_bool(const char*,bool){return fixture::autodetail;}int get_int(const char*,int d){return d;}}
namespace dedup {bool text(const char*k,std::wstring_view v,Scope){if(fixture::guards[k]==v)return false;fixture::guards[k]=v;return true;}void reset(const char*k){fixture::guards.erase(k);}}
namespace log_ {void write(const char*,...){} }
int main(){using namespace fixture;background::prepare();int failed=0;auto expect=[&](bool ok,const char*s){if(!ok){++failed;std::printf("FAIL %s\n",s);}};
 Node party,ordinary,target,row,view,unit,copy,owned;party.active=false;owned.id=4;set(unit,"owned",&owned);set(copy,"owned",&owned);List empty,rows{&row};set(party,"contentList",&empty);set(party,"targetSelectContentList",&rows);set(row,"view",&view);set(row,"<TargetData>k__BackingField",&unit);
 Node name=make_text(L"Rydia"),hp=make_text(L"30"),maximum=make_text(L"30"),mp=make_text(L"10");set(view,"nameText",&name);set(view,"currentHpText",&hp);set(view,"maxHpText",&maximum);set(view,"currentMpText",&mp);row.enabled=false;
 for(int n=1;n<=6;++n){fixture::game=static_cast<game::Id>(n);ffpr::battle::observe_target_party(&party);expect(ffpr::battle::displayed_player(&copy)==L"Rydia, HP 30/30, MP 10","all-six actual party reader matches rendered target by native character identity with hidden controller");expect(displayed::target(&row)==L"Rydia, HP 30/30, MP 10","disabled target scripts do not hide their rendered name/vitals");}
 Node cursor,anchor,empty_row;cursor.parent=&anchor;set(view,"cursorParent",&anchor);List raw_slots{&empty_row,&row};
 expect(displayed::cursor_row(&cursor,&raw_slots)==&row,"field/formation focus follows native cursor anchor despite empty first slot and separate row GameObject");
 name.alpha=0;expect(ffpr::battle::displayed_player(&unit).empty(),"fully transparent names do not disclose a target");name.alpha=1;
 Node window,portrait_owner,portrait=image(),front,back;window.command=5;portrait.parent=&front;set(row,"menuCharacterController",&portrait_owner);set(portrait_owner,"characterImageView",&portrait);set(view,"frontParent",&front);set(view,"backParent",&back);
 expect(displayed::formation_row(&window,&row)==L"Front Row","Formation row uses displayed portrait parent");portrait.parent=&back;expect(displayed::formation_row(&window,&row)==L"Back Row","whole FF4 formation shift reports actual resulting row");window.command=2;expect(displayed::formation_row(&window,&row).empty(),"magic/item/status selections never announce row");
 fixture::game=game::Id::FF6;Node esper,list_view,scroll,stone,stone_view,icon,equipped=image(),master=image();Node stone_name=make_text(L"Alexander"),desc=make_text(L"Divine Judgment: Sears all enemies with a beam of holy light."),cost_label=make_text(L"MP Cost"),cost=make_text(L"45");List stone_rows{&stone};List data{&unit};set(esper,"view",&list_view);set(esper,"contentDataList",&data);set(list_view,"scrollView",&scroll);set(scroll,"rows",&stone_rows);set(stone,"view",&stone_view);set(stone_view,"IconTextView",&icon);set(icon,"nameText",&stone_name);set(stone_view,"equipIcon",&equipped);set(stone_view,"masterImage",&master);set(list_view,"descriptionText",&desc);set(list_view,"fixedMpText",&cost_label);set(list_view,"mpValueText",&cost);
 Node ability_window,status_controller,status_view,terra_name=make_text(L"Terra"),job=make_text(L"Magitek Elite"),current_mp=make_text(L"585"),max_mp=make_text(L"713"),hidden_level=make_text(L"61");hidden_level.active=false;
 owners["AbilityWindowController"]=&ability_window;set(ability_window,"magicStoneListController",&esper);set(ability_window,"statusController",&status_controller);set(status_controller,"view",&status_view);set(status_view,"nameText",&terra_name);set(status_view,"jobNameText",&job);set(status_view,"currentMpText",&current_mp);set(status_view,"maxMpText",&max_mp);set(status_view,"currentLevelText",&hidden_level);
 ffpr::ff6::o_ListSelectContent=&list_select;ffpr::ff6::d_ListSelectContent(&esper,0,0,nullptr);tick();expect(contains(L"Alexander, Equipped, Mastered, MP Cost 45")&&ui::review::active(),"Esper list reads equip marker through native virtual-scroll lookup and builds current panel buffer");spoken.clear();ui::review::details();expect(contains(L"Divine Judgment")&&contains(L"45")&&!contains(L"Terra")&&!contains(L"61"),"Esper-only list buffer excludes active-character header and retains current description/cost");
 Node details,detail_view,spell,spell_view,spell_icon,holy=make_text(L"Holy"),progress=make_text(L"100"),percent=make_text(L"%"),rate=make_text(L"2"),times=make_text(L"x"),progress_label=make_text(L"Progress"),rate_label=make_text(L"Acquisition Rate"),level_label=make_text(L"At level up:"),strength=make_text(L"Strength"),plus=make_text(L"+"),bonus=make_text(L"2");List spells{&spell};set(details,"view",&detail_view);set(detail_view,"nameText",&stone_name);set(detail_view,"targetEquipeedIcon",&equipped);set(detail_view,"masterImage",&master);set(detail_view,"skillLevelText",&progress_label);set(detail_view,"acquisitionRatetext",&rate_label);set(detail_view,"contents",&spells);set(spell,"view",&spell_view);set(spell_view,"magicNameText",&spell_icon);set(spell_icon,"nameText",&holy);set(spell_view,"skillLevelText",&progress);set(spell_view,"percentText",&percent);set(spell_view,"acquisitionRateText",&rate);set(spell_view,"multiplicationText",&times);set(spell_view,"masterImage",&master);set(detail_view,"levelUpText",&level_label);set(detail_view,"parameterNameText",&strength);set(detail_view,"parameterFixText",&plus);set(detail_view,"parameterValueText",&bonus);set(detail_view,"descriptionText",&desc);set(detail_view,"fixedMpText",&cost_label);set(detail_view,"mpValueText",&cost);
 set(ability_window,"magicStoneDetailsController",&details);
 ffpr::ff6::o_DetailsShow=&show;ffpr::ff6::o_DetailsUpdateView=&details_update;spoken.clear();ffpr::ff6::d_DetailsShow(&details,nullptr,nullptr,nullptr,nullptr);ffpr::ff6::d_DetailsUpdateView(&details,nullptr,nullptr,nullptr);tick();tick();spoken.clear();ui::review::details();expect(contains(L"Alexander, Equipped")&&contains(L"Holy, 100%, x 2, Mastered")&&contains(L"At level up: Strength + 2")&&contains(L"MP Cost 45"),"acquired page buffer includes every rendered progress/rate/bonus/description/cost field");spoken.clear();ui::review::next();expect(contains(L"Progress, Acquisition Rate"),"arrows browse the assembled page without changing native Esper selection");
 Node replacement;replacement.id=2;stone_name.value=L"Ramuh";spoken.clear();ffpr::ff6::d_DetailsUpdateView(&details,nullptr,&replacement,nullptr);tick();expect(contains(L"Ramuh, Equipped"),"native details highlight change announces the newly selected Esper and its equip marker");

 // Same cursor, two native pages: return must rebuild without a SelectContent callback.
 Node list_cursor;set(esper,"selectCursor",&list_cursor);
 ffpr::ff6::o_details_ready=&phase;
 esper.active=false;spoken.clear();ffpr::ff6::d_DetailsShow(&details,nullptr,nullptr,&replacement,nullptr);
 ffpr::ff6::d_DetailsUpdateView(&details,nullptr,&replacement,nullptr);
 ffpr::ff6::d_details_ready(&details,nullptr);tick();tick();spoken.clear();ui::review::details();
 expect(contains(L"Holy, 100%, x 2")&&!contains(L"Terra"),"details native ready phase rebuilds on Q with the unchanged Esper identity and excludes active character");
 on_details_show=[&]{ffpr::ff6::d_DetailsUpdateView(&details,nullptr,&replacement,nullptr);ffpr::ff6::d_details_ready(&details,nullptr);};
 spoken.clear();ffpr::ff6::d_DetailsShow(&details,nullptr,nullptr,&replacement,nullptr);tick();tick();
 expect(contains(L"Ramuh, Equipped")&&ui::review::active(),"Show cannot cancel its nested native state-entry UpdateView/ready publication");on_details_show={};
 ffpr::ff6::reset_esper_list_guard();details.active=false;esper.active=true;spoken.clear();
 ffpr::ff6::refresh_esper_list(&ability_window);tick();tick();spoken.clear();ui::review::details();
 expect(contains(L"Divine Judgment")&&!contains(L"Holy, 100%")&&!contains(L"Terra"),"return to list rebuilds current panel without moving the native cursor");
 // An unowned, rendered blank slot must replace stale/pending content, including I.
 Node blank_data,blank_row,blank_view,blank_icon,blank_name=make_text(L"");blank_data.owned=false;
 set(blank_row,"view",&blank_view);set(blank_view,"IconTextView",&blank_icon);set(blank_icon,"nameText",&blank_name);
 data.push_back(&blank_data);data.push_back(&blank_data);stone_rows.push_back(&blank_row);stone_rows.push_back(&blank_row);
 spoken.clear();ffpr::ff6::d_ListSelectContent(&esper,1,0,nullptr);tick();
 expect(spoken==std::vector<std::wstring>{L"Empty"},"unowned visible blank speaks Empty once");
 spoken.clear();ui::review::details();expect(spoken==std::vector<std::wstring>{L"Empty"}&&ffpr::ff6::esper_details_reannounce_text()==L"Empty","blank buffer and manual details contain only Empty, without hidden Esper name or prior footer");
 spoken.clear();ffpr::ff6::d_ListSelectContent(&esper,2,0,nullptr);tick();expect(spoken==std::vector<std::wstring>{L"Empty"},"moving to another blank slot remains an announcement event");
 spoken.clear();ffpr::ff6::d_ListSelectContent(&esper,0,0,nullptr);tick();expect(contains(L"Ramuh, Equipped"),"blank to former Esper speaks its name again despite the retained local text guard");
 ffpr::ff6::d_ListSelectContent(&esper,0,0,nullptr);ffpr::ff6::d_ListSelectContent(&esper,1,0,nullptr);tick();spoken.clear();ui::review::details();
 expect(spoken==std::vector<std::wstring>{L"Empty"},"obsolete queued named buffer cannot replace the latest blank");
 stone_rows[1]=nullptr;spoken.clear();ffpr::ff6::d_ListSelectContent(&esper,1,0,nullptr);tick();
 expect(spoken.empty()&&!ui::review::active()&&ffpr::ff6::esper_details_reannounce_text().empty(),"unready virtual row cancels stale information without prematurely announcing Empty");
 stone_rows[1]=&blank_row;ffpr::ff6::d_ListSelectContent(&esper,1,0,nullptr);tick();
 ffpr::ff6::reset_esper_list_guard();ffpr::ff6::refresh_esper_list(&ability_window);ffpr::ff6::reset_esper_list_guard();tick();expect(!ui::review::active(),"rapid page return then close cancels pending native reads");
 ffpr::ff6::reset_esper_list_guard();expect(!ui::review::active(),"closing Esper context cancels queued buffer work");expect(wrong==0,"renderer/native pointers remain exclusively on their owner thread");std::printf("Party/Esper actual-source renderer, targeting, formation and page buffers: %d failures\n",failed);return failed?1:0;
}
