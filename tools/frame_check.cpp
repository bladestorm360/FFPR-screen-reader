// Offline scheduler lifetime/concurrency regression; no game or IL2CPP runtime.
#include "../src/core/frame.cpp"
namespace sound { void initialize() {} }
#include "../src/core/background.cpp"
#include "../src/core/worker_job.h"
#include <atomic>
#include <cstdio>
#include <memory>
#include <thread>

namespace ref {
Class::Class(const char* n, const char* c, uint8_t g, const char* a) : ns(n), name(c), games(g), assembly(a) {}
Method::Method(Class& c, const char* n, int a) : cls(c), name(n), argc(a) {}
}
namespace hooks {
bool attach_raw(ref::Method&, void*, void**) { return false; }
void note_fault(const char*) { std::abort(); }
}
namespace speech { void request_load() {} }
namespace log_ { void write(const char*, ...) {} }
namespace core { Install::Install(const char* n, int (*f)(), int o, uint8_t g) : name(n), fn(f), order(o), games(g) {} }

namespace fixture {
int native_calls = 0;
DWORD main_thread = GetCurrentThreadId();
HANDLE destroyed = nullptr;
std::atomic<bool> destroyed_on_main{false};
void destroy(int* value) { destroyed_on_main=GetCurrentThreadId()==main_thread;delete value;SetEvent(destroyed); }
void update(void*, const void*) { ++native_calls; }
}
int main()
{
    background::prepare();
    int failures = 0;
    auto expect = [&](bool ok, const char* message) { if (!ok) { ++failures; std::printf("FAIL %s\n", message); } };
    auto lifetime = std::make_shared<int>(17);
    std::weak_ptr<int> weak = lifetime;
    bool nested = false, later_cancelled = false;
    frame::every_frame([lifetime, weak, &expect, &nested] {
        frame::cancel_label("Lifetime");
        expect(!weak.expired() && *lifetime == 17, "cancelling an executing callback retains its captures");
        frame::next_frame([&nested] { nested = true; }, "Nested");
        return true;
    }, "Lifetime", 3);
    lifetime.reset();
    frame::next_frame([&later_cancelled] { later_cancelled = true; }, "Lifetime.Next");
    frame::run_tick();
    expect(weak.expired(), "captures release after the cancelled callback returns");
    expect(!later_cancelled && !nested, "cancellation skips later work and newly queued work waits");
    frame::run_tick();
    expect(nested, "work queued during a tick runs on its next frame");

    int readiness_reads = 0;
    frame::every_frame([frames = 0, &readiness_reads]() mutable {
        if (++frames < 2) return true;
        ++readiness_reads;
        return false;
    }, "Popup.Readiness", 31);
    frame::run_tick();
    expect(readiness_reads == 0, "popup reader waits for its native presentation frame");
    frame::run_tick();
    expect(readiness_reads == 1 && frame::pending() == 0,
           "mutable readiness counter survives between frames and reader finishes once");

    std::vector<int> ticks;
    frame::every_frame([value = 0, &ticks]() mutable {
        ticks.push_back(++value);
        return value < 3;
    }, "Mutable.State", 5);
    for (int i = 0; i < 3; ++i) frame::run_tick();
    expect(ticks == std::vector<int>({1, 2, 3}), "mutable callback state progresses on every invocation");

    std::atomic<int> delivered = 0;
    const auto main_id = std::this_thread::get_id();
    std::atomic<bool> wrong_thread = false;
    std::vector<std::thread> producers;
    for (int t = 0; t < 4; ++t) producers.emplace_back([&] {
        for (int i = 0; i < 250; ++i) frame::post([&] {
            if (std::this_thread::get_id() != main_id) wrong_thread = true;
            ++delivered;
        });
    });
    for (auto& t : producers) t.join();
    const double posts_by = frame::now() + 5;
    while (delivered < 1000 && frame::now() < posts_by) frame::run_tick();
    expect(delivered == 1000 && !wrong_thread, "bounded continuation delivers all cross-thread posts exactly once on the dispatcher thread");
    expect(frame::pending() == 0, "finished/cancelled tasks leave no retained queue entries");
    std::atomic<bool> worker_started = false;
    std::atomic<int> worker_priority = 0;
    int published = 0;
    HANDLE release = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE completed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    expect(background::post([&] {
        if (std::this_thread::get_id() == main_id) wrong_thread = true;
        worker_priority = GetThreadPriority(GetCurrentThread());
        worker_started = true;
        WaitForSingleObject(release, 5000);
        frame::post([&] { ++published; });
        SetEvent(completed);
    }), "native data worker starts without a game runtime");
    const double deadline = frame::now() + 5;
    while (!worker_started && frame::now() < deadline) std::this_thread::yield();
    const auto before = frame::count();
    for (int i = 0; i < 100; ++i) frame::run_tick();
    expect(frame::count() == before + 100 && published == 0,
           "main dispatcher keeps running while background preparation is blocked");
    HANDLE independent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    expect(background::post([&] { SetEvent(independent); }, background::Lane::Compute), "compute lane starts independently");
    expect(WaitForSingleObject(independent, 5000) == WAIT_OBJECT_0, "blocked data work cannot hold up compute jobs");
    CloseHandle(independent);
    SetEvent(release);
    expect(WaitForSingleObject(completed, 5000) == WAIT_OBJECT_0, "worker completion is bounded");
    frame::run_tick();
    expect(published == 1 && !wrong_thread && worker_priority == THREAD_PRIORITY_BELOW_NORMAL,
           "data worker has lower priority and publishes once through the main dispatcher");
    CloseHandle(release); CloseHandle(completed);
    worker_job::Owner owner;
    int result = 0;
    HANDLE returned = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    expect(worker_job::start(owner, 7, [returned](int input) { SetEvent(returned); return input * 2; },
        [&](int output) { if (std::this_thread::get_id() != main_id) wrong_thread = true; result = output; }), "owned snapshot job is accepted");
    expect(WaitForSingleObject(returned, 5000) == WAIT_OBJECT_0, "snapshot computation finishes on the worker");
    // Wait for the posted result using dispatcher ticks, never a game-thread worker join.
    const double publish_by = frame::now() + 5;
    while (!result && frame::now() < publish_by) { frame::run_tick(); std::this_thread::yield(); }
    expect(result == 14 && !wrong_thread, "publication remains on the native owner");
    HANDLE entered = CreateEventW(nullptr, TRUE, FALSE, nullptr), unblock = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    ResetEvent(returned);
    expect(worker_job::start(owner, 9, [=](int input) {
        SetEvent(entered); WaitForSingleObject(unblock, 5000); SetEvent(returned); return input;
    }, [&](int) { result = -1; }), "cancellable snapshot job is accepted");
    expect(WaitForSingleObject(entered, 5000) == WAIT_OBJECT_0, "worker captures the old context");
    owner.cancel(); SetEvent(unblock); WaitForSingleObject(returned, 5000);
    for (int i = 0; i < 100; ++i) { frame::run_tick(); std::this_thread::yield(); }
    expect(result == 14, "phase cancellation cannot publish an obsolete job");
    CloseHandle(entered); CloseHandle(unblock); CloseHandle(returned);
    // A ready-result burst must not keep native input inside the mod dispatcher.
    // Synthetic 50-us owner callbacks make the old unbounded 150-post drain decisive.
    int posted_burst = 0, due_burst = 0;
    auto short_work = [] { const double until = frame::now() + .00005; while (frame::now() < until) {} };
    std::thread burst([&] { for (int i = 0; i < 150; ++i) frame::post([&, short_work] { short_work(); ++posted_burst; }); });
    burst.join();
    for (int i = 0; i < 150; ++i) frame::next_frame([&, short_work] { short_work(); ++due_burst; }, "Load.Burst");
    frame::o_Update = &fixture::update;
    frame::d_Update(nullptr, nullptr);
    expect(fixture::native_calls == 1 && posted_burst > 0 && posted_burst < 150 && due_burst > 0 && due_burst < 150,
           "native update proceeds while both publication and owner-reader bursts remain pending");
    double largest_ms = 0;
    const double burst_by = frame::now() + 5;
    while ((posted_burst < 150 || due_burst < 150) && frame::now() < burst_by) {
        const double started = frame::now(); frame::d_Update(nullptr, nullptr);
        largest_ms = std::max(largest_ms, (frame::now() - started) * 1000);
    }
    expect(posted_burst == 150 && due_burst == 150 && frame::pending() == 0,
           "fair bounded dispatcher preserves every burst callback without starving either queue");
    HANDLE locked = CreateEventW(nullptr,TRUE,FALSE,nullptr), unlock = CreateEventW(nullptr,TRUE,FALSE,nullptr);
    std::atomic<bool> lock_active{false};
    std::thread lock_owner([&]{AcquireSRWLockExclusive(&frame::g_post_lock);lock_active=true;SetEvent(locked);WaitForSingleObject(unlock,5000);lock_active=false;ReleaseSRWLockExclusive(&frame::g_post_lock);});
    expect(WaitForSingleObject(locked,5000)==WAIT_OBJECT_0,"worker holds publication lock for a decisive nonblocking check");
    int owner_post=0;const int native_before=fixture::native_calls;
    frame::post([&]{++owner_post;});
    for(int i=0;i<100;++i)frame::d_Update(nullptr,nullptr);
    expect(lock_active&&owner_post==1&&fixture::native_calls==native_before+100,
           "native update and owner-side post proceed while a worker holds the publication lock");
    SetEvent(unlock);lock_owner.join();CloseHandle(locked);CloseHandle(unlock);

    fixture::destroyed=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    HANDLE owned_entered=CreateEventW(nullptr,TRUE,FALSE,nullptr),owned_release=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    using Owned=std::shared_ptr<int>;
    expect(worker_job::start(owner,42,[=](int value){SetEvent(owned_entered);WaitForSingleObject(owned_release,5000);return Owned(new int(value),&fixture::destroy);},
        [&](Owned){result=-2;}),"owned cancelled-result job accepted");
    expect(WaitForSingleObject(owned_entered,5000)==WAIT_OBJECT_0,"owned job computes on helper");
    owner.cancel();SetEvent(owned_release);
    expect(WaitForSingleObject(fixture::destroyed,5000)==WAIT_OBJECT_0&&!fixture::destroyed_on_main,
           "result cancelled during computation is destroyed on its helper without game-thread publication");
    frame::d_Update(nullptr,nullptr);expect(result==14,"cancelled owned result cannot publish stale output");
    CloseHandle(owned_entered);CloseHandle(owned_release);CloseHandle(fixture::destroyed);
    std::printf("Loading burst: 300 synthetic 50-us callbacks across %d native updates; worst dispatch %.3f ms offline\n",
                fixture::native_calls, largest_ms);
    std::printf("%s: scheduler state/lifetime, 1000 posts, nonblocking data worker; %d failures\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
