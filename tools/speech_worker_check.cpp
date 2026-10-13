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
    // Exact pre-fix consumer expression from 8a89e04 speech.cpp. Reproduce the
    // pressure handoff with the unchanged production Queue before testing repair.
    static speech::Queue legacy;
    std::atomic<bool> legacy_carry{true};
    auto original_interrupt = [&](bool interrupt) { return interrupt || legacy_carry.exchange(false); };
    legacy.say(L"Category: NPCs", original_interrupt(true));
    legacy.say(L"Soldier1", original_interrupt(false));
    expect(legacy.pop(line) && std::wstring(line.text) == L"Soldier1" && line.interrupt && !legacy.pop(line),
        "original pressure expression reproduces the missing category/interrupting focus failure");
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
    const auto ordered = std::make_shared<const speech::Parts>(speech::Parts{L"<b>Category: NPCs</b>", L"Soldier1", L"Soldier1"});
    speech::say_sequence(ordered, true);
    expect(dequeue(&line) && std::wstring(line.text) == L"Category: NPCs" && line.interrupt,
        "category starts its ordered event with one interruption");
    const auto category_generation = line.generation;
    expect(dequeue(&line) && std::wstring(line.text) == L"Soldier1" && !line.interrupt && line.generation == category_generation,
        "initial destination queues behind category in the same generation");
    expect(dequeue(&line) && std::wstring(line.text) == L"Soldier1" && !line.interrupt,
        "sequence keeps distinct identical parts without deduplication");
    speech::say_sequence(std::make_shared<const speech::Parts>(speech::Parts{L"", L"<b></b>", L"Category: Events", L"Magic Circle"}), true);
    expect(dequeue(&line) && std::wstring(line.text) == L"Category: Events" && line.interrupt,
        "empty or markup-only leading parts do not consume the single interrupt");
    enqueue(L"new manual selection", true, false);
    expect(dequeue(&line) && std::wstring(line.text) == L"new manual selection" && !dequeue(&line),
        "a later real input retires the old sequence tail");
    g_carry_interrupt = true; // simulate the retained flag after ingress overflow
    enqueue(L"category after overflow", true, false); enqueue(L"initial destination", false, false);
    expect(dequeue(&line) && std::wstring(line.text) == L"category after overflow" && line.interrupt,
        "an explicit interrupt consumes any carried overflow interrupt");
    expect(dequeue(&line) && std::wstring(line.text) == L"initial destination" && !line.interrupt,
        "carried overflow interrupt cannot clear the category or interrupt its queued focus");
    g_carry_interrupt = true;
    speech::say_sequence(ordered, true);
    expect(dequeue(&line) && std::wstring(line.text) == L"Category: NPCs" && line.interrupt,
        "sequence consumes the carried overflow interrupt once");
    expect(dequeue(&line) && !line.interrupt && dequeue(&line) && !line.interrupt && !g_carry_interrupt,
        "all sequence follow-ups remain queued after pressure recovery");
    g_carry_interrupt = true;
    enqueue(L"<b></b>", false, false); enqueue(L"usable follow-up", false, false);
    expect(dequeue(&line) && std::wstring(line.text) == L"usable follow-up" && line.interrupt,
        "an empty cleaned message cannot lose a carried interruption");
    const double requested_at = speech_now();
    speech::say_sequence(ordered, true, {speech::Feature::EntityCategory, requested_at, requested_at - 0.01});
    expect(dequeue(&line) && line.timing.feature == speech::Feature::EntityCategory && line.timing.first &&
        line.timing.requested_at == requested_at && line.timing.enqueued_at >= requested_at &&
        line.timing.captured_at == requested_at - 0.01,
        "owned request, queue and snapshot-age timestamps survive the real ingress/chunk path");
    expect(dequeue(&line) && !line.timing.first && dequeue(&line) && !line.timing.first,
        "follow-ups/chunks cannot be mistaken for another first usable output");
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
