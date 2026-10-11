#include "background.h"
#include "hooks.h"
#include <windows.h>
#include <deque>
#include <mutex>

namespace background {
namespace {
struct Queue {
    std::mutex lock;
    std::deque<std::function<void()>> work;
    HANDLE wake = nullptr;
    bool started = false;
};
Queue& queue()
{
    // The proxy and worker live until process exit. Avoid CRT teardown destroying
    // a mutex/queue while the native worker is still unwinding its last task.
    static auto* q = new Queue;
    return *q;
}

DWORD WINAPI worker(void* context)
{
    auto& q = *static_cast<Queue*>(context);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
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
}
bool post(std::function<void()> work)
{
    auto& q = queue();
    std::lock_guard lock(q.lock);
    if (!q.started) {
        q.wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!q.wake) return false;
        HANDLE thread = CreateThread(nullptr, 0, worker, &q, 0, nullptr);
        if (!thread) { CloseHandle(q.wake); q.wake = nullptr; return false; }
        CloseHandle(thread);
        q.started = true;
    }
    q.work.push_back(std::move(work));
    SetEvent(q.wake);
    return true;
}
}
