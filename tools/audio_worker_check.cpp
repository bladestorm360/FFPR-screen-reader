// Actual audio command worker with a blocked fake device; no SDL/game is loaded.
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <thread>
#include "../src/core/audio/audio_engine.cpp"
namespace sdl {
#define DEFINE(name) decltype(&::name) name = nullptr;
SDL_DYNAMIC_FUNCTIONS(DEFINE)
#undef DEFINE
bool load() { return true; }
}
namespace log_ { void write(const char*, ...) {} }
namespace fixture {
HANDLE opening, opened, putting, release_put;
DWORD main_thread = GetCurrentThreadId();
std::atomic<DWORD> callback_thread{0};
std::atomic<bool> wrong_thread{false};
std::atomic<int> queued[9]{};
std::atomic<int> last_sample{-1}, clears{0};
int stream_count = 0;
void check_worker() {
    if (GetCurrentThreadId() == main_thread || GetCurrentThreadId() == callback_thread) wrong_thread = true;
}
int index(SDL_AudioStream* s) { return static_cast<int>(reinterpret_cast<uintptr_t>(s)) - 1; }
bool SDLCALL init(SDL_InitFlags) { if (GetCurrentThreadId() != main_thread) wrong_thread = true; return true; }
SDL_AudioDeviceID SDLCALL open(SDL_AudioDeviceID, const SDL_AudioSpec*) {
    check_worker(); SetEvent(opening); WaitForSingleObject(opened, 5000); return 1;
}
SDL_AudioStream* SDLCALL create(const SDL_AudioSpec*, const SDL_AudioSpec*) {
    check_worker(); return reinterpret_cast<SDL_AudioStream*>(static_cast<uintptr_t>(++stream_count));
}
bool SDLCALL bind(SDL_AudioDeviceID, SDL_AudioStream*) { check_worker(); return true; }
bool SDLCALL resume(SDL_AudioDeviceID) { check_worker(); return true; }
bool SDLCALL clear(SDL_AudioStream* s) { check_worker(); ++clears; queued[index(s)] = 0; return true; }
bool SDLCALL gain(SDL_AudioStream*, float) { check_worker(); return true; }
bool SDLCALL put(SDL_AudioStream* s, const void* data, int bytes) {
    check_worker(); SetEvent(putting); WaitForSingleObject(release_put, 5000);
    queued[index(s)] += bytes; last_sample = *static_cast<const int16_t*>(data); return true;
}
int SDLCALL get_queued(SDL_AudioStream* s) { check_worker(); return queued[index(s)]; }
void SDLCALL destroy(SDL_AudioStream*) { check_worker(); }
void SDLCALL close(SDL_AudioDeviceID) { check_worker(); }
const char* SDLCALL error() { return "fake"; }
}
int main() {
    using namespace fixture; using namespace sound::engine;
    int failures = 0;
    auto expect = [&](bool ok, const char* message) { if (!ok) { ++failures; std::printf("FAIL %s\n", message); } };
    opening = CreateEventW(nullptr, TRUE, FALSE, nullptr); opened = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    putting = CreateEventW(nullptr, TRUE, FALSE, nullptr); release_put = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    sdl::SDL_InitSubSystem = &init; sdl::SDL_OpenAudioDevice = &open; sdl::SDL_CreateAudioStream = &create;
    sdl::SDL_BindAudioStream = &bind; sdl::SDL_ResumeAudioDevice = &resume;
    sdl::SDL_ClearAudioStream = &fixture::clear; sdl::SDL_SetAudioStreamGain = &gain;
    sdl::SDL_PutAudioStreamData = &put; sdl::SDL_GetAudioStreamQueued = &get_queued;
    sdl::SDL_DestroyAudioStream = &destroy; sdl::SDL_CloseAudioDevice = &close; sdl::SDL_GetError = &error;
    expect(!initialize(), "device request returns before a blocked driver opens");
    expect(WaitForSingleObject(opening, 5000) == WAIT_OBJECT_0 && !initialized(), "device opening is isolated on the worker");
    SetEvent(opened);
    DWORD until = GetTickCount() + 5000;
    while (!initialized() && GetTickCount() < until) std::this_thread::yield();
    expect(initialized(), "worker publishes initialized streams");
    int16_t old_pcm[] = {1, 2, 3, 4}; submit(Stream::Counter, old_pcm, sizeof old_pcm);
    expect(WaitForSingleObject(putting, 5000) == WAIT_OBJECT_0, "device submission reaches the blocking fake driver");
    HANDLE done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    std::thread callback([&] {
        callback_thread = GetCurrentThreadId();
        // Models transition cleanup while the worker is stalled in a driver call.
        sound::engine::clear(Stream::Counter); set_gain(Stream::Counter, .5f);
        int16_t new_pcm[] = {9, 8, 7, 6};
        for (int i = 0; i < 200; ++i) submit(Stream::Counter, new_pcm, sizeof new_pcm);
        SetEvent(done);
    });
    expect(WaitForSingleObject(done, 5000) == WAIT_OBJECT_0, "battle cancellation/gain/full-ring submissions never wait for the audio driver");
    callback.join();
    expect(!wrong_thread && GetThreadPriority(g_worker) == THREAD_PRIORITY_BELOW_NORMAL, "only subsystem initialization remains on main; stream calls stay on worker");
    SetEvent(release_put);
    until = GetTickCount() + 5000;
    while ((g_head != g_tail || last_sample != 9) && GetTickCount() < until) std::this_thread::yield();
    expect(last_sample == 9 && clears > 0 && g_head == g_tail, "cancellation removes old audio before copied new-generation PCM");
    std::atomic<bool> generated{false};
    play_generated(Stream::Beacon, sizeof old_pcm, [&generated](int16_t* pcm) {
        fixture::check_worker(); pcm[0] = 12; generated = true;
    });
    until = GetTickCount() + 5000;
    while (!generated && GetTickCount() < until) std::this_thread::yield();
    expect(generated && !wrong_thread, "tone synthesis runs on the audio worker");
    sound::engine::clear(Stream::Counter); shutdown();
    expect(WaitForSingleObject(g_worker, 5000) == WAIT_OBJECT_0, "worker tears down without blocking the shutdown requester");
    expect(!wrong_thread, "teardown remains worker-owned");
    CloseHandle(g_worker); CloseHandle(g_wake);
    CloseHandle(done); CloseHandle(opening); CloseHandle(opened); CloseHandle(putting); CloseHandle(release_put);
    std::printf("%s: blocked driver, nonblocking battle cleanup, copied PCM, cancellation, worker ownership; %d failures\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
