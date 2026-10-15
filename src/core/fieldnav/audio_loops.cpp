// Ports FF1 Core/AudioLoopManager.cs (wall-tone and beacon coroutine loops) and, from
// Core/FFI_ScreenReaderMod.cs: the reactions of ToggleWallTones / ToggleAudioBeacons,
// RestartBeacon, RestartEntityBeacon, the audio-loop part of OnSceneLoaded (StopAll +
// SuppressBriefly(1.0)) and native MapChanged readiness (start audio on a
// field scene).
//
// The C# coroutines resumed once per frame and paced themselves with Time.time. Here one frame
// subscriber does the same with frame::now(); every early "continue" of the C# loops is a
// return that leaves the next check time where the C# code set it.
//
// From FF5 Core/AudioLoopManager.cs (all five games): the landing-ping loop (LandingPingLoop,
// Start/StopLandingPingLoop, the scene-load and vehicle-transition suppression of landing pings)
// and PathSearchMode.Quick for the beacon's path test (field/path_search.h). Landing pings play
// while sailing a ship/submarine or riding the FF1-3 canoe. Current native vehicle ownership
// also supports loaded boarded saves; FF4/6 and FF2's snowcraft are excluded.
//
// Sound: FF5 kept one looping stream per direction (660/440/550/520 Hz, wall-tone pans, a ping
// every 250 ms). sound.h has one landing ping (one pitch, one stream that each call restarts),
// so the directions with a landing spot take turns, one ping every 250 ms, each with FF5's pan
// and amplitude for its direction, retaining all four pitches in stereo fallback.
#include <cmath>
#include <memory>
#include <vector>
#include "audio_loops.h"
#include "fieldnav_util.h"
#include "landing_spots.h"
#include "map_transition.h"
#include "move_state.h"
#include "nav_target.h"
#include "waypoints.h"
#include "../audio/sound.h"
#include "../audio/sound_constants.h"
#include "../core.h"
#include "../ui/preferences.h"
#include "../frame.h"
#include "../hooks.h"
#include "../log.h"
#include "../say.h"
#include "../modtext.h"
#include "../refs.h"
#include "../settings.h"
#include "../input/input.h"
#include "../unity.h"
#include "../system/system.h"
#include "../field/baked_nav.h"
#include "../field/baked_map.h"
#include "../field/field_api.h"
#include "../field/path_search.h"

namespace ffpr::fieldnav::audio_loops {
namespace {

constexpr double kWallToneLoopInterval = 0.1;

// Beacon navigation: proximity-based interval modulation.
// Mode A (valid path): 1.0 s at 31.5 tiles (pathfinding limit) -> 0.2 s at 2 tiles; silent at <= 1 tile.
// Mode B (no valid path / out of range): 1.0 s at >= 100 tiles -> 0.5 s at 32 tiles; halved pitch.
constexpr float kModeAIntervalFar = 1.0f;
constexpr float kModeAIntervalNear = 0.2f;
constexpr float kModeAFarTiles = 31.5f;
constexpr float kModeANearTiles = 2.0f;
constexpr float kBeaconStopTiles = 1.0f;
constexpr float kModeBIntervalFar = 1.0f;
constexpr float kModeBIntervalNear = 0.5f;
constexpr float kModeBFarTiles = 100.0f;
constexpr float kModeBNearTiles = 32.0f;
constexpr float kTileSize = 16.0f;

// Direction vectors for the map-exit checks.
constexpr unity::Vec3 kDirNorth{0, 16, 0};
constexpr unity::Vec3 kDirSouth{0, -16, 0};
constexpr unity::Vec3 kDirEast{16, 0, 0};
constexpr unity::Vec3 kDirWest{-16, 0, 0};

/// The beacon's target identity (C# compared the target object by reference).
struct TargetRef {
    nav_target::Kind kind = nav_target::Kind::None;
    const void* ptr = nullptr;
    waypoints::EntityPtr keep;   // keeps a waypoint alive so its address cannot be reused
    ::field::TargetKind route_kind = ::field::TargetKind::Infer;   // where the route to it ends
    bool same(const TargetRef& o) const { return kind == o.kind && ptr == o.ptr; }
};

/// The scanner's category (field::Target::category) as a route target kind: chests and NPCs
/// are reached from beside and faced, exits are stepped on, vehicles stood on; an event by
/// what is baked at its cell.
::field::TargetKind route_kind_of_category(int category)
{
    switch (category) {
    case 1:   // Chests
    case 2: return ::field::TargetKind::Interact;   // NPCs
    case 3: return ::field::TargetKind::Transition; // MapExits
    case 5: return ::field::TargetKind::Spot;       // Vehicles
    default: return ::field::TargetKind::Infer;
    }
}

// Loop state ("coroutine running")
bool g_wall_running = false;
bool g_beacon_running = false;
double g_wall_next_check = 0;

// Beacon state
bool g_beacon_silenced = false;
TargetRef g_last_beacon_target;
double g_next_beacon_time = 0;

// Map transition suppression
int g_wall_tone_map_id = -1;
double g_wall_tone_suppressed_until = 0;
double g_beacon_suppressed_until = 0;
double g_last_beacon_played_at = 0;

// Reused direction buffer
std::vector<sound::Direction> g_wall_directions;

// Landing pings (FF5 AudioLoopManager.LandingPingLoop)
constexpr double kLandingPingInterval = 0.1;            // GameConstants.LANDING_PING_INTERVAL
constexpr double kLandingPingPeriod = 0.25;             // SoundConstants.LandingPingTiming.TOTAL_MS
constexpr double kVehicleTransitionSuppression = 0.2;   // GameConstants.VEHICLE_TRANSITION_SUPPRESSION
constexpr double kSceneLoadSuppression = 1.0;           // GameConstants.SCENE_LOAD_SUPPRESSION
const char* const kLandingPingsKey = "LandingPings";    // PreferencesManager: default off
constexpr bool kLandingPingsDefault = false;

bool g_landing_running = false;
double g_landing_next_check = 0;
double g_landing_next_ping = 0;
int g_landing_map_id = -1;
double g_landing_suppressed_until = 0;
double g_vehicle_transition_until = 0;
bool g_was_on_ship = false;
unsigned g_landing_mask = 0;   // directions with a landing spot (bit = sound::Direction)
int g_landing_turn = 0;        // the direction to consider first for the next ping
std::vector<sound::Direction> g_landing_directions;

float clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
float clamp01(float v) { return clamp(v, 0.0f, 1.0f); }
float lerp(float a, float b, float t) { return a + (b - a) * clamp01(t); }

void stop_tone_if_playing()
{
    if (sound::wall_tone_playing())
        sound::stop_wall_tone();
}

/// Silenced while any menu or battle is active, while a mod dialog is open (mod menu, text
/// input, confirmation: their A* work would stutter the text-input keyboard read), or while an
/// NPC message box shows dialogue.
bool audio_blocked()
{
    return !settings::get_bool("NavigationAudio",true) || !input::game_window_focused()
        || !system::is_field_navigation_ready() || !queries::is("ControllerRouter.IsFieldActive")
        || queries::is("ControllerRouter.SuppressGameInput")
        || queries::is("MessageWindowPatches.IsInDialogue");
}

// --- wall tones -------------------------------------------------------------------------

void wall_tone_check(double current_time)
{
    // Detect sub-map transitions and suppress tones briefly
    int current_map_id = ::field::current_map_id();
    if (current_map_id > 0 && g_wall_tone_map_id > 0 && current_map_id != g_wall_tone_map_id) {
        g_wall_tone_suppressed_until = current_time + 1.0;
        stop_tone_if_playing();
    }
    if (current_map_id > 0)
        g_wall_tone_map_id = current_map_id;

    if (current_time < g_wall_tone_suppressed_until) {
        stop_tone_if_playing();
        return;
    }

    if (map_transition::is_screen_fading()) {
        stop_tone_if_playing();
        return;
    }

    if (audio_blocked()) {
        stop_tone_if_playing();
        return;
    }

    void* player = ::field::player();
    if (!player) {
        stop_tone_if_playing();
        return;
    }

    ::field::WallDistances walls = ::field::nearby_walls(player);
    std::vector<unity::Vec3> map_exit_positions = ::field::map_exit_positions();
    unity::Vec3 player_pos = unity::local_position(unity::transform(player));

    g_wall_directions.clear();

    if (walls.north == 0 && !::field::is_direction_near_map_exit(player_pos, kDirNorth, map_exit_positions))
        g_wall_directions.push_back(sound::Direction::North);

    if (walls.south == 0 && !::field::is_direction_near_map_exit(player_pos, kDirSouth, map_exit_positions))
        g_wall_directions.push_back(sound::Direction::South);

    if (walls.east == 0 && !::field::is_direction_near_map_exit(player_pos, kDirEast, map_exit_positions))
        g_wall_directions.push_back(sound::Direction::East);

    if (walls.west == 0 && !::field::is_direction_near_map_exit(player_pos, kDirWest, map_exit_positions))
        g_wall_directions.push_back(sound::Direction::West);

    sound::play_wall_tones_looped(g_wall_directions);
}

/// The coroutine's exit path ("while (WallTonesEnabled)" became false).
void finish_wall_tone_loop()
{
    g_wall_running = false;
    stop_tone_if_playing();
}

void wall_tone_step()
{
    if (!settings::get_bool("WallTones", false)) {
        finish_wall_tone_loop();
        return;
    }
    double now = frame::now();
    if (now < g_wall_next_check)
        return;
    g_wall_next_check = now + kWallToneLoopInterval;

    hooks::guarded("WallTones", [&] { wall_tone_check(now); });
}

// --- beacon -----------------------------------------------------------------------------

bool object_beacon_remote() {
    if(ui::prefs::beacon_mode()!=ui::prefs::kBeaconObjectLocation || nav_target::last_kind()!=nav_target::Kind::Entity) return false;
    const auto target=::field::selected();
    return target.valid && target.remote_map_id>0;
}

void beacon_iteration(double now)
{
    TargetRef target;
    unity::Vec3 target_pos{};
    int remote_map_id = 0;   // the selected entity is on another floor (baked data): routed by its cell
    ::field::baked_route::Cell remote_cell;
    switch (nav_target::last_kind()) {
    case nav_target::Kind::Entity: {
        ::field::Target e = ::field::selected();
        if (e.valid) {
            target.kind = nav_target::Kind::Entity;
            target.ptr = e.identity;
            target.route_kind = route_kind_of_category(e.category);
            target_pos = e.position;
            remote_map_id = e.remote_map_id;
            remote_cell = ::field::baked_route::Cell{e.cell_layer, e.cell_x, e.cell_y};
        }
        break;
    }
    case nav_target::Kind::Waypoint: {
        waypoints::EntityPtr w = waypoints::nav::selected();
        if (w) {
            target.kind = nav_target::Kind::Waypoint;
            target.ptr = w.get();
            target.keep = w;
            target.route_kind = ::field::TargetKind::Spot;
            target_pos = w->position;
        }
        break;
    }
    default:
        break;
    }

    if (target.kind == nav_target::Kind::None) {
        g_next_beacon_time = now + 0.2;
        return;
    }

    // A selection change clears the silence latch so new targets always ping.
    if (!target.same(g_last_beacon_target)) {
        g_beacon_silenced = false;
        g_last_beacon_target = target;
    }

    void* player = ::field::player();
    if (!player) {
        g_next_beacon_time = now + 0.2;
        return;
    }

    unity::Vec3 player_pos = unity::local_position(unity::transform(player));

    const bool direct=ui::prefs::beacon_mode()==ui::prefs::kBeaconObjectLocation;
    // A projected entry has no meaningful local position, even during the
    // brief interval before a map change publishes its new live selection.
    if(direct && remote_map_id>0) { g_next_beacon_time=now+.5; return; }

    // Sanity check: skip if positions look invalid (garbage data during load)
    if (!std::isfinite(player_pos.x) || !std::isfinite(player_pos.y) ||
        std::fabs(player_pos.x) > 10000.0f || std::fabs(player_pos.y) > 10000.0f ||
        (remote_map_id<=0 && (!std::isfinite(target_pos.x) || !std::isfinite(target_pos.y) ||
        std::fabs(target_pos.x) > 10000.0f || std::fabs(target_pos.y) > 10000.0f))) {
        g_next_beacon_time = now + 0.2;
        return;
    }

    float dist_tiles = remote_map_id>0?0:distance(player_pos, target_pos) / kTileSize;
    unity::Vec3 aim = target_pos;   // what the ping points at
    if(direct) {
        const int map_id=::field::current_map_id();
        if(const auto* sm=::field::baked::submap_by_map_id(map_id);sm && sm->world) {
            auto nearest=[](float delta,float size){return size>0?delta-std::round(delta/size)*size:delta;};
            aim.x=player_pos.x+nearest(target_pos.x-player_pos.x,sm->width*kTileSize);
            aim.y=player_pos.y+nearest(target_pos.y-player_pos.y,sm->height*kTileSize);
        }
        dist_tiles=distance(player_pos,aim)/kTileSize;
    }

    // On a baked map the beacon leads along the planned route, corner by corner (the FFXII
    // mod's model; user, 2026-10-11): it points at the end of the current leg, not across walls
    // at the target, speeds up as that corner nears, moves on silently at the corner, and goes
    // quiet on the route's last cell. The plan is the spoken route's own (baked_nav), so the
    // voice and the beacon never disagree; off the route it re-plans silently. A map the baked
    // data does not know keeps the old test: a path exists (Quick, never chaining) or not.
    // Object mode uses the ordinary ping without asserting traversability, and
    // makes no follow/route/quick-search request, even for an object behind a wall.
    bool path_valid=direct;
    bool following = false;
    void* map = direct ? nullptr : ::field::map_handle();
    if (!direct && ::field::baked_nav::available(map)) {
        ::field::baked_nav::Corner corner;
        if (remote_map_id > 0) {
            // Another floor (user, 2026-10-11): the leg to the exit to take, and after the
            // transition the next one. With no route there is nothing to point at.
            ::field::baked_nav::MapGoal goal;
            goal.map_id = remote_map_id;
            goal.cell = remote_cell;
            path_valid = ::field::baked_nav::follow_goal(map, player, goal, target.route_kind, corner);
            if (!path_valid) {
                g_next_beacon_time = now + 0.5;
                return;
            }
        } else {
            path_valid = ::field::baked_nav::follow(map, player, target_pos, target.route_kind, corner);
        }
        if (path_valid) {
            if (corner.arrived) {
                g_beacon_silenced = true;
                g_next_beacon_time = now + 0.2;
                return;
            }
            following = true;
            aim = corner.point;
            dist_tiles = distance(player_pos, aim) / kTileSize;
        }
    } else if(!direct) {
        try {
            ::field::PathInfo path_info = ::field::find_path_to_quick(player_pos, target_pos, map, player);
            path_valid = path_info.success;
        } catch (...) {
            path_valid = false;
        }
    }

    float interval;
    bool low_pitch;
    if (path_valid) {
        // Mode A: valid path
        if (!following && dist_tiles <= kBeaconStopTiles) {
            g_beacon_silenced = true;
            g_next_beacon_time = now + 0.2;
            return;
        }
        // Moved back out of the stop radius: release the arrival latch so the beacon resumes
        // when the player walks away from a reached, still-selected target.
        g_beacon_silenced = false;
        float t = clamp01((dist_tiles - kModeANearTiles) / (kModeAFarTiles - kModeANearTiles));
        interval = lerp(kModeAIntervalNear, kModeAIntervalFar, t);
        low_pitch = false;
    } else {
        // Mode B: out of range or blocked - halved pitch, no silence latch
        float t = clamp01((dist_tiles - kModeBNearTiles) / (kModeBFarTiles - kModeBNearTiles));
        interval = lerp(kModeBIntervalNear, kModeBIntervalFar, t);
        low_pitch = true;
    }

    // The silence latch only holds while the path is valid (Mode A).
    if (g_beacon_silenced && path_valid) {
        g_next_beacon_time = now + 0.2;
        return;
    }

    g_next_beacon_time = now + interval;

    const float max_dist = 500.0f;
    float volume_scale = clamp(1.0f - (dist_tiles * kTileSize / max_dist), 0.15f, 0.60f);

    // FF1 computed pan as Clamp(dx / 100, -1, 1) * 0.5 + 0.5 for its 0..1 SoundPlayer; sound.h
    // takes -1 (left) .. 1 (right), which is the same position without the shift. The ping
    // points at the next corner of the route when one is being followed, else at the target.
    float delta_x = aim.x - player_pos.x;
    float pan = clamp(delta_x / 100.0f, -1.0f, 1.0f);

    bool is_south = aim.y < player_pos.y - 8.0f;

    // Debounce: at least 80% of the current interval since the last ping
    double time_since_last = now - g_last_beacon_played_at;
    if (time_since_last < static_cast<double>(interval) * 0.8)
        return;

    sound::play_beacon(is_south, pan, volume_scale, low_pitch);
    g_last_beacon_played_at = now;
}

/// The coroutine's exit path ("while (AudioBeaconsEnabled)" became false).
void finish_beacon_loop()
{
    g_beacon_running = false;
    g_beacon_silenced = false;
    g_last_beacon_target = TargetRef{};
}

void beacon_step()
{
    if (!settings::get_bool("AudioBeacons", false)) {
        finish_beacon_loop();
        return;
    }
    double now = frame::now();
    if (now < g_next_beacon_time)
        return;

    // Suppress beacons briefly after a scene load
    if (now < g_beacon_suppressed_until) {
        g_next_beacon_time = now + 0.1;
        return;
    }

    if (audio_blocked()) {
        g_next_beacon_time = now + 0.1;
        return;
    }

    bool ok = hooks::guarded_r("Beacon", false, [&] { beacon_iteration(now); return true; });
    if (!ok)
        g_next_beacon_time = now + 0.5;
}

// --- landing pings (FF5) --------------------------------------------------------------------

/// SoundPlayer.StopLandingPing: no more pings. (A ping already playing is a one-shot of at most
/// 250 ms; sound.h has no call to cut it short.)
void stop_landing_ping()
{
    g_landing_mask = 0;
}

/// One landing ping for a direction: FF5's pitch (LandingPingFrequencies), pan (WallTonePan,
/// 0..1 -> -1..1) and amplitude
/// (WallToneVolumeMultipliers on the landing BASE_VOLUME); the gain is "LandingPingVolume".
void play_landing_ping_for(int direction)
{
    namespace C = sound::constants;
    float pan = C::WallTonePan::NORTH;
    float scale = C::WallToneVolumeMultipliers::NORTH;
    int frequency = C::LandingPingFrequencies::NORTH;
    switch (static_cast<sound::Direction>(direction)) {
    case sound::Direction::North: pan = C::WallTonePan::NORTH; scale = C::WallToneVolumeMultipliers::NORTH; frequency = C::LandingPingFrequencies::NORTH; break;
    case sound::Direction::South: pan = C::WallTonePan::SOUTH; scale = C::WallToneVolumeMultipliers::SOUTH; frequency = C::LandingPingFrequencies::SOUTH; break;
    case sound::Direction::East: pan = C::WallTonePan::EAST; scale = C::WallToneVolumeMultipliers::EAST; frequency = C::LandingPingFrequencies::EAST; break;
    case sound::Direction::West: pan = C::WallTonePan::WEST; scale = C::WallToneVolumeMultipliers::WEST; frequency = C::LandingPingFrequencies::WEST; break;
    }
    sound::play_landing_ping(pan * 2.0f - 1.0f, scale, frequency);
}

/// The 0.1 s check of FF5's LandingPingLoop (after its IsAudioSuppressed gate).
void landing_ping_check(double current_time)
{
    // FF5 suppressed briefly on every GetOn/GetOff (boarding animation). The core's vehicle hooks
    // are FF1's and do not call into the loops, so the boarding edge is taken from the ship state.
    bool on_ship = landing_spots::riding_watercraft();
    if (on_ship != g_was_on_ship) {
        g_was_on_ship = on_ship;
        g_vehicle_transition_until = current_time + kVehicleTransitionSuppression;
    }

    int current_map_id = ::field::current_map_id();
    if (current_map_id > 0 && g_landing_map_id > 0 && current_map_id != g_landing_map_id) {
        g_landing_suppressed_until = current_time + kSceneLoadSuppression;
        stop_landing_ping();
    }
    if (current_map_id > 0)
        g_landing_map_id = current_map_id;

    if (current_time < g_landing_suppressed_until) {
        stop_landing_ping();
        return;
    }
    if (current_time < g_vehicle_transition_until) {
        stop_landing_ping();
        return;
    }
    if (map_transition::is_screen_fading()) {
        stop_landing_ping();
        return;
    }
    void* player = ::field::player();
    if (!player) {
        stop_landing_ping();
        return;
    }
    if (!on_ship) {
        stop_landing_ping();
        return;
    }

    landing_spots::nearby(player, g_landing_directions);

    unsigned mask = 0;
    for (sound::Direction d : g_landing_directions) mask |= 1u << static_cast<int>(d);
    unsigned newly_on = mask & ~g_landing_mask;
    g_landing_mask = mask;
    if (newly_on) {
        // FF5 started a newly active direction's loop at once: it pings next, now.
        for (int d = 0; d < 4; ++d) {
            if (newly_on & (1u << d)) {
                g_landing_turn = d;
                break;
            }
        }
        g_landing_next_ping = 0;
    }
}

/// The coroutine's exit path ("while (LandingPingsEnabled)" became false).
void finish_landing_ping_loop()
{
    g_landing_running = false;
    stop_landing_ping();
}

void landing_ping_step()
{
    if (!settings::get_bool(kLandingPingsKey, kLandingPingsDefault)) {
        finish_landing_ping_loop();
        return;
    }

    // Stop-gate: battle, event/cutscene, dialogue, menus, mod overlays.
    if (audio_blocked()) {
        stop_landing_ping();
        return;
    }

    double now = frame::now();
    if (now >= g_landing_next_check) {
        g_landing_next_check = now + kLandingPingInterval;
        hooks::guarded("LandingPing", [&] { landing_ping_check(now); });
    }

    // One ping per 250 ms loop, the directions taking turns.
    if (g_landing_mask == 0 || now < g_landing_next_ping)
        return;
    for (int i = 0; i < 4; ++i) {
        int d = (g_landing_turn + i) % 4;
        if (g_landing_mask & (1u << d)) {
            play_landing_ping_for(d);
            g_landing_turn = (d + 1) % 4;
            g_landing_next_ping = now + kLandingPingPeriod;
            break;
        }
    }
}

// --- frame subscriber and wiring ------------------------------------------------------------

void tick()
{
    if (g_wall_running) wall_tone_step();
    if (g_beacon_running) beacon_step();
    if (g_landing_running) landing_ping_step();
}

/// Scene loads stop audio; native MapChanged readiness restarts it.
void on_scene_loaded()
{
    // Stop audio loops during the scene transition and suppress briefly
    stop_all();
    suppress_briefly(1.0f);

    // Additive UI scenes do not signal a ready field. MapTransition owns the one
    // bounded scan at native Player/Transport entry, after construction finishes.
    // An additive UI scene can finish after Player entry. Restore only the flags
    // for field audio here; never scan entities or look for a player component.
    if (system::is_field_navigation_ready() && !queries::is("BattleStateHelper.IsInBattle")) start_if_enabled();
}

int install()
{
    frame::subscribe(&tick, "fieldnav.audio_loops");

    actions::define("Settings.Changed.WallTones", [] {
        if (settings::get_bool("WallTones", false)) start_wall_tone_loop();
        else stop_wall_tone_loop();
    });
    actions::define("Settings.Changed.AudioBeacons", [] {
        if (settings::get_bool("AudioBeacons", false)) start_beacon_loop();
        else stop_beacon_loop();
    });
    actions::define("Settings.Changed.BeaconMode", [] {
        g_beacon_silenced=false;g_next_beacon_time=0;g_last_beacon_played_at=0;
    });
    actions::define("Mod.ToggleBeaconMode", [] {
        const int mode=1-ui::prefs::beacon_mode();
        settings::set_int(ui::prefs::kBeaconMode,mode);
        ui::prefs::notify_changed(ui::prefs::kBeaconMode);
        ffpr::say(T(L"Beacon mode")+L": "+T(ui::prefs::kBeaconModeNames[mode]));
    });
    actions::define("Settings.Changed.BeaconWithDirections", [] {
        if (ui::prefs::beacon_with_directions_enabled()) {
            // Enabling the combined option must actually enable both aids. The existing
            // beacon switch can still be turned off explicitly afterwards.
            ui::prefs::save_audio_beacons(true);
            ui::prefs::notify_changed(ui::prefs::kAudioBeacons);
        }
    });
    // Landing-ping setting changes from the shared mod menu.
    actions::define("Settings.Changed.LandingPings", [] {
        if (settings::get_bool(kLandingPingsKey, kLandingPingsDefault)) start_landing_ping_loop();
        else stop_landing_ping_loop();
    });
    actions::define("Mod.RestartBeacon", [] { restart_beacon(); });
    // Re-target the beacon to the current entity; with "announce destination on beacon
    // restart" on, also re-speak the entity (same as cycling the list).
    actions::define("Mod.RestartEntityBeacon", [] {
        restart_beacon();
        if(object_beacon_remote()) return; // retain the explicit unavailable-target explanation
        if (settings::get_bool("AnnounceOnBeaconRestart", false))
            actions::run("Mod.AnnounceEntityOnly");
    });

    events::on(events::Kind::SceneLoaded, [] { on_scene_loaded(); });
    events::on(events::Kind::MapChanged, [] { start_if_enabled(); });
    events::on(events::Kind::BattleStart, [] { stop_all(); });
    events::on(events::Kind::BattleEnd, [] { start_if_enabled(); });

    // Loops are NOT started here: they start at native field readiness, or from the
    // player's toggle (as in FF1).
    return 0;
}
core::Install s_install{"AudioLoopManager", &install, 110};

} // namespace

void start_wall_tone_loop()
{
    if (g_wall_running) return;
    g_wall_running = true;
    g_wall_next_check = frame::now() + 0.3;   // first check 300 ms later, for scene stability
}

void stop_wall_tone_loop()
{
    g_wall_running = false;
    stop_tone_if_playing();
}

void start_beacon_loop()
{
    if (g_beacon_running) return;
    g_beacon_running = true;
    g_next_beacon_time = frame::now() + 0.3;  // first beacon 300 ms later, for scene stability
}

void stop_beacon_loop()
{
    g_beacon_running = false;
    g_beacon_silenced = false;
    g_last_beacon_target = TargetRef{};
}

void restart_beacon()
{
    g_beacon_silenced = false;
    g_next_beacon_time = 0;
    // Explicit navigation request, not a repeating audio-loop announcement.
    if(object_beacon_remote()) ffpr::say(T(L"Object beacon requires a destination on the current floor"));
}

void start_landing_ping_loop()
{
    if (!settings::get_bool(kLandingPingsKey, kLandingPingsDefault)) return;
    if (g_landing_running) return;
    g_landing_running = true;
    g_landing_next_check = frame::now() + 0.3;   // GameConstants.INITIAL_LOOP_DELAY
    g_landing_next_ping = 0;
    g_landing_mask = 0;
}

void stop_landing_ping_loop()
{
    g_landing_running = false;
    stop_landing_ping();
}

void stop_all()
{
    stop_wall_tone_loop();
    stop_beacon_loop();
    stop_landing_ping_loop();
}

void suppress_briefly(float seconds)
{
    double until = frame::now() + seconds;
    g_wall_tone_suppressed_until = until;
    g_beacon_suppressed_until = until;
    g_landing_suppressed_until = until;   // FF5 OnSceneTransition
}

void start_if_enabled()
{
    if (settings::get_bool("WallTones", false)) start_wall_tone_loop();
    if (settings::get_bool("AudioBeacons", false)) start_beacon_loop();
    start_landing_ping_loop();   // no-op when "LandingPings" is off
}

} // namespace ffpr::fieldnav::audio_loops
