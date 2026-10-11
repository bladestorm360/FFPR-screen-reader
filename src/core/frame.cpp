#include <windows.h>
#include <cstring>
#include <string>
#include <vector>
#include "frame.h"
#include "hooks.h"
#include "speech.h"

namespace frame {
namespace {

ref::Class  kInputSystemManager{"System.Input", "InputSystemManager"};
ref::Method mUpdate{kInputSystemManager, "Update", 0};

using Update_t = void (*)(void* self, const void* mi);
Update_t o_Update = nullptr;

struct Pending {
    int id;
    std::string label;
    uint64_t due_frame;     // 0 = time-based
    double due_time;        // 0 = frame-based
    Task task;
    Step step;              // set for every_frame
    int frames_left;
    bool cancelled = false;
};

struct Sub { void (*fn)(); const char* label; };

std::vector<Sub> g_subs;
std::vector<Pending> g_tasks;
std::vector<Pending> g_adding;          // tasks queued while the tick runs
std::vector<Task> g_posted;             // cross-thread
SRWLOCK g_post_lock = SRWLOCK_INIT;
bool g_in_tick = false;
bool g_live = false;
uint64_t g_frame = 0;
int g_next_id = 1;
DWORD g_main_tid = 0;

void add(Pending&& p)
{
    if (g_in_tick) g_adding.push_back(std::move(p));
    else g_tasks.push_back(std::move(p));
}

void run_tick()
{
    ++g_frame;
    g_in_tick = true;

    for (const Sub& s : g_subs)
        hooks::guarded(s.label, [&] { s.fn(); });

    {
        std::vector<Task> posted;
        AcquireSRWLockExclusive(&g_post_lock);
        posted.swap(g_posted);
        ReleaseSRWLockExclusive(&g_post_lock);
        for (Task& t : posted) hooks::guarded("frame.post", [&] { t(); });
    }

    const double t_now = now();
    for (size_t i = 0; i < g_tasks.size();) {
        Pending& p = g_tasks[i];
        bool done = p.cancelled;
        if (!done && p.step) {
            // A callback can cancel itself. Keep its closure alive until it returns.
            Step step = p.step;
            bool again = hooks::guarded_r(p.label.c_str(), false, [&] { return step(); });
            done = p.cancelled || !again || --p.frames_left <= 0;
            if (again && p.frames_left <= 0)
                LOGF("frame: stopped %s after its frame budget", p.label.c_str());
        } else if (!done && ((p.due_frame && g_frame >= p.due_frame) || (p.due_time > 0 && t_now >= p.due_time))) {
            Task t = std::move(p.task);
            done = true;
            g_tasks.erase(g_tasks.begin() + static_cast<std::ptrdiff_t>(i));
            hooks::guarded(t ? "frame.task" : "frame.empty", [&] { if (t) t(); });
            continue;
        }
        if (done) g_tasks.erase(g_tasks.begin() + static_cast<std::ptrdiff_t>(i));
        else ++i;
    }

    g_in_tick = false;
    if (!g_adding.empty()) {
        for (Pending& p : g_adding) g_tasks.push_back(std::move(p));
        g_adding.clear();
    }
}

void d_Update(void* self, const void* mi)
{
    if (!g_live) {
        g_live = true;
        g_main_tid = GetCurrentThreadId();
        LOGF("frame: tick live on thread %lu", g_main_tid);
        speech::request_load();
    }
    hooks::guarded("frame.tick", [] { run_tick(); });
    o_Update(self, mi);
}

} // namespace

bool install()
{
    (void)now(); // Resolve the immutable clock frequency before enabling the tick.
    return hooks::attach(mUpdate, &d_Update, &o_Update);
}

bool live() { return g_live; }
uint64_t count() { return g_frame; }

double now()
{
    static const double frequency = [] {
        LARGE_INTEGER f{}; QueryPerformanceFrequency(&f);
        return static_cast<double>(f.QuadPart);
    }();
    LARGE_INTEGER c{};
    QueryPerformanceCounter(&c);
    return static_cast<double>(c.QuadPart) / frequency;
}

bool on_main_thread() { return g_main_tid && GetCurrentThreadId() == g_main_tid; }

void subscribe(void (*fn)(), const char* label) { g_subs.push_back({fn, label}); }

void next_frame(Task t, const char* label) { after_frames(1, std::move(t), label); }

void after_frames(int n, Task t, const char* label)
{
    if (n < 1) n = 1;
    add(Pending{g_next_id++, label ? label : "task", g_frame + static_cast<uint64_t>(n), 0.0,
                std::move(t), {}, 0});
}

void after_seconds(double s, Task t, const char* label)
{
    add(Pending{g_next_id++, label ? label : "timer", 0, now() + (s > 0 ? s : 0.0001),
                std::move(t), {}, 0});
}

int every_frame(Step s, const char* label, int max_frames)
{
    int id = g_next_id++;
    add(Pending{id, label ? label : "step", 0, 0.0, {}, std::move(s), max_frames > 0 ? max_frames : 1});
    return id;
}

void cancel(int id)
{
    for (auto* v : {&g_tasks, &g_adding})
        for (size_t i = 0; i < v->size(); ++i)
            if ((*v)[i].id == id) {
                if (g_in_tick && v == &g_tasks) {
                    // Do not destroy an executing callback's closure or shift the running loop.
                    (*v)[i].cancelled = true;
                } else {
                    v->erase(v->begin() + static_cast<std::ptrdiff_t>(i));
                }
                return;
            }
}

void cancel_label(const char* prefix)
{
    const size_t n = strlen(prefix);
    for (auto* v : {&g_tasks, &g_adding})
        for (size_t i = 0; i < v->size();) {
            if ((*v)[i].label.compare(0, n, prefix) == 0) {
                if (g_in_tick && v == &g_tasks) {
                    (*v)[i].cancelled = true;
                    ++i;
                } else {
                    v->erase(v->begin() + static_cast<std::ptrdiff_t>(i));
                }
            } else {
                ++i;
            }
        }
}

void post(Task t)
{
    AcquireSRWLockExclusive(&g_post_lock);
    g_posted.push_back(std::move(t));
    ReleaseSRWLockExclusive(&g_post_lock);
}

int pending() { return static_cast<int>(g_tasks.size() + g_adding.size()); }

} // namespace frame
