#pragma once
#include <cstdint>
#include <vector>

// The audio area's additions to sound.h (sound.h itself is the fixed contract).
//
//  * Volume settings: FF1 Core/PreferencesManager.cs's five volume getters and clamping
//    setters (0-100, default 50; 50 = the tone's reference loudness, gain = volume / 50), plus
//    FF5's "LandingPingVolume" for sound::play_landing_ping.
//  * sound::engine: FF1 Utils/AudioEngine.cs. One SDL3 playback device with one SDL audio
//    stream per sound kind bound to it; SDL mixes the bound streams on its own audio thread.
//    The mod queues raw S16LE stereo 22050 Hz PCM (src == dst spec, no resampling) and sets a
//    per-stream gain. Looping sounds (wall tones, EXP counter) are kept fed by their caller's
//    ~100 ms tick topping up the queue; nothing here runs a timer or an audio callback.
//
// Lifetime: the device opens lazily, on the first sound request made on the main thread once
// the frame tick is live (never under the loader lock, never on the boot thread). Before
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
/// device is unavailable; audio then stays off and the mod keeps running. Main thread.
bool initialize();
/// Destroy the streams, close the device and quit ONLY the audio subsystem.
void shutdown();
bool initialized();

/// Stream gain (1.0 = unity, 0.0 = silence).
void set_gain(Stream id, float gain);
/// Bytes queued on a stream (0 when unavailable).
int queued_bytes(Stream id);
/// Drop everything queued on a stream (immediate stop).
void clear(Stream id);
/// Append PCM to a stream's queue (SDL copies it).
void submit(Stream id, const int16_t* pcm, int bytes);
void submit(Stream id, const std::vector<int16_t>& pcm);
/// One-shot: set the gain, drop what is queued, queue this buffer.
void play_one_shot(Stream id, const std::vector<int16_t>& pcm, float gain);

/// Beacon path: `fill` writes `length` bytes of PCM into the reusable scratch buffer, which
/// then replaces the Beacon stream's queue. No allocation per ping. Volume is baked into the
/// samples, so the Beacon stream keeps unity gain.
using Fill = void (*)(int16_t* buffer, void* context);
void play_beacon_direct(int length, Fill fill, void* context);

} // namespace engine
} // namespace sound
