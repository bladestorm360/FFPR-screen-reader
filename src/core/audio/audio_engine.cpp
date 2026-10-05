// Ports FF1 Utils/AudioEngine.cs.
//
// SDL3-backed output: one logical playback device with one SDL_AudioStream per sound kind
// bound to it. SDL mixes every bound stream on its own device thread, so the game's main
// thread only queues buffers (SDL_PutAudioStreamData copies synchronously) and sets gains.
// Every SDL call goes through sdl_dyn.h (no import library); sdl::load() runs first.
#include "audio_engine.h"
#include <atomic>
#include "sound_constants.h"
#include "../log.h"
#include "../sdl_dyn.h"

namespace sound::engine {
namespace {

constexpr int kStreamCount = static_cast<int>(Stream::Count);

SDL_AudioDeviceID g_device = 0;
SDL_AudioStream* g_streams[kStreamCount] = {};
std::vector<int16_t> g_beacon_scratch;   // CHANNEL_BUFFER_SIZE bytes
std::atomic<bool> g_initialized{false};

const char* stream_name(int i)
{
    static const char* const kNames[kStreamCount] = {
        "Footstep", "WallBump", "Beacon", "WallNorth", "WallSouth", "WallEast", "WallWest",
        "Counter", "Landing"};
    return (i >= 0 && i < kStreamCount) ? kNames[i] : "?";
}

/// SDL's last error, or a placeholder (used by the failure logs).
const char* sdl_error()
{
    const char* e = sdl::SDL_GetError ? sdl::SDL_GetError() : nullptr;
    return (e && *e) ? e : "(no SDL error)";
}

SDL_AudioStream* stream_of(Stream id)
{
    int i = static_cast<int>(id);
    if (!g_initialized.load() || i < 0 || i >= kStreamCount) return nullptr;
    return g_streams[i];
}

void release_all()
{
    for (int i = 0; i < kStreamCount; i++) {
        if (g_streams[i]) {
            try { sdl::SDL_ClearAudioStream(g_streams[i]); } catch (...) {}
            try { sdl::SDL_DestroyAudioStream(g_streams[i]); } catch (...) {}
            g_streams[i] = nullptr;
        }
    }
    if (g_device != 0) {
        try { sdl::SDL_CloseAudioDevice(g_device); } catch (...) {}
        g_device = 0;
    }
    g_beacon_scratch.clear();
    g_beacon_scratch.shrink_to_fit();
}

} // namespace

bool initialize()
{
    if (g_initialized.load()) return true;
    try {
        // Subsystem-scoped: independent of the gamepad's SDL_INIT_GAMEPAD (SDL ref-counts each
        // subsystem). Without SDL3.dll there is no playback path at all.
        if (!sdl::load()) {
            LOGF("[SDL Audio] SDL3.dll NOT FOUND or unusable - audio DISABLED (no fallback; nothing will play)");
            return false;
        }
        if (!sdl::SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            LOGF("[SDL Audio] SDL_InitSubSystem(AUDIO) failed: %s - audio DISABLED (no fallback)", sdl_error());
            return false;
        }

        SDL_AudioSpec spec{};
        spec.format = SDL_AUDIO_S16LE;
        spec.channels = 2;
        spec.freq = constants::SAMPLE_RATE;

        g_device = sdl::SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
        if (g_device == 0) {
            LOGF("[SDL Audio] SDL_OpenAudioDevice failed: %s - audio DISABLED (no fallback)", sdl_error());
            sdl::SDL_QuitSubSystem(SDL_INIT_AUDIO);
            return false;
        }
        for (int i = 0; i < kStreamCount; i++) {
            SDL_AudioStream* s = sdl::SDL_CreateAudioStream(&spec, &spec);
            if (!s) {
                LOGF("[SDL Audio] SDL_CreateAudioStream failed for %s: %s", stream_name(i), sdl_error());
            } else if (!sdl::SDL_BindAudioStream(g_device, s)) {
                LOGF("[SDL Audio] SDL_BindAudioStream failed for %s: %s", stream_name(i), sdl_error());
                sdl::SDL_DestroyAudioStream(s);
                s = nullptr;
            }
            g_streams[i] = s;
        }

        // A freshly opened device starts paused; resuming a running one is harmless. Skipping
        // this is a silent-no-audio failure.
        if (!sdl::SDL_ResumeAudioDevice(g_device))
            LOGF("[SDL Audio] SDL_ResumeAudioDevice failed: %s", sdl_error());

        g_beacon_scratch.assign(constants::CHANNEL_BUFFER_SIZE / sizeof(int16_t), 0);

        g_initialized = true;
        LOGF("[SDL Audio] device %u open, %d streams, S16LE stereo %d Hz", static_cast<unsigned>(g_device),
             kStreamCount, constants::SAMPLE_RATE);
        return true;
    } catch (...) {
        LOGF("[SDL Audio] initialization failed - audio DISABLED");
        g_initialized = false;
        release_all();   // each SDL call inside is guarded
        return false;
    }
}

void shutdown()
{
    if (!g_initialized.exchange(false)) return;
    release_all();
    try { sdl::SDL_QuitSubSystem(SDL_INIT_AUDIO); } catch (...) {}
}

bool initialized() { return g_initialized.load(); }

void set_gain(Stream id, float gain)
{
    SDL_AudioStream* s = stream_of(id);
    if (!s) return;
    sdl::SDL_SetAudioStreamGain(s, gain);
}

int queued_bytes(Stream id)
{
    SDL_AudioStream* s = stream_of(id);
    if (!s) return 0;
    int q = sdl::SDL_GetAudioStreamQueued(s);
    return q < 0 ? 0 : q;
}

void clear(Stream id)
{
    SDL_AudioStream* s = stream_of(id);
    if (!s) return;
    sdl::SDL_ClearAudioStream(s);
}

void submit(Stream id, const int16_t* pcm, int bytes)
{
    SDL_AudioStream* s = stream_of(id);
    if (!s || !pcm || bytes <= 0) return;
    sdl::SDL_PutAudioStreamData(s, pcm, bytes);
}

void submit(Stream id, const std::vector<int16_t>& pcm)
{
    if (pcm.empty()) return;
    submit(id, pcm.data(), static_cast<int>(pcm.size() * sizeof(int16_t)));
}

void play_one_shot(Stream id, const std::vector<int16_t>& pcm, float gain)
{
    SDL_AudioStream* s = stream_of(id);
    if (!s) return;
    sdl::SDL_SetAudioStreamGain(s, gain);
    sdl::SDL_ClearAudioStream(s);
    submit(id, pcm);
}

void play_beacon_direct(int length, Fill fill, void* context)
{
    SDL_AudioStream* s = stream_of(Stream::Beacon);
    if (!s || !fill) return;
    if (length <= 0 || length > constants::CHANNEL_BUFFER_SIZE) return;
    if (g_beacon_scratch.size() * sizeof(int16_t) < static_cast<size_t>(length)) return;

    fill(g_beacon_scratch.data(), context);
    sdl::SDL_ClearAudioStream(s);
    sdl::SDL_PutAudioStreamData(s, g_beacon_scratch.data(), length);
}

} // namespace sound::engine
