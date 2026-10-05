#pragma once
#include "MinHook.h"   // for MH_STATUS / MH_OK / MH_ALL_HOOKS only — we never link its lib

// MinHook is loaded at RUNTIME rather than imported.
//
// Why: a static import means that if MinHook.x64.dll is missing, blocked by antivirus, or
// the wrong architecture, the Windows loader fails to load version.dll — and because
// version.dll is a proxy the game imports, THE GAME WOULD NOT START. A screen-reader mod
// must never be able to prevent the game from launching. Loaded this way, the worst case
// is a silent mod and a line in the log.
namespace mh {

/// Load MinHook.x64.dll and bind the entry points. False = hooking unavailable.
bool load();

MH_STATUS initialize();
MH_STATUS create(void* target, void* detour, void** original);
MH_STATUS enable(void* target);
MH_STATUS disable(void* target);
/// Batched enable/disable: every MH_EnableHook / MH_DisableHook suspends EVERY thread in
/// the process, walks their contexts and resumes them. 147 readers' hooks enabled one at a
/// time were 147 such freezes of the game's main thread during its own startup (2026-09-25
/// logs: 5-21 s spent in hook installation). Queue them all and apply once.
MH_STATUS queue_enable(void* target);
MH_STATUS queue_disable(void* target);
MH_STATUS apply_queued();
MH_STATUS uninitialize();

bool available();

} // namespace mh
