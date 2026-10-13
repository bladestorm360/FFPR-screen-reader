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
//   2. ready posts and due tasks share a 0.5 ms elapsed dispatch budget,
//      with fair continuation on subsequent ticks (no fixed entry cap).
//
// Native observations stay on the game's owning thread and must themselves be bounded,
// ready reads. Heavy processing uses owned worker snapshots; no owner callback waits for
// a worker, queue or lock. Deferred readers see native UI after its population event.
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

/// Run once no earlier than the next frame; overload yields without blocking native input.
void next_frame(Task t, const char* label = nullptr);
/// Run once after n frames (n >= 1).
void after_frames(int n, Task t, const char* label = nullptr);
/// Run once after s seconds (WaitForSeconds). Main thread.
void after_seconds(double s, Task t, const char* label = nullptr);
/// Run every frame until it returns false or is cancelled. Returns an id for cancel().
/// A step that executes more than `max_frames` times is stopped (bounded reads).
int every_frame(Step s, const char* label, int max_frames = 1800);
void cancel(int id);
/// Cancel every task whose label starts with `prefix` (a reader cancels its own pending work).
void cancel_label(const char* prefix);

/// From ANY thread: queue `t` for bounded main-owner dispatch. Thread safe.
/// Owner calls never acquire the worker lock; worker publication may wait on that lock.
void post(Task t);

/// Number of pending tasks (diagnostics).
int pending();

} // namespace frame
