// check-sources: text.cpp displayed_values.cpp ui/navigation_buffer.cpp
// Real menu, party, warning and chest readers with fake native graphics/owners.
#include <map>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include "../src/core/frame.cpp"
#include "../src/core/background.cpp"
#include "../src/core/ui/review_buffer.cpp"
#define s_install equip_registration
#define install equip_install
#include "../src/core/menus/equip_menu.cpp"
#undef install
#undef s_install
#define s_install extra_registration
#define install extra_install
#include "../src/core/menus/item_equipment.cpp"
#undef install
#undef s_install
#include "../src/core/battle/battle_party.cpp"
#include "../src/core/field/condition_feedback.cpp"
#include "../src/core/field/navigable_entity.cpp"
#define kShopController tracked_shop_class
#define kShopInfoController tracked_shop_info_class
#define STATE_SELECT_COMMAND tracked_command_state
#define STATE_SELECT_PRODUCT tracked_product_state
#define STATE_SELECT_SELL_ITEM tracked_sell_state
#define install tracker_install
#define s_install tracker_registration
#include "../src/core/shop/shop_tracker.cpp"
#undef kShopController
#undef kShopInfoController
#undef STATE_SELECT_COMMAND
#undef STATE_SELECT_PRODUCT
#undef STATE_SELECT_SELL_ITEM
#undef install
#undef s_install
#define install shop_install
#define s_install shop_registration
#include "../src/core/shop/shop_patches.cpp"
#undef install
#undef s_install

namespace fixture {
struct Node {int effect=0;bool has=true;void* animation=nullptr;void* list=nullptr;void* view=nullptr;void* description=nullptr;Node* parent=nullptr;bool alive=true,active=true,enabled=true;int state=0,id=0;std::wstring name;std::map<std::string,void*> fields;std::map<std::string,std::wstring> text;};
using List=std::vector<void*>;
std::map<std::string,void*> owners;std::map<std::string,actions::Fn> actions;std::map<std::string,bool> states, bools;std::map<std::string,std::function<void()>> resets;
std::vector<std::wstring> spoken;int delta=0,native_calls=0,wrong_thread=0;bool battle=false;
DWORD thread=GetCurrentThreadId();game::Id game=game::Id::FF2;
void native(){if(GetCurrentThreadId()!=thread)++wrong_thread;}
void tick(){frame::run_tick();DWORD end=GetTickCount()+5000;while(ui::review::g_build_pending && GetTickCount()<end){std::this_thread::yield();frame::run_tick();}}
bool contains(std::wstring_view s){for(const auto&x:spoken)if(x.find(s)!=x.npos)return true;return false;}
std::function<void(bool)> on_parameter;std::function<void(int)> on_equipment_page;
void phase(void*,const void*){++native_calls;}
void parameter_flag(void*,bool p,const void*){++native_calls;if(on_parameter)on_parameter(p);}
void equipment_page(void*,int p,const void*){++native_calls;if(on_equipment_page)on_equipment_page(p);}
void active(void*,bool,const void*){++native_calls;}
void description(void*,void*,bool,const void*){++native_calls;}
void difference(void*,void*,void*,int,const void*){++native_calls;}
void slot(void*,void*,const void*){++native_calls;}
std::function<void()> on_select;
void select(void*,void*,int,const void*){++native_calls;if(on_select)on_select();}
void update(void*,void*,bool,const void*){++native_calls;}
void attribute(void*,int32_t,const void*){++native_calls;}
void focus(void*,void*,bool,const void*){++native_calls;}
bool damage(void*,void*,void*,int,int,const void*){++native_calls;return true;}
}
namespace core {Install::Install(const char*n,int(*f)(),int o,uint8_t g):name(n),fn(f),order(o),games(g){} }
namespace ref {
Class::Class(const char*n,const char*c,uint8_t g,const char*a):ns(n),name(c),games(g),assembly(a){}
Method::Method(Class&c,const char*n,int a):cls(c),name(n),argc(a){} Field::Field(Class&c,const char*n,const char*a):cls(c),name(n),alt(a){}
bool Class::is(void*p){fixture::native();return p!=nullptr;}void*Method::ptr(){fixture::native();return nullptr;}il2cpp::Method*Method::get(){return nullptr;}
int32_t Field::get(){fixture::native();std::string n=name;if(n=="<IsActive>k__BackingField")return offsetof(fixture::Node,enabled);if(n=="<ContentId>k__BackingField")return offsetof(fixture::Node,id);if(n=="iconTextView")return offsetof(fixture::Node,animation);if(n=="shopListItemContentView")return offsetof(fixture::Node,view);if(n=="nameText"||n=="priceText")return offsetof(fixture::Node,description);if(n=="attributeType")return offsetof(fixture::Node,id);if(n=="contentList"||n=="contentDataList"||n=="targetSelectContentList")return offsetof(fixture::Node,list);if(n=="view")return offsetof(fixture::Node,view);if(n=="descriptionText")return offsetof(fixture::Node,description);if(n=="currentEffect")return offsetof(fixture::Node,effect);if(n=="animationController")return offsetof(fixture::Node,animation);return -1;}
std::wstring rd_str(void*p,Field&){fixture::native();return p?static_cast<fixture::Node*>(p)->name:L"";}void call_failed_note(const char*){std::abort();}
}
namespace hooks {bool attach_raw(ref::Method&,void*,void**){return false;}void note_fault(const char*){std::abort();}}
namespace actions {void define(const char*n,Fn f){fixture::actions[n]=std::move(f);}bool run(const char*n){auto i=fixture::actions.find(n);if(i==fixture::actions.end())return false;i->second();return true;}bool defined(const char*n){return fixture::actions.count(n)!=0;}}
namespace queries {void define(const char*,std::function<bool()>){}void define_text(const char*,std::function<std::wstring()>){}bool is(const char*n){return std::string(n)=="GameStatePatches.IsFieldPlayerState";} }
namespace events {void on(Kind,Handler){} }
namespace settings {bool get_bool(const char*k,bool d){auto i=fixture::bools.find(k);return i==fixture::bools.end()?d:i->second;}int get_int(const char*k,int d){return std::string(k)=="StatDeltaMode"?fixture::delta:d;} }
namespace game {Id current(){return fixture::game;}const Flags&flags(){static Flags f;return f;} }
namespace modtext {std::wstring T(std::wstring_view s){return std::wstring(s);}std::wstring TF(std::wstring_view s,std::initializer_list<std::wstring>a){return text::format(s,a);}const std::wstring&lang(){static std::wstring s=L"en";return s;}LocaleScope::LocaleScope(std::wstring){}LocaleScope::~LocaleScope(){} }
namespace ffpr {void say(std::wstring_view s,bool){fixture::spoken.emplace_back(s);}void say_parts(std::vector<std::wstring>p,bool){fixture::spoken.emplace_back(text::join(p,L". "));}void say_sequence(std::vector<std::wstring>p,bool){say_parts(std::move(p),true);} }
namespace ui {std::wstring menu_position(const std::wstring&s,int,int){return s;}int cursor_index(void*p){return static_cast<fixture::Node*>(p)->id;}int state_tag(void*p,ref::Field&){return p?static_cast<fixture::Node*>(p)->state:0;} }
namespace unity {bool alive(void*p){fixture::native();return p&&static_cast<fixture::Node*>(p)->alive;}bool active_in_hierarchy(void*p){return alive(p)&&static_cast<fixture::Node*>(p)->active&&(!static_cast<fixture::Node*>(p)->parent||active_in_hierarchy(static_cast<fixture::Node*>(p)->parent));}void*transform(void*p){fixture::native();return p;}void*get_component_in_parent(void*p,ref::Class&){fixture::native();return p?static_cast<fixture::Node*>(p)->parent:nullptr;}Vec3 local_position(void*){fixture::native();return {};}Vec3 position(void*){fixture::native();return {};}std::wstring ui_text(void*p){return p?static_cast<fixture::Node*>(p)->name:L"";}std::wstring name(void*p){return p?static_cast<fixture::Node*>(p)->name:L"";}int list_count(void*p){fixture::native();return p?static_cast<int>(static_cast<fixture::List*>(p)->size()):0;}void*list_at(void*p,int i){fixture::native();return p&&i>=0&&i<list_count(p)?(*static_cast<fixture::List*>(p))[i]:nullptr;} }
namespace displayed {
bool container_visible(void*p){return unity::active_in_hierarchy(p);}bool visible(void*p){return container_visible(p)&&static_cast<fixture::Node*>(p)->enabled;}bool world_visible(void*p){return visible(p);}void*image_sprite(void*p){return visible(p)?p:nullptr;}
void*field(void*p,const char*n){fixture::native();if(!p)return nullptr;auto&o=*static_cast<fixture::Node*>(p);if(std::string(n)=="view")return o.view;if(std::string(n)=="contentList")return o.list;auto i=o.fields.find(n);return i==o.fields.end()?nullptr:i->second;}
std::wstring text(void*p){return visible(p)?static_cast<fixture::Node*>(p)->name:L"";}std::wstring text_field(void*p,const char*n){fixture::native();if(!container_visible(p))return {};auto&node=*static_cast<fixture::Node*>(p);auto f=node.fields.find(n);if(f!=node.fields.end())return text(f->second);auto i=node.text.find(n);return i==node.text.end()?L"":i->second;}
std::vector<std::wstring>texts(void*){return {};}std::wstring comparison_arrow(void*){return {};}std::wstring character(void*p){return text_field(p,"nameText")+L", HP "+text_field(p,"currentHpText")+L"/"+text_field(p,"maxHpText")+L", MP "+text_field(p,"currentMpText");}std::wstring conditions(void*){return {};}
}
namespace field::cache {void*get(ref::Class&c){auto i=fixture::owners.find(c.name);return i==fixture::owners.end()?nullptr:i->second;}void register_object(ref::Class&c,void*p){fixture::owners[c.name]=p;}void clear(ref::Class&c){fixture::owners.erase(c.name);} }
namespace field {bool live_entity(void*p){return p!=nullptr;}bool treasure_opened(void*){return false;}bool entity_is_present(void*){return true;}std::wstring get_message(const std::wstring&s,bool){return s;} }
namespace field::r {ref::Class kFieldMapObjectDefault{"Last.Entity.Field","FieldMapObjectDefault"};ref::Method mMapObjectGetCanAction{kFieldMapObjectDefault,"get_CanAction",0};}
namespace field {bool call_virtual_bool(ref::Method&,void* p){fixture::native();return p&&static_cast<fixture::Node*>(p)->enabled;}}
namespace il2cpp {Class*object_class(void*){return nullptr;} }
namespace ffpr::menus::detail {bool auto_detail(){return ui::prefs::auto_detail_enabled();}bool blank(std::wstring_view s){return s.empty();}std::wstring strip_icon(std::wstring_view s){return std::wstring(s);}void clear_other_menu_states(const char*){}bool shop_menu_active(){return false;}void shop_clear_for_equipment_submenu(){}void shop_set_menu_active(){} }
namespace ffpr::battle {bool in_battle(){return fixture::battle;}int character_id(void*p){return p?static_cast<fixture::Node*>(p)->id:-1;}void*player_owned_character(void*p){return displayed::field(p,"owned");}void*current_actor(){return nullptr;}std::wstring unit_name(void*p){return p?static_cast<fixture::Node*>(p)->name:L"";} }
namespace menu_state {void reset_all(){}void set_active(const char*n,bool b){fixture::states[n]=b;}bool is_active(const char*n){return fixture::states[n];}void reset(const char*n){fixture::states[n]=false;auto i=fixture::resets.find(n);if(i!=fixture::resets.end())i->second();}void register_reset(const char*n,std::function<void()>f){fixture::resets[n]=std::move(f);} }
namespace dedup {bool index(const char*,long long,Scope){return true;}void reset(const char*){} }
namespace ffpr::shop {bool live(void*p,ref::Class&){return unity::alive(p);}void*master_get_data(ref::Class&,int){return nullptr;}std::wstring strip_icon_markup(std::wstring_view s){return std::wstring(s);}void magic_reset_state(){} }
namespace input {void add_binding(Key,Mod,Context,const char*,const char*){} }
namespace log_ {void write(const char*,...){} }
namespace sound {void initialize(){} }
namespace speech {void request_load(){} }
int main(){background::prepare();using namespace fixture;using namespace ffpr::menus;int failures=0;auto expect=[&](bool b,const char*s){if(!b){++failures;std::printf("FAIL %s\n",s);}};
 equip_install();o_EquipWindowSetActive=&active;o_EquipInfoSelectContent=&slot;o_EquipSelectSelectContent=&select;o_difference=&difference;
 Node window,info,select,desc,desc_view,panel,cursor,slot_view,icon,candidate,status,status_view,stat,row,slot_label,slot_option,slot_both;
 List slots{&slot_view},candidates{&candidate},stats{&row};
 slot_view.parent=&info;icon.parent=&slot_view;slot_view.fields["equipTextView"]=&icon;icon.text["nameText"]=L"Dagger";slot_label.parent=&slot_view;slot_option.parent=&slot_view;slot_both.parent=&slot_view;slot_label.name=L"R. Hand";slot_view.fields["partText"]=&slot_label;slot_view.fields["optionText"]=&slot_option;slot_view.fields["bothHandsText"]=&slot_both;info.list=&slots;info.view=&info;select.view=&select;candidate.name=L"Spear";Node candidate_view,candidate_row;candidate_row.view=&candidate_view;cursor.parent=&candidate_row;candidate_view.text["countText"]=L"4";select.list=&candidates;select.fields["statusController"]=&status;
 row.view=&stat;stat.text={{"categoryText",L"Attack"},{"beforValueText",L"26"},{"afterValueText",L"30"}};status.list=&stats;status.view=&status_view;desc.view=&desc_view;desc_view.description=&panel;panel.name=L"Attack +7 Accuracy +65";window.fields={{"statusController",&status},{"descriptionController",&desc},{"infoWindowController",&info},{"selectWindowController",&select}};
 d_EquipWindowSetActive(&window,true,nullptr);
 for(int n=1;n<=6;++n){fixture::game=static_cast<game::Id>(n);for(bool auto_detail:{false,true}){bools["AutoDetail"]=auto_detail;spoken.clear();d_EquipInfoSelectContent(&info,&cursor,nullptr);tick();expect(spoken.size()==1&&spoken[0]==(auto_detail?L"R. Hand: Dagger. Attack +7 Accuracy +65":L"R. Hand: Dagger"),"slot speaks visible label, name and optional current info panel");spoken.clear();d_difference(&status,nullptr,nullptr,0,nullptr);tick();expect(spoken.empty()&&!ui::review::active(),"slot native stat updates never create or speak comparison deltas");panel.name=L"Attack +11 Accuracy +55";spoken.clear();d_EquipSelectSelectContent(&select,&cursor,0,nullptr);tick();expect(spoken[0]==L"Spear, 4"&&contains(L"Attack 26 to 30")==auto_detail,"candidate Autodetail controls full comparison only");spoken.clear();ui::review::next();expect(contains(L"Attack 26 to 30"),"arrows review assembled comparisons");panel.name=L"A metal-tipped spear.";spoken.clear();ui::review::details();expect(spoken.size()==1&&spoken[0]==panel.name,"I reads live toggled description, independently of comparison buffer");panel.name=L"Attack +7 Accuracy +65";}}
 for(int n=1;n<=6;++n){fixture::game=static_cast<game::Id>(n);for(bool autodetail:{false,true}){bools["AutoDetail"]=autodetail;
  const std::wstring labels[]{L"R. Hand",L"L. Hand",L"Arms",L"Head",L"Body"};
  const std::wstring names[]{L"Mythril Sword",L"Mythril Sword",L"Mythril Gloves",L"Gold Hairpin",L"Silver Cuirass"};
  for(int slot_index=0;slot_index<5;++slot_index){slot_label.name=labels[slot_index];icon.text["nameText"]=names[slot_index];slot_option.name=slot_index==1?L"(Favored Hand)":L"";spoken.clear();d_EquipInfoSelectContent(&info,&cursor,nullptr);tick();auto expected=labels[slot_index]+(slot_index==1?L" (Favored Hand)":L"")+L": "+names[slot_index]+(autodetail?L". Attack +7 Accuracy +65":L"");expect(spoken.size()==1&&spoken[0]==expected,"all-six slot labels and displayed favored hand remain audible in both Autodetail states");}
 }}
 slot_label.name=L"L. Hand";slot_option.name=L"(Favored Hand)";slot_option.enabled=false;icon.text["nameText"]=L"Mythril Sword";bools["AutoDetail"]=false;spoken.clear();d_EquipInfoSelectContent(&info,&cursor,nullptr);tick();expect(spoken.size()==1&&spoken[0]==L"L. Hand: Mythril Sword","hidden favored-hand text is not announced");slot_option.enabled=true;slot_option.name.clear();
 for(int n:{5,6}){fixture::game=static_cast<game::Id>(n);slot_both.name=L"Both Hands";spoken.clear();d_EquipInfoSelectContent(&info,&cursor,nullptr);tick();expect(spoken.size()==1&&spoken[0]==L"L. Hand Both Hands: Mythril Sword","FF5/FF6 rendered both-hands qualification remains audible");}
 slot_both.name.clear();slot_label.name=L"R. Hand";icon.text["nameText"]=L"Dagger";
 bools["AutoDetail"]=false;info.active=false;on_select=[&]{d_EquipInfoSelectContent(&info,&cursor,nullptr);};spoken.clear();d_EquipSelectSelectContent(&select,&cursor,0,nullptr);tick();expect(spoken.size()==1&&spoken[0]==L"Spear, 4","hidden nested slot preview cannot announce or steal candidate focus");spoken.clear();ui::review::next();expect(contains(L"Attack 26 to 30"),"outer candidate keeps its comparison after nested preview");on_select={};info.active=true;
 bools["AutoDetail"]=false;delta=1;spoken.clear();d_EquipSelectSelectContent(&select,&cursor,0,nullptr);tick();spoken.clear();ui::review::next();expect(contains(L"Attack: gained 4")&&!contains(L"26"),"equipment comparison buffer respects Brief");panel.name=L"A metal-tipped spear.";ffpr::menus::o_description=&description;spoken.clear();ffpr::menus::d_description(&desc,nullptr,false,nullptr);tick();expect(spoken.empty(),"page toggle refreshes buffer without re-announcing the item");ui::review::last();expect(contains(L"A metal-tipped spear."),"page toggle replaces stale information-panel buffer text");d_EquipWindowSetActive(&window,false,nullptr);expect(!ui::review::active(),"equipment close releases buffer");
 Node item_window,equipment,equipment_view,selected,item_icon,detail,detail_view,extra_row,extra_view,element_row,element_view,fire_icon,fire_image;
 List extra_stats{&extra_row},elements{&element_row},element_icons{&fire_icon};extra_row.view=&extra_view;extra_view.text={{"nameText",L"Attack"},{"plusText",L"+"},{"valueText",L"12"}};
 equipment.view=&equipment_view;equipment_view.fields["selectItemView"]=&selected;selected.fields["IconTextView"]=&item_icon;item_icon.text["nameText"]=L"Mythril Sword";
 equipment.fields["detailController"]=&detail;detail.view=&detail_view;detail_view.fields={{"statusContentList",&extra_stats},{"attributeContentList",&elements}};
 element_row.view=&element_view;element_view.text["nameText"]=L"Attack element";element_view.fields["iconsList"]=&element_icons;fire_icon.id=1;fire_icon.fields["image"]=&fire_image;
 item_window.state=7;item_window.fields["itemEquipController"]=&equipment;item_window.view=&desc_view;desc_view.text["descriptionText"]=L"A light sword.";ffpr::menus::o_enter=&phase;ffpr::menus::o_window_update=&phase;ffpr::menus::o_equipable_update=&slot;ffpr::menus::o_all_attributes=&active;ffpr::menus::o_one_attribute=&attribute;ffpr::menus::d_all_attributes(&element_row,false,nullptr);ffpr::menus::d_one_attribute(&element_row,1,nullptr);
 actions::define("ItemDescriptionAnnouncer.AnnounceCurrentDescription",[&]{ffpr::say(desc_view.text["descriptionText"],true);});
 for(int n=1;n<=6;++n){fixture::game=static_cast<game::Id>(n);for(bool autodetail:{false,true}){bools["AutoDetail"]=autodetail;spoken.clear();ffpr::menus::d_enter(&item_window,nullptr);ffpr::menus::d_window_update(&item_window,nullptr);tick();expect(spoken[0]==L"Mythril Sword"&&contains(L"Attack +12")==autodetail&&contains(L"Attack element Fire")==autodetail,"all-six extra item screen reads visible stats/icons only with Autodetail");spoken.clear();ui::review::next();expect(contains(L"Attack +12"),"extra item stats stay in arrow buffer with Autodetail off");spoken.clear();desc_view.text["descriptionText"]=L"Attack +12";ffpr::menus::capture(&equipment,false);tick();spoken.clear();ui::review::details();expect(contains(L"Mythril Sword")&&contains(L"Attack +12"),"extra-screen I reads the complete current-screen buffer");desc_view.text["descriptionText"]=L"A light sword.";}}
 ffpr::menus::d_all_attributes(&element_row,false,nullptr);spoken.clear();ffpr::menus::d_enter(&item_window,nullptr);ffpr::menus::d_window_update(&item_window,nullptr);tick();ui::review::last();ui::review::previous();expect(!contains(L"Fire"),"gray elemental icons are never disclosed as active item properties");
 Node page_visible,page_hidden,eligible,ineligible,eligible_view,ineligible_view,can,cannot;
 List pages{&page_visible,&page_hidden},eligible_rows{&eligible},ineligible_rows{&ineligible};
 page_visible.list=&eligible_rows;page_hidden.list=&ineligible_rows;page_hidden.active=false;
 eligible.view=&eligible_view;eligible_view.text["nameText"]=L"Warrior";ineligible.view=&ineligible_view;ineligible_view.text["nameText"]=L"Mage";
 equipment.fields["equipableList"]=&pages;can.enabled=true;cannot.enabled=false;
 ffpr::menus::d_equipable_update(&eligible,&can,nullptr);ffpr::menus::d_equipable_update(&ineligible,&cannot,nullptr);
 detail.active=false;ffpr::menus::capture(&equipment,false);tick();spoken.clear();ui::review::details();
 expect(contains(L"Warrior, Can equip")&&!contains(L"Mage")&&!contains(L"Attack element"),"current eligibility page is buffered; hidden pages and hidden stats are excluded");
 page_visible.active=false;page_hidden.active=true;ffpr::menus::capture(&equipment,false);tick();spoken.clear();ui::review::details();
 expect(contains(L"Mage, Cannot equip")&&!contains(L"Warrior"),"page change replaces names and native rendered eligibility colors");

 Node pager,pager_view;equipment.fields["pager"]=&pager;pager.view=&pager_view;
 pager_view.text={{"currentPageText",L"1"},{"separatorText",L"/"},{"maxPageText",L"2"}};
 selected.text["countText"]=L"1";
 ffpr::menus::o_parameter_flag=&parameter_flag;ffpr::menus::o_page=&equipment_page;
 on_parameter=[&](bool parameter){desc_view.text["descriptionText"]=parameter?L"Attack +12":L"A light sword.";};
 on_equipment_page=[&](int page){detail.active=page==0;page_visible.active=false;page_hidden.active=page==1;pager_view.text["currentPageText"]=page==0?L"1":L"2";};
 bools["AutoDetail"]=false;
 for(int n=1;n<=6;++n){fixture::game=static_cast<game::Id>(n);
  on_equipment_page(0);on_parameter(false);spoken.clear();d_enter(&item_window,nullptr);d_window_update(&item_window,nullptr);tick();
  expect(spoken==std::vector<std::wstring>{L"Mythril Sword, quantity 1"},"all-six equipment entry speaks rendered item and quantity exactly once");
  spoken.clear();d_parameter_flag(&equipment,true,nullptr);tick();expect(spoken.empty(),"Q refresh does not repeat the item entry");ui::review::details();
  expect(contains(L"Attack +12")&&!contains(L"A light sword.")&&contains(L"Page 1/2"),"description/stat-summary toggle replaces current footer without inventing a page change");
  spoken.clear();d_page(&equipment,1,nullptr);tick();ui::review::details();
  expect(contains(L"Page 2/2")&&contains(L"Mage, Cannot equip")&&!contains(L"Warrior")&&!contains(L"Attack element"),"numbered page hook replaces eligibility, page text and hides first-page statistics");
  spoken.clear();d_page(&equipment,0,nullptr);d_parameter_flag(&equipment,false,nullptr);tick();ui::review::details();
  expect(contains(L"Page 1/2")&&contains(L"A light sword.")&&!contains(L"Mage"),"rapid page/description return publishes only latest rendered panel");
 }

 on_parameter={};on_equipment_page={};
 item_window.state=0;ffpr::menus::d_window_update(&item_window,nullptr);tick();expect(!ui::review::active(),"native equipment-state exit releases its buffer");
 item_window.state=7;item_window.active=false;ffpr::menus::d_enter(&item_window,nullptr);ffpr::menus::d_window_update(&item_window,nullptr);tick();expect(!ui::review::active()&&!ffpr::menus::g_pending_entry,"closed equipment owner cancels pending entry");
 ui::review::clear();
 Node shop,shop_info,shop_view,shop_panel,shop_item,shop_icon,shop_name,shop_item_view,shop_price;shop.state=2;shop.fields["infoController"]=&shop_info;shop_info.view=&shop_view;shop_view.description=&shop_panel;shop_item.animation=&shop_icon;shop_icon.description=&shop_name;shop_name.name=L"Spear";shop_item.view=&shop_item_view;shop_item_view.description=&shop_price;shop_price.name=L"100 Gil";shop_panel.name=L"Attack +11";ffpr::shop::set_cached_controller(&shop);ffpr::shop::set_shop_menu_active(true);
 const std::pair<const wchar_t*,const wchar_t*> shop_examples[]={{L"Potion",L"50 Gil"},{L"Mythril Sword",L"100 Gil"},{L"Bronze Shield",L"75 Gil"},{L"Fire",L"200 Gil"},{L"Mythril Ring",L"300 Gil"}};
 for(int n=1;n<=6;++n){fixture::game=static_cast<game::Id>(n);for(const auto&example:shop_examples){shop_name.name=example.first;shop_price.name=example.second;for(bool autodetail:{false,true}){bools["AutoDetail"]=autodetail;for(const auto*panel_text:{L"Attack +11",L"A metal-tipped spear."}){shop_panel.name=panel_text;spoken.clear();ffpr::shop::announce_focused_item(&shop_item,0,1);std::wstring expected=shop_name.name+L". "+shop_price.name;if(autodetail)expected+=L". "+shop_panel.name;expect(spoken.size()==1&&spoken[0]==expected,"shop focus always speaks rendered price before optional current panel across all games, item types and panel states");}shop_panel.name=L"A metal-tipped spear.";spoken.clear();ffpr::shop::announce_current_item_details();expect(spoken.size()==1&&spoken[0]==shop_panel.name,"shop I reads currently toggled description in both Autodetail states");shop_panel.name=L"Attack +11";}}}
 bools["AutoDetail"]=false;shop_name.name=L"Mythril Ring";shop_price.enabled=false;spoken.clear();ffpr::shop::announce_focused_item(&shop_item,0,1);expect(spoken.size()==1&&spoken[0]==L"Mythril Ring","hidden shop price is not fabricated");shop_price.enabled=true;shop_price.name=L"  ";spoken.clear();ffpr::shop::announce_focused_item(&shop_item,0,1);expect(spoken.size()==1&&spoken[0]==L"Mythril Ring","blank shop price is not fabricated");shop_price.name=L"300 Gil";
 shop_panel.enabled=false;expect(ffpr::shop::description_from_ui().empty(),"hidden shop description cannot disclose cached content");shop_panel.enabled=true;ffpr::shop::clear_state();
 field::Entity consumed;consumed.kind=field::Kind::Event;consumed.game=&shop_item;
 shop_item.enabled=true;expect(consumed.is_alive(),"native-actionable inspection remains available");
 shop_item.enabled=false;expect(!consumed.is_alive(),"a consumed native action is unavailable even before its graphics are removed");shop_item.enabled=true;
 Node party,p1,p2,p3,v1,v2,v3,unit1,unit3;List members{&p1,&p2,&p3};party.list=&members;party.enabled=false;p1.view=&v1;p2.view=&v2;p3.view=&v3;v1.enabled=false;v1.text={{"nameText",L"Firion"},{"currentHpText",L"120"},{"maxHpText",L"200"},{"currentMpText",L"30"}};v2.active=false;v3.text={{"nameText",L"Guy"},{"currentHpText",L"170"},{"maxHpText",L"250"},{"currentMpText",L"20"}};p1.fields["<TargetData>k__BackingField"]=&unit1;p3.fields["<TargetData>k__BackingField"]=&unit3;battle=true;ffpr::battle::o_update=&update;ffpr::battle::o_focus=&focus;ffpr::battle::d_update(&party,nullptr,false,nullptr);ffpr::battle::d_focus(&party,&unit1,true,nullptr);
 spoken.clear();ffpr::battle::announce_active_status();expect(contains(L"Firion, HP 120/200, MP 30"),"H reads native focused member even with disabled controller/view scripts");spoken.clear();ffpr::battle::announce_slot(3);expect(contains(L"Guy"),"hidden middle row does not renumber party slots");spoken.clear();ffpr::battle::announce_slot(2);expect(spoken.empty(),"hidden member parameters are not disclosed");
 // Controllers can be hidden while their serialized target panel is rendered.
 party.active=false;Node wrong_owner;owners["BattlePlayerInfomationController"]=&wrong_owner;
 for(int n=1;n<=6;++n){fixture::game=static_cast<game::Id>(n);ffpr::battle::observe_target_party(&party);expect(ffpr::battle::displayed_player(&unit1)==L"Firion, HP 120/200, MP 30","all-six native target owner reads independently rooted visible ally row");}
 v1.active=false;expect(ffpr::battle::displayed_player(&unit1).empty(),"target panel visibility still vetoes hidden ally statistics");v1.active=true;party.active=true;

 Node damage_owner,animation,entity;damage_owner.animation=&animation;o_execute=&damage;battle=false;spoken.clear();d_execute(&damage_owner,nullptr,&entity,1,1,nullptr);tick();expect(contains(L"Status damage"),"field warning defaults on");bools["StatusDamage"]=false;spoken.clear();int before=native_calls;for(int i=0;i<20;++i){d_execute(&damage_owner,nullptr,&entity,i,0,nullptr);tick();}expect(native_calls==before+20&&spoken.empty(),"toggle off preserves every native damage operation while muting warning");bools["StatusDamage"]=true;for(int i=0;i<3;++i){d_execute(&damage_owner,nullptr,&entity,i,0,nullptr);tick();}expect(spoken.size()==3,"identical native damage events remain distinct with warning on");
 field::Entity chest;chest.kind=field::Kind::TreasureChest;chest.name=L"Egil's Torch";chest.remote_map_id=2010;chest.floor_name=L"Castle Deist";chest.opened_known=false;bools.erase("SpoilTreasureChestContents");expect(chest.display_name()==L"Treasure Chest"&&chest.format_description({}).find(L"Torch")==std::wstring::npos,"remote chest contents cannot leak by default");chest.opened_known=true;expect(chest.display_name()==L"Unopened Treasure Chest","remote chest renders known unopened state without spoilers");chest.opened=true;expect(chest.display_name()==L"Opened Treasure Chest"&&chest.format_description({}).find(L"Castle Deist")!=std::wstring::npos,"remote chest renders opened state and floor without spoilers");bools["SpoilTreasureChestContents"]=true;expect(chest.display_name()==L"Opened Egil's Torch","explicit spoil preference allows contents while retaining opened state");bools["SpoilTreasureChestContents"]=false;chest.opened=false;chest.remote_map_id=0;expect(chest.display_name()==L"Unopened Treasure Chest","live/cached chest labels obey preference immediately");expect(wrong_thread==0,"no native pointers or graphics are read on review worker");
 for (const auto kind : {field::Kind::NPC,field::Kind::Event,field::Kind::MapExit,field::Kind::SavePoint,field::Kind::Vehicle,field::Kind::ScriptTrigger}) {
  field::Entity object;object.kind=kind;object.name=L"Conversation";object.event_type=L"Interactive Object";
  object.remote_map_id=187;object.floor_name=L"Deist Cavern B5";
  expect(object.format_description({},true).find(L"(Deist Cavern B5)")!=std::wstring::npos && object.format_description({},true).find(L"steps")==std::wstring::npos,
   "every other-floor object type retains its floor and suppresses distance even if given direct evidence");
  object.remote_map_id=0;object.pos={0,224,0};
  expect(object.format_description({},false,L"Requires travelling through B2 first").find(L"(Requires travelling through B2 first)")!=std::wstring::npos && object.format_description({}).find(L"steps")==std::wstring::npos,
   "same-floor indirect and unknown reachability never invent steps/bearing");
  expect(object.format_description({},true).find(L"14.0 steps North")!=std::wstring::npos,"directly reachable object retains ordinary distance and direction");
 }
 chest.remote_map_id=187;chest.floor_name=L"Deist Cavern B5";chest.opened_known=true;
 for(bool opened : {false,true}) {
  chest.opened=opened;
  expect(chest.format_description({})==(opened?L"Opened Treasure Chest (Deist Cavern B5) - Treasure Chest":L"Unopened Treasure Chest (Deist Cavern B5) - Treasure Chest"),
   "known remote chest state updates preserve the exact cross-floor label");
 }
 std::printf("Equipment/party/status warning/chest disclosure and object floor labels: %d failures\n",failures);return failures?1:0;
}
