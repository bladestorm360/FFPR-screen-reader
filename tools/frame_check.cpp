// Offline scheduler lifetime/concurrency regression; no game or IL2CPP runtime.
#include "../src/core/frame.cpp"
#include "../src/core/background.cpp"
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

int main()
{
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
    frame::run_tick();
    expect(delivered == 1000 && !wrong_thread, "all cross-thread posts run exactly once on the dispatcher thread");
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
    SetEvent(release);
    expect(WaitForSingleObject(completed, 5000) == WAIT_OBJECT_0, "worker completion is bounded");
    frame::run_tick();
    expect(published == 1 && !wrong_thread && worker_priority == THREAD_PRIORITY_BELOW_NORMAL,
           "data worker has lower priority and publishes once through the main dispatcher");
    CloseHandle(release); CloseHandle(completed);
    std::printf("%s: scheduler state/lifetime, 1000 posts, nonblocking data worker; %d failures\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
