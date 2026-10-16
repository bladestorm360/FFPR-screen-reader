#include <windows.h>
#include <cstring>
#include <string>
#include <vector>
#include "frame.h"
#include "hooks.h"
#include "speech.h"
#include "audio/sound.h"

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
std::vector<Task> g_ready_posts;        // owner-only backlog, never shared with workers
size_t g_post_cursor = 0;
size_t g_task_cursor = 0;
constexpr double kDispatchSeconds = 0.0005; // shared elapsed budget, no fixed entry cap
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
    timing::frame.store(g_frame, std::memory_order_relaxed);
    g_in_tick = true;

    for (const Sub& s : g_subs)
        hooks::guarded(s.label, [&] { s.fn(); });

    // Native observations above retain their own bounded contracts. Ready jobs
    // share one elapsed allowance instead of each draining a full batch. Reserve
    // half for pending native-owner readers so a post burst cannot starve them.
    const double deadline = now() + kDispatchSeconds;
    const double post_deadline = deadline - kDispatchSeconds / 2;
    if (g_post_cursor == g_ready_posts.size()) {
        g_ready_posts.clear(); g_post_cursor = 0;
        // A producer may be appending. Never wait for its lock on the game thread.
        if (TryAcquireSRWLockExclusive(&g_post_lock)) {
            g_ready_posts.swap(g_posted);
            ReleaseSRWLockExclusive(&g_post_lock);
        }
    }
    while (g_post_cursor < g_ready_posts.size() && now() < post_deadline) {
        Task task = std::move(g_ready_posts[g_post_cursor++]);
        hooks::guarded("frame.post", [&] { if (task) task(); });
    }

    const double t_now = now();
    size_t remaining = g_tasks.size();
    // Rotate the start position: a long-lived capture cannot monopolize every
    // frame, and each eligible reader still runs at most once per tick.
    while (remaining && !g_tasks.empty() && now() < deadline) {
        --remaining;
        if (g_task_cursor >= g_tasks.size()) g_task_cursor = 0;
        Pending& p = g_tasks[g_task_cursor];
        bool done = p.cancelled;
        if (!done && p.step) {
            Step step = std::move(p.step);
            bool again = hooks::guarded_r(p.label.c_str(), false, [&] { return step(); });
            done = p.cancelled || !again || --p.frames_left <= 0;
            if (!done) p.step = std::move(step);
            if (again && p.frames_left <= 0)
                LOGF("frame: stopped %s after its frame budget", p.label.c_str());
        } else if (!done && ((p.due_frame && g_frame >= p.due_frame) || (p.due_time > 0 && t_now >= p.due_time))) {
            Task task = std::move(p.task);
            g_tasks.erase(g_tasks.begin() + static_cast<std::ptrdiff_t>(g_task_cursor));
            hooks::guarded(task ? "frame.task" : "frame.empty", [&] { if (task) task(); });
            continue;
        }
        if (done) g_tasks.erase(g_tasks.begin() + static_cast<std::ptrdiff_t>(g_task_cursor));
        else ++g_task_cursor;
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
        sound::initialize(); // only signals the dedicated SDL owner
    }
    const double before = now();
    static double previous = 0;
    const double gap_ms = previous ? (before - previous) * 1000 : 0;
    previous = before;
    timing::observe_gap(gap_ms/1000);
    { timing::Scope cpu("Input dispatch",timing::Kind::Mod,nullptr,0,true);
      hooks::guarded("frame.tick", [] { run_tick(); }); }
    const double native_started = now();
    { timing::Scope cpu("Native input update",timing::Kind::Original,nullptr,0,true);
      o_Update(self, mi); }
    const double finished = now();
    const double mod_ms = (native_started - before) * 1000;
    const double native_ms = (finished - native_started) * 1000;
    timing::summarize();
    if (mod_ms >= 1 || native_ms >= 25 || gap_ms >= 250)
        LOGF("[FrameTiming] frame %llu: mod %.3f ms, native %.3f ms, interval %.3f ms, %zu owner tasks, %zu ready posts",
            static_cast<unsigned long long>(g_frame), mod_ms, native_ms, gap_ms,
            g_tasks.size() + g_adding.size(), g_ready_posts.size() - g_post_cursor);
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
    if (on_main_thread()) { next_frame(std::move(t), "frame.main_post"); return; }
    AcquireSRWLockExclusive(&g_post_lock);
    g_posted.push_back(std::move(t));
    ReleaseSRWLockExclusive(&g_post_lock);
}

int pending() { return static_cast<int>(g_tasks.size() + g_adding.size() + g_ready_posts.size() - g_post_cursor); }

} // namespace frame
