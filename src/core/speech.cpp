#include <windows.h>
#include <cwchar>
#include <atomic>
#include "speech.h"
#include "speech_queue.h"
#include "prism_backend.h"
#include "settings.h"
#include "log.h"
#include "text.h"
#include "nonblocking_queue.h"

// ---------------------------------------------------------------------------
// Prism lives on ONE thread of ours: the boot thread, which loads it (wait_and_load) and then
// spends the rest of the process's life in pump(), speaking whatever the queue holds.
//
// WHY A QUEUE (2026-09-25). Tolk_Output was called straight from the game's hooks, on the
// game's main thread. With NVDA that call is an RPC into another process, answered on
// NVDA's side only when its interpreter is free - so a keypress cost the game thread
// however long NVDA took, and the wait grew whenever NVDA was busy with the previous line.
// The user heard it as lag while browsing battle menus and after every route key. Now a
// hook owns its ready text/parts, attempts publication to a fixed ingress queue and
// signals; the RPC happens here, and the game thread never waits for a screen reader.
//
// ORDER AND INTERRUPTION are preserved: the pump sends the lines in the order they were
// queued, each with its own interrupt flag, exactly as they were sent before. The one
// deliberate difference: a line queued with interrupt=true DROPS whatever is still waiting
// in the ring - those lines were about to be cancelled by it anyway, so this only saves
// the RPCs (the backend receives the same interrupt flag). That is an EVENT
// decision (a newer interrupting line arrived), not a text comparison: two identical lines
// queued one after the other are both spoken (CLAUDE.md rule 13).
//
// Ingress also holds speech before Prism is up. The pump drains when load completes;
// newer input generations and fixed-capacity overload can still retire pending output.
// ---------------------------------------------------------------------------

namespace {

HMODULE g_prism = nullptr;
speech::prism::Backend g_backend;
std::atomic<bool> g_braille_available{false},g_stopping{false};

std::wstring runtime_path() {
    HMODULE self = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&runtime_path), &self)) return {};
    wchar_t path[32768]{};
    const DWORD length = GetModuleFileNameW(self, path, 32768);
    if (!length || length >= 32768) return {};
    std::wstring result(path, length);
    const auto slash = result.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return {};
    return result.substr(0, slash + 1) + L".ffpr-deps\\prism\\prism.dll";
}

volatile LONG g_requested = 0;
volatile LONG g_ready     = 0;
HANDLE g_event = nullptr, g_ring_event = nullptr;
INIT_ONCE g_events_once = INIT_ONCE_STATIC_INIT;
speech::Queue g_ring; // chunking queue: speech worker only
struct Message { std::wstring text; bool interrupt = false, silence = false; speech::Kind kind = speech::Kind::Speech; uint64_t generation = 0; std::shared_ptr<const speech::Parts> parts; bool sequence = false; speech::Timing timing; };
NonblockingQueue<Message, 1024> g_ingress;
std::atomic<uint64_t> g_output_generation{0};
std::atomic<bool> g_carry_interrupt{false};
double g_clock_frequency = 0; // prepared before hooks; never lazily initialized by a producer
double speech_now() {
    LARGE_INTEGER value{}; QueryPerformanceCounter(&value);
    return g_clock_frequency ? static_cast<double>(value.QuadPart) / g_clock_frequency : 0;
}

BOOL CALLBACK create_events(PINIT_ONCE, PVOID, PVOID*) {
    LARGE_INTEGER frequency{}; QueryPerformanceFrequency(&frequency);
    g_clock_frequency = static_cast<double>(frequency.QuadPart);
    g_event = CreateEventW(nullptr,TRUE,FALSE,nullptr);
    g_ring_event = CreateEventW(nullptr,FALSE,FALSE,nullptr);
    // Complete initialization even on failure: no busy retry loop on the game thread.
    if (!g_event || !g_ring_event) LOGF("speech: event creation failed (err=%lu); running silent",GetLastError());
    return TRUE;
}
void ensure_ring() { InitOnceExecuteOnce(&g_events_once,create_events,nullptr,nullptr); }

void enqueue_owned(Message message) {
    if(g_stopping.load()) return;
    if (!g_ring_event) return;
    if (message.timing.feature != speech::Feature::None) {
        message.timing.enqueued_at = speech_now(); message.timing.first = true;
    }
    message.generation = message.interrupt || message.silence ? ++g_output_generation : g_output_generation.load();
    if (!g_ingress.push(std::move(message))) {
        // Preserve the established oldest-output overflow policy. No text
        // comparison, deduplication, producer lock or waiting for a reader RPC.
        Message dropped;
        if (g_ingress.pop(dropped) && dropped.interrupt) g_carry_interrupt = true;
        if (!g_ingress.push(std::move(message))) return;
    }
    SetEvent(g_ring_event);
}
void enqueue(const wchar_t* text, bool interrupt, bool silence, speech::Kind kind = speech::Kind::Speech) {
    enqueue_owned(Message{text ? text : L"", interrupt, silence, kind});
}
bool dequeue(speech::Line* out) {
    for (;;) {
        // Incoming interruptions must cancel pending chunks before the next RPC.
        Message message;
        while (g_ingress.pop(message)) {
            if (message.generation < g_output_generation.load()) continue;
            if (message.sequence && message.parts) {
                const bool carried = g_carry_interrupt.exchange(false);
                bool interrupt = message.interrupt || carried;
                for (const auto& part : *message.parts) {
                    const auto clean = text::clean(part);
                    if (clean.empty()) continue;
                    g_ring.say(clean.c_str(), interrupt, message.generation, message.timing);
                    interrupt = false;
                    message.timing.first = false;
                }
                if (interrupt) g_carry_interrupt = true; // no usable part consumed it
                continue;
            }
            const auto clean = text::clean(message.parts ? text::join(*message.parts, L". ") : message.text);
            if (message.silence) { g_carry_interrupt = false; g_ring.silence(message.generation); }
            else if (message.kind == speech::Kind::Braille) g_ring.braille(clean.c_str(), message.generation);
            else {
                // Consume the overflow handoff even when this line already
                // interrupts. Short-circuit OR left it armed for the queued tail.
                const bool carried = g_carry_interrupt.exchange(false);
                if (clean.empty()) { if (message.interrupt || carried) g_carry_interrupt = true; continue; }
                g_ring.say(clean.c_str(), message.interrupt || carried, message.generation, message.timing);
            }
        }
        if (!g_ring.pop(*out)) return false;
        if (out->generation >= g_output_generation.load()) return true;
    }
}

} // namespace

namespace speech {

void init()
{
    ensure_ring();
}

void request_load()
{
    // First caller wins; every subsequent hook call is a single interlocked read.
    if (InterlockedCompareExchange(&g_requested, 1, 0) != 0) return;
    if (g_event) SetEvent(g_event);
}

bool wait_and_load(int timeout_ms)
{
    ensure_ring();
    if (!g_event || !g_ring_event) return false;

    // If a hook already fired before we got here, the flag is set and we proceed at once.
    if (!InterlockedCompareExchange(&g_requested, 1, 1)) {
        LOGF("speech: waiting for the game to prove it is running (a hook firing)...");
        WaitForSingleObject(g_event, timeout_ms);
    }
    if (!InterlockedCompareExchange(&g_requested, 1, 1)) {
        // No hook fired at all — something is wrong with the hooks, not with speech.
        // Load anyway so the user is not left in total silence.
        LOGF("speech: no hook fired within %d ms; loading anyway", timeout_ms);
    }

    if(g_stopping.load()) return false;
    const auto path = runtime_path();
    g_prism = path.empty() ? nullptr : LoadLibraryExW(path.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!g_prism) {
        LOGF("speech: Prism prerequisite unavailable (err=%lu); run install_prism.bat", GetLastError());
        return false;
    }
    prism::Api api;
#define FFPR_PRISM_RESOLVE(n) api.n = reinterpret_cast<decltype(api.n)>(GetProcAddress(g_prism, "prism_" #n));
    FFPR_PRISM_API(FFPR_PRISM_RESOLVE)
#undef FFPR_PRISM_RESOLVE
    if (!g_backend.load(api)) {
        LOGF("speech: Prism initialization/version/backend failed; required version " PRISM_VERSION_STRING);
        FreeLibrary(g_prism);
        g_prism = nullptr;
        return false;
    }
    g_braille_available = g_backend.has_braille();
    InterlockedExchange(&g_ready, 1);
    LOGF("speech: Prism " PRISM_VERSION_STRING " ready, backend = %s", g_backend.name());
    // Whatever the first hooks wanted to say while we were loading is in the ring; the pump
    // (this thread, a moment from now) speaks it.
    const int queued = static_cast<int>(g_ingress.size());
    if (queued > 0) LOGF("speech: %d line(s) queued during load",queued);
    return true;
}

bool available() { return InterlockedCompareExchange(&g_ready, 1, 1) != 0; }

void say(const wchar_t* text, bool interrupt)
{
    say(text, interrupt, {});
}
void say(const wchar_t* text, bool interrupt, Timing timing)
{
    if (!text || !*text) return;
    Message message; message.text = text; message.interrupt = interrupt; message.timing = timing;
    enqueue_owned(std::move(message));
}
void say_parts(std::shared_ptr<const Parts> parts, bool interrupt) {
    if (!parts || parts->empty()) return;
    Message message; message.interrupt = interrupt; message.parts = std::move(parts);
    enqueue_owned(std::move(message));
}

void say_sequence(std::shared_ptr<const Parts> parts, bool interrupt) {
    say_sequence(std::move(parts), interrupt, {});
}
void say_sequence(std::shared_ptr<const Parts> parts, bool interrupt, Timing timing) {
    if (!parts || parts->empty()) return;
    Message message; message.interrupt = interrupt; message.parts = std::move(parts); message.sequence = true;
    message.timing = timing;
    enqueue_owned(std::move(message));
}

void silence()
{
    enqueue(nullptr, true, true);
}

// Speech-owner dispatch, shared with the fake-SDK end-to-end fixture.
void drain_backend()
{
    Line l;
    while (dequeue(&l)) {
        if (l.silence) { (void)g_backend.stop(); }
        else if (l.text[0]) {
            // Logging and screen-reader RPCs belong to this worker, never a UI hook.
            LOGW(l.interrupt ? L"say: " : L"say (queued): ",l.text);
            const double submitting = l.timing.feature != Feature::None && l.timing.first ? speech_now() : 0;
            const auto error = g_backend.output(l.text, l.interrupt, l.kind == Kind::Braille);
            if (submitting) {
                const char* feature = l.timing.feature == Feature::EntityBrowse ? "browse" :
                    l.timing.feature == Feature::EntityCategory ? "category" :
                    l.timing.feature == Feature::Directions ? "directions" : "mod-menu";
                LOGF("[SpeechTiming] %s generation %llu: request-to-submit %.3f ms, queue %.3f ms, data-age %.3f ms, SDK %.3f ms, result %d",
                    feature, static_cast<unsigned long long>(l.generation),
                    l.timing.requested_at ? (submitting - l.timing.requested_at) * 1000 : -1,
                    l.timing.enqueued_at ? (submitting - l.timing.enqueued_at) * 1000 : -1,
                    l.timing.captured_at ? (submitting - l.timing.captured_at) * 1000 : -1,
                    (speech_now() - submitting) * 1000, static_cast<int>(error));
            }
            if (error != PRISM_OK && error != PRISM_ERROR_NOT_INITIALIZED)
                LOGF("speech: Prism output failed: %s", g_backend.error(error));
        }
    }
}

void pump()
{
    // Even without Prism this detached worker saves settings and parks on the event.
    if(!g_ring_event) return;
    LOGF("speech: pump running on thread %lu", GetCurrentThreadId());
    for (;;) {
        const DWORD wake = WaitForSingleObject(g_ring_event, 250);
        if (wake == WAIT_TIMEOUT) { settings::save_pending(); continue; } // persistence retry, no game/SDK polling
        if (wake != WAIT_OBJECT_0) {
            LOGF("speech: queue wait failed (err=%lu); running silent",GetLastError());
            InterlockedExchange(&g_ready,0);
            return;
        }
        settings::save_pending();
        if(g_stopping.load()) { shutdown(); LOGF("shutdown: speech backend unloaded"); return; }
        g_braille_available = g_backend.has_braille();
        drain_backend();
    }
}

void wake_worker() { if(g_ring_event) SetEvent(g_ring_event); }
bool has_braille() { return g_braille_available.load(); }
void braille(const wchar_t* text) {
    if(!text || !*text) return;
    if(g_stopping.load()) return;
    enqueue(text, false, false, Kind::Braille);
}

void request_shutdown() { g_stopping=true; if(g_event) SetEvent(g_event); wake_worker(); }
void shutdown()
{
    g_backend.close();
    if (g_prism) FreeLibrary(g_prism);
    g_prism = nullptr;
    g_braille_available = false;
    InterlockedExchange(&g_ready, 0);
}

} // namespace speech
