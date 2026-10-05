// hid.dll proxy.
//
// UnityPlayer.dll imports hid.dll statically, and hid.dll is not a KnownDLL, so the loader takes
// this copy from the game folder at every start - the same moment MelonLoader's version.dll
// loads. That is how the mod gets into the game without an injector and without touching
// MelonLoader (UNIFIED_MOD_PLAN.md, "DLL name and MelonLoader").
//
// Every one of the 47 real exports must be forwarded: a static import of a name we do not
// export fails the module load, and that would stop the GAME from starting. SDL3 (the game's
// and ours) also loads hid.dll for gamepads and gets this copy, so the forwarding is on the
// controller path too.
//
// One generic forwarder serves every export. All 47 functions take at most nine integer or
// pointer arguments (HidP_GetUsageValueArray and HidP_SetUsageValueArray have nine), none takes
// a float, and none returns a struct, so under the Microsoft x64 ABI a ten-argument
// pointer-sized superset is exact: four arguments travel in RCX/RDX/R8/R9, the rest on the
// stack, and a callee taking fewer ignores the surplus. Reading the unused stack slots reads
// the caller's own frame, which is always mapped. BOOLEAN, NTSTATUS and void returns all come
// back in RAX untouched.
//
// The function pointer is a plain static, not a magic static: these exports can be called
// under the loader lock (UnityPlayer imports them), and a thread-safe-init guard there is a
// deadlock risk. Two threads racing resolve the same address; the worst case is a repeated
// GetProcAddress.
#include <windows.h>
#include "hid_names.h"

namespace {

HMODULE g_real = nullptr;

FARPROC real(const char* name)
{
    if (!g_real) {
        wchar_t path[MAX_PATH]{};
        UINT n = GetSystemDirectoryW(path, MAX_PATH);
        if (n == 0 || n + 9 >= MAX_PATH) return nullptr;
        lstrcatW(path, L"\\hid.dll");
        // Full path: the loader compares full paths, so this loads the system copy as a
        // separate module instead of handing back this proxy.
        g_real = LoadLibraryW(path);
        if (!g_real) {
            OutputDebugStringA("[FFPR reader] proxy: could not load System32\\hid.dll\n");
            return nullptr;
        }
    }
    return GetProcAddress(g_real, name);
}

using fwd_t = UINT_PTR (WINAPI*)(UINT_PTR, UINT_PTR, UINT_PTR, UINT_PTR, UINT_PTR,
                                 UINT_PTR, UINT_PTR, UINT_PTR, UINT_PTR, UINT_PTR);

} // namespace

#define FORWARD_HID(name)                                                              \
    extern "C" UINT_PTR WINAPI fwd_##name(UINT_PTR a, UINT_PTR b, UINT_PTR c,           \
                                          UINT_PTR d, UINT_PTR e, UINT_PTR f,           \
                                          UINT_PTR g, UINT_PTR h, UINT_PTR i, UINT_PTR j) \
    {                                                                                  \
        static fwd_t fn = nullptr;                                                     \
        if (!fn) fn = reinterpret_cast<fwd_t>(real(#name));                            \
        if (!fn) return 0;                                                             \
        return fn(a, b, c, d, e, f, g, h, i, j);                                       \
    }

HID_EXPORTS(FORWARD_HID)

// For the smoke test: proves the real library resolves without calling a HID function.
extern "C" __declspec(dllexport) int WINAPI ffpr_proxy_selftest()
{
    int ok = 0;
#define COUNT_ONE(name) if (real(#name)) ++ok;
    HID_EXPORTS(COUNT_ONE)
#undef COUNT_ONE
    return ok;
}
