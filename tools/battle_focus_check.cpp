// check-sources: text.cpp
// Actual battle target/command and quantity-list detours; native collections/views are fixtures.
#include <map>
#include <cstdio>
#include <cstdlib>
#include "../src/core/frame.cpp"
#define install command_install
#define s_install command_registration
#include "../src/core/battle/battle_command.cpp"
#undef install
#undef s_install
#define install quantity_install
#define s_install quantity_registration
#include "../src/core/battle/battle_quantity.cpp"
#undef install
#undef s_install
#define install item_install
#define s_install item_registration
#define o_SelectContent item_original
#define d_SelectContent item_select
#define announce_current_description item_description_action
#include "../src/core/battle/battle_item.cpp"
#undef announce_current_description
#undef d_SelectContent
#undef o_SelectContent
#undef install
#undef s_install
#define install save_install
#define s_install save_registration
#include "../src/core/system/save_load.cpp"
#undef install
#undef s_install
#define install field_magic_install
#define s_install field_magic_registration
#include "../src/games/ff3/magic_menu.cpp"
#undef install
#undef s_install
#define install ff2_magic_install
#define s_install ff2_magic_registration
#include "../src/games/ff2/magic_menu.cpp"
#undef install
#undef s_install
#define install formation_install
#define s_install formation_registration
#include "../src/games/ff6/status_list.cpp"
#undef install
#undef s_install
#define install choice_install
#define s_install choice_registration
#include "../src/core/dialogue/message_choice.cpp"
#undef install
#undef s_install
namespace fixture {
struct Node {void* list=nullptr;void* enemies=nullptr;void* players=nullptr;void* cursor=nullptr;void* data=nullptr;void* view=nullptr;bool alive=true,visible=true,machine=false;Node* parent=nullptr;int state=0,index=0;std::wstring name;std::map<std::string,void*> fields;std::map<std::string,std::wstring> texts;};
using List=std::vector<void*>;
std::vector<std::wstring> spoken;
std::map<std::string,bool> states;
std::map<std::string,void*> owners;
std::map<std::string,std::wstring> guard;
game::Id game=game::Id::FF6;
bool player_rendered=true, nested=false,menu_open=true,side_only=false,cursor_row_first=false;Node manager;int original_calls=0;
std::function<void()> on_item_init;
void item_entry(void*,const void*){++original_calls;if(on_item_init)on_item_init();}
void phase(void*,const void*){++original_calls;}
void show(void* p,bool visible,const void*){++original_calls;static_cast<Node*>(p)->view=p;static_cast<Node*>(p)->visible=visible;}
void command_cursor(void*,int32_t,const void*){++original_calls;}
void select(void*,void*,int32_t,const void*){++original_calls;}
void quantity(void*,void*,int32_t,const void*){++original_calls;}
void show_quantity(void*,void*,void*,void*,void*,const void*){++original_calls;}
void* managed_name(void*p,const void*){return p;}
bool is_open(void*,const void*){return menu_open;}
bool side_range(int,const void*){return side_only;}
void* party(void*p,const void*){return static_cast<Node*>(p)->list;}
void spell_cursor(void*,void*,bool,int32_t,const void*){++original_calls;}
void use_target_cursor(void*,void*,const void*){++original_calls;}
}
namespace core {Install::Install(const char*n,int(*f)(),int o,uint8_t g):name(n),fn(f),order(o),games(g){} }
namespace ref {
Class::Class(const char*n,const char*c,uint8_t g,const char*a):ns(n),name(c),games(g),assembly(a){}
Method::Method(Class&c,const char*n,int a):cls(c),name(n),argc(a){}Field::Field(Class&c,const char*n,const char*a):cls(c),name(n),alt(a){}
bool Class::is(void*p){return p!=nullptr;}il2cpp::Class*Class::get(){return nullptr;}
int32_t Field::get(){std::string n=name;if(n=="rangeType")return offsetof(fixture::Node,state);if(n=="isMachineState")return offsetof(fixture::Node,machine);if(n=="contentList")return offsetof(fixture::Node,list);if(n=="enemyDataList"||n=="<TargetEnamyList>k__BackingField")return offsetof(fixture::Node,enemies);if(n=="playerDataList"||n=="<TargetPlayerList>k__BackingField")return offsetof(fixture::Node,players);if(n=="selectCursor")return offsetof(fixture::Node,cursor);if(n=="listController")return offsetof(fixture::Node,list);if(n=="<TargetCommand>k__BackingField"||n=="<Data>k__BackingField"||n=="<CurrentData>k__BackingField"||n=="<CharacterData>k__BackingField")return offsetof(fixture::Node,data);return -1;}
void* instance(Class&){return &fixture::manager;}
void*Method::ptr(){if(std::string(name)=="IsPlayerSingleSideAll"||std::string(name)=="IsEnemySingleSideAll")return reinterpret_cast<void*>(&fixture::side_range);if(std::string(name)=="get_IsOpen")return reinterpret_cast<void*>(&fixture::is_open);if(std::string(name)=="get_PartyList")return reinterpret_cast<void*>(&fixture::party);if(std::string(name)=="get_Name")return reinterpret_cast<void*>(&fixture::managed_name);return std::string(name).find("MesId")!=std::string::npos?reinterpret_cast<void*>(&fixture::managed_name):nullptr;}il2cpp::Method*Method::get(){return nullptr;}
std::wstring str(void*p){return p?static_cast<fixture::Node*>(p)->name:L"";}std::wstring rd_str(void*p,Field&){return str(p);}void call_failed_note(const char*){std::abort();}
}
namespace hooks {bool attach_raw(ref::Method&,void*,void**){return false;}bool attach_addr(void*,const char*,void*,void**){return false;}void note_fault(const char*){std::abort();}}
namespace unity {void*transform(void*p){return p;}void*parent(void*p){return p?static_cast<fixture::Node*>(p)->parent:nullptr;}void*get_component_in_parent(void*p,ref::Class&){return parent(p);}void*get_component(void*p,ref::Class&){return p;}void*get_component_in_children(void*p,ref::Class&,bool){return p;}void*find_descendant(void*,const wchar_t*,int){return nullptr;}int child_count(void*){return 0;}void*child(void*,int){return nullptr;}std::wstring name(void*p){return p?static_cast<fixture::Node*>(p)->name:L"";}std::wstring ui_text(void*p){return name(p);}bool alive(void*p){return p&&static_cast<fixture::Node*>(p)->alive;}bool active_in_hierarchy(void*p){return alive(p)&&static_cast<fixture::Node*>(p)->visible;}int list_count(void*p){return p?static_cast<int>(static_cast<fixture::List*>(p)->size()):0;}void*list_at(void*p,int i){return p&&i>=0&&i<list_count(p)?(*static_cast<fixture::List*>(p))[i]:nullptr;}
bool enumerate_bounded(void*p,std::vector<void*>&out,int bound){out.clear();if(!p||list_count(p)>bound)return false;out=*static_cast<fixture::List*>(p);return true;}}
namespace displayed {bool visible(void*p){return unity::active_in_hierarchy(p);}bool container_visible(void*p){return visible(p);}void*field(void*p,const char*n){if(!p)return nullptr;auto&v=*static_cast<fixture::Node*>(p);if(std::string(n)=="view")return v.view;auto it=v.fields.find(n);return it==v.fields.end()?nullptr:it->second;}std::wstring text_field(void*p,const char*n){if(!visible(p))return {};auto&v=*static_cast<fixture::Node*>(p);auto it=v.texts.find(n);return it==v.texts.end()?L"":it->second;}std::wstring pair(void*p,const char*a,const char*b){auto x=text_field(p,a),y=text_field(p,b);return x.empty()||y.empty()?L"":x+L" "+y;}}
namespace speech {void request_load(){}}
namespace sound {void initialize(){}}
namespace displayed {
std::wstring text(void*p){return visible(p)?unity::ui_text(p):L"";}
std::wstring character(void*p){return text_field(p,"nameText");}std::wstring target(void*p){return visible(p)?character(field(p,"view")):L"";}
int target_count(void*p){int n=0;for(int i=0;i<unity::list_count(p);++i)if(!target(unity::list_at(p,i)).empty())++n;return n;}
void*cursor_row(void*,void*p){return fixture::cursor_row_first&&unity::list_count(p)>0?unity::list_at(p,0):nullptr;}
std::wstring formation_row(void*,void*){return {};}
std::wstring spell_row(void*p){return text_field(field(p,"view"),"spell");}
}
namespace ffpr {void say(std::wstring_view v,bool){fixture::spoken.emplace_back(v);}}
namespace ui {void*list_item(void*p,int i){return unity::list_at(p,i);}int state_tag(void*p,ref::Field&){return p?static_cast<fixture::Node*>(p)->state:-1;}int cursor_index(void*p){return p?static_cast<fixture::Node*>(p)->index:-1;}std::wstring menu_position(const std::wstring&s,int i,int n){return s+L" ("+std::to_wstring(i+1)+L" of "+std::to_wstring(n)+L")";}}
namespace game {Id current(){return fixture::game;}}
namespace settings {bool get_bool(const char*,bool d){return d;}}
namespace modtext {std::wstring T(std::wstring_view v){return std::wstring(v);}std::wstring TF(std::wstring_view v,std::initializer_list<std::wstring>a){return text::format(v,a);}}
namespace menu_state {void set_active(const char*n,bool v){fixture::states[n]=v;}bool is_active(const char*n){return fixture::states[n];}void reset(const char*n){fixture::states[n]=false;}void set_active_exclusive(const char*n){fixture::states.clear();fixture::states[n]=true;}void register_reset(const char*,std::function<void()>){}}
namespace ffpr::ff2 {void set_active_exclusive(const char*n){menu_state::set_active_exclusive(n);}}
namespace dedup {bool text(const char*k,std::wstring_view v,Scope){if(fixture::guard[k]==v)return false;fixture::guard[k]=v;return true;}bool index(const char*,long long,Scope){return true;}void reset(const char*k){fixture::guard.erase(k);}}
namespace field::cache {void*get(ref::Class&c){return fixture::owners[c.name];}void register_object(ref::Class&c,void*p){fixture::owners[c.name]=p;}void clear(ref::Class&c){fixture::owners.erase(c.name);}}
namespace actions {void define(const char*,Fn){}}
namespace queries {void define(const char*,std::function<bool()>){ }void define_text(const char*,std::function<std::wstring()>){ }bool is(const char*){return false;}}
namespace cursor {void add_suppressor(const char*){}}
namespace log_ {void write(const char*,...){}}
namespace ffpr::system {void clear_other_menu_states(const std::string&){}bool is_field_state(){return false;}std::wstring ability_command_name(int){return L"Use";}void announce_command(const std::wstring&s,int,int){ffpr::say(s);}}
namespace ffpr::menus {std::wstring spell_description(void*){return L"Removes poison.";}std::wstring spell_name(void*p){return p?static_cast<fixture::Node*>(p)->name:L"";}void set_last_announced_ability(void*){}}

namespace ffpr::dialogue {std::wstring read_focused_button(void*,ref::Field&,ref::Field&){return {};}void read_popup_when_ready(void*,const char*,std::function<std::wstring()>,std::function<bool()>){ }
namespace popup_state {void*active_popup(){return nullptr;}void set_active(const char*,void*,ref::Field*,bool){}void clear(){}}}
namespace il2cpp {void*method_pointer(Method*){return nullptr;}}
namespace ffpr::battle {
bool formation_first_turn(){return false;}
void observe_target_party(void*){}
void clear_other_menu_states(const char*){}
bool state_active(const char*k){return menu_state::is_active(k);}void state_set(const char*k,bool v){menu_state::set_active(k,v);}void set_target_selection_active(bool v){menu_state::set_active(menu_state::BATTLE_TARGET,v);}void set_last_selected_command_index(int){}void set_current_actor(void*){}
int character_id(void*p){return p?static_cast<fixture::Node*>(p)->index:-1;}std::wstring character_name(void*p){return static_cast<fixture::Node*>(p)->name;}std::wstring command_mes_id(void*p){return static_cast<fixture::Node*>(p)->name;}std::wstring enemy_name(void*p){return p?static_cast<fixture::Node*>(p)->name:L"";}std::wstring displayed_player(void*p){return fixture::player_rendered&&p?static_cast<fixture::Node*>(p)->name+L", HP 30/30":L"";}std::wstring optional_enemy_hp(void*){return {};}
bool blank(const std::wstring&s){return s.empty();}std::wstring message(const std::wstring&s,bool){return s;}std::wstring strip_icon(std::wstring_view s){return std::wstring(s);}il2cpp::Method*find_method_param_type(il2cpp::Class*,const char*,int,int,const char*){return nullptr;}
}
int main(){using namespace fixture;namespace b=ffpr::battle;int failures=0;auto expect=[&](bool ok,const char*why){if(!ok){++failures;std::printf("FAIL %s\n",why);}};
 b::o_SelectPlayer=b::o_SelectEnemy=&select;b::o_ShowWindow=&show;b::o_EnemysInit=b::o_PlayerInit=b::o_PlayerAll=b::o_EnemyAll=b::o_All=b::o_SidePlayerAll=b::o_SideEnemyAll=&phase;b::o_SetCursor=&command_cursor;
 Node target,cursor,enemy_a,enemy_b,ally,command,content,attack;enemy_a.name=enemy_b.name=L"Rukh";ally.name=L"Rydia";attack.name=L"Attack";content.data=&attack;
 List enemies{&enemy_a,&enemy_b},allies{&ally},commands{&content};target.cursor=&cursor;target.enemies=&enemies;target.players=&allies;target.view=&target;command.list=&commands;
 b::d_ShowWindow(&target,true,nullptr);b::d_EnemysInit(&target,nullptr);for(int i=0;i<120;++i)b::d_SelectEnemy(&target,&enemies,0,nullptr);
 expect(spoken.size()==1&&spoken[0]==L"Rukh A (1 of 2)","FF6 continuous native target refresh speaks initial focus only once");
 b::d_SelectEnemy(&target,&enemies,1,nullptr);b::d_SelectEnemy(&target,&enemies,0,nullptr);expect(spoken.size()==3,"distinct identical-name targets and returning to prior target remain audible");
 // FF4/FF5 Items may be command 5, not command 3. Native graphics own readiness.
 for(int n=1;n<=6;++n){fixture::game=static_cast<game::Id>(n);Node item_ctl,item_view,root,item_row,row_view,icon,item_cursor;List rows{&item_row};
 item_ctl.fields["selectCursor"]=&item_cursor;item_ctl.view=&item_view;item_view.fields["rootObject"]=&root;item_ctl.fields["contentList"]=&rows;item_ctl.fields["displayDataList"]=&rows;item_row.view=&row_view;row_view.fields["IconTextView"]=&icon;icon.texts["nameText"]=L"Potion";row_view.texts["countText"]=L"1";item_view.texts["descriptionText"]=L"Restores HP.";item_cursor.parent=&item_row;
 b::item_original=&quantity;b::o_use_init=b::o_close=&phase;spoken.clear();b::g_command_turn_ready=false;b::g_command_reannounce_pending=false;b::item_select(&item_ctl,&item_cursor,0,nullptr);
 expect(spoken.size()==1&&spoken[0].find(L"Potion, 1")==0&&b::g_command_reannounce_pending&&b::g_command_turn_ready,"all-five rendered battle item entry arms command return regardless of command index");
 spoken.clear();b::o_use_init=&item_entry;on_item_init=[&]{root.visible=false;b::item_select(&item_ctl,&item_cursor,0,nullptr);root.visible=true;};b::d_use_init(&item_ctl,nullptr);expect(spoken.size()==1&&spoken[0].find(L"Potion, 1")==0,"native Items entry reads its first focus once after enabling the root");on_item_init={};
 if(n==6){item_ctl.machine=true;item_ctl.fields["machineContentList"]=&rows;icon.texts["nameText"]=L"Auto Crossbow";row_view.texts["countText"].clear();spoken.clear();b::item_select(&item_ctl,&item_cursor,0,nullptr);expect(spoken.size()==1&&spoken[0].find(L"Auto Crossbow")==0,"shared FF6 reader retains rendered Tools names without hidden item counts");}
 root.visible=false;spoken.clear();b::item_select(&item_ctl,&item_cursor,0,nullptr);expect(spoken.empty(),"hidden item teardown cannot steal command context");root.visible=true;b::d_close(&item_ctl,nullptr);expect(!states[menu_state::BATTLE_ITEM],"item close releases its menu context");
 }
 fixture::game=game::Id::FF6;
 b::reset_target_focus();spoken.clear();player_rendered=false;b::d_PlayerInit(&target,nullptr);frame::run_tick();expect(spoken.empty(),"ally entry waits for the displayed party row");player_rendered=true;frame::run_tick();for(int i=0;i<3;++i)frame::run_tick();expect(spoken.size()==1&&spoken[0].find(L"Rydia")==0,"delayed party rendering publishes the initially focused ally once");
 b::reset_target_focus();spoken.clear();b::d_PlayerInit(&target,nullptr);b::d_ShowWindow(&target,false,nullptr);spoken.clear();for(int i=0;i<12;++i)frame::run_tick();expect(spoken.empty(),"target close cancels initial-read continuations");
 for(int n=1;n<=6;++n){fixture::game=static_cast<game::Id>(n);spoken.clear();b::command_reset_state();b::d_ShowWindow(&target,true,nullptr);b::d_PlayerInit(&target,nullptr);expect(spoken.size()==1&&spoken[0].find(L"Rydia")==0,"shared single-ally entry is audible in every game");}
 for(int n=1;n<=6;++n){fixture::game=static_cast<game::Id>(n);spoken.clear();b::d_EnemyAll(&target,nullptr);b::d_PlayerAll(&target,nullptr);b::d_All(&target,nullptr);expect(spoken==std::vector<std::wstring>{L"All enemies",L"All allies",L"All allies and enemies"},"all-six whole-side entry is audible with no cursor or movement callback");}
 expect(b::mEnemyAll.cls.games==game::kAll && b::mPlayerAll.cls.games==game::kAll,"group targeting hook registration covers all six games");
 fixture::game=game::Id::FF6;for(bool restricted:{false,true}){side_only=restricted;spoken.clear();b::d_SidePlayerAll(&target,nullptr);b::d_SideEnemyAll(&target,nullptr);expect(spoken==std::vector<std::wstring>{restricted?L"All allies on this side":L"All allies",restricted?L"All enemies on this side":L"All enemies"},"FF6 separate side-entry phases announce the actual selected group");}
 List oversized(65,&enemy_a);spoken.clear();expect(!b::announce_enemy_target(&oversized,0)&&spoken.empty(),"oversized target snapshot is rejected without partial speech");
 b::o_SelectContent=&quantity;b::o_ShowUseSelect=&show_quantity;
 Node spell,spell_view,spell_icon,owned,magic_cursor,magic;spell.data=&owned;spell.view=&spell_view;owned.name=L"Cura";spell_view.fields["iconText"]=&spell_icon;spell_icon.texts["nameText"]=L"Cura";spell_view.texts={{"fixText",L"MP"},{"valueText",L"9"}};magic.view=&magic;List spells{&spell};magic.list=&spells;
 for(int n:{4,5}){fixture::game=static_cast<game::Id>(n);spoken.clear();b::command_reset_state();b::d_ShowWindow(&target,false,nullptr);b::d_ShowUseSelect(&magic,nullptr,nullptr,nullptr,nullptr,nullptr);b::d_SelectContent(&magic,&magic_cursor,0,nullptr);expect(b::g_command_turn_ready&&menu_state::is_active(menu_state::BATTLE_MAGIC),"quantity list restores command input ownership after target teardown");b::d_SetCursor(&command,0,nullptr);frame::run_tick();expect(spoken.size()==2&&spoken.back().find(L"Attack")==0,"FF4/FF5 magic cancel-return announces focused battle command");
 b::d_ShowUseSelect(&magic,nullptr,nullptr,nullptr,nullptr,nullptr);b::d_SelectContent(&magic,&magic_cursor,0,nullptr);expect(spoken.back().find(L"Cura, MP 9")==0,"spell list reopen reads displayed native cost and first focus");}
 namespace fm=ffpr::ff3;fixture::game=game::Id::FF3;
 Node field_window,field_list,field_cursor,poisona,poisona_view;List field_spells{&poisona};field_window.list=&field_list;field_list.list=&field_spells;field_list.cursor=&field_cursor;poisona.data=&owned;poisona.view=&poisona_view;
 fm::o_UseListInit=fm::o_WindowUpdate=&phase;fm::o_ListSetCursor=&spell_cursor;
 menu_state::reset(menu_state::MAGIC_MENU);spoken.clear();fm::d_ListSetCursor(&field_list,&field_cursor,false,0,nullptr);expect(spoken.empty(),"native list cursor before focus does not speak during pre-entry");
 fm::d_UseListInit(&field_window,nullptr);field_window.state=1;fm::d_WindowUpdate(&field_window,nullptr);expect(spoken.empty()&&fm::g_pending_spell_window,"initial spell waits for native rendered row readiness");
 poisona_view.texts["spell"]=L"Poisona, Lv. 1, MP 25/25";fm::d_WindowUpdate(&field_window,nullptr);fm::d_WindowUpdate(&field_window,nullptr);expect(spoken.size()==1&&spoken[0].find(L"Poisona, Lv. 1, MP 25/25")==0,"FF3 Use entry speaks its first ready spell exactly once");
 fm::g_pending_spell_window=&field_window;fm::on_set_next_state(&field_window,7);spoken.clear();fm::d_WindowUpdate(&field_window,nullptr);expect(spoken.empty()&&!fm::g_pending_spell_window,"command return cancels pending spell read");
 fm::g_pending_spell_window=&field_window;fm::on_set_next_state(&field_window,2);field_window.state=2;fm::d_WindowUpdate(&field_window,nullptr);expect(spoken.empty()&&!fm::g_pending_spell_window,"spell target entry cancels pending list speech before the target owns focus");
 Node empty_first,terra,terra_view,target_cursor,field_use;empty_first.visible=false;terra.data=&ally;terra.view=&terra_view;terra_view.texts["nameText"]=L"Terra";List field_members{&empty_first,&terra};field_use.list=&field_members;target_cursor.parent=&terra;
 spoken.clear();fm::on_use_set_cursor(&field_use,&target_cursor);fm::on_use_set_cursor(&field_use,&target_cursor);expect(spoken==std::vector<std::wstring>{L"Terra (1 of 1)",L"Terra (1 of 1)"},"compacted field target is read from native cursor parent and deliberate arrows repeat it");
 namespace f2=ffpr::ff2;Node target_window,target_list,target_cursor2,target_data[4],target_views[4],target_rows[4];List target_contents;
 target_window.state=2;target_window.view=&target_window;owners["AbilityWindowController"]=&target_window;target_cursor2.parent=&target_rows[0];
 const wchar_t* target_names[]={L"Firion",L"Maria",L"Guy",L"Leon"};for(int i=0;i<4;++i){target_data[i].name=target_names[i];target_rows[i].data=&target_data[i];target_rows[i].view=&target_views[i];target_views[i].texts["nameText"]=target_names[i];target_contents.push_back(&target_rows[i]);}
 target_list.list=&target_contents;f2::o_UseSetCursor=&use_target_cursor;cursor_row_first=true;spoken.clear();
 for(int state:{2,8}){target_window.state=state;for(int index:{0,1,2,3,2,1,0}){target_cursor2.index=index;f2::d_UseSetCursor(&target_list,&target_cursor2,nullptr);}}
 expect(spoken.size()==14,"FF2 target cursor reads every actual row in both target states, including backtracking");
 for(int state:{2,8})for(int index=0;index<4;++index){const size_t n=(state==2?0:7)+index;expect(spoken[n]==std::wstring(target_names[index])+L" ("+std::to_wstring(index+1)+L" of 4)","FF2 target cursor selects the row at native cursor.Index despite first-row ancestry");}
 target_window.state=2;spoken.clear();target_cursor2.index=-1;f2::d_UseSetCursor(&target_list,&target_cursor2,nullptr);target_cursor2.index=4;f2::d_UseSetCursor(&target_list,&target_cursor2,nullptr);
 target_cursor2.index=1;target_views[1].visible=false;f2::d_UseSetCursor(&target_list,&target_cursor2,nullptr);target_views[1].visible=true;
 target_cursor2.index=1;target_rows[1].data=nullptr;f2::d_UseSetCursor(&target_list,&target_cursor2,nullptr);target_rows[1].data=&target_data[1];
 target_cursor2.index=1;target_views[1].texts["nameText"].clear();f2::d_UseSetCursor(&target_list,&target_cursor2,nullptr);target_views[1].texts["nameText"]=target_names[1];
 target_cursor2.index=2;target_contents[2]=nullptr;f2::d_UseSetCursor(&target_list,&target_cursor2,nullptr);target_contents[2]=&target_rows[2];
 target_cursor2.index=0;target_list.visible=false;f2::d_UseSetCursor(&target_list,&target_cursor2,nullptr);target_list.visible=true;
 target_window.state=1;f2::d_UseSetCursor(&target_list,&target_cursor2,nullptr);target_window.state=2;
 List too_many(10,&target_rows[0]);target_list.list=&too_many;target_cursor2.index=0;f2::d_UseSetCursor(&target_list,&target_cursor2,nullptr);target_list.list=&target_contents;
 expect(spoken.empty(),"FF2 target cursor stays silent for hidden, invalid, out-of-range, hole and oversized-party rows");cursor_row_first=false;
 namespace form=ffpr::ff6;Node empty_slot;empty_slot.view=&empty_slot;List formation{&empty_slot,&terra,&empty_slot,&empty_slot};target_cursor.parent=&empty_slot;spoken.clear();form::on_select_content(&formation,2,&target_cursor);form::on_select_content(&formation,3,&target_cursor);expect(spoken==std::vector<std::wstring>{L"Empty",L"Empty"},"consecutive highlighted empty formation slots are distinct audible events");
 Node save,save_view,save_data,cecil,tellah;save.view=&save_view;cecil.name=L"Cecil";tellah.name=L"Tellah";List saved_party{&cecil,nullptr,&tellah};save_data.list=&saved_party;
 save_view.texts={{"slotNameText",L"Quick Save"},{"areaNameText",L"Underground Waterway"},{"floorNameText",L"South Entrance"},{"charaNameText",L"Cecil"},{"fixedLevelText",L"Lv."},{"levelText",L"12"},{"timeStampDate",L"03/02/2026"},{"timeStampTime",L"13:58"},{"fixedTimeText",L"Time"},{"hourText",L"03"},{"timeSeparateText",L":"},{"minuteText",L"03"}};
 for(int n=1;n<=6;++n){fixture::game=static_cast<game::Id>(n);auto value=ffpr::system::read_from_controller(&save,&save_data,1);expect(value==L"Quick Save, 03/02/2026, 13:58, Cecil, Lv. 12, Underground Waterway, South Entrance, Time 03:03. Party: Cecil, Tellah","all-six save slot reader preserves displayed fields and full-party exception across holes");}
 save_view.texts["emptyText"]=L"Save data is corrupted.";expect(ffpr::system::read_from_controller(&save,&save_data,0)==L"Quick Save: Save data is corrupted.","corrupted save row excludes stale hidden details and party");save_view.texts.erase("emptyText");save_view.texts.erase("charaNameText");save_view.texts.erase("levelText");auto slot=ffpr::system::read_from_controller(&save,&save_data,1);expect(slot.find(L"Lv.")==slot.npos&&slot.find(L"Underground Waterway")!=slot.npos,"missing rendered leader and level do not hide quicksave location or invent values");
 Node choice,choice_cursor,choice_row,choice_view;List choices{&choice_row};choice.cursor=&choice_cursor;choice.list=&choices;choice_row.view=&choice_view;choice_view.texts["text"]=L"Take the wheel";
 ffpr::dialogue::o_show=[](void*,int32_t,const void*){};ffpr::dialogue::o_close=&phase;spoken.clear();ffpr::dialogue::d_show(&choice,0,nullptr);frame::run_tick();expect(spoken.size()==1&&spoken[0].find(L"Take the wheel")==0,"native dialogue choice announces default focus after text population");spoken.clear();ffpr::dialogue::d_show(&choice,0,nullptr);ffpr::dialogue::d_close(&choice,nullptr);frame::run_tick();expect(spoken.empty(),"choice close cancels pending initial focus");
 std::printf("Battle/field magic, save rows, formation and choice focus: %d failures\n",failures);return failures?1:0;
}
