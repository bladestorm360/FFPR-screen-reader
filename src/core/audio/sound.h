#pragma once
#include <vector>

// The mod's sound effects (FF1 Utils/SoundPlayer.cs). Implemented in src/core/audio/ on the
// SDL3 audio engine (FF1 AudioEngine.cs + ToneGenerator.cs). This header is the contract the
// field, battle and audio-loop code call; every function is safe to call when audio is
// unavailable (it does nothing).
namespace sound {

enum class Direction { North, South, East, West };

/// Open the SDL3 audio device and pre-generate the tones. Idempotent; main thread.
void initialize();
void shutdown();
bool available();

void play_wall_bump();
void play_footstep();

/// Continuous wall tones for the given directions; replaces the previous set.
void play_wall_tones_looped(const std::vector<Direction>& directions);
void stop_wall_tone();
bool wall_tone_playing();

/// One beacon ping. pan -1 (left) .. 1 (right); volume_scale 0..1.
void play_beacon(bool is_south, float pan, float volume_scale, bool low_pitch = false);

/// Battle-result EXP counter tick (a rapid ticking tone while the bar animates).
void play_exp_counter();
void top_up_exp_counter();
void stop_exp_counter();

/// Vehicle landing ping (FF1 ToneGenerator.GenerateLandingPing), panned.
void play_landing_ping(float pan, float volume_scale);

} // namespace sound
