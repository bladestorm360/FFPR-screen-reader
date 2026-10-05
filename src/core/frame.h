#pragma once
#include <cstdint>
#include <functional>

// The main-thread frame tick and the deferred-work scheduler.
//
// This replaces MelonLoader's OnUpdate and the C# mods' coroutines. The core detours
// System.Input.InputSystemManager.Update - the game's own input manager, present in all five
// games, called once per frame on the main thread (its body is not folded: one RVA in every
// dump.cs). At the start of each call, before the game reads any key:
//
//   1. per-frame subscribers run (the mod's input handling: keyboard, gamepad, router);
//   2. due tasks run (next_frame / after_frames / after_seconds / every_frame).
//
// Everything here runs ON THE GAME'S MAIN THREAD, so a task may read and call game objects
// freely (guarded as usual). That is the property the C# mods relied on: hotkeys read live
// game state, and a reader deferred "one frame" sees the UI the game filled in.
//
// Rules carried over from the C# mods (OPEN_ISSUES.md, "Timers and per-frame work"):
// per-frame work is only the allowed core - input, the controller router, audio loops,
// map-transition detection, and bounded 1-30-frame reads that start from an event. A reader
// never polls; it starts from a hook and ends.
namespace frame {

using Task = std::function<void()>;
/// Return true to run again next frame, false when finished.
using Step = std::function<bool()>;

/// Install the tick (core, boot thread). False when InputSystemManager.Update is missing.
bool install();
/// True once the tick has run at least once.
bool live();

/// Frames since the tick went live.
uint64_t count();
/// Seconds since an arbitrary start (QueryPerformanceCounter). Usable on any thread.
double now();
bool on_main_thread();

/// Per-frame subscriber, called every tick in registration order (input, router, audio).
void subscribe(void (*fn)(), const char* label);

/// Run once at the start of the next frame ("yield return null" then the work).
void next_frame(Task t, const char* label = nullptr);
/// Run once after n frames (n >= 1).
void after_frames(int n, Task t, const char* label = nullptr);
/// Run once after s seconds (WaitForSeconds). Main thread.
void after_seconds(double s, Task t, const char* label = nullptr);
/// Run every frame until it returns false or is cancelled. Returns an id for cancel().
/// A step that runs more than `max_frames` frames is stopped (bounded reads).
int every_frame(Step s, const char* label, int max_frames = 1800);
void cancel(int id);
/// Cancel every task whose label starts with `prefix` (a reader cancels its own pending work).
void cancel_label(const char* prefix);

/// From ANY thread: run `t` on the main thread at the next tick. Thread safe.
void post(Task t);

/// Number of pending tasks (diagnostics).
int pending();

} // namespace frame
