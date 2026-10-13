#include "background.h"
#include "hooks.h"
#include "core.h"
#include <windows.h>
#include <deque>
#include <mutex>
#include <array>

namespace background {
namespace {
struct Queue {
    std::mutex lock;
    std::deque<std::function<void()>> work;
    HANDLE wake = nullptr;
    bool started = false;
    int priority = THREAD_PRIORITY_BELOW_NORMAL;
};
Queue& queue(Lane lane)
{
    // The proxy and worker live until process exit. Avoid CRT teardown destroying
    // a mutex/queue while the native worker is still unwinding its last task.
    static auto* queues = new std::array<Queue, 4>;
    return (*queues)[static_cast<size_t>(lane)];
}

DWORD WINAPI worker(void* context)
{
    auto& q = *static_cast<Queue*>(context);
    SetThreadPriority(GetCurrentThread(), q.priority);
    for (;;) {
        WaitForSingleObject(q.wake, INFINITE);
        for (;;) {
            std::function<void()> work;
            {
                std::lock_guard lock(q.lock);
                if (q.work.empty()) break;
                work = std::move(q.work.front());
                q.work.pop_front();
            }
            hooks::guarded("background.data", [&] { work(); });
        }
    }
}
bool start(Queue& q) {
    if (q.started) return true;
    q.wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!q.wake) return false;
    HANDLE thread = CreateThread(nullptr, 0, worker, &q, 0, nullptr);
    if (!thread) { CloseHandle(q.wake); q.wake = nullptr; return false; }
    CloseHandle(thread); q.started = true; return true;
}
int install() { prepare(); return 0; }
core::Install s_install{"Workers", &install, 9};
}
void prepare() {
    for (Lane lane : {Lane::Data, Lane::Compute, Lane::Io, Lane::Navigation}) {
        auto& q = queue(lane);
        // Interactive navigation must not sit behind text/buffer preparation or
        // lose CPU priority while the player is waiting for each key response.
        if (lane == Lane::Navigation) q.priority = THREAD_PRIORITY_NORMAL;
        (void)start(q);
    }
}
bool post(std::function<void()> work, Lane lane)
{
    auto& q = queue(lane);
    // Owner-side requests never wait for a worker queue. A busy/full lane fails
    // explicitly; callers retain context/error handling, never run work inline.
    std::unique_lock lock(q.lock, std::try_to_lock);
    if (!lock.owns_lock() || q.work.size() >= 256) return false;
    if (!q.started) return false; // never create threads inside a game callback
    q.work.push_back(std::move(work));
    SetEvent(q.wake);
    return true;
}
}
