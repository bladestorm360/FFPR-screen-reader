// Ports FF1 Core/AudioLoopManager.cs (wall-tone and beacon coroutine loops) and, from
// Core/FFI_ScreenReaderMod.cs: the reactions of ToggleWallTones / ToggleAudioBeacons,
// RestartBeacon, RestartEntityBeacon, the audio-loop part of OnSceneLoaded (StopAll +
// SuppressBriefly(1.0)) and of DelayedInitialScan (0.5 s later: scan, then StartIfEnabled on a
// field scene).
//
// The C# coroutines resumed once per frame and paced themselves with Time.time. Here one frame
// subscriber does the same with frame::now(); every early "continue" of the C# loops is a
// return that leaves the next check time where the C# code set it.
#include <cmath>
#include <memory>
#include <vector>
#include "audio_loops.h"
#include "fieldnav_util.h"
#include "map_transition.h"
#include "nav_target.h"
#include "waypoints.h"
#include "../audio/sound.h"
#include "../core.h"
#include "../frame.h"
#include "../hooks.h"
#include "../log.h"
#include "../refs.h"
#include "../settings.h"
#include "../unity.h"
#include "../field/field_api.h"

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

ref::Class kFieldPlayerController{"Last.Map", "FieldPlayerController"};

/// The beacon's target identity (C# compared the target object by reference).
struct TargetRef {
    nav_target::Kind kind = nav_target::Kind::None;
    const void* ptr = nullptr;
    waypoints::EntityPtr keep;   // keeps a waypoint alive so its address cannot be reused
    bool same(const TargetRef& o) const { return kind == o.kind && ptr == o.ptr; }
};

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
    return !queries::is("ControllerRouter.IsFieldActive")
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

void beacon_iteration(double now)
{
    TargetRef target;
    unity::Vec3 target_pos{};
    switch (nav_target::last_kind()) {
    case nav_target::Kind::Entity: {
        ::field::Target e = ::field::selected();
        if (e.valid) {
            target.kind = nav_target::Kind::Entity;
            target.ptr = e.identity;
            target_pos = e.position;
        }
        break;
    }
    case nav_target::Kind::Waypoint: {
        waypoints::EntityPtr w = waypoints::nav::selected();
        if (w) {
            target.kind = nav_target::Kind::Waypoint;
            target.ptr = w.get();
            target.keep = w;
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

    // Sanity check: skip if positions look invalid (garbage data during load)
    if (std::isnan(player_pos.x) || std::isnan(target_pos.x) ||
        std::fabs(player_pos.x) > 10000.0f || std::fabs(target_pos.x) > 10000.0f) {
        g_next_beacon_time = now + 0.2;
        return;
    }

    float dist_tiles = distance(player_pos, target_pos) / kTileSize;

    // Mode selection: expensive (A* per beacon tick) but only 1-5 Hz.
    bool path_valid;
    try {
        ::field::PathInfo path_info = ::field::find_path_to(player_pos, target_pos, ::field::map_handle(), player);
        path_valid = path_info.success;
    } catch (...) {
        path_valid = false;
    }

    float interval;
    bool low_pitch;
    if (path_valid) {
        // Mode A: valid path
        if (dist_tiles <= kBeaconStopTiles) {
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
    // takes -1 (left) .. 1 (right), which is the same position without the shift.
    float delta_x = target_pos.x - player_pos.x;
    float pan = clamp(delta_x / 100.0f, -1.0f, 1.0f);

    bool is_south = target_pos.y < player_pos.y - 8.0f;

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

// --- frame subscriber and wiring ------------------------------------------------------------

void tick()
{
    if (g_wall_running) wall_tone_step();
    if (g_beacon_running) beacon_step();
}

/// FFI_ScreenReaderMod.OnSceneLoaded (audio part) and DelayedInitialScan.
void on_scene_loaded()
{
    // Stop audio loops during the scene transition and suppress briefly
    stop_all();
    suppress_briefly(1.0f);

    // DelayedInitialScan: wait 0.5 s for the scene to initialise and entities to spawn
    frame::after_seconds(0.5, [] {
        ::field::request_rescan();
        // Only restart audio loops on field scenes (not title, boot, splash...)
        if (unity::find_object(kFieldPlayerController))
            start_if_enabled();
    }, "fieldnav.DelayedInitialScan");
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
    actions::define("Mod.RestartBeacon", [] { restart_beacon(); });
    // Re-target the beacon to the current entity; with "announce destination on beacon
    // restart" on, also re-speak the entity (same as cycling the list).
    actions::define("Mod.RestartEntityBeacon", [] {
        restart_beacon();
        if (settings::get_bool("AnnounceOnBeaconRestart", false))
            actions::run("Mod.AnnounceEntityOnly");
    });

    events::on(events::Kind::SceneLoaded, [] { on_scene_loaded(); });

    // Loops are NOT started here: they start after a scene load on a field scene, or from the
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
}

void stop_all()
{
    stop_wall_tone_loop();
    stop_beacon_loop();
}

void suppress_briefly(float seconds)
{
    double until = frame::now() + seconds;
    g_wall_tone_suppressed_until = until;
    g_beacon_suppressed_until = until;
}

void start_if_enabled()
{
    if (settings::get_bool("WallTones", false)) start_wall_tone_loop();
    if (settings::get_bool("AudioBeacons", false)) start_beacon_loop();
}

} // namespace ffpr::fieldnav::audio_loops
