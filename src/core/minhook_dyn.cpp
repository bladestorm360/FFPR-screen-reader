#include <windows.h>
#include "minhook_dyn.h"
#include "log.h"

namespace {
HMODULE g_mod = nullptr;

using fn_init   = MH_STATUS (WINAPI*)();
using fn_create = MH_STATUS (WINAPI*)(LPVOID, LPVOID, LPVOID*);
using fn_enable = MH_STATUS (WINAPI*)(LPVOID);
using fn_disable= MH_STATUS (WINAPI*)(LPVOID);
using fn_uninit = MH_STATUS (WINAPI*)();

fn_init    p_init    = nullptr;
fn_create  p_create  = nullptr;
fn_enable  p_enable  = nullptr;
fn_disable p_disable = nullptr;
using fn_queue  = MH_STATUS (WINAPI*)(LPVOID);
using fn_apply  = MH_STATUS (WINAPI*)();
fn_queue   p_qenable  = nullptr;
fn_queue   p_qdisable = nullptr;
fn_apply   p_apply    = nullptr;
fn_uninit  p_uninit  = nullptr;
bool       g_ok      = false;
bool       g_inited  = false;
}

namespace mh {

bool load()
{
    if (g_ok) return true;

    g_mod = LoadLibraryW(L"MinHook.x64.dll");   // app directory is searched first
    if (!g_mod) {
        LOGF("minhook: MinHook.x64.dll not found (err=%lu) — hooks disabled, game unaffected",
             GetLastError());
        return false;
    }

    p_init    = reinterpret_cast<fn_init>   (GetProcAddress(g_mod, "MH_Initialize"));
    p_create  = reinterpret_cast<fn_create> (GetProcAddress(g_mod, "MH_CreateHook"));
    p_enable  = reinterpret_cast<fn_enable> (GetProcAddress(g_mod, "MH_EnableHook"));
    p_disable = reinterpret_cast<fn_disable>(GetProcAddress(g_mod, "MH_DisableHook"));
    p_qenable  = reinterpret_cast<fn_queue>(GetProcAddress(g_mod, "MH_QueueEnableHook"));
    p_qdisable = reinterpret_cast<fn_queue>(GetProcAddress(g_mod, "MH_QueueDisableHook"));
    p_apply    = reinterpret_cast<fn_apply>(GetProcAddress(g_mod, "MH_ApplyQueued"));
    p_uninit  = reinterpret_cast<fn_uninit> (GetProcAddress(g_mod, "MH_Uninitialize"));

    if (!p_init || !p_create || !p_enable) {
        LOGF("minhook: exports missing — hooks disabled, game unaffected");
        FreeLibrary(g_mod);
        g_mod = nullptr;
        return false;
    }

    g_ok = true;
    LOGF("minhook: loaded");
    return true;
}

bool available() { return g_ok; }

/// Idempotent on purpose: there is more than one caller now (the runtime gate arms its
/// trip-wires before the readers install theirs), and MinHook answers a second
/// MH_Initialize with MH_ERROR_ALREADY_INITIALIZED — which a caller checking `!= MH_OK`
/// would read as failure and silently give up on all its hooks.
MH_STATUS initialize()
{
    if (!g_ok) return MH_ERROR_NOT_INITIALIZED;
    if (g_inited) return MH_OK;
    MH_STATUS s = p_init();
    if (s == MH_OK || s == MH_ERROR_ALREADY_INITIALIZED) { g_inited = true; return MH_OK; }
    return s;
}
MH_STATUS create(void* t, void* d, void** o)             { return g_ok ? p_create(t, d, o) : MH_ERROR_NOT_INITIALIZED; }
MH_STATUS enable(void* t)                                { return g_ok ? p_enable(t) : MH_ERROR_NOT_INITIALIZED; }
MH_STATUS disable(void* t)                               { return (g_ok && p_disable) ? p_disable(t) : MH_ERROR_NOT_INITIALIZED; }
// Fall back to the immediate calls if this MinHook build lacks the queue exports, so a
// different DLL on a user's machine degrades to the old one-freeze-per-hook behaviour
// rather than to no hooks at all.
MH_STATUS queue_enable(void* t)                          { return !g_ok ? MH_ERROR_NOT_INITIALIZED : p_qenable  ? p_qenable(t)  : p_enable(t); }
MH_STATUS queue_disable(void* t)                         { return !g_ok ? MH_ERROR_NOT_INITIALIZED : p_qdisable ? p_qdisable(t) : (p_disable ? p_disable(t) : MH_ERROR_NOT_INITIALIZED); }
MH_STATUS apply_queued()                                 { return !g_ok ? MH_ERROR_NOT_INITIALIZED : p_apply    ? p_apply()     : MH_OK; }
MH_STATUS uninitialize()                                 { return (g_ok && p_uninit)  ? p_uninit()   : MH_ERROR_NOT_INITIALIZED; }

} // namespace mh
