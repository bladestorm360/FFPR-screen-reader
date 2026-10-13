// Real worker lanes; block unrelated computation and verify navigation runs.
#include <cstdio>
#include "../src/core/background.cpp"
namespace core { Install::Install(const char* n, int (*f)(), int o, uint8_t g) : name(n), fn(f), order(o), games(g) {} }
namespace hooks { void note_fault(const char*) { std::abort(); } }
int main() {
    int failures = 0;
    auto expect = [&](bool ok, const char* reason) { if (!ok) { ++failures; std::printf("FAIL %s\n", reason); } };
    const DWORD owner = GetCurrentThreadId();
    HANDLE entered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE release = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE nav_done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE compute_done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    background::prepare();
    expect(background::post([=] {
        SetEvent(entered); WaitForSingleObject(release, 5000); SetEvent(compute_done);
    }, background::Lane::Compute), "unrelated compute task accepted");
    expect(WaitForSingleObject(entered, 2000) == WAIT_OBJECT_0, "compute worker parked behind fixture barrier");
    DWORD nav_owner = owner; int priority = THREAD_PRIORITY_ERROR_RETURN;
    expect(background::post([&] {
        nav_owner = GetCurrentThreadId(); priority = GetThreadPriority(GetCurrentThread()); SetEvent(nav_done);
    }, background::Lane::Navigation), "navigation request publishes without waiting for blocked compute");
    expect(WaitForSingleObject(nav_done, 2000) == WAIT_OBJECT_0, "navigation completes while compute remains blocked");
    expect(WaitForSingleObject(compute_done, 0) == WAIT_TIMEOUT, "fixture actually held unrelated worker through navigation completion");
    expect(nav_owner != owner && priority == THREAD_PRIORITY_NORMAL, "interactive lane has its own normal-priority worker");
    SetEvent(release); WaitForSingleObject(compute_done, 2000);
    CloseHandle(entered); CloseHandle(release); CloseHandle(nav_done); CloseHandle(compute_done);
    std::printf("Navigation worker: independent lane, blocked-compute handoff, priority/ownership: %d failures\n", failures);
    return failures ? 1 : 0;
}
