#pragma once
#include <cstdint>
#include <vector>
#include <functional>

// The audio area's additions to sound.h (sound.h itself is the fixed contract).
//
//  * Volume settings: FF1 Core/PreferencesManager.cs's five volume getters and clamping
//    setters (0-100, default 50; 50 = the tone's reference loudness, gain = volume / 50), plus
//    FF5's "LandingPingVolume" for sound::play_landing_ping.
//  * sound::engine: FF1 Utils/AudioEngine.cs. One SDL3 playback device with one SDL audio
//    stream per sound kind bound to it; SDL mixes the bound streams on its own audio thread.
//    The mod queues raw S16LE stereo 22050 Hz PCM (src == dst spec, no resampling) and sets a
//    per-stream gain. Looping sounds (wall tones, EXP counter) are kept fed by their caller's
//    ~100 ms tick topping up the queue. The audio worker refreshes byte snapshots;
//    no SDL device/stream call runs on a game hook.
//
// Lifetime: the first live frame initializes the SDL audio subsystem on its required
// owner thread and requests device construction on the worker, without waiting. Before
// that, and when SDL3.dll or the device is unavailable, every call does nothing.
namespace sound {

enum class Volume { WallBump, Footstep, WallTone, Beacon, ExpCounter, LandingPing };

/// The settings key ("WallBumpVolume", "FootstepVolume", "WallToneVolume", "BeaconVolume",
/// "ExpCounterVolume", "LandingPingVolume").
const char* volume_key(Volume v);
/// PreferencesManager.<X>Volume: the stored value (default 50).
int volume(Volume v);
/// PreferencesManager.Set<X>Volume: clamp to 0..100 and store.
void set_volume(Volume v, int value);

/// Generate the cached tones (FF1 SoundPlayer.Initialize's tables). Idempotent, any thread.
/// The audio Install runs it on the boot thread; the first sound runs it if that did not.
void prepare_tables();

namespace engine {

/// AudioEngine.Stream, plus Landing for sound::play_landing_ping.
enum class Stream : int {
    Footstep,
    WallBump,
    Beacon,
    WallNorth,
    WallSouth,
    WallEast,
    WallWest,
    Counter,
    Landing,
    Count
};

/// Open the device and create and bind every stream. False (logged) when SDL3.dll or the
/// device is unavailable; audio then stays off and the mod keeps running. Main thread
/// requests construction; return is current readiness, not a blocking result.
bool initialize();
/// Request worker teardown without waiting; SDL subsystem remains process-owned.
void shutdown();
bool initialized();

/// Stream gain (1.0 = unity, 0.0 = silence).
void set_gain(Stream id, float gain);
/// Worker snapshot plus pending PCM bytes (0 when unavailable), without SDL calls.
int queued_bytes(Stream id);
/// Publish cancellation immediately; the worker clears without a caller wait.
void clear(Stream id);
/// Copy PCM into a bounded command slot; the worker submits it to SDL.
void submit(Stream id, const int16_t* pcm, int bytes);
void submit(Stream id, const std::vector<int16_t>& pcm);
/// One-shot: set the gain, drop what is queued, queue this buffer.
void play_one_shot(Stream id, const std::vector<int16_t>& pcm, float gain);

/// Copied, game-pointer-free synthesis closure, executed only on the audio worker.
void play_generated(Stream id, int bytes, std::function<void(int16_t*)> generate, float gain = 1);

} // namespace engine
} // namespace sound
