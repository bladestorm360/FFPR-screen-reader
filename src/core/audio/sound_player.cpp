// Ports FF1 Utils/SoundPlayer.cs (the sound.h contract) and the volume getters/setters of
// FF1 Core/PreferencesManager.cs.
//
// High-level playback: tones come from tone_generator (ToneGenerator.cs), output and mixing
// from engine (AudioEngine.cs). The device opens lazily on the first sound requested on the
// first live frame. The dedicated SDL owner initializes subsystems, the audio
// worker owns device/stream calls, and the Install below builds the tone tables.
#include "sound.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include "audio_engine.h"
#include "sound_constants.h"
#include "tone_generator.h"
#include "../core.h"
#include "../frame.h"
#include "../hooks.h"
#include "../log.h"
#include "../settings.h"
#include "../input/physical_gamepad.h"

namespace sound {
namespace {

namespace C = constants;
using engine::Stream;

constexpr double kPi = 3.14159265358979323846;

// --- pre-cached sounds ------------------------------------------------------------------------
std::once_flag g_tables_once;
tone::Pcm g_wall_bump;
tone::Pcm g_footstep;
tone::Pcm g_exp_counter;
// Sustain wall tones, one per direction (indexed by Direction), for looping. Generated at
// REFERENCE amplitude (BASE_VOLUME x direction multiplier, pan baked in). User volume and
// clipping headroom are applied at play time as stream gain; SDL sums the active streams.
tone::Pcm g_wall_sustain[4];

// --- playback state (main thread) -------------------------------------------------------------
// Current wall-tone directions as a bitmask, to tell newly-on from already-on directions
// across the ~100 ms audio-loop ticks.
int g_current_wall_mask = 0;
// EXP counter loop state.
bool g_exp_counter_active = false;

// --- lazy device open -------------------------------------------------------------------------
std::atomic<bool> g_open_tried{false};    // AudioEngine.Initialize runs once per initialize()
std::atomic<bool> g_shut_down{false};     // shutdown() stops lazy reopening

void build_tables()
{
    // Wall bump: deep thud with soft attack.
    g_wall_bump = tone::mono_to_stereo(
        tone::thud_tone(C::WallBump::FREQUENCY, C::WallBump::DURATION_MS, C::WallBump::VOLUME));

    // Footstep: light click.
    g_footstep = tone::mono_to_stereo(
        tone::click_tone(C::Footstep::FREQUENCY, C::Footstep::DURATION_MS, C::Footstep::VOLUME));

    const float bv = C::WallToneVolumeMultipliers::BASE_VOLUME;
    const int sustain = C::WallToneTiming::SUSTAIN_DURATION_MS;

    // Sustain tones (no decay, cycle-aligned for seamless looping) at reference amplitude.
    g_wall_sustain[static_cast<int>(Direction::North)] = tone::stereo_tone(
        C::WallToneFrequencies::NORTH, sustain, bv * C::WallToneVolumeMultipliers::NORTH, C::WallTonePan::NORTH, true);
    g_wall_sustain[static_cast<int>(Direction::South)] = tone::stereo_tone(
        C::WallToneFrequencies::SOUTH, sustain, bv * C::WallToneVolumeMultipliers::SOUTH, C::WallTonePan::SOUTH, true);
    g_wall_sustain[static_cast<int>(Direction::East)] = tone::stereo_tone(
        C::WallToneFrequencies::EAST, sustain, bv * C::WallToneVolumeMultipliers::EAST, C::WallTonePan::EAST, true);
    g_wall_sustain[static_cast<int>(Direction::West)] = tone::stereo_tone(
        C::WallToneFrequencies::WEST, sustain, bv * C::WallToneVolumeMultipliers::WEST, C::WallTonePan::WEST, true);

    // EXP counter beep: short tone + silence for a rapid ticking effect (volume baked in).
    g_exp_counter = tone::landing_ping(C::ExpCounter::FREQUENCY, C::ExpCounter::BEEP_MS + C::ExpCounter::SILENCE_MS,
                                       C::ExpCounter::BEEP_MS, C::ExpCounter::VOLUME, 0.5f);   // centre pan
}

/// Request device preparation without waiting. True only after worker publication.
bool ensure_open()
{
    if (engine::initialized()) return true;
    if (g_open_tried.load() || g_shut_down.load()) return false;
    if (!frame::live() || !frame::on_main_thread()) return false;
    initialize();
    return engine::initialized();
}

/// sound.h pans are -1 (left) .. 1 (right); the tone maths (as in C#) uses 0 .. 1.
float pan_to_unit(float pan)
{
    if (!(pan >= -1.0f)) pan = -1.0f;   // also catches NaN
    if (pan > 1.0f) pan = 1.0f;
    return pan * 0.5f + 0.5f;
}

int to_mask(const std::vector<Direction>& dirs)
{
    int mask = 0;
    for (Direction d : dirs) mask |= (1 << static_cast<int>(d));
    return mask;
}

int count_bits(int mask)
{
    int count = 0;
    while (mask != 0) {
        count += mask & 1;
        mask >>= 1;
    }
    return count;
}

Stream direction_stream(Direction dir)
{
    switch (dir) {
    case Direction::North: return Stream::WallNorth;
    case Direction::South: return Stream::WallSouth;
    case Direction::East: return Stream::WallEast;
    case Direction::West: return Stream::WallWest;
    default: return Stream::WallNorth;
    }
}

/// Activate / refresh / deactivate one direction's stream for this tick. Reads the OLD mask
/// to tell newly-on from already-on.
void update_direction_stream(Direction dir, int new_mask, float gain)
{
    int bit = 1 << static_cast<int>(dir);
    bool now_active = (new_mask & bit) != 0;
    bool was_active = (g_current_wall_mask & bit) != 0;
    Stream stream = direction_stream(dir);

    if (!now_active) {
        if (was_active) engine::clear(stream);
        return;
    }

    const tone::Pcm& buf = g_wall_sustain[static_cast<int>(dir)];
    if (buf.empty()) return;
    int len = static_cast<int>(buf.size() * sizeof(int16_t));

    engine::set_gain(stream, gain);

    if (!was_active) {
        // Newly active: prime ~2 loops ahead so the queue can't drain before the next tick.
        engine::clear(stream);
        engine::submit(stream, buf);
        engine::submit(stream, buf);
    } else if (engine::queued_bytes(stream) < len * 2) {
        // Keep ~2 buffers queued (about 2 ticks) so back-to-back loops stay seamless.
        engine::submit(stream, buf);
    }
}

// Beacon synthesis straight into the engine's scratch buffer (no allocation per ping).
struct BeaconFill {
    int samples;
    int frequency;
    float left_vol;
    float right_vol;
};

void fill_beacon(int16_t* buffer, void* context)
{
    const BeaconFill& f = *static_cast<const BeaconFill*>(context);
    int attack_samples = f.samples / 10;
    for (int i = 0; i < f.samples; i++) {
        double t = static_cast<double>(i) / C::SAMPLE_RATE;
        double attack = std::min(1.0, static_cast<double>(i) / attack_samples);
        double decay = static_cast<double>(f.samples - i) / f.samples;
        double envelope = attack * decay;
        double sine_value = std::sin(2 * kPi * f.frequency * t) * envelope;
        buffer[i * 2] = static_cast<int16_t>(static_cast<int32_t>(sine_value * f.left_vol * 32767));
        buffer[i * 2 + 1] = static_cast<int16_t>(static_cast<int32_t>(sine_value * f.right_vol * 32767));
    }
}

int install()
{
    // Tables only: the device opens later, on the main thread, at the first sound.
    prepare_tables();

    // FFI_ScreenReaderMod.ToggleExpCounter: turned off mid-tally stops the tone now.
    actions::define("Settings.Changed.ExpCounter", [] {
        if (!settings::get_bool("ExpCounter", true)) stop_exp_counter();
    });
    return 0;
}

core::Install s_install{"SoundPlayer", &install, 20};

} // namespace

// --- volume settings (PreferencesManager) ------------------------------------------------------

const char* volume_key(Volume v)
{
    switch (v) {
    case Volume::WallBump: return "WallBumpVolume";
    case Volume::Footstep: return "FootstepVolume";
    case Volume::WallTone: return "WallToneVolume";
    case Volume::Beacon: return "BeaconVolume";
    case Volume::ExpCounter: return "ExpCounterVolume";
    case Volume::LandingPing: return "LandingPingVolume";
    default: return "WallToneVolume";
    }
}

int volume(Volume v) { return settings::get_int(volume_key(v), 50); }

void set_volume(Volume v, int value) { settings::set_int(volume_key(v), std::clamp(value, 0, 100)); }

void prepare_tables()
{
    std::call_once(g_tables_once, [] { hooks::guarded("SoundPlayer.Tables", [] { build_tables(); }); });
}

// --- sound.h -----------------------------------------------------------------------------------

void initialize()
{
    g_shut_down = false;
    if (engine::initialized() || g_open_tried.load()) return;
    if (!frame::live() || !frame::on_main_thread()) return;
    g_open_tried = true;
    input::physical::request(); // owned flag/event only; no SDK call or waiting
}

void shutdown()
{
    g_shut_down = true;
    engine::shutdown();
    g_open_tried = false;
    g_current_wall_mask = 0;
    g_exp_counter_active = false;
}

bool available() { return ensure_open(); }

void play_wall_bump()
{
    if (!ensure_open() || g_wall_bump.empty()) return;
    float gain = volume(Volume::WallBump) / 50.0f;
    engine::play_one_shot(Stream::WallBump, g_wall_bump, gain);
}

void play_footstep()
{
    if (!ensure_open() || g_footstep.empty()) return;
    float gain = volume(Volume::Footstep) / 50.0f;
    engine::play_one_shot(Stream::Footstep, g_footstep, gain);
}

void play_wall_tones_looped(const std::vector<Direction>& directions)
{
    int new_mask = directions.empty() ? 0 : to_mask(directions);
    if (new_mask == 0) {
        if (!engine::initialized()) return;
        if (g_current_wall_mask != 0) stop_wall_tone();
        return;
    }
    if (!ensure_open()) return;

    int vol = volume(Volume::WallTone);
    int active_count = count_bits(new_mask);

    // User volume x clipping headroom (1/sqrt(n) for n simultaneous directions), as stream
    // gain; the pan stays baked into the samples.
    float gain = (vol / 50.0f) * (active_count > 1 ? static_cast<float>(1.0 / std::sqrt(active_count)) : 1.0f);

    // Reconcile each stream against the OLD mask, then commit the new one.
    update_direction_stream(Direction::North, new_mask, gain);
    update_direction_stream(Direction::South, new_mask, gain);
    update_direction_stream(Direction::East, new_mask, gain);
    update_direction_stream(Direction::West, new_mask, gain);

    g_current_wall_mask = new_mask;
}

void stop_wall_tone()
{
    g_current_wall_mask = 0;
    engine::clear(Stream::WallNorth);
    engine::clear(Stream::WallSouth);
    engine::clear(Stream::WallEast);
    engine::clear(Stream::WallWest);
}

bool wall_tone_playing() { return g_current_wall_mask != 0; }

void play_beacon(bool is_south, float pan, float volume_scale, bool low_pitch)
{
    hooks::guarded("SoundPlayer.PlayBeacon", [&] {
        if (!ensure_open()) return;

        int frequency = is_south ? C::Beacon::FREQUENCY_SOUTH : C::Beacon::FREQUENCY_NORTH;
        if (low_pitch) frequency /= 2;
        float pref_multiplier = volume(Volume::Beacon) / 50.0f;
        float vol = std::max(C::Beacon::MIN_VOLUME, std::min(C::Beacon::MAX_VOLUME, volume_scale * pref_multiplier));

        int samples = (C::SAMPLE_RATE * C::Beacon::DURATION_MS) / 1000;
        int data_length = samples * 4;   // stereo 16-bit

        double pan_angle = pan_to_unit(pan) * kPi / 2;
        BeaconFill fill{samples, frequency, vol * static_cast<float>(std::cos(pan_angle)),
                        vol * static_cast<float>(std::sin(pan_angle))};
        engine::play_generated(Stream::Beacon, data_length, [fill](int16_t* pcm) mutable {
            fill_beacon(pcm, &fill);
        });
    });
}

void play_exp_counter()
{
    if (!ensure_open() || g_exp_counter.empty()) return;

    engine::set_gain(Stream::Counter, volume(Volume::ExpCounter) / 50.0f);

    // Prime ~2 loops ahead so the queue can't drain before the next top-up tick.
    engine::clear(Stream::Counter);
    engine::submit(Stream::Counter, g_exp_counter);
    engine::submit(Stream::Counter, g_exp_counter);
    g_exp_counter_active = true;
}

void top_up_exp_counter()
{
    if (!engine::initialized() || !g_exp_counter_active || g_exp_counter.empty()) return;
    int len = static_cast<int>(g_exp_counter.size() * sizeof(int16_t));

    // Keep ~2 buffers queued (about 2 ticks) so back-to-back loops stay seamless.
    if (engine::queued_bytes(Stream::Counter) < len * 2)
        engine::submit(Stream::Counter, g_exp_counter);
}

void stop_exp_counter()
{
    g_exp_counter_active = false;
    engine::clear(Stream::Counter);
}

void play_landing_ping(float pan, float volume_scale, int frequency)
{
    // No FF1 source (FF1 never plays one): FF5's landing-ping tone as a panned one-shot,
    // amplitude BASE_VOLUME x volume_scale, gain from "LandingPingVolume".
    hooks::guarded("SoundPlayer.PlayLandingPing", [&] {
        if (!ensure_open()) return;
        float scale = std::clamp(volume_scale, 0.0f, 1.0f);
        const int pitch = frequency > 0 ? frequency : C::LandingPing::FREQUENCY;
        const float position = pan_to_unit(pan);
        const int bytes = (C::SAMPLE_RATE * C::LandingPing::TOTAL_MS / 1000) * 4;
        engine::play_generated(Stream::Landing, bytes, [pitch, scale, position, bytes](int16_t* pcm) {
            auto ping = tone::landing_ping(pitch, C::LandingPing::TOTAL_MS, C::LandingPing::PING_MS,
                C::LandingPing::BASE_VOLUME * scale, position);
            if (ping.size() * sizeof(int16_t) == static_cast<size_t>(bytes)) memcpy(pcm, ping.data(), bytes);
            else memset(pcm, 0, bytes);
        }, volume(Volume::LandingPing) / 50.0f);
    });
}

} // namespace sound
