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

namespace ui::mod_menu {
namespace {

enum class Kind : uint8_t { Header, Toggle, Volume, Enum, Action };

struct Item {
    Kind kind = Kind::Header;
    const wchar_t* name = L"";              // mod_text key
    const char* key = nullptr;              // settings key (Toggle, Volume, Enum)
    bool bool_def = false;                  // Toggle default
    int int_def = 0;                        // Volume / Enum default
    int int_max = 0;                        // Enum: PreferencesManager clamp maximum
    std::vector<const wchar_t*> options;    // Enum option keys
    const wchar_t* desc = nullptr;          // description (Toggle: when on)
    const wchar_t* desc_off = nullptr;      // Toggle: description when off
    void (*action)() = nullptr;             // Action
};

struct State {
    bool open = false;
    int index = 0;
    int page = -1;                         // -1: category list; otherwise its header index
    std::vector<Item> items;
    uint64_t input_frame = UINT64_MAX;
    bool input_result = false;
};

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
    v.push_back(toggle(L"Auto Detail", p::kAutoDetail, p::kAutoDetailDefault,
                       L"On. Reads displayed descriptions and current information buffers after their titles, including status and battle results.",
                       L"Off. Use the I key to read descriptions on demand."));
    v.push_back(enumeration(L"Stat delta detail", {L"Verbose", L"Brief"},
                            p::kStatDeltaMode, p::kStatDeltaModeDefault, p::kStatDeltaModeMax,
                            L"Stat delta detail help"));
    v.push_back(toggle(L"Status damage", p::kStatusDamage, p::kStatusDamageDefault,
                       L"Status damage on", L"Status damage off"));
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
                       L"On. Backslash, P, Slash for a waypoint, or the controller left trigger walks to the destination. Press again to stop. Manual input, menus and battles stop the walk.",
                       L"Off. Navigation requests give directions or restart the beacon."));
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

    // All audio settings, including the results-screen EXP sound, live together.
    v.push_back(header(L"Navigation audio"));
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
                       L"On. Audio pings follow the route. Enable Beacon with directions to hear directions as well.",
                       L"Off. Turn-by-turn pathfinding is used for navigation."));
    v.push_back(volume(L"Beacon Volume", p::kBeaconVolume, p::kBeaconVolumeDefault,
                       L"Volume of audio beacon pings, zero to one hundred percent."));
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
}

// Keep setting definitions in one place while presenting only the current category.
std::vector<int> page_items()
{
    const State& s = st();
    std::vector<int> result;
    const int count = static_cast<int>(s.items.size());
    if (s.page < 0) {
        for (int i = 0; i < count; ++i)
            if (s.items[i].kind == Kind::Header || i == count - 1) result.push_back(i);
    } else {
        for (int i = s.page + 1; i < count - 1 && s.items[i].kind != Kind::Header; ++i)
            result.push_back(i);
    }
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
    ffpr::say_sequence({T(s.items[header_index].name), format_current_item()}, true,
        {speech::Feature::ModMenu, requested_at, requested_at});
}

// --- item behaviour ---------------------------------------------------------------------------

bool toggle_value(const Item& i) { return settings::get_bool(i.key, i.bool_def); }
int int_value(const Item& i) { return settings::get_int(i.key, i.int_def); }

/// FF1 FFI_ScreenReaderMod.Toggle*: flip and store, then let the owning area react.
void flip(const Item& i)
{
    settings::toggle(i.key, i.bool_def);
    ui::prefs::notify_changed(i.key);
}

/// FF1 PreferencesManager.Set*: clamp and store, then let the owning area react.
void set_int(const Item& i, int value)
{
    const int max = i.kind == Kind::Volume ? ui::prefs::kVolumeMax : i.int_max;
    const int min = i.kind == Kind::Volume ? ui::prefs::kVolumeMin : 0;
    ui::prefs::set_int_clamped(i.key, value, min, max);
    ui::prefs::notify_changed(i.key);
}

/// MenuItem.GetValueString.
std::wstring value_string(const Item& i)
{
    switch (i.kind) {
    case Kind::Toggle:
        return toggle_value(i) ? T(L"On") : T(L"Off");
    case Kind::Volume:
        return text::num(int_value(i)) + L"%";
    case Kind::Enum: {
        const int index = int_value(i);
        if (index >= 0 && index < static_cast<int>(i.options.size()))
            return T(i.options[static_cast<size_t>(index)]);
        return T(L"Unknown");
    }
    case Kind::Header:
    case Kind::Action:
        break;
    }
    return {};
}

/// MenuItem.Adjust.
void adjust(const Item& i, int delta)
{
    switch (i.kind) {
    case Kind::Toggle:
        flip(i);
        break;
    case Kind::Volume:
        set_int(i, std::clamp(int_value(i) + delta * 5, 0, 100));
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
        break;
    case Kind::Header:
        break;
    }
}

/// MenuItem.Toggle.
void activate(const Item& i)
{
    switch (i.kind) {
    case Kind::Toggle:
        flip(i);
        break;
    case Kind::Volume:
        // Toggle between 0 and 50 for quick mute/unmute
        set_int(i, int_value(i) == 0 ? 50 : 0);
        break;
    case Kind::Enum:
        adjust(i, 1);
        break;
    case Kind::Action:
        if (i.action) i.action();
        break;
    case Kind::Header:
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
    ffpr::say(format_current_item(), interrupt);
}

void announce_current_item_description()
{
    const State& s = st();
    if (s.index < 0 || s.index >= static_cast<int>(s.items.size())) return;

    const std::wstring desc = description(s.items[static_cast<size_t>(s.index)]);
    ffpr::say(text::is_blank(desc) ? T(L"No description") : desc, true);
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

    modal_keys::sample();
    using modal_keys::pressed;

    if (pressed(VK_F8)) {
        close();
        return true;
    }
    if (pressed(VK_ESCAPE)) { back(); return true; }
    // Up arrow - previous item
    if (pressed(VK_UP)) {
        navigate_previous();
        return true;
    }
    // Down arrow - next item
    if (pressed(VK_DOWN)) {
        navigate_next();
        return true;
    }
    // Left arrow - decrease value
    if (pressed(VK_LEFT)) {
        adjust_current_item(-1);
        return true;
    }
    // Right arrow - increase value
    if (pressed(VK_RIGHT)) {
        adjust_current_item(1);
        return true;
    }
    // Enter or Space - toggle/activate
    if (pressed(VK_RETURN) || pressed(VK_SPACE)) {
        toggle_current_item();
        return true;
    }
    // I - read the current item's description
    if (pressed('I')) {
        announce_current_item_description();
        return true;
    }
    return true;   // consume all input while the menu is open
}

int install()
{
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

    s.open = true;
    s.index = 0;
    s.page = -1;
    modal_keys::seed();

    // F8 and the controller Start button leave the controller in the same state: MOD_MENU, so
    // the D-pad / stick drive the menu instead of acting underneath it.
    sync_router(true);

    ffpr::say_sequence({T(L"Mod menu"), format_current_item()}, true,
        {speech::Feature::ModMenu, requested_at, requested_at});
}

void close()
{
    State& s = st();
    if (!s.open) return;

    s.open = false;
    // Every close path (keyboard, menu item, controller) returns the controller to NORMAL.
    sync_router(false);
    ffpr::say(T(L"Mod menu closed"), true);
}

void back()
{
    const double requested_at = frame::now();
    State& s = st();
    if (!s.open) return;
    if (s.page < 0) { close(); return; }
    s.index = s.page;
    s.page = -1;
    ffpr::say_sequence({T(L"Mod menu"), format_current_item()}, true,
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
    announce_current_item();
}

void navigate_previous()
{
    State& s = st();
    const auto items = page_items();
    if (items.empty()) return;
    auto it = std::find(items.begin(), items.end(), s.index);
    s.index = it == items.end() || it == items.begin() ? items.back() : *--it;
    announce_current_item();
}

void adjust_current_item(int delta)
{
    State& s = st();
    if (s.index < 0 || s.index >= static_cast<int>(s.items.size())) return;

    const Item& item = s.items[static_cast<size_t>(s.index)];
    if (item.kind == Kind::Header) {
        if (delta > 0) enter_page(s.index);
        return;
    }

    adjust(item, delta);
    announce_current_item();
}

void toggle_current_item()
{
    State& s = st();
    if (s.index < 0 || s.index >= static_cast<int>(s.items.size())) return;

    const Item& item = s.items[static_cast<size_t>(s.index)];
    if (item.kind == Kind::Header) { enter_page(s.index); return; }

    activate(item);

    // For action items (like Close Menu), don't re-announce
    if (item.kind == Kind::Action) return;

    announce_current_item();
}

} // namespace ui::mod_menu
