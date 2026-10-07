#include <windows.h>
#include "minhook_dyn.h"
#include "log.h"

namespace {
bool g_inited = false;
bool g_logged = false;
}

namespace mh {

bool load()
{
    if (!g_logged) {
        g_logged = true;
        LOGF("minhook: built in (1.3.4 + FFPR coexistence: resume past %d bytes, no unhook over "
             "another mod)", MH_COEXIST_RESUME);
    }
    return true;
}

bool available() { return true; }

/// Idempotent on purpose: there is more than one caller (the runtime gate arms its
/// trip-wires before the readers install theirs), and MinHook answers a second
/// MH_Initialize with MH_ERROR_ALREADY_INITIALIZED — which a caller checking `!= MH_OK`
/// would read as failure and silently give up on all its hooks.
MH_STATUS initialize()
{
    if (g_inited) return MH_OK;
    MH_STATUS s = MH_Initialize();
    if (s == MH_OK || s == MH_ERROR_ALREADY_INITIALIZED) { g_inited = true; return MH_OK; }
    return s;
}

MH_STATUS create(void* t, void* d, void** o) { return MH_CreateHook(t, d, o); }
MH_STATUS enable(void* t)                    { return MH_EnableHook(t); }

MH_STATUS disable(void* t)
{
    MH_STATUS s = MH_DisableHook(t);
    if (s == MH_ERROR_PATCHED_OVER) {
        LOGF("minhook: %p was hooked by another mod after us; our hook stays as a pass-through", t);
        return MH_OK;
    }
    return s;
}

MH_STATUS queue_enable(void* t)  { return MH_QueueEnableHook(t); }
MH_STATUS queue_disable(void* t) { return MH_QueueDisableHook(t); }
MH_STATUS apply_queued()         { return MH_ApplyQueued(); }   // patched-over disables are counted, not errors
MH_STATUS uninitialize()         { return MH_Uninitialize(); }

int resume_offset(void* target)
{
    UINT off = 0;
    return MH_GetResumeOffset(target, &off) == MH_OK ? static_cast<int>(off) : -1;
}

void stats(unsigned& long_resume, unsigned& short_resume, unsigned& patched_over)
{
    UINT l = 0, s = 0, p = 0;
    MH_CoexistStats(&l, &s, &p);
    long_resume = l;
    short_resume = s;
    patched_over = p;
}

} // namespace mh
