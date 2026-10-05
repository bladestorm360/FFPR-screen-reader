#include <windows.h>
#include <cstdlib>
#include <map>
#include <string>
#include "settings.h"
#include "game.h"
#include "log.h"
#include "speech.h"
#include "text.h"

namespace settings {
namespace {

SRWLOCK g_lock = SRWLOCK_INIT;
std::map<std::string, std::string>* g_values = nullptr;   // raw UTF-8 values
bool g_dirty = false;
bool g_imported = false;
std::wstring g_path;

std::map<std::string, std::string>& values()
{
    if (!g_values) g_values = new std::map<std::string, std::string>();
    return *g_values;
}

bool read_file(const std::wstring& path, std::string& out)
{
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(h, &size) || size.QuadPart > (8 << 20)) { CloseHandle(h); return false; }
    out.resize(static_cast<size_t>(size.QuadPart));
    DWORD got = 0;
    BOOL ok = out.empty() ? TRUE : ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &got, nullptr);
    CloseHandle(h);
    if (!ok) return false;
    out.resize(got);
    return true;
}

std::string strip(const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

/// key = value lines; '#' comments; [sections] select which section is read (empty = all).
void parse(const std::string& textbuf, const std::string& section, std::map<std::string, std::string>& out)
{
    std::string current;
    size_t start = 0;
    while (start <= textbuf.size()) {
        size_t end = textbuf.find('\n', start);
        if (end == std::string::npos) end = textbuf.size();
        std::string line = strip(textbuf.substr(start, end - start));
        start = end + 1;
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line[0] == '[') {
            size_t close = line.find(']');
            current = close == std::string::npos ? line.substr(1) : line.substr(1, close - 1);
            continue;
        }
        if (!section.empty() && current != section) continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = strip(line.substr(0, eq));
        std::string val = strip(line.substr(eq + 1));
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"') val = val.substr(1, val.size() - 2);
        if (!key.empty()) out[key] = val;
    }
}

void mark_dirty()
{
    g_dirty = true;
    speech::wake_worker();
}

std::string get_raw(const char* key, bool& found)
{
    AcquireSRWLockShared(&g_lock);
    auto& v = values();
    auto it = v.find(key);
    found = it != v.end();
    std::string out = found ? it->second : std::string();
    ReleaseSRWLockShared(&g_lock);
    return out;
}

void set_raw(const char* key, const std::string& val)
{
    AcquireSRWLockExclusive(&g_lock);
    auto& v = values();
    auto it = v.find(key);
    if (it == v.end() || it->second != val) {
        v[key] = val;
        mark_dirty();
    }
    ReleaseSRWLockExclusive(&g_lock);
}

} // namespace

void init()
{
    g_path = std::wstring(game::dir()) + L"ffpr_screen_reader.ini";
    std::string buf;
    if (read_file(g_path, buf)) {
        AcquireSRWLockExclusive(&g_lock);
        parse(buf, std::string(), values());
        ReleaseSRWLockExclusive(&g_lock);
        LOGF("settings: loaded %zu entries", values().size());
        return;
    }
    // First start: import the old mod's MelonPreferences category once.
    const game::Module* m = game::module();
    if (m && m->settings_category) {
        std::wstring melon = std::wstring(game::dir()) + L"UserData\\MelonPreferences.cfg";
        if (read_file(melon, buf)) {
            AcquireSRWLockExclusive(&g_lock);
            parse(buf, m->settings_category, values());
            ReleaseSRWLockExclusive(&g_lock);
            g_imported = !values().empty();
            LOGF("settings: imported %zu entries from MelonPreferences [%s]", values().size(), m->settings_category);
        } else {
            LOGF("settings: no settings file and no MelonPreferences.cfg; using defaults");
        }
    }
    mark_dirty();   // write the file once so the import happens only once
}

bool get_bool(const char* key, bool def)
{
    bool found = false;
    std::string v = get_raw(key, found);
    if (!found) return def;
    return v == "true" || v == "True" || v == "1";
}

int get_int(const char* key, int def)
{
    bool found = false;
    std::string v = get_raw(key, found);
    if (!found || v.empty()) return def;
    return static_cast<int>(std::strtol(v.c_str(), nullptr, 10));
}

float get_float(const char* key, float def)
{
    bool found = false;
    std::string v = get_raw(key, found);
    if (!found || v.empty()) return def;
    return static_cast<float>(std::strtod(v.c_str(), nullptr));
}

std::wstring get_str(const char* key, const wchar_t* def)
{
    bool found = false;
    std::string v = get_raw(key, found);
    if (!found) return def ? def : L"";
    return text::from_utf8(v);
}

void set_bool(const char* key, bool v) { set_raw(key, v ? "true" : "false"); }
void set_int(const char* key, int v) { set_raw(key, std::to_string(v)); }

void set_float(const char* key, float v)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%.4g", static_cast<double>(v));
    set_raw(key, buf);
}

void set_str(const char* key, const std::wstring& v) { set_raw(key, text::to_utf8(v)); }

bool toggle(const char* key, bool def)
{
    bool v = !get_bool(key, def);
    set_bool(key, v);
    return v;
}

void save_pending()
{
    if (!g_dirty || g_path.empty()) return;
    std::string out = "# FFPR screen reader settings. Written by the mod; safe to edit while the game is closed.\r\n";
    AcquireSRWLockShared(&g_lock);
    for (const auto& kv : values()) out += kv.first + " = " + kv.second + "\r\n";
    g_dirty = false;
    ReleaseSRWLockShared(&g_lock);

    std::wstring tmp = g_path + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) { LOGF("settings: cannot write (err=%lu)", GetLastError()); return; }
    DWORD wrote = 0;
    WriteFile(h, out.data(), static_cast<DWORD>(out.size()), &wrote, nullptr);
    CloseHandle(h);
    if (!MoveFileExW(tmp.c_str(), g_path.c_str(), MOVEFILE_REPLACE_EXISTING))
        LOGF("settings: replace failed (err=%lu)", GetLastError());
}

bool imported() { return g_imported; }

} // namespace settings
