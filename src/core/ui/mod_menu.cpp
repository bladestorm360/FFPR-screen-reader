// Ports FF1 Core/ModMenu.cs.
//
// Audio-only virtual menu for the screen reader's settings, opened with F8 or the controller's
// Start button. No Unity UI: navigation state plus announcements.
//
// Item names, values and descriptions are mod_text keys translated when spoken (FF1 translated
// the names once, when the menu was built). Each setting keeps FF1's key and default
// (preferences.h). A changed setting is stored, then "Settings.Changed.<Key>" runs when an area
// defines it - the side effects of FF1's FFI_ScreenReaderMod.Toggle* methods (audio loops,
// entity filters, stopping the EXP tick) belong there.
//
// Cross-game layout: exactly Speech, Navigation, Navigation audio and Battle categories, followed by the
// root Close Menu action. Game-specific entries remain conditional; FF2 has no EXP counter.
// FF5's Landing Pings are available where a ship-type vehicle exists (not FF4 or FF6), and
// Enemy Letters remains FF5-only. Displayed hit counters are supported wherever native UI shows them.
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "core.h"
#include "frame.h"
#include "game.h"
#include "log.h"
#include "modtext.h"
#include "say.h"
#include "settings.h"
#include "text.h"
#include "ui_util.h"
#include "mod_ui.h"
#include "modal_keys.h"
#include "preferences.h"
#include "../audio/sound.h"
#include "../input/bindings.h"
#include "../audio/volume_settings.h"

namespace ui::mod_menu {
namespace {

enum class Kind : uint8_t { Header, Submenu, Toggle, Volume, Range, Enum, Action, BindingInput, BindingModifier, BindingCapture, BindingUnbind, BindingReset };

struct Item {
    Kind kind = Kind::Header;
    std::wstring name;                       // mod_text key
    int parent = -2;                         // -2 infer category; -1 root
    const char* key = nullptr;              // settings key (Toggle, Volume, Enum)
    bool bool_def = false;                  // Toggle default
    int int_def = 0;                        // Volume / Enum default
    int int_max = 0;                        // Enum: PreferencesManager clamp maximum
    int int_min = 0, int_step = 1;
    std::vector<const wchar_t*> options;    // Enum option keys
    const wchar_t* desc = nullptr;          // description (Toggle: when on)
    const wchar_t* desc_off = nullptr;      // Toggle: description when off
    void (*action)() = nullptr;             // Action
    int binding = -1;
    sound::cues::Id cue = sound::cues::Id::Count;
};

struct State {
    bool open = false;
    int index = 0;
    int page = -1;                         // -1: category list; otherwise its header index
    std::vector<Item> items;
    uint64_t input_frame = UINT64_MAX;
    bool input_result = false;
    bool feedback = false;
};

int menu_key_close,menu_key_back,menu_key_previous,menu_key_next,menu_key_decrease,menu_key_increase,menu_key_select,menu_key_select_space,menu_key_help;
State* g_state = nullptr;
State& st()
{
    if (!g_state) g_state = new State();
    return *g_state;
}

// --- item builders (FF1's SectionHeader / ToggleItem / VolumeItem / EnumItem / ActionItem) ---

Item header(const wchar_t* name)
{
    Item i;
    i.kind = Kind::Header;
    i.name = name;
    return i;
}

Item toggle(const wchar_t* name, const char* key, bool def, const wchar_t* desc_on, const wchar_t* desc_off)
{
    Item i;
    i.kind = Kind::Toggle;
    i.name = name;
    i.key = key;
    i.bool_def = def;
    i.desc = desc_on;
    i.desc_off = desc_off;
    return i;
}

Item volume(const wchar_t* name, const char* key, int def, const wchar_t* desc)
{
    Item i;
    i.kind = Kind::Volume;
    i.name = name;
    i.key = key;
    i.int_def = def;
    i.desc = desc;
    i.int_max = 100; i.int_step = 5;
    return i;
}

Item enumeration(const wchar_t* name, std::vector<const wchar_t*> options, const char* key, int def, int max,
                 const wchar_t* desc)
{
    Item i;
    i.kind = Kind::Enum;
    i.name = name;
    i.options = std::move(options);
    i.key = key;
    i.int_def = def;
    i.int_max = max;
    i.desc = desc;
    return i;
}

Item action(const wchar_t* name, void (*fn)(), const wchar_t* desc)
{
    Item i;
    i.kind = Kind::Action;
    i.name = name;
    i.action = fn;
    i.desc = desc;
    return i;
}

void close_menu() { close(); }

/// FF1 ModMenu.Initialize.
void initialize()
{
    namespace p = ui::prefs;
    std::vector<Item>& v = st().items;
    v.clear();

    // FF4 and FF6 have no ship-type vehicle (FF6's mod menu had no Landing Pings entry either)
    const bool landing_pings = !game::is(game::Id::FF4) && !game::is(game::Id::FF6);

    // Speech
    v.push_back(header(L"Speech"));
    v.push_back(toggle(L"Game speech", p::kSpeechEnabled, p::kSpeechEnabledDefault,
                       L"Game speech help", L"Game speech help"));
    v.push_back(toggle(L"Auto Detail", p::kAutoDetail, p::kAutoDetailDefault,
                       L"On. Reads displayed descriptions and current information buffers after their titles, including status and battle results.",
                       L"Off. Use the I key to read descriptions on demand."));
    v.push_back(toggle(L"Battle results: notifications once", p::kResultNotificationsOnce, p::kResultNotificationsOnceDefault,
                       L"Battle results: notifications once on", L"Battle results: notifications once off"));
    v.push_back(enumeration(L"Stat delta detail", {L"Verbose", L"Brief"},
                            p::kStatDeltaMode, p::kStatDeltaModeDefault, p::kStatDeltaModeMax,
                            L"Stat delta detail help"));
    v.push_back(toggle(L"Status damage", p::kStatusDamage, p::kStatusDamageDefault,
                       L"Status damage on", L"Status damage off"));
    v.push_back(toggle(L"Mod warnings", p::kModWarnings, p::kModWarningsDefault,
                       L"Announce mod warning messages, including event and trap warnings.",
                       L"Announce mod warning messages, including event and trap warnings."));
    v.push_back(toggle(L"Spoil treasure chest contents", p::kSpoilTreasureChestContents,
                       p::kSpoilTreasureChestContentsDefault,
                       L"Spoil treasure chest contents on", L"Spoil treasure chest contents off"));
    v.push_back(toggle(L"Beacon Destination Announcement", p::kAnnounceOnBeaconRestart, p::kAnnounceOnBeaconRestartDefault,
                       L"On. Restarting the beacon also re-speaks the current destination.",
                       L"Off. Restarting the beacon only re-pings, without speaking."));
    v.push_back(toggle(L"Menu Position Announcements", p::kMenuPositionAnnouncements, p::kMenuPositionAnnouncementsDefault,
                       L"On. List entries announce their position, for example, 3 of 12.",
                       L"Off. List entries do not announce position."));

    // Navigation
    v.push_back(header(L"Navigation"));
    v.push_back(toggle(L"Autowalk", p::kAutowalk, p::kAutowalkDefault,
                       L"On. The selected layout's navigation key, Slash for a waypoint, or controller LT walks to the destination. Press again to stop. Manual input, menus and battles stop the walk.",
                       L"Off. Navigation requests give directions or restart the beacon."));
    v.push_back(enumeration(L"Navigation keymap", {L"Letters", L"Brackets and backslash"},
                            p::kNavigationKeymap, p::kNavigationKeymapDefault, p::kNavigationKeymapMax,
                            L"Navigation keymap help"));
    v.push_back(toggle(L"Teleport shortcuts", p::kTeleportShortcuts, p::kTeleportShortcutsDefault,
                       L"Enable Ctrl+arrow and controller mod-mode right-stick shortcuts to teleport beside the selected destination.",
                       L"Teleport shortcuts are off."));
    v.push_back(toggle(L"Speak autowalk path", p::kSpeakAutowalkPath, p::kSpeakAutowalkPathDefault,
                       L"On. Reads the full current route when autowalk starts, and the next leg after a map transition.",
                       L"Off. Autowalk starts without reading the route. Arrival and required actions are still announced."));
    v.push_back(enumeration(L"Pathfinding Filter", {p::kPathfindingFilterModeNames[0], p::kPathfindingFilterModeNames[1], p::kPathfindingFilterModeNames[2]},
                            p::kPathfindingFilterMode, p::kPathfindingFilterModeDefault, p::kPathfindingFilterModeMax,
                            L"Current floor: entity cycling shows only what walking reaches on this floor. Current map with vehicles: also what a vehicle you have here reaches. Current map: everything, and a target cut off on this floor is routed through another floor."));
    v.push_back(toggle(L"Map Exit Filter", p::kMapExitFilter, p::kMapExitFilterDefault,
                       L"On. Multiple exits leading to the same destination collapse to the closest one.",
                       L"Off. All map exits appear in navigation."));
    v.push_back(toggle(L"Layer Transition Filter", p::kToLayerFilter, p::kToLayerFilterDefault,
                       L"On. Layer transition entities are hidden from navigation.",
                       L"Off. Layer transition entities appear in navigation."));
    v.push_back(toggle(L"Stick Click Normalization", p::kStickClickNormalization, p::kStickClickNormalizationDefault,
                       L"On. L3 and R3 pass through to the game (auto-dash and encounter toggle). Mod functions move to mod mode.",
                       L"Off. L3 toggles beacon navigation; R3 toggles pathfinding filter; game cannot see them."));

    const int controls_page=int(v.size());
    Item controls=header(L"Controls");controls.kind=Kind::Submenu;controls.desc=L"Keyboard bindings help";v.push_back(controls);
    for(bool controller:{false,true}) {
        const int list_page=int(v.size());
        Item list=header(controller?L"Controller bindings":L"Keyboard bindings");list.kind=Kind::Submenu;list.parent=controls_page;list.desc=controller?L"Controller bindings help":L"Keyboard bindings help";v.push_back(list);
        const auto& bindings=input::bindings::entries();
        for(size_t n=0;n<bindings.size();++n) {
            const auto& binding=bindings[n];if(binding.controller!=controller)continue;
            const int binding_page=int(v.size());
            Item row=header(L"");row.name=binding.label;
            row.name+=L" ("+input::bindings::scope_name(binding.scope)+L")";
            if(binding.layout>=0)row.name+=binding.layout?L" [Brackets]":L" [Letters]";
            row.kind=Kind::Submenu;row.parent=list_page;row.binding=int(n);v.push_back(row);
            auto child=[&](Kind kind,const wchar_t* label) {Item item=header(label);item.kind=kind;item.parent=binding_page;item.binding=int(n);v.push_back(item);};
            child(Kind::BindingCapture,controller?L"Capture controller input":L"Capture keyboard input");
            child(Kind::BindingInput,controller?L"Primary input":L"Key");
            child(Kind::BindingModifier,controller?L"Modifier input":L"Modifiers");
            child(Kind::BindingUnbind,L"Unbind");
            child(Kind::BindingReset,L"Reset binding");
        }
    }
    Item reset=action(L"Reset all bindings",[] {
        ui::confirmation::open(T(L"Restore all bindings to their defaults?"),[] {input::bindings::reset_all();ffpr::say_menu(T(L"All bindings restored"),false);});
    },L"Reset all bindings help");reset.parent=controls_page;v.push_back(reset);

    // All audio settings, including the results-screen EXP sound, live together.
    v.push_back(header(L"Navigation audio"));
    v.push_back(toggle(L"Navigation audio master", "NavigationAudio", true,
                       L"Navigation audio master help", L"Navigation audio master help"));
    v.push_back(toggle(L"HRTF", "HRTF", false, L"HRTF help", L"HRTF help"));
    v.push_back(toggle(L"Soundscape", "Soundscape", false, L"Soundscape help", L"Soundscape help"));
    const int soundscape_page = static_cast<int>(v.size());
    Item submenu = header(L"Soundscape settings"); submenu.kind = Kind::Submenu;
    v.push_back(submenu);
    auto nested = [&](Item item) { item.parent = soundscape_page; v.push_back(std::move(item)); };
    nested(volume(L"Soundscape volume", "SoundscapeVolume", 50, L"Soundscape volume help"));
    Item distance = volume(L"Soundscape distance", "SoundscapeDistance", 10, L"Soundscape distance help");
    distance.kind = Kind::Range; distance.int_min=5; distance.int_max=25;
    nested(std::move(distance));
    for(size_t n=sound::cues::first_entity;n<sound::cues::count;++n) {
        const auto& cue=sound::cues::definitions[n];
        nested(toggle(cue.label,cue.toggle,true,L"Soundscape cue help",L"Soundscape cue help"));
        Item level=volume(L"",cue.volume,50,L"Soundscape cue volume help");
        level.name=std::wstring(cue.label)+L" volume"; nested(std::move(level));
        Item preview=action(L"",nullptr,L"Sound preview help");
        preview.name=L"Preview "+std::wstring(cue.label); preview.cue=sound::cues::Id(n);
        nested(std::move(preview));
    }
    v.push_back(toggle(L"Wall Tones", p::kWallTones, p::kWallTonesDefault,
                       L"On. Directional tones play as you approach walls.",
                       L"Off. No directional wall feedback."));
    v.push_back(volume(L"Wall Tone Volume", p::kWallToneVolume, p::kWallToneVolumeDefault,
                       L"Volume of directional wall proximity tones, zero to one hundred percent."));
    v.push_back(volume(L"Wall Bump Volume", p::kWallBumpVolume, p::kWallBumpVolumeDefault,
                       L"Volume of wall bump sound effects, zero to one hundred percent."));
    v.push_back(toggle(L"Footsteps", p::kFootsteps, p::kFootstepsDefault,
                       L"On. A click plays for each tile of player movement.",
                       L"Off. No per-tile movement sound."));
    v.push_back(volume(L"Footstep Volume", p::kFootstepVolume, p::kFootstepVolumeDefault,
                       L"Volume of footstep sounds, zero to one hundred percent."));
    v.push_back(toggle(L"Beacon Navigation", p::kAudioBeacons, p::kAudioBeaconsDefault,
                       L"On. Audio pings use the selected beacon mode. Enable Beacon with directions to hear directions as well.",
                       L"Off. Turn-by-turn pathfinding is used for navigation."));
    v.push_back(volume(L"Beacon Volume", p::kBeaconVolume, p::kBeaconVolumeDefault,
                       L"Volume of audio beacon pings, zero to one hundred percent."));
    v.push_back(enumeration(L"Beacon mode",{L"Follow route",L"Object location"},p::kBeaconMode,
                           p::kBeaconModeDefault,p::kBeaconModeMax,L"Beacon mode help"));
    v.push_back(toggle(L"Beacon with directions", p::kBeaconWithDirections, p::kBeaconWithDirectionsDefault,
                       L"On. Enables Beacon Navigation. A navigation request both restarts the beacon and reads directions. During autowalk, Speak autowalk path controls route speech.",
                       L"Off. Beacon Navigation restarts the beacon without directions."));
    if (landing_pings) {
        v.push_back(toggle(L"Landing Pings", p::kLandingPings, p::kLandingPingsDefault,
                           L"While riding a vehicle, pings the directions in which you can land or get off.",
                           L"While riding a vehicle, pings the directions in which you can land or get off."));
        v.push_back(volume(L"Landing Ping Volume", p::kLandingPingVolume, p::kLandingPingVolumeDefault,
                           L"Volume of the landing pings."));
    }

    // EXP counter remains conditional because FF2 has no experience points.
    if (!game::is(game::Id::FF2)) {
        v.push_back(toggle(L"EXP Counter Sound", p::kExpCounter, p::kExpCounterDefault,
                           L"On. A rapid tick plays while the EXP bar fills on the battle results screen.",
                           L"Off. The EXP bar fills silently on the battle results screen."));
        v.push_back(volume(L"EXP Counter Volume", p::kExpCounterVolume, p::kExpCounterVolumeDefault,
                           L"Volume of the EXP counter tick, zero to one hundred percent."));
    }

    // Battle
    const int previews_page=static_cast<int>(v.size());
    Item previews=header(L"Sound previews"); previews.kind=Kind::Submenu; v.push_back(previews);
    for(size_t n=0;n<sound::cues::first_entity;++n) {
        if((n==size_t(sound::cues::Id::Counter) && game::is(game::Id::FF2)) ||
           (n==size_t(sound::cues::Id::Landing) && !landing_pings)) continue;
        Item preview=action(sound::cues::definitions[n].label,nullptr,L"Sound preview help");
        preview.parent=previews_page; preview.cue=sound::cues::Id(n); v.push_back(std::move(preview));
    }
    v.push_back(header(L"Battle"));
    v.push_back(enumeration(L"Enemy HP Display", {L"Numbers", L"Percentage", L"Hidden"},
                            p::kEnemyHPDisplay, p::kEnemyHPDisplayDefault, p::kEnemyHPDisplayMax,
                            L"Optional enemy HP disclosure. Numbers reads current and maximum HP; Percentage reads remaining HP as a percentage; Hidden omits HP. These values are not displayed in native targeting panels."));
    if (game::is(game::Id::FF5)) {
        v.push_back(toggle(L"Enemy Letters", p::kEnemyLetters, p::kEnemyLettersDefault,
                           L"Adds a letter such as A or B to enemies that share a name, so you can tell them apart.",
                           L"Adds a letter such as A or B to enemies that share a name, so you can tell them apart."));
    }
    v.push_back(enumeration(L"Multi-hit Damage", {L"Total only", L"With hit count"},
                            p::kMultiHitDamage, p::kMultiHitDamageDefault, p::kMultiHitDamageMax,
                            L"Announces the number of hits when the game displays a hit counter. Total damage remains a separate number."));
    v.push_back(toggle(L"Battle results: changed stats", p::kAnnounceResultStatChanges, p::kAnnounceResultStatChangesDefault,
                       L"On. Announces only the displayed stats that changed on each battle growth page. Arrows still review the full table.",
                       L"Off. Changed stats remain available with I and arrow review."));

    // Close Menu action
    v.push_back(action(L"Close Menu", &close_menu, L"Closes the mod menu and returns to the game."));
    int parent=-1;
    for(size_t n=0;n<v.size();++n) {
        auto& item=v[n];
        if(item.kind==Kind::Header) { parent=int(n); item.parent=-1; }
        else if(item.parent==-2) item.parent=parent;
    }
    v.back().parent=-1;
}

// Keep setting definitions in one place while presenting only the current category.
std::vector<int> page_items()
{
    const State& s = st();
    std::vector<int> result;
    const int count = static_cast<int>(s.items.size());
    for(int i=0;i<count;++i) if(s.items[i].parent==s.page) result.push_back(i);
    return result;
}

std::wstring format_current_item();

void enter_page(int header_index)
{
    const double requested_at = frame::now();
    State& s = st();
    s.page = header_index;
    const auto items = page_items();
    if (items.empty()) { s.page = -1; return; }
    s.index = items.front();
    ffpr::say_menu_sequence({T(s.items[header_index].name), format_current_item()}, true,
        {speech::Feature::ModMenu, requested_at, requested_at});
}

// --- item behaviour ---------------------------------------------------------------------------

bool toggle_value(const Item& i) { return settings::get_bool(i.key, i.bool_def); }
int int_value(const Item& i) {
    const int value=i.kind==Kind::Volume?sound::levels::get(i.key,i.int_def):settings::get_int(i.key,i.int_def);
    return i.kind==Kind::Range ? std::clamp(value/5*5,i.int_min,i.int_max) : value;
}

/// FF1 FFI_ScreenReaderMod.Toggle*: flip and store, then let the owning area react.
void flip(const Item& i)
{
    settings::toggle(i.key, i.bool_def);
    ui::prefs::notify_changed(i.key);
    actions::run("Audio.RefreshSettings");
}

/// FF1 PreferencesManager.Set*: clamp and store, then let the owning area react.
void set_int(const Item& i, int value)
{
    const int max = i.int_max;
    const int min = i.int_min;
    if(i.kind==Kind::Volume) sound::levels::set(i.key,std::clamp(value,min,max));
    else ui::prefs::set_int_clamped(i.key, value, min, max);
    ui::prefs::notify_changed(i.key);
    actions::run("Audio.RefreshSettings");
}

/// MenuItem.GetValueString.
std::wstring value_string(const Item& i)
{
    if(i.binding>=0) {
        const auto& b=input::bindings::entries()[i.binding];
        if(i.kind==Kind::BindingModifier) {
            if(b.controller)return b.modifier<0?T(L"None"):input::bindings::input_name(true,b.modifier);
            const wchar_t* names[]={L"None",L"Shift",L"Control",L"Control + Shift",L"Alt",L"Alt + Shift",L"Alt + Control",L"Alt + Control + Shift"};
            return T(names[b.modifier]);
        }
        if(i.kind==Kind::Submenu || i.kind==Kind::BindingInput || i.kind==Kind::BindingCapture)return input::bindings::value(i.binding);
    }
    switch (i.kind) {
    case Kind::Toggle:
        return toggle_value(i) ? T(L"On") : T(L"Off");
    case Kind::Volume:
        return text::num(int_value(i)) + L"%";
    case Kind::Range:
        return TF(L"{0} steps",{text::num(int_value(i))});
    case Kind::Enum: {
        const int index = int_value(i);
        if (index >= 0 && index < static_cast<int>(i.options.size()))
            return T(i.options[static_cast<size_t>(index)]);
        return T(L"Unknown");
    }
    case Kind::Header:
    case Kind::Submenu:
    case Kind::Action:
        break;
    }
    return {};
}

/// MenuItem.Adjust.
void adjust(const Item& i, int delta)
{
    if(i.binding>=0 && (i.kind==Kind::BindingInput || i.kind==Kind::BindingModifier)) {
        const auto b=input::bindings::entries()[i.binding];
        const int count=i.kind==Kind::BindingInput ? (b.controller?input::bindings::kPadInputs:int(input::Key::Count)) : (b.controller?input::bindings::kPadInputs:8);
        const int min=i.kind==Kind::BindingModifier&&!b.controller?0:-1;
        const int old=i.kind==Kind::BindingInput?b.primary:b.modifier;
        int value=old;
        // Cycle available inputs; an occupied key must not trap the picker before
        // every later option. Capture is the exact-input conflict-reporting path.
        for(int attempt=0;attempt<=count-min;++attempt) {
            value+=delta;if(value>=count)value=min;if(value<min)value=count-1;
            const int primary=i.kind==Kind::BindingInput?value:b.primary;
            const int modifier=i.kind==Kind::BindingModifier?value:b.modifier;
            if(b.controller && primary>=0 && primary==modifier)continue;
            std::wstring conflict;
            if(input::bindings::set(i.binding,primary,modifier,conflict))break;
        }
        return;
    }
    switch (i.kind) {
    case Kind::Toggle:
        flip(i);
        break;
    case Kind::Volume:
    case Kind::Range:
        set_int(i, std::clamp(int_value(i) + delta * i.int_step, i.int_min, i.int_max));
        break;
    case Kind::Enum: {
        const int count = static_cast<int>(i.options.size());
        int value = int_value(i) + delta;
        if (value < 0) value = count - 1;
        if (value >= count) value = 0;
        set_int(i, value);
        break;
    }
    case Kind::Action:
        if (i.action) i.action();
        if(i.cue!=sound::cues::Id::Count) sound::preview(i.cue);
        break;
    case Kind::Header:
    case Kind::Submenu:
        break;
    }
}

/// MenuItem.Toggle.
void activate(const Item& i)
{
    if(i.binding>=0) {
        if(i.kind==Kind::BindingCapture) {input::bindings::capture(i.binding);return;}
        if(i.kind==Kind::BindingReset) {
            st().feedback=true;
            const auto b=input::bindings::entries()[i.binding];std::wstring conflict;
            if(input::bindings::set(i.binding,b.default_primary,b.default_modifier,conflict))ffpr::say_menu(T(L"Binding restored"),true);
            else ffpr::say_menu(TF(L"Already assigned to {0}. Unbind it first.",{T(conflict)}),true);
            return;
        }
        if(i.kind==Kind::BindingUnbind) {std::wstring ignored;input::bindings::set(i.binding,-1,input::bindings::entries()[i.binding].modifier,ignored);st().feedback=true;ffpr::say_menu(T(L"Binding removed"),true);return;}
        if(i.kind==Kind::BindingInput || i.kind==Kind::BindingModifier) {adjust(i,1);return;}
    }
    switch (i.kind) {
    case Kind::Toggle:
        flip(i);
        break;
    case Kind::Volume:
        // Toggle between 0 and 50 for quick mute/unmute
        set_int(i, int_value(i) == 0 ? 50 : 0);
        break;
    case Kind::Enum:
    case Kind::Range:
        adjust(i, 1);
        break;
    case Kind::Action:
        if (i.action) i.action();
        if(i.cue!=sound::cues::Id::Count) sound::preview(i.cue);
        break;
    case Kind::Header:
    case Kind::Submenu:
        break;
    }
}

/// MenuItem.DescriptionGetter.
std::wstring description(const Item& i)
{
    const wchar_t* key = i.kind == Kind::Toggle ? (toggle_value(i) ? i.desc : i.desc_off) : i.desc;
    return key ? T(key) : std::wstring();
}

/// Position of the current item among the navigable (non-header) items. Section headers are
/// skipped silently during navigation, so the player hears "(N of total settings)" without
/// counting the headers.
std::pair<int, int> navigable_position()
{
    const State& s = st();
    const auto items = page_items();
    const auto it = std::find(items.begin(), items.end(), s.index);
    return {it == items.end() ? -1 : static_cast<int>(it - items.begin()), static_cast<int>(items.size())};
}

std::wstring format_current_item()
{
    const State& s = st();
    if (s.index < 0 || s.index >= static_cast<int>(s.items.size())) return {};

    const Item& item = s.items[static_cast<size_t>(s.index)];
    const std::wstring value = value_string(item);
    std::wstring announcement = T(item.name);
    if (!value.empty()) announcement += L": " + value;

    const auto [index, count] = navigable_position();
    announcement = ui::menu_position(announcement, index, count);

    return announcement;
}

void announce_current_item(bool interrupt = true)
{
    ffpr::say_menu(format_current_item(), interrupt);
}

void announce_current_item_description()
{
    const State& s = st();
    if (s.index < 0 || s.index >= static_cast<int>(s.items.size())) return;

    const std::wstring desc = description(s.items[static_cast<size_t>(s.index)]);
    ffpr::say_menu(text::is_blank(desc) ? T(L"No description") : desc, true);
}

/// FF1 ControllerRouter.SyncWithModMenu(open): the router follows the menu however it was
/// opened or closed (F8 / Escape / "Close Menu" on the keyboard, Start / B on the controller).
void sync_router(bool open)
{
    const char* name = open ? "ControllerRouter.SyncWithModMenuOpen" : "ControllerRouter.SyncWithModMenuClosed";
    if (actions::defined(name)) actions::run(name);
}

bool handle_input_now()
{
    State& s = st();
    if (!s.open) return false;
    if (s.items.empty()) return false;

    if(input::bindings::update_capture())return true;
    modal_keys::sample();
    using modal_keys::pressed;

    if (input::bindings::pressed(menu_key_close)) {
        close();
        return true;
    }
    if (input::bindings::pressed(menu_key_back)) { back(); return true; }
    // Up arrow - previous item
    if (input::bindings::pressed(menu_key_previous)) {
        navigate_previous();
        return true;
    }
    // Down arrow - next item
    if (input::bindings::pressed(menu_key_next)) {
        navigate_next();
        return true;
    }
    // Left arrow - decrease value
    if (input::bindings::pressed(menu_key_decrease)) {
        adjust_current_item(-1);
        return true;
    }
    // Right arrow - increase value
    if (input::bindings::pressed(menu_key_increase)) {
        adjust_current_item(1);
        return true;
    }
    // Enter or Space - toggle/activate
    if (input::bindings::pressed(menu_key_select) || input::bindings::pressed(menu_key_select_space)) {
        toggle_current_item();
        return true;
    }
    // I - read the current item's description
    if (input::bindings::pressed(menu_key_help)) {
        announce_current_item_description();
        return true;
    }
    return true;   // consume all input while the menu is open
}

int install()
{
    menu_key_help=input::bindings::add("key.menu.help","Read mod setting help",false,-1,int(input::Key::I));
    menu_key_select_space=input::bindings::add("key.menu.select.space","Select mod setting (Space)",false,-1,int(input::Key::Space));
    menu_key_select=input::bindings::add("key.menu.select","Select mod setting",false,-1,int(input::Key::Return));
    menu_key_increase=input::bindings::add("key.menu.increase","Increase mod setting",false,-1,int(input::Key::RightArrow));
    menu_key_decrease=input::bindings::add("key.menu.decrease","Decrease mod setting",false,-1,int(input::Key::LeftArrow));
    menu_key_next=input::bindings::add("key.menu.next","Next mod setting",false,-1,int(input::Key::DownArrow));
    menu_key_previous=input::bindings::add("key.menu.previous","Previous mod setting",false,-1,int(input::Key::UpArrow));
    menu_key_back=input::bindings::add("key.menu.back","Back in mod menu",false,-1,int(input::Key::Escape));
    menu_key_close=input::bindings::add("key.menu.close","Close mod menu",false,-1,int(input::Key::F8));
    initialize();

    queries::define("ModMenu.IsOpen", [] { return is_open(); });

    actions::define("ModMenu.Open", [] { open(); });
    actions::define("ModMenu.Close", [] { close(); });
    actions::define("ModMenu.Back", [] { back(); });
    actions::define("ModMenu.HandleInput", [] { handle_input(); });
    actions::define("ModMenu.NavigateNext", [] { navigate_next(); });
    actions::define("ModMenu.NavigatePrevious", [] { navigate_previous(); });
    actions::define("ModMenu.AdjustCurrentItemLeft", [] { adjust_current_item(-1); });
    actions::define("ModMenu.AdjustCurrentItemRight", [] { adjust_current_item(1); });
    actions::define("ModMenu.ToggleCurrentItem", [] { toggle_current_item(); });
    return 0;
}

core::Install s_install{"ModMenu", &install, 100};

} // namespace

bool is_open() { return g_state && g_state->open; }

void open()
{
    const double requested_at = frame::now();
    State& s = st();
    if (s.open) return;

    initialize();
    s.open = true;
    s.index = 0;
    s.page = -1;
    modal_keys::seed();

    // F8 and the controller Start button leave the controller in the same state: MOD_MENU, so
    // the D-pad / stick drive the menu instead of acting underneath it.
    sync_router(true);

    ffpr::say_menu_sequence({T(L"Mod menu"), format_current_item()}, true,
        {speech::Feature::ModMenu, requested_at, requested_at});
}

void close()
{
    State& s = st();
    if (!s.open) return;

    s.open = false;
    input::bindings::cancel_capture();
    // Every close path (keyboard, menu item, controller) returns the controller to NORMAL.
    sync_router(false);
    ffpr::say_menu(T(L"Mod menu closed"), true);
}

void back()
{
    const double requested_at = frame::now();
    State& s = st();
    if (!s.open) return;
    if (s.page < 0) { close(); return; }
    s.index = s.page;
    s.page = s.items[s.page].parent;
    ffpr::say_menu_sequence({s.page<0?T(L"Mod menu"):T(s.items[s.page].name), format_current_item()}, true,
        {speech::Feature::ModMenu, requested_at, requested_at});
}

bool handle_input()
{
    State& s = st();
    const uint64_t f = frame::count();
    if (s.input_frame == f) return s.input_result;   // already handled this frame
    s.input_frame = f;
    s.input_result = handle_input_now();
    return s.input_result;
}

void navigate_next()
{
    State& s = st();
    const auto items = page_items();
    if (items.empty()) return;
    auto it = std::find(items.begin(), items.end(), s.index);
    s.index = it == items.end() || ++it == items.end() ? items.front() : *it;
    if(!input::bindings::capturing())announce_current_item();
}

void navigate_previous()
{
    State& s = st();
    const auto items = page_items();
    if (items.empty()) return;
    auto it = std::find(items.begin(), items.end(), s.index);
    s.index = it == items.end() || it == items.begin() ? items.back() : *--it;
    if(!input::bindings::capturing())announce_current_item();
}

void adjust_current_item(int delta)
{
    State& s = st();
    if (s.index < 0 || s.index >= static_cast<int>(s.items.size())) return;

    const Item& item = s.items[static_cast<size_t>(s.index)];
    if (item.kind == Kind::Header || item.kind == Kind::Submenu) {
        if (delta > 0) enter_page(s.index);
        return;
    }

    s.feedback=false;
    adjust(item, delta);
    if(!input::bindings::capturing())announce_current_item(!s.feedback);
}

void toggle_current_item()
{
    State& s = st();
    if (s.index < 0 || s.index >= static_cast<int>(s.items.size())) return;

    const Item& item = s.items[static_cast<size_t>(s.index)];
    if (item.kind == Kind::Header || item.kind == Kind::Submenu) { enter_page(s.index); return; }

    s.feedback=false;
    activate(item);

    // For action items (like Close Menu), don't re-announce
    if (item.kind == Kind::Action) return;

    if(!input::bindings::capturing())announce_current_item(!s.feedback);
}

} // namespace ui::mod_menu
