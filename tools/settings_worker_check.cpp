// check-sources: text.cpp
// Real preference snapshot writer, fake disk/queue; never opens a game/config file.
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <thread>
#include <functional>
#include <string>
namespace fixture {
DWORD owner = GetCurrentThreadId();
HANDLE blocked, release;
bool busy = false;
std::atomic<bool> fail_write{false}, wrong_owner{false};
std::atomic<int> replacements{0};
std::function<void()> queued;
std::string contents;
HANDLE WINAPI create(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE) {
    if (GetCurrentThreadId() == owner) wrong_owner = true;
    return reinterpret_cast<HANDLE>(123);
}
BOOL WINAPI close(HANDLE) { return TRUE; }
BOOL WINAPI write(HANDLE, LPCVOID bytes, DWORD size, LPDWORD count, LPOVERLAPPED) {
    if (GetCurrentThreadId() == owner) wrong_owner = true;
    SetEvent(blocked); WaitForSingleObject(release, 5000);
    contents.assign(static_cast<const char*>(bytes), size);
    *count = fail_write ? 0 : size;
    return !fail_write;
}
BOOL WINAPI move(LPCWSTR, LPCWSTR, DWORD) { ++replacements; return TRUE; }
BOOL WINAPI remove(LPCWSTR) { return TRUE; }
}
#define CreateFileW fixture::create
#define CloseHandle fixture::close
#define WriteFile fixture::write
#define MoveFileExW fixture::move
#define DeleteFileW fixture::remove
#include "../src/core/settings.cpp"
#undef CreateFileW
#undef CloseHandle
#undef WriteFile
#undef MoveFileExW
#undef DeleteFileW
namespace game { const wchar_t* dir() { return L"fixture\\"; } const Module* module() { return nullptr; } }
namespace log_ { void write(const char*, ...) {} }
namespace speech { void wake_worker() {} }
namespace background {
bool post(std::function<void()> work, Lane lane) {
    if (fixture::busy || lane != Lane::Io) return false;
    fixture::queued = std::move(work); return true;
}
}
int main() {
    using namespace fixture;
    int failures = 0;
    auto expect = [&](bool ok, const char* reason) { if (!ok) { ++failures; std::printf("FAIL %s\n", reason); } };
    blocked = CreateEventW(nullptr, TRUE, FALSE, nullptr); release = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    settings::g_path = L"fixture.ini";
    settings::set_int("value", 1);
    busy = true; settings::save_pending();
    expect(settings::g_pending.load() && !settings::g_writing && !queued, "busy lane retains save without inline I/O");
    busy = false; settings::save_pending();
    std::thread io([] { queued(); });
    expect(WaitForSingleObject(blocked, 5000) == WAIT_OBJECT_0, "I/O worker reaches blocked disk");
    for (int i = 2; i <= 10000; ++i) settings::set_int("value", i);
    settings::save_pending(); // serialized writer already owns this lane
    expect(settings::get_int("value", 0) == 10000 && replacements == 0, "owner changes/reads complete while disk stays blocked");
    SetEvent(release); io.join();
    expect(contents.find("value = 10000") != std::string::npos && replacements == 2 && !wrong_owner,
           "latest owned snapshot follows the active write, only on I/O worker");
    fail_write = true; settings::set_int("value", 10001); settings::save_pending();
    std::thread failed([] { queued(); }); failed.join();
    expect(replacements == 2 && settings::g_pending.load(), "partial write preserves old file and retries latest snapshot");
    fail_write = false; settings::save_pending(); std::thread retry([] { queued(); }); retry.join();
    expect(replacements == 3 && !settings::g_pending.load(), "retained snapshot saves after recovery");
    CloseHandle(blocked); CloseHandle(release);
    std::printf("Settings: blocked disk, busy lane, latest snapshot, atomic failure/retry: %d failures\n", failures);
    return failures ? 1 : 0;
}
