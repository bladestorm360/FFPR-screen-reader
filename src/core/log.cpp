#include "text_util.h"
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <locale.h>
#include <array>
#include <atomic>
#include "log.h"

// ---------------------------------------------------------------------------
// Win32 file I/O ONLY — deliberately no CRT stdio.
//
// The first version of this file opened the log with _wfopen_s(..., L"w, ccs=UTF-8") and
// then wrote to it with narrow fprintf. That combination puts the stream in Unicode
// orientation and then violates it, which trips the CRT's invalid-parameter handler ->
// __fastfail -> STATUS_STACK_BUFFER_OVERRUN (0xC0000409). Inside a DLL injected into
// someone else's process that does not throw an error, it TERMINATES THE GAME. It did:
// the game refused to launch, with VERSION.dll named as the faulting module and a 3-byte
// log containing nothing but the BOM.
//
// CreateFileW/WriteFile have no orientation, no locale, and no invalid-parameter handler.
// We format into a fixed buffer, convert to UTF-8 ourselves, and write bytes. Nothing in
// this file can abort the process.
// ---------------------------------------------------------------------------

namespace {

_locale_t g_utf8 = nullptr;   // see init()

HANDLE           g_file  = INVALID_HANDLE_VALUE;
SRWLOCK          g_lock = SRWLOCK_INIT;
std::atomic<bool> g_ready{false}, g_stopping{false};
HANDLE           g_wake = nullptr, g_worker = nullptr;
struct Line { DWORD size = 0; char bytes[2048]{}; };
constexpr size_t kQueueSize = 2048;
std::array<Line, kQueueSize> g_queue;
size_t g_head = 0, g_count = 0;
std::atomic<unsigned> g_dropped{0};
size_t           g_bytes = 0;
wchar_t          g_base[MAX_PATH]{};

void build_path(wchar_t* out, size_t n, int index)
{
    if (index == 0) _snwprintf_s(out, n, _TRUNCATE, L"%s.log", g_base);
    else            _snwprintf_s(out, n, _TRUNCATE, L"%s.%d.log", g_base, index);
}

void rotate()
{
    wchar_t from[MAX_PATH], to[MAX_PATH];
    build_path(to, MAX_PATH, log_::kKeep);
    DeleteFileW(to);
    for (int i = log_::kKeep - 1; i >= 0; --i) {
        build_path(from, MAX_PATH, i);
        build_path(to,   MAX_PATH, i + 1);
        MoveFileExW(from, to, MOVEFILE_REPLACE_EXISTING);
    }
}

void open_current()
{
    wchar_t path[MAX_PATH];
    build_path(path, MAX_PATH, 0);
    // FILE_SHARE_DELETE as well as READ: without it, a rename or delete of the file we hold
    // open FAILS, so a rotation while the game is running would move some of the numbered
    // logs and silently skip others (that is why a ffpr_screen_reader.2.log went missing). Sharing
    // delete also lets the user copy or clear the log without closing the game.
    g_file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                         CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    g_bytes = 0;
    if (g_file != INVALID_HANDLE_VALUE) {
        const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
        DWORD wrote = 0;
        WriteFile(g_file, bom, 3, &wrote, nullptr);
        g_bytes += wrote;
    }
}

void write_bytes(const char* data, int len)
{
    if (g_file == INVALID_HANDLE_VALUE || len <= 0) return;
    DWORD wrote = 0;
    WriteFile(g_file, data, static_cast<DWORD>(len), &wrote, nullptr);
    g_bytes += wrote;
    // NO FlushFileBuffers here. WriteFile has already handed the bytes to the kernel's file
    // cache, and a crash of THIS process (the only crash the mod can cause) never loses
    // them - only a kernel crash or a power cut could. The flush that used to sit on this
    // line forced a synchronous disk write per log line, 15-30 ms each on the game thread:
    // every battle cursor move logged two lines and stalled the game for two frames, and a
    // route's `smooth=28ms` stage timing was the flush of the log line inside it, not the
    // smoothing (2026-09-25 logs, nav_open_issues.md section 6). Every hook logs, so the
    // logger must cost microseconds, not milliseconds.

    if (g_bytes >= log_::kMaxBytes) {
        CloseHandle(g_file);
        g_file = INVALID_HANDLE_VALUE;
        rotate();
        open_current();
    }
}

// Producers never take a disk/rotation lock, wait for the writer, or perform I/O.
// A full/contended diagnostic queue drops logs, never game input or speech.
void emit_utf8(const char* utf8_line)
{
    char out[2048];
    DWORD ms = GetTickCount();
    int n = _snprintf_s(out, sizeof(out), _TRUNCATE, "[%7u.%03u] %s\r\n",
                        ms / 1000, ms % 1000, utf8_line);
    if (n <= 0 || !g_ready.load(std::memory_order_acquire)) return;
    if (!TryAcquireSRWLockExclusive(&g_lock)) { ++g_dropped; return; }
    if (g_count < kQueueSize) {
        auto& line = g_queue[(g_head + g_count++) % kQueueSize];
        line.size = static_cast<DWORD>(n);
        memcpy(line.bytes, out, n);
    } else ++g_dropped;
    ReleaseSRWLockExclusive(&g_lock);
    SetEvent(g_wake);
}

DWORD WINAPI writer(void*)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    rotate();
    open_current();
    for (;;) {
        WaitForSingleObject(g_wake, INFINITE);
        for (;;) {
            Line line;
            AcquireSRWLockExclusive(&g_lock);
            const bool have = g_count != 0;
            if (have) {
                line = g_queue[g_head];
                g_head = (g_head + 1) % kQueueSize;
                --g_count;
            }
            ReleaseSRWLockExclusive(&g_lock);
            if (!have) break;
            write_bytes(line.bytes, static_cast<int>(line.size));
        }
        const unsigned lost = g_dropped.exchange(0);
        if (lost) {
            char line[128];
            const int n = _snprintf_s(line, sizeof(line), _TRUNCATE,
                "[log] %u diagnostic lines dropped; producer never waits for disk\r\n", lost);
            if (n > 0) write_bytes(line, n);
        }
        if (g_stopping.load(std::memory_order_acquire)) break;
    }
    if (g_file != INVALID_HANDLE_VALUE) { CloseHandle(g_file); g_file = INVALID_HANDLE_VALUE; }
    return 0;
}

} // namespace

namespace log_ {

void init()
{
    if (g_ready) return;

    GetModuleFileNameW(nullptr, g_base, MAX_PATH);
    if (wchar_t* slash = wcsrchr(g_base, L'\\')) *(slash + 1) = 0;
    text_util::append(g_base, MAX_PATH, L"ffpr_screen_reader", _TRUNCATE);

    // %ls / %S in a NARROW printf converts through the C locale, and a wide character the
    // locale cannot represent (the en dash in "Kingdom of Caldisla – Town") makes the whole
    // call fail: the line came out EMPTY, and every route diagnostic on the world map was a
    // blank line (2026-09-22). Formatting through a UTF-8 locale keeps them. The CRT is
    // static (/MT), so this locale object is ours alone and never touches the game's.
    g_utf8 = _create_locale(LC_ALL, ".UTF8");
    g_wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_wake) return;
    g_worker = CreateThread(nullptr, 0, writer, nullptr, 0, nullptr);
    if (!g_worker) { CloseHandle(g_wake); g_wake = nullptr; return; }
    g_ready.store(true, std::memory_order_release);

    // Name the host process in the header. Only the game boots the mod now (see
    // host_is_game in dllmain.cpp), but if a log ever turns up that was written by something
    // else, it should say so itself rather than being mistaken for the run you just played.
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    const wchar_t* slash = wcsrchr(exe, L'\\');
    char           host[64]{};
    WideCharToMultiByte(CP_UTF8, 0, slash ? slash + 1 : exe, -1, host, sizeof(host) - 1,
                        nullptr, nullptr);

    SYSTEMTIME st{};
    GetLocalTime(&st);
    write("=== FFPR native screen reader - %04d-%02d-%02d %02d:%02d:%02d - %s pid=%lu ===",
          st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
          host[0] ? host : "?", GetCurrentProcessId());
}

void write(const char* fmt, ...)
{
    if (!g_ready) return;
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = g_utf8 ? _vsnprintf_s_l(buf, sizeof(buf), _TRUNCATE, fmt, g_utf8, ap)
                   : _vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
    if (n < 0 && buf[0] == 0) _snprintf_s(buf, sizeof(buf), _TRUNCATE, "(unformattable line: %s)", fmt);
    va_end(ap);
    emit_utf8(buf);

}

void writeW(const wchar_t* prefix, const wchar_t* text)
{
    if (!g_ready) return;
    wchar_t combined[1024];
    _snwprintf_s(combined, sizeof(combined) / sizeof(combined[0]), _TRUNCATE, L"%s%s",
                 prefix ? prefix : L"", text ? text : L"");

    char utf8[2048];
    int n = WideCharToMultiByte(CP_UTF8, 0, combined, -1, utf8, sizeof(utf8), nullptr, nullptr);
    if (n > 0) emit_utf8(utf8);   // n includes the terminator; emit_utf8 uses strlen semantics

}

void shutdown()
{
    // Explicit offline-host teardown only. DllMain intentionally does not call this.
    // Keep immutable locale/event storage alive for any producer already formatting.
    if (!g_ready.exchange(false)) return;
    g_stopping.store(true, std::memory_order_release);
    SetEvent(g_wake);
    WaitForSingleObject(g_worker, INFINITE);
    CloseHandle(g_worker); g_worker = nullptr;
}

} // namespace log_
