// Offline scheduler lifetime/concurrency regression; no game or IL2CPP runtime.
#include "../src/core/frame.cpp"
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
    std::printf("%s: scheduler lifetime and 1000 concurrent posts; %d failures\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
