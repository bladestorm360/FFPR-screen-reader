#pragma once

// Ports FF1 Core/AudioLoopManager.cs: the wall-tone loop and the audio-beacon loop, plus the
// audio-loop parts of FFI_ScreenReaderMod (ToggleWallTones / ToggleAudioBeacons reactions,
// RestartBeacon, RestartEntityBeacon, the OnSceneLoaded stop/suppress and the restart in
// native MapChanged readiness).
//
// Both loops are allowed per-frame audio work. One frame subscriber runs them with the C#
// coroutines' cadence: wall tones every 100 ms, beacons on their own proximity interval, each
// starting 300 ms after it is started.
//
// From FF5 Core/AudioLoopManager.cs: the landing-ping loop ("LandingPings", default off; volume
// "LandingPingVolume"), checked every 100 ms while on a ship-type vehicle.
//
// Actions defined:
//   "Settings.Changed.WallTones"    start/stop the wall-tone loop to match the "WallTones" setting
//   "Settings.Changed.AudioBeacons" start/stop the beacon loop to match "AudioBeacons"
//   "Settings.Changed.BeaconMode" reset cadence for route/object targeting
//   "Mod.ToggleBeaconMode" switch saved route/object targeting (Shift+F6)
//   "Settings.Changed.LandingPings" start/stop the landing-ping loop to match "LandingPings"
//   "Mod.RestartBeacon"             FFI_ScreenReaderMod.RestartBeacon
//   "Mod.RestartEntityBeacon"       FFI_ScreenReaderMod.RestartEntityBeacon
namespace ffpr::fieldnav::audio_loops {

void start_wall_tone_loop();
void stop_wall_tone_loop();
void start_beacon_loop();
void stop_beacon_loop();
/// FF5 Start/StopLandingPingLoop (start is a no-op while "LandingPings" is off).
void start_landing_ping_loop();
void stop_landing_ping_loop();
/// Force a ping on the next beacon iteration and clear the arrival-silence latch.
void restart_beacon();
void stop_all();
/// Suppress wall tones, beacons and landing pings for `seconds` (scene loads, map transitions).
void suppress_briefly(float seconds);
/// Start the loops whose settings are on (after a scene load on a field scene).
void start_if_enabled();

} // namespace ffpr::fieldnav::audio_loops
