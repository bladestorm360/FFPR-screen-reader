// Real logger with a blocked fake file write; no game DLL or screen reader is loaded.
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <thread>
namespace fixture {
HANDLE blocked, release;
DWORD producer_thread = GetCurrentThreadId();
std::atomic<bool> wrong_thread{false};
BOOL WINAPI write_file(HANDLE, LPCVOID, DWORD count, LPDWORD written, LPOVERLAPPED) {
    if (GetCurrentThreadId() == producer_thread) wrong_thread = true;
    SetEvent(blocked);
    WaitForSingleObject(release, INFINITE);
    *written = count;
    return TRUE;
}
}
#define WriteFile fixture::write_file
#include "../src/core/log.cpp"
#undef WriteFile
int main() {
    fixture::blocked = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    fixture::release = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    log_::init();
    int failures = 0;
    auto expect = [&](bool ok, const char* message) { if (!ok) { ++failures; std::printf("FAIL %s\n", message); } };
    expect(WaitForSingleObject(fixture::blocked, 5000) == WAIT_OBJECT_0, "writer reaches a blocked file write");
    HANDLE done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    std::thread producer([&] {
        for (int i = 0; i < 6000; ++i) log_::write("diagnostic %d", i);
        log_::writeW(L"speech: ", L"Japanese \u65e5\u672c\u8a9e");
        SetEvent(done);
    });
    // Event evidence, independent of a timing benchmark: all producers finish
    // while disk remains blocked, even when the fixed diagnostic queue is full.
    expect(WaitForSingleObject(done, 5000) == WAIT_OBJECT_0, "producers never wait for disk or log rotation");
    expect(g_dropped > 0 && g_count <= kQueueSize, "full queue remains bounded and records diagnostic loss");
    expect(!fixture::wrong_thread && GetThreadPriority(g_worker) == THREAD_PRIORITY_BELOW_NORMAL, "file I/O is confined to the lower-priority logger worker");
    SetEvent(fixture::release);
    producer.join();
    log_::shutdown();
    expect(g_count == 0, "explicit host teardown drains the logger queue");
    CloseHandle(done); CloseHandle(fixture::blocked); CloseHandle(fixture::release);
    std::printf("%s: nonblocking diagnostic producers, bounded queue, worker-only I/O; %d failures\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
