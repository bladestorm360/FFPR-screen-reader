// Ports FF1 Utils/AudioEngine.cs.
//
// SDL3-backed output: one logical playback device with one SDL_AudioStream per sound kind
// bound to it. A dedicated worker owns device/stream calls; game callbacks only
// copy bounded PCM commands and publish cancellation/gain values without waiting.
// Every SDL call goes through sdl_dyn.h (no import library); sdl::load() runs first.
#include "audio_engine.h"
#include <windows.h>
#include <atomic>
#include <array>
#include <cstring>
#include "sound_constants.h"
#include "cue_runtime.h"
#include "../log.h"
#include "../sdl_dyn.h"

namespace sound::engine {
namespace {

constexpr int kStreamCount = static_cast<int>(Stream::Count);

SDL_AudioDeviceID g_device = 0;
SDL_AudioStream* g_streams[kStreamCount] = {};
std::atomic<bool> g_initialized{false};
std::atomic<bool> g_started{false}, g_running{true};
std::atomic<HANDLE> g_wake{nullptr};
HANDLE g_worker = nullptr;
// Single producer: game callbacks. Single consumer: audio worker. Fixed slots
// own their PCM; no Unity pointers, stack callbacks, mutexes or disk I/O.
constexpr unsigned kCommands = 64;
struct Command {
    Stream stream{};
    unsigned generation = 0;
    int bytes = 0;
    std::function<void(int16_t*)> generate;
    std::array<int16_t, constants::CHANNEL_BUFFER_SIZE / sizeof(int16_t)> pcm;
};
std::array<Command, kCommands> g_commands;
std::atomic<unsigned> g_head{0}, g_tail{0};
std::atomic<unsigned> g_generation[kStreamCount]{};
std::atomic<int> g_queued[kStreamCount]{};
std::atomic<float> g_gain[kStreamCount];

const char* stream_name(int i)
{
    static const char* const kNames[kStreamCount] = {
        "Footstep", "WallBump", "Beacon", "WallNorth", "WallSouth", "WallEast", "WallWest",
        "Counter", "Landing", "Cues"};
    return (i >= 0 && i < kStreamCount) ? kNames[i] : "?";
}

/// SDL's last error, or a placeholder (used by the failure logs).
const char* sdl_error()
{
    const char* e = sdl::SDL_GetError ? sdl::SDL_GetError() : nullptr;
    return (e && *e) ? e : "(no SDL error)";
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
}

} // namespace

bool open_device()
{
    if (g_initialized.load()) return true;
    try {
        SDL_AudioSpec spec{};
        spec.format = SDL_AUDIO_S16LE;
        spec.channels = 2;
        spec.freq = constants::SAMPLE_RATE;

        g_device = sdl::SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
        if (g_device == 0) {
            LOGF("[SDL Audio] SDL_OpenAudioDevice failed: %s - audio DISABLED (no fallback)", sdl_error());
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
        if(!g_streams[static_cast<int>(Stream::Cues)]) {
            release_all();
            return false;
        }

        // A freshly opened device starts paused; resuming a running one is harmless. Skipping
        // this is a silent-no-audio failure.
        if (!sdl::SDL_ResumeAudioDevice(g_device))
            LOGF("[SDL Audio] SDL_ResumeAudioDevice failed: %s", sdl_error());

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

namespace {
int index(Stream id) { return static_cast<int>(id); }
DWORD WINAPI audio_worker(void*)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    if (!open_device()) return 0;
    unsigned applied[kStreamCount]{};
    float gain[kStreamCount];
    for (float& value : gain) value = -1;
    auto update_stream = [&](int i) {
        if (!g_streams[i]) return;
        const unsigned current = g_generation[i].load(std::memory_order_acquire);
        if (applied[i] != current) {
            sdl::SDL_ClearAudioStream(g_streams[i]);
            applied[i] = current;
        }
        const float wanted = g_gain[i].load(std::memory_order_relaxed);
        if (gain[i] != wanted) {
            sdl::SDL_SetAudioStreamGain(g_streams[i], wanted);
            gain[i] = wanted;
        }
    };
    try { while (g_running.load(std::memory_order_acquire)) {
        WaitForSingleObject(g_wake, 20); // audio-queue snapshots, no game polling
        for (int i = 0; i < kStreamCount; ++i) update_stream(i);
        unsigned head = g_head.load(std::memory_order_relaxed);
        while (head != g_tail.load(std::memory_order_acquire)) {
            const auto& command = g_commands[head];
            const int i = index(command.stream);
            update_stream(i);
            if (g_streams[i] && command.generation == g_generation[i].load(std::memory_order_acquire)) {
                if (command.generate) command.generate(g_commands[head].pcm.data());
                sdl::SDL_PutAudioStreamData(g_streams[i], command.pcm.data(), command.bytes);
            }
            head = (head + 1) % kCommands;
            g_head.store(head, std::memory_order_release);
        }
        for (int i = 0; i < kStreamCount; ++i) {
            update_stream(i); // a cancellation arriving during a device call still wins
            const int bytes = g_streams[i] ? sdl::SDL_GetAudioStreamQueued(g_streams[i]) : 0;
            g_queued[i].store(bytes > 0 ? bytes : 0, std::memory_order_relaxed);
        }
        runtime::pump(g_streams[index(Stream::Cues)]);
    } } catch (...) {
        LOGF("[SDL Audio] worker call failed; mod audio disabled, game continues");
        g_running.store(false, std::memory_order_release);
    }
    g_initialized.store(false, std::memory_order_release);
    runtime::close();
    release_all();
    return 0;
}
}

bool initialize()
{
    if (g_started.exchange(true)) return initialized();
    // Called exclusively by the dedicated SDL owner, which initializes and pumps
    // this Windows SDL instance. Device/stream work belongs to the audio worker.
    if (!sdl::load() || !sdl::SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
    for (auto& value : g_gain) value.store(1);
    g_wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_wake) return false;
    g_worker = CreateThread(nullptr, 0, audio_worker, nullptr, 0, nullptr);
    if (!g_worker) { CloseHandle(g_wake); g_wake = nullptr; return false; }
    return initialized(); // no wait for device construction
}

void shutdown()
{
    g_initialized.store(false, std::memory_order_release);
    g_running.store(false, std::memory_order_release);
    if (g_wake) SetEvent(g_wake);
    // Process-lifetime SDL subsystem. Never wait for device teardown or quit an
    // SDL subsystem while its worker may still be using it; DllMain does nothing.
}

bool initialized() { return g_initialized.load(); }

void set_gain(Stream id, float gain)
{
    const int i = index(id);
    if (i < 0 || i >= kStreamCount) return;
    g_gain[i].store(gain, std::memory_order_relaxed);
    if (g_wake) SetEvent(g_wake);
}

int queued_bytes(Stream id)
{
    const int i = index(id);
    if (!initialized() || i < 0 || i >= kStreamCount) return 0;
    int result = g_queued[i].load(std::memory_order_relaxed);
    const unsigned generation = g_generation[i].load(std::memory_order_relaxed);
    const unsigned tail = g_tail.load(std::memory_order_relaxed);
    for (unsigned head = g_head.load(std::memory_order_acquire); head != tail; head = (head + 1) % kCommands) {
        const auto& command = g_commands[head];
        if (command.stream == id && command.generation == generation) result += command.bytes;
    }
    return result;
}

void clear(Stream id)
{
    const int i = index(id);
    if (i < 0 || i >= kStreamCount) return;
    ++g_generation[i]; // cannot be lost even if the PCM ring is full
    g_queued[i].store(0, std::memory_order_relaxed);
    if (g_wake) SetEvent(g_wake);
}

void submit(Stream id, const int16_t* pcm, int bytes)
{
    const int i = index(id);
    if (!initialized() || i < 0 || i >= kStreamCount || !pcm || bytes <= 0 || bytes > constants::CHANNEL_BUFFER_SIZE) return;
    const unsigned tail = g_tail.load(std::memory_order_relaxed), next = (tail + 1) % kCommands;
    if (next == g_head.load(std::memory_order_acquire)) return; // bounded: never wait on a driver
    auto& command = g_commands[tail];
    command.stream = id; command.bytes = bytes;
    command.generate = {};
    command.generation = g_generation[i].load(std::memory_order_relaxed);
    memcpy(command.pcm.data(), pcm, bytes);
    g_tail.store(next, std::memory_order_release);
    SetEvent(g_wake);
}

void submit(Stream id, const std::vector<int16_t>& pcm)
{
    if (pcm.empty()) return;
    submit(id, pcm.data(), static_cast<int>(pcm.size() * sizeof(int16_t)));
}

void play_one_shot(Stream id, const std::vector<int16_t>& pcm, float gain)
{
    if (!initialized()) return;
    set_gain(id, gain);
    clear(id);
    submit(id, pcm);
}

void play_generated(Stream id, int bytes, std::function<void(int16_t*)> generate, float gain)
{
    const int i = index(id);
    if (!initialized() || i < 0 || i >= kStreamCount || !generate || bytes <= 0 || bytes > constants::CHANNEL_BUFFER_SIZE) return;
    clear(id); set_gain(id, gain);
    const unsigned tail = g_tail.load(std::memory_order_relaxed), next = (tail + 1) % kCommands;
    if (next == g_head.load(std::memory_order_acquire)) return;
    auto& command = g_commands[tail];
    command.stream = id; command.bytes = bytes;
    command.generation = g_generation[i].load(std::memory_order_relaxed);
    command.generate = std::move(generate);
    g_tail.store(next, std::memory_order_release);
    SetEvent(g_wake);
}

} // namespace sound::engine
