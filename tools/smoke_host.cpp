// Deterministic smoke-test host (BDFFHD's, adapted to hid.dll).
//
// Loads the built hid.dll the way the game's loader would, checks that all 47 real hid.dll
// exports resolve, calls a forwarded export against the real system DLL, holds the process
// open long enough for the boot thread to initialise and log, then exits cleanly.
//
// A non-zero exit means the DLL would have taken the GAME down the same way.
#include <windows.h>
#include <cstdio>
#include "../src/proxy/hid_names.h"

int wmain(int argc, wchar_t** argv)
{
    const wchar_t* dll = (argc > 1) ? argv[1] : L"hid.dll";
    // The loader would take a bare "hid.dll" from System32's KnownDLL-free search path only
    // after the application folder; give the full path so this always tests OUR copy.
    wchar_t full[MAX_PATH]{};
    GetFullPathNameW(dll, MAX_PATH, full, nullptr);

    wprintf(L"[smoke] loading %s\n", full);
    HMODULE h = LoadLibraryW(full);
    if (!h) {
        wprintf(L"[smoke] FAIL: LoadLibrary error %lu\n", GetLastError());
        return 2;
    }
    wprintf(L"[smoke] loaded at %p\n", (void*)h);

    const char* required[] = {
#define NAME_ONE(n) #n,
        HID_EXPORTS(NAME_ONE)
#undef NAME_ONE
    };
    int missing = 0;
    for (const char* name : required)
        if (!GetProcAddress(h, name)) { printf("[smoke] MISSING export: %s\n", name); ++missing; }
    if (missing) { wprintf(L"[smoke] FAIL: %d export(s) missing\n", missing); return 3; }
    wprintf(L"[smoke] all %d exports resolve\n", kHidExportCount);

    // The proxy resolves every name against the real System32 hid.dll.
    using selftest_t = int (WINAPI*)();
    auto selftest = reinterpret_cast<selftest_t>(GetProcAddress(h, "ffpr_proxy_selftest"));
    int resolved = selftest ? selftest() : -1;
    wprintf(L"[smoke] proxy resolves %d of %d real exports\n", resolved, kHidExportCount);
    if (resolved != kHidExportCount) { wprintf(L"[smoke] FAIL: proxy cannot reach the real hid.dll\n"); return 4; }

    // Actually CALL a forwarded export: HidD_GetHidGuid(GUID*) is harmless and always succeeds.
    using guid_t = void (WINAPI*)(GUID*);
    auto get_guid = reinterpret_cast<guid_t>(GetProcAddress(h, "HidD_GetHidGuid"));
    GUID g{};
    get_guid(&g);
    // The HID class GUID is {4D1E55B2-F16F-11CF-88CB-001111000030}.
    bool guid_ok = g.Data1 == 0x4D1E55B2 && g.Data2 == 0xF16F && g.Data3 == 0x11CF;
    wprintf(L"[smoke] forwarded HidD_GetHidGuid -> %08lX %s\n", g.Data1, guid_ok ? L"(OK)" : L"(WRONG)");
    if (!guid_ok) return 5;

    // Let the boot thread run: it initialises the logger, then waits for GameAssembly.dll
    // (absent here), reports that it never arrived, and parks.
    wprintf(L"[smoke] waiting for boot thread...\n");
    Sleep(4000);

    wprintf(L"[smoke] host exiting normally - no fastfail, no crash\n");
    return 0;
}
