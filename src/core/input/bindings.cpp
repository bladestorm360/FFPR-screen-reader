#include "bindings.h"
#include "input_internal.h"
#include "../settings.h"
#include "../text.h"
#include "../modtext.h"
#include "../say.h"
#include "../ui/preferences.h"
#include <algorithm>
#include <cmath>

namespace input::bindings {
namespace {
std::vector<Entry>& catalog() { static auto* v = new std::vector<Entry>; return *v; }
bool current[kPadInputs]{}, previous[kPadInputs]{}, owned[kPadInputs]{};
int capture_index = -1;
int capture_pad_first=-1;
bool capture_ready = false;
Context game_context=Context::Global;
bool pad_ready=false,had_pad=false;
bool allow_keyboard=true;
std::string setting(const Entry& e, const char* suffix) { return "Binding." + e.id + suffix; }
int mods() { return (shift_held()?1:0) | (ctrl_held()?2:0) | (alt_held()?4:0); }
bool modifier_key(int k) { return k >= int(Key::LeftShift) && k <= int(Key::RightWindows); }
bool enabled(const Entry& e) { return e.layout < 0 || e.layout == ui::prefs::navigation_keymap(); }
bool down(const Entry& e, bool edge) {
    if (!game_window_focused() || !enabled(e) || e.primary < 0) return false;
    if (!e.controller) {
        int actual=mods();
        if(e.primary==int(Key::LeftShift)||e.primary==int(Key::RightShift))actual&=~1;
        if(e.primary==int(Key::LeftControl)||e.primary==int(Key::RightControl))actual&=~2;
        if(e.primary==int(Key::LeftAlt)||e.primary==int(Key::RightAlt))actual&=~4;
        return actual == e.modifier && (edge ? key_pressed(Key(e.primary)) : key_held(Key(e.primary)));
    }
    return current[e.primary] && (e.modifier < 0 || current[e.modifier]) && (!edge || !previous[e.primary] || (e.modifier >= 0 && !previous[e.modifier]));
}
int find(const char* id) { const auto& v=catalog(); for(size_t i=0;i<v.size();++i) if(v[i].id==id) return int(i); return -1; }
void own(const Entry& e) {
    if(e.primary>=0 && (current[e.primary] || previous[e.primary])) owned[e.primary]=true;
    if(e.modifier>=0 && current[e.modifier]) owned[e.modifier]=true;
}
}
int add(const char* id, const char* label, bool controller, int scope, int primary, int modifier, int layout) {
    if(int i=find(id); i>=0) return i;
    Entry e{id,text::widen(label),controller,scope,layout,primary,modifier,primary,modifier};
    const int p=settings::get_int(setting(e,".Input").c_str(),primary);
    const int m=settings::get_int(setting(e,".Modifier").c_str(),modifier);
    if(p>=-1 && p<(controller?kPadInputs:int(Key::Count)) && m>=(controller?-1:0) && m<(controller?kPadInputs:8) && (!controller || p<0 || p!=m)) { e.primary=p; e.modifier=m; }
    catalog().push_back(std::move(e)); return int(catalog().size()-1);
}
const std::vector<Entry>& entries() { return catalog(); }
std::wstring scope_name(int scope) {
    const wchar_t* names[]={L"Global",L"Field",L"Battle",L"Shop",L"Status",L"Bestiary",L"Results",L"Controls",L"Information"};
    if(scope>=0 && scope<9)return T(names[scope]);
    switch(scope) {
    case -1:case -18:return T(L"Mod menu");
    case -2:return T(L"Confirmation");case -3:return T(L"Text input");
    case -10:return T(L"Global");case -11:return T(L"Field");case -12:return T(L"Information");
    case -13:return T(L"Puzzle");case -14:return T(L"Mod mode: dialogue");case -15:return T(L"Mod mode: results");
    case -16:return T(L"Mod mode: battle");case -17:return T(L"Mod mode: field");default:return T(L"Unknown");
    }
}
std::wstring input_name(bool pad, int p) {
    if(p<0) return T(L"Unbound");
    if(!pad) return text::widen(key_name(Key(p)));
    if(p<kButtonCount) return button_label(p);
    const wchar_t* names[]={L"Left trigger",L"Right trigger",L"Left stick up",L"Left stick down",L"Left stick left",L"Left stick right",L"Right stick up",L"Right stick down",L"Right stick left",L"Right stick right"};
    return T(names[p-kButtonCount]);
}
std::wstring value(int index) {
    const auto& e=catalog().at(index); std::wstring s;
    if(e.primary<0) return T(L"Unbound");
    if(e.controller) { if(e.modifier>=0) s=input_name(true,e.modifier)+L" + "; }
    else { if(e.modifier&2)s+=L"Control + "; if(e.modifier&1)s+=L"Shift + "; if(e.modifier&4)s+=L"Alt + "; }
    return s+input_name(e.controller,e.primary);
}
bool set(int index, int p, int m, std::wstring& conflict) {
    auto& e=catalog().at(index);
    if(p < -1 || p >= (e.controller?kPadInputs:int(Key::Count)) || m < (e.controller?-1:0) || m >= (e.controller?kPadInputs:8) || (e.controller && p>=0 && p==m)) return false;
    for(size_t i=0;p>=0 && i<catalog().size();++i) {
        const auto& other=catalog()[i];
        if(int(i)==index || other.controller!=e.controller || other.primary<0) continue;
        bool same=other.primary==p && other.modifier==m;
        if(e.controller) {
            // A global mode/menu button must never fire underneath a chord.
            same|=m<0 && (p==other.primary || p==other.modifier);
            same|=other.modifier<0 && (other.primary==p || other.primary==m);
            same|=m>=0 && other.modifier>=0 && other.primary==m && other.modifier==p;
        }
        if(!same)continue;
        if(e.layout>=0 && other.layout>=0 && e.layout!=other.layout) continue;
        if(p==e.default_primary && m==e.default_modifier && other.primary==other.default_primary && other.modifier==other.default_modifier && e.scope!=other.scope && !e.controller)continue;
        const bool original_click_chord=e.controller && e.scope==-11 && other.scope==-11 &&
            (e.id=="pad.field.normalize" || other.id=="pad.field.normalize") &&
            p==e.default_primary && m==e.default_modifier && other.primary==other.default_primary && other.modifier==other.default_modifier;
        if(original_click_chord)continue;
        // Global overlaps game contexts, but menu/modal contexts own their own inputs.
        const bool controller_global=e.controller && (e.scope==-10 || other.scope==-10) && e.scope!=-2 && e.scope!=-3 && other.scope!=-2 && other.scope!=-3;
        if(e.scope!=other.scope && !(e.scope>=0 && other.scope>=0 && (!e.scope || !other.scope)) && !controller_global) continue;
        conflict=other.label; return false;
    }
    e.primary=p; e.modifier=m;
    settings::set_int(setting(e,".Input").c_str(),p); settings::set_int(setting(e,".Modifier").c_str(),m);
    return true;
}
void reset(int i) { auto& e=catalog().at(i); e.primary=e.default_primary; e.modifier=e.default_modifier; settings::set_int(setting(e,".Input").c_str(),e.primary); settings::set_int(setting(e,".Modifier").c_str(),e.modifier); }
void reset_all() { for(size_t i=0;i<catalog().size();++i)reset(int(i)); }
bool pressed(int i) { return i>=0 && down(catalog().at(i),true); }
bool held(int i) { return i>=0 && down(catalog().at(i),false); }
bool keyboard_in_use(Context context) {
    if(!allow_keyboard)return false;
    for(const auto& e:catalog()) if(!e.controller && (e.scope==0 || e.scope==int(context)) && enabled(e) && e.primary>=0 && mods()==e.modifier && key_in_use(Key(e.primary))) {
        if((e.label==L"Report game speed" || e.label==L"Report encounters") && e.primary==e.default_primary && e.modifier==e.default_modifier)continue;
        return true;
    }
    return false;
}
void context(Context value) {game_context=value;}
bool remapped_keyboard_in_use() {
    if(!allow_keyboard)return false;
    for(const auto& e:catalog())if(!e.controller && (e.scope==0 || e.scope==int(game_context)) && (e.primary!=e.default_primary || e.modifier!=e.default_modifier) && enabled(e) && e.primary>=0 && mods()==e.modifier && key_in_use(Key(e.primary)))return true;
    return false;
}
bool keyboard_action_pressed(const char* action) {
    for(size_t i=0;i<catalog().size();++i) if(!catalog()[i].controller && catalog()[i].id.find(action)!=std::string::npos && pressed(int(i)))return true;
    return false;
}
void begin_frame() {
    for(int i=0;i<kPadInputs;++i) { previous[i]=current[i]; owned[i]=owned[i] && current[i]; }
    for(int i=0;i<kButtonCount;++i)current[i]=button_held(i)||button_pressed(i);
    current[kLeftTrigger]=left_trigger()>.5f; current[kRightTrigger]=right_trigger()>.5f;
    const float x[]={left_stick_x(),right_stick_x()},y[]={left_stick_y(),right_stick_y()};
    for(int i=0;i<2;++i) { const int b=kLeftUp+4*i;
        const bool vertical=std::fabs(y[i])>.4883f && std::fabs(y[i])>std::fabs(x[i])+.2441f;
        const bool horizontal=std::fabs(x[i])>.4883f && std::fabs(x[i])>std::fabs(y[i])+.2441f;
        // Physical snapshots publish positive Y for up on both sticks.
        current[b]=vertical&&y[i]>0;current[b+1]=vertical&&y[i]<0;
        current[b+2]=horizontal&&x[i]<0;current[b+3]=horizontal&&x[i]>0;
    }
    current[kLeftUp]|=gamepad::lstick_up_pressed();current[kLeftDown]|=gamepad::lstick_down_pressed();
    current[kLeftLeft]|=gamepad::lstick_left_pressed();current[kLeftRight]|=gamepad::lstick_right_pressed();
    current[kRightUp]|=gamepad::rstick_up_pressed();current[kRightDown]|=gamepad::rstick_down_pressed();
    current[kRightLeft]|=gamepad::rstick_left_pressed();current[kRightRight]|=gamepad::rstick_right_pressed();
    const bool available=game_window_focused()&&gamepad_available();
    if(!available || !had_pad)pad_ready=false;
    had_pad=available;
    if(!pad_ready) {
        bool neutral=true;for(bool b:current)neutral&=!b;
        pad_ready=available&&neutral;
        for(int i=0;i<kPadInputs;++i)current[i]=previous[i]=owned[i]=false;
    }
}
bool pad(const char* id, bool release) {
    const int i=find(id); if(i<0 || capture_index>=0)return false;
    const auto& e=catalog()[i]; if(e.primary<0)return false;
    const bool chord=e.modifier<0 || current[e.modifier] || (release && previous[e.modifier]);
    if(chord)own(e);
    return release ? previous[e.primary] && (e.modifier<0 || previous[e.modifier]) && !down(e,false) : chord && down(e,true);
}
bool pad_held(const char* id) { const int i=find(id); if(i<0)return false; const auto& e=catalog()[i]; if(down(e,false)) {own(e); return true;} return false; }
bool consumed(int i) { return i>=0 && i<kPadInputs && owned[i]; }
bool navigation_request_pressed() { return keyboard_action_pressed("Navigate to current entity / stop autowalk") || keyboard_action_pressed("Pathfind to waypoint") || pressed(find("pad.field.route")); }
bool navigation_controller_input(int physical) {
    const int index=find("pad.field.route");if(index<0)return false;
    const auto& e=catalog()[index];
    return e.primary>=0 && ((physical==e.primary && down(e,false)) || (physical==e.modifier && current[physical]));
}
void keyboard_allowed(bool allowed) {allow_keyboard=allowed;}
void capture(int i) { capture_index=i; capture_ready=false;capture_pad_first=-1; ffpr::say_menu(T(catalog()[i].controller?L"Press a controller button, trigger, or stick direction. Escape cancels.":L"Press a key. Escape cancels."),true); }
bool capturing() { return capture_index>=0; }
void cancel_capture() { capture_index=-1; capture_ready=false;capture_pad_first=-1; }
bool update_capture() {
    if(capture_index<0)return false;
    if(!game_window_focused()) {capture_ready=false;capture_pad_first=-1;return true;}
    if(key_pressed(Key::Escape)) {cancel_capture();ffpr::say_menu(T(L"Cancelled"),true);return true;}
    const auto& e=catalog()[capture_index];
    if(!capture_ready) {
        bool held_input=false;
        if(e.controller) for(bool b:current)held_input|=b;
        else for(int k=0;k<int(Key::Count);++k) if(!modifier_key(k))held_input|=key_held(Key(k));
        capture_ready=!held_input; return true;
    }
    int p=-1,m=e.controller?-1:mods();
    if(e.controller) {
        int edge=-1;for(int i=0;i<kPadInputs;++i)if(current[i]&&!previous[i]){edge=i;break;}
        if(capture_pad_first<0)capture_pad_first=edge;
        if(capture_pad_first>=0) {
            if(edge>=0 && edge!=capture_pad_first && current[capture_pad_first]) {p=edge;m=capture_pad_first;}
            else if(!current[capture_pad_first])p=capture_pad_first;
        }
    }
    else for(int i=0;i<int(Key::Count);++i)if(!modifier_key(i)&&key_pressed(Key(i))){p=i;break;}
    if(p>=0) { std::wstring conflict; const int index=capture_index; if(set(index,p,m,conflict)) {cancel_capture();ffpr::say_menu(TF(L"Binding set to {0}",{value(index)}),true);} else {capture_ready=false;capture_pad_first=-1;ffpr::say_menu(TF(L"Already assigned to {0}. Unbind it first.",{T(conflict)}),true);} }
    return true;
}
}
