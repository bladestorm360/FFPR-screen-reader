#include <windows.h>
#include <cwchar>
#include "game.h"
#include "log.h"

namespace game {
namespace {

Id g_id = Id::Unknown;
bool g_detected = false;
bool g_product_ok = false;
wchar_t g_dir[MAX_PATH]{};
const Module* g_module = nullptr;
Flags g_flags{};

struct Known { const wchar_t* exe; const char* product; Id id; };
const Known kKnown[] = {
    { L"FINAL FANTASY.exe",     "FINAL FANTASY",     Id::FF1 },
    { L"FINAL FANTASY II.exe",  "FINAL FANTASY II",  Id::FF2 },
    { L"FINAL FANTASY III.exe", "FINAL FANTASY III", Id::FF3 },
    { L"FINAL FANTASY IV.exe",  "FINAL FANTASY IV",  Id::FF4 },
    { L"FINAL FANTASY V.exe",   "FINAL FANTASY V",   Id::FF5 },
};

/// <exe>_Data\app.info is two lines: company, then product. Read with Win32 only (no CRT stdio).
bool read_product(const wchar_t* exe_path, char* out, size_t n)
{
    wchar_t info[MAX_PATH]{};
    lstrcpynW(info, exe_path, MAX_PATH);
    wchar_t* dot = wcsrchr(info, L'.');
    if (!dot) return false;
    *dot = 0;
    if (lstrlenW(info) + 16 >= MAX_PATH) return false;
    lstrcatW(info, L"_Data\\app.info");
    HANDLE h = CreateFileW(info, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    char buf[256]{};
    DWORD got = 0;
    ReadFile(h, buf, sizeof(buf) - 1, &got, nullptr);
    CloseHandle(h);
    buf[got] = 0;
    char* line2 = strchr(buf, '\n');
    if (!line2) return false;
    ++line2;
    size_t i = 0;
    while (line2[i] && line2[i] != '\r' && line2[i] != '\n' && i + 1 < n) { out[i] = line2[i]; ++i; }
    out[i] = 0;
    return i > 0;
}

} // namespace

Id detect()
{
    if (g_detected) return g_id;
    g_detected = true;
    wchar_t path[MAX_PATH]{};
    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return g_id;
    lstrcpynW(g_dir, path, MAX_PATH);
    if (wchar_t* slash = wcsrchr(g_dir, L'\\')) *(slash + 1) = 0;
    const wchar_t* slash = wcsrchr(path, L'\\');
    const wchar_t* exe = slash ? slash + 1 : path;
    for (const Known& k : kKnown) {
        if (_wcsicmp(exe, k.exe) != 0) continue;
        g_id = k.id;
        char product[128]{};
        if (read_product(path, product, sizeof(product)))
            g_product_ok = _stricmp(product, k.product) == 0;
        break;
    }
    return g_id;
}

Id current() { return g_id; }
bool product_confirmed() { return g_product_ok; }
const wchar_t* dir() { return g_dir; }

const char* key()
{
    static const char* k[] = { "unknown", "ff1", "ff2", "ff3", "ff4", "ff5" };
    return k[static_cast<int>(g_id)];
}
const char* label()
{
    static const char* k[] = { "unknown", "FF1", "FF2", "FF3", "FF4", "FF5" };
    return k[static_cast<int>(g_id)];
}
int number() { return static_cast<int>(g_id); }

const Module* module() { return g_module; }
void set_module(const Module* m) { g_module = m; }
const Flags& flags() { return g_flags; }
Flags& flags_mut() { return g_flags; }

} // namespace game
