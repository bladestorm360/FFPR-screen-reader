#pragma once
#include "MinHook.h"   // third_party/minhook: MinHook 1.3.4, built into hid.dll with [FFPR] changes

// MinHook, built into the DLL from source (third_party/minhook/src) since 2026-10-07.
//
// Why built in: coexistence with MelonLoader and BepInEx. Both hook game functions too (with
// Dobby, MonoMod or funchook), and stacked inline hooks break when one hooker's trampoline
// jumps back into bytes the other overwrote (FF1 crashed that way with MelonLoader on
// 2026-10-06). The [FFPR] changes in MinHook make our trampolines resume past the first
// MH_COEXIST_RESUME (16) bytes of a target and refuse to unhook over another hooker's patch.
// A stock MinHook.x64.dll cannot do either, and a copy shipped by another mod could replace
// ours in the game folder. Built in, it also cannot be missing.
//
// The mh:: names are unchanged from the LoadLibrary era so callers did not change.
namespace mh {

/// Always true now (kept for the callers). Logs the build once.
bool load();

MH_STATUS initialize();
MH_STATUS create(void* target, void* detour, void** original);
MH_STATUS enable(void* target);
/// MH_ERROR_PATCHED_OVER (another mod hooked the target after us) is reported as MH_OK and
/// logged: the hook stays in place, so a detour that can be "disabled" must pass calls
/// through while its feature is off (the runtime gate and the timer freezes do).
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

/// Where target's trampoline resumes the original code: 0 = never (the copied code ends in
/// a jump or return), -1 = no hook. Below MH_COEXIST_RESUME = not safe if another mod hooks
/// the same function after us.
int resume_offset(void* target);
/// Coexistence counters since initialize().
void stats(unsigned& long_resume, unsigned& short_resume, unsigned& patched_over);

} // namespace mh
