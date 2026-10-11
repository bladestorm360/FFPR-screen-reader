#pragma once
// Ports FF1 Core/PreferencesManager.cs, plus FF5's LandingPings, LandingPingVolume and
// EnemyLetters entries (FF5 Core/PreferencesManager.cs: same keys and defaults).
//
// Every MelonPreferences entry of FF1's [FFI_ScreenReader] category: its key (the native
// settings key keeps the MelonPreferences name, so the one-time import is a copy), its default,
// and FF1's getters and setters. Every area reads a setting through these names so all of them
// agree on the default. settings.h stores the values; nothing here keeps state.
//
// FF1 setters saved the MelonPreferences file at once; settings:: saves off the game thread.
// After changing a setting, run notify_changed(key): it runs "Settings.Changed.<Key>" when an
// area defines it (audio loops, entity filters react there).
#include <algorithm>
#include <string>
#include "core.h"
#include "settings.h"

namespace ui::prefs {

// --- keys ---------------------------------------------------------------------------------
// key                                                                    FF1 display name
inline constexpr const char* kPathfindingFilter = "PathfindingFilter";      // Pathfinding Filter (the old on/off; read once for the mode)
// Pathfinding filter mode (user, 2026-10-11): 0 Current floor (walking reaches it on this
// sub-map), 1 Current map with vehicles (a vehicle the party has here reaches it too),
// 2 Current map (everything; a target cut off on this floor is routed through another floor).
inline constexpr const char* kPathfindingFilterMode = "PathfindingFilterMode";
inline constexpr int kFilterCurrentFloor = 0;
inline constexpr int kFilterCurrentMapWithVehicles = 1;
inline constexpr int kFilterCurrentMap = 2;
inline constexpr const char* kMapExitFilter = "MapExitFilter";              // Map Exit Filter
inline constexpr const char* kToLayerFilter = "ToLayerFilter";              // Layer Transition Filter
inline constexpr const char* kWallTones = "WallTones";                      // Wall Tones
inline constexpr const char* kFootsteps = "Footsteps";                      // Footsteps
inline constexpr const char* kAudioBeacons = "AudioBeacons";                // Beacon Navigation
inline constexpr const char* kAutoDetail = "AutoDetail";                    // Auto Announcement Detail
inline constexpr const char* kStickClickNormalization = "StickClickNormalization"; // Stick Click Normalization
inline constexpr const char* kAnnounceOnBeaconRestart = "AnnounceOnBeaconRestart"; // Announce Destination on Beacon Restart
inline constexpr const char* kMenuPositionAnnouncements = "MenuPositionAnnouncements"; // Menu Position Announcements
inline constexpr const char* kExpCounter = "ExpCounter";                    // EXP Counter Sound
inline constexpr const char* kWallBumpVolume = "WallBumpVolume";            // Wall Bump Volume (0-100)
inline constexpr const char* kFootstepVolume = "FootstepVolume";            // Footstep Volume (0-100)
inline constexpr const char* kWallToneVolume = "WallToneVolume";            // Wall Tone Volume (0-100)
inline constexpr const char* kBeaconVolume = "BeaconVolume";                // Beacon Volume (0-100)
inline constexpr const char* kExpCounterVolume = "ExpCounterVolume";        // EXP Counter Volume (0-100)
inline constexpr const char* kEnemyHPDisplay = "EnemyHPDisplay";            // Enemy HP Display: 0 Numbers, 1 Percentage, 2 Hidden
// Stored as "MultiHitDamage", not the obsolete "DamageDisplay" entry (FF1 moved everyone to the
// new default once). 0 = Total only, 1 = With hit count ("14x1552 damage").
inline constexpr const char* kMultiHitDamage = "MultiHitDamage";            // Multi-hit Damage
// FF5's entries (FF5 PreferencesManager), kept by their MelonPreferences names.
inline constexpr const char* kLandingPings = "LandingPings";                // Landing Pings
inline constexpr const char* kLandingPingVolume = "LandingPingVolume";      // Landing Ping Volume (0-100)
inline constexpr const char* kEnemyLetters = "EnemyLetters";                // Enemy Letters (FF5 only)

// --- defaults -----------------------------------------------------------------------------
inline constexpr bool kPathfindingFilterDefault = false;
inline constexpr bool kMapExitFilterDefault = false;
inline constexpr bool kToLayerFilterDefault = false;
inline constexpr bool kWallTonesDefault = false;
inline constexpr bool kFootstepsDefault = false;
inline constexpr bool kAudioBeaconsDefault = false;
inline constexpr bool kAutoDetailDefault = false;
inline constexpr bool kStickClickNormalizationDefault = false;
inline constexpr bool kAnnounceOnBeaconRestartDefault = false;
inline constexpr bool kMenuPositionAnnouncementsDefault = false;
inline constexpr bool kExpCounterDefault = true;
inline constexpr int kWallBumpVolumeDefault = 50;
inline constexpr int kFootstepVolumeDefault = 50;
inline constexpr int kWallToneVolumeDefault = 50;
inline constexpr int kBeaconVolumeDefault = 50;
inline constexpr int kExpCounterVolumeDefault = 50;
inline constexpr int kEnemyHPDisplayDefault = 0;
inline constexpr int kMultiHitDamageDefault = 1;
inline constexpr bool kLandingPingsDefault = false;
inline constexpr int kLandingPingVolumeDefault = 50;
inline constexpr bool kEnemyLettersDefault = false;

// --- ranges (FF1's setter clamps) -----------------------------------------------------------
inline constexpr int kVolumeMin = 0;
inline constexpr int kVolumeMax = 100;
inline constexpr int kEnemyHPDisplayMax = 2;
inline constexpr int kMultiHitDamageMax = 1;
inline constexpr int kPathfindingFilterModeDefault = kFilterCurrentMap;
inline constexpr int kPathfindingFilterModeMax = 2;
/// The spoken names of the three modes, mod-text keys in mode order.
inline const wchar_t* const kPathfindingFilterModeNames[3] = {L"Current floor", L"Current map with vehicles", L"Current map"};

// --- change notification ------------------------------------------------------------------
/// Run "Settings.Changed.<key>" when an area defines it.
inline void notify_changed(const char* key)
{
    const std::string name = std::string("Settings.Changed.") + key;
    if (actions::defined(name.c_str())) actions::run(name.c_str());
}

// --- toggle getters (FF1: single source of truth, read directly by menu, loops, gameplay) ---
inline bool pathfinding_filter_enabled() { return settings::get_bool(kPathfindingFilter, kPathfindingFilterDefault); }
inline bool map_exit_filter_enabled() { return settings::get_bool(kMapExitFilter, kMapExitFilterDefault); }
inline bool to_layer_filter_enabled() { return settings::get_bool(kToLayerFilter, kToLayerFilterDefault); }
inline bool wall_tones_enabled() { return settings::get_bool(kWallTones, kWallTonesDefault); }
inline bool footsteps_enabled() { return settings::get_bool(kFootsteps, kFootstepsDefault); }
inline bool audio_beacons_enabled() { return settings::get_bool(kAudioBeacons, kAudioBeaconsDefault); }
inline bool auto_detail_enabled() { return settings::get_bool(kAutoDetail, kAutoDetailDefault); }
inline bool stick_click_normalization_enabled() { return settings::get_bool(kStickClickNormalization, kStickClickNormalizationDefault); }
inline bool announce_on_beacon_restart_enabled() { return settings::get_bool(kAnnounceOnBeaconRestart, kAnnounceOnBeaconRestartDefault); }
inline bool menu_position_announcements_enabled() { return settings::get_bool(kMenuPositionAnnouncements, kMenuPositionAnnouncementsDefault); }
inline bool exp_counter_enabled() { return settings::get_bool(kExpCounter, kExpCounterDefault); }
inline bool landing_pings_enabled() { return settings::get_bool(kLandingPings, kLandingPingsDefault); }
inline bool enemy_letters_enabled() { return settings::get_bool(kEnemyLetters, kEnemyLettersDefault); }

// --- value getters ------------------------------------------------------------------------
inline int wall_bump_volume() { return settings::get_int(kWallBumpVolume, kWallBumpVolumeDefault); }
inline int footstep_volume() { return settings::get_int(kFootstepVolume, kFootstepVolumeDefault); }
inline int wall_tone_volume() { return settings::get_int(kWallToneVolume, kWallToneVolumeDefault); }
inline int beacon_volume() { return settings::get_int(kBeaconVolume, kBeaconVolumeDefault); }
inline int exp_counter_volume() { return settings::get_int(kExpCounterVolume, kExpCounterVolumeDefault); }
inline int landing_ping_volume() { return settings::get_int(kLandingPingVolume, kLandingPingVolumeDefault); }
inline int enemy_hp_display() { return settings::get_int(kEnemyHPDisplay, kEnemyHPDisplayDefault); }
/// FF1 PreferencesManager.DamageDisplay (stored as MultiHitDamage).
inline int damage_display() { return settings::get_int(kMultiHitDamage, kMultiHitDamageDefault); }

// --- value setters (FF1: clamp, then save) --------------------------------------------------
/// FF1 SetIntPreference.
inline void set_int_clamped(const char* key, int value, int min, int max)
{
    settings::set_int(key, std::clamp(value, min, max));
}
inline void set_wall_bump_volume(int v) { set_int_clamped(kWallBumpVolume, v, kVolumeMin, kVolumeMax); }
inline void set_footstep_volume(int v) { set_int_clamped(kFootstepVolume, v, kVolumeMin, kVolumeMax); }
inline void set_wall_tone_volume(int v) { set_int_clamped(kWallToneVolume, v, kVolumeMin, kVolumeMax); }
inline void set_beacon_volume(int v) { set_int_clamped(kBeaconVolume, v, kVolumeMin, kVolumeMax); }
inline void set_exp_counter_volume(int v) { set_int_clamped(kExpCounterVolume, v, kVolumeMin, kVolumeMax); }
inline void set_landing_ping_volume(int v) { set_int_clamped(kLandingPingVolume, v, kVolumeMin, kVolumeMax); }
inline void set_enemy_hp_display(int v) { set_int_clamped(kEnemyHPDisplay, v, 0, kEnemyHPDisplayMax); }
inline void set_damage_display(int v) { set_int_clamped(kMultiHitDamage, v, 0, kMultiHitDamageMax); }

// --- toggle persistence (FF1 Save*) -------------------------------------------------------
inline void save_pathfinding_filter(bool v) { settings::set_bool(kPathfindingFilter, v); }
inline void save_map_exit_filter(bool v) { settings::set_bool(kMapExitFilter, v); }
inline void save_to_layer_filter(bool v) { settings::set_bool(kToLayerFilter, v); }
inline void save_wall_tones(bool v) { settings::set_bool(kWallTones, v); }
inline void save_footsteps(bool v) { settings::set_bool(kFootsteps, v); }
inline void save_audio_beacons(bool v) { settings::set_bool(kAudioBeacons, v); }
inline void save_auto_detail(bool v) { settings::set_bool(kAutoDetail, v); }
inline void save_stick_click_normalization(bool v) { settings::set_bool(kStickClickNormalization, v); }
inline void save_announce_on_beacon_restart(bool v) { settings::set_bool(kAnnounceOnBeaconRestart, v); }
inline void save_menu_position_announcements(bool v) { settings::set_bool(kMenuPositionAnnouncements, v); }
inline void save_exp_counter(bool v) { settings::set_bool(kExpCounter, v); }
inline void save_landing_pings(bool v) { settings::set_bool(kLandingPings, v); }
inline void save_enemy_letters(bool v) { settings::set_bool(kEnemyLetters, v); }

} // namespace ui::prefs
