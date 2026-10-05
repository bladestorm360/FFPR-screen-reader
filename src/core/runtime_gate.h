#pragma once

// ---------------------------------------------------------------------------
// "Is the IL2CPP runtime actually up?" — answered by the game, not by us.
//
// There is no safe way to ASK. Every il2cpp_* export begins with a spin barrier that waits
// on the runtime's world-stopped flag (UnityPlayer drives collections itself — it resolves
// il2cpp_stop_gc_world / il2cpp_start_gc_world), so any question we ask is itself a call
// into the runtime. Asking too early is what hung the game on 2026-08-12: il2cpp_domain_get
// starts returning non-null partway through il2cpp_init, so "the domain exists" is NOT
// "init has finished", and il2cpp_thread_attach on a half-built runtime deadlocked against
// the main thread that was still building it.
//
// So we do not ask. We put a trip-wire on a handful of exports that only UnityPlayer calls,
// and only after il2cpp_init has returned: the runtime's internals call their own C++
// functions directly, never these exported wrappers, so a call arriving here can only have
// come from the embedder. The first one to fire means init is done and the game is running
// its own code — the same "the game telling us IS the event" rule the readers follow.
//
// Several wires rather than one because a missed signal means a silent mod, and there is no
// cost to redundancy: whichever fires first wins and the rest are ignored.
// ---------------------------------------------------------------------------

namespace gate {

/// Patch the trip-wires. Requires il2cpp::bind() and MinHook. Cheap: this only rewrites
/// code bytes, it never calls into the runtime.
bool arm();

/// Block until the game calls one of the wires. Returns false on timeout — in which case
/// the caller must go quiet rather than proceed, because proceeding is what hangs the game.
bool wait(int timeout_ms);

/// After wait() succeeded: queue the wires for removal from the game's hot paths
/// (il2cpp_string_new / object_new / runtime_invoke / runtime_class_init run millions of
/// times a session; a fired wire is a permanent extra branch on every one of them). Only
/// QUEUES — the disable is applied in the same thread freeze that enables the readers'
/// hooks (hooks::install -> mh::apply_queued), so it costs no freeze of its own.
void disarm();

/// Name of the export that fired, for the log. Never null.
const char* fired_by();

} // namespace gate
