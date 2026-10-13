#pragma once
#include "speech_timing.h"
#include <memory>
#include <string>
#include <vector>
namespace speech { void request_shutdown(); }

// Prism is loaded LAZILY, triggered by the first game hook that fires.
//
// WHY: Tolk_Load() drags the screen reader's in-process stack into the game
// (nvdaHelperRemote, IAccessible2Proxy, ISimpleDOM, OLEACC, UIAutomation), which creates
// threads and initialises COM. Doing that during Unity's startup made the IL2CPP garbage
// collector's first stop-the-world fail and abort the process
// (ABORT "SuspendThread loop failed" -> int 3 in GameAssembly).
//
// A fixed settle delay "fixed" it but cost seconds of silence at the title screen, which
// for a blind player is menu sounds and no words — unacceptable. So instead we wait for
// PROOF that the game is past startup: a hook firing means the game is executing its own
// UI code. request_load() is called from every hook (cheap, idempotent); the boot thread
// blocks on that signal and does the actual LoadLibrary off the main thread, so there is
// no frame hitch either.
//
// SPEAKING IS ASYNCHRONOUS (2026-09-25). say() only copies the line into a fixed ring and
// signals; the boot thread, which loaded Prism, spends the rest of the process in pump()
// making the actual combined speech/braille calls. A screen reader's RPC latency therefore never
// reaches the game thread (it was the battle-menu and route-key lag). Lines spoken before
// Prism is up wait in the same ring and are spoken the moment it is.
namespace speech {

/// Boot thread, before any hook is installed: prepares events and ingress.
void init();

/// Called from hooks / any thread. Idempotent, allocation-free, returns immediately.
void request_load();

/// Boot thread only: block until request_load() fires (or timeout), then load Prism.
/// Returns true if speech became available.
bool wait_and_load(int timeout_ms);

/// Detached boot thread only: drain output and pending settings saves on the existing
/// worker event. Remains alive even without Prism, so persistence still works silently.
void pump();

/// Copy an owned UTF-16 message through bounded nonblocking ingress. Cleaning/
/// word-boundary splitting and backend calls remain on the speech worker.
void say(const wchar_t* text, bool interrupt = true);
void say(const wchar_t* text, bool interrupt, Timing timing);
using Parts = std::vector<std::wstring>;
void say_parts(std::shared_ptr<const Parts> parts, bool interrupt = true);
/// One owned event: first nonempty part may interrupt; every subsequent part
/// queues behind it. Cleaning and splitting stay on the speech owner.
void say_sequence(std::shared_ptr<const Parts> parts, bool interrupt = true);
void say_sequence(std::shared_ptr<const Parts> parts, bool interrupt, Timing timing);
void silence();
void braille(const wchar_t* text);
bool has_braille();
void wake_worker();
void shutdown();
bool available();

} // namespace speech
