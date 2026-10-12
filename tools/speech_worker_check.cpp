// check-sources: text.cpp
// Actual ingress/clean/chunk code; never loads Prism or invokes a speech backend.
#include "../src/core/speech.cpp"
#include <cstdio>
#include <thread>
#include <vector>
namespace settings { void save_pending() {} }
namespace log_ { void write(const char*, ...) {} void writeW(const wchar_t*, const wchar_t*) {} }
int main() {
    int failures = 0;
    auto expect = [&](bool ok, const char* message) { if (!ok) { ++failures; std::printf("FAIL %s\n", message); } };
    speech::init(); speech::Line line;
    enqueue(L"<b>Same</b>", false, false); enqueue(L"<b>Same</b>", false, false);
    expect(dequeue(&line) && std::wstring(line.text) == L"Same", "worker cleans raw rendered markup");
    expect(dequeue(&line) && std::wstring(line.text) == L"Same", "distinct identical speech events are retained");
    auto parts = std::make_shared<const speech::Parts>(speech::Parts{L"<b>Results</b>", L"100 gil", L"Weapons"});
    speech::say_parts(parts, true);
    bool joined = false;
    std::thread consumer([&] {
        joined = dequeue(&line) && std::wstring(line.text) == L"Results. 100 gil. Weapons" && line.interrupt;
    });
    consumer.join();
    expect(joined, "owned ready parts are joined and cleaned by the consumer worker");
    enqueue(L"old", false, false); enqueue(L"new", true, false); enqueue(L"detail", false, false);
    expect(dequeue(&line) && std::wstring(line.text) == L"new" && line.interrupt, "new native event cancels obsolete pending speech");
    expect(dequeue(&line) && std::wstring(line.text) == L"detail" && !line.interrupt, "following detail retains queue order");
    std::wstring page(2200, L'\u65e5'); enqueue(page.c_str(), true, false);
    size_t copied = 0; int interrupts = 0;
    while (dequeue(&line)) { copied += wcslen(line.text); interrupts += line.interrupt; }
    expect(copied == page.size() && interrupts == 1, "worker chunks the complete page with one interrupt");
    enqueue(L"obsolete", false, false); enqueue(nullptr, true, true);
    expect(dequeue(&line) && line.silence && !dequeue(&line), "silence is a native event cancellation");
    enqueue(L"<i>braille</i>", false, false, speech::Kind::Braille);
    expect(dequeue(&line) && line.kind == speech::Kind::Braille && std::wstring(line.text) == L"braille", "braille-only events retain their channel");
    // No consumer runs here; queue fullness cannot make the producer wait.
    for (int i = 0; i < 10000; ++i) enqueue(L"bounded output", false, false);
    int retained = 0; while (dequeue(&line)) ++retained;
    expect(retained == 1024, "full ingress preserves its bounded oldest-output policy");

    NonblockingQueue<int, 256> queue;
    std::atomic<int> produced{0}, received{0}; std::atomic<long long> sum{0};
    std::vector<std::thread> producers;
    for (int p = 0; p < 4; ++p) producers.emplace_back([&, p] {
        for (int i = 0; i < 2000; ++i) {
            int value = p * 2000 + i + 1;
            while (!queue.push(std::move(value))) std::this_thread::yield(); // test workers only
            ++produced;
        }
    });
    while (received < 8000) { int value; if (queue.pop(value)) { ++received; sum += value; } else std::this_thread::yield(); }
    for (auto& producer : producers) producer.join();
    expect(produced == 8000 && sum == 8000LL * 8001 / 2, "concurrent ingress transfers every successfully committed value once");
    std::printf("%s: nonblocking ingress, worker clean/chunk, interruption/order/channels, 8000 transfers; %d failures\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
