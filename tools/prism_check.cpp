// check-sources:
// Isolated fake API: never loads Prism or calls a real screen reader.
#include "../src/core/prism_backend.h"
#include "../src/core/speech_queue.h"
#include <cstdio>
#include <vector>

namespace fake {
int freed = 0, stopped = 0, shutdowns = 0;
bool context_ok = true, backend_ok = true;
PrismError initialized = PRISM_ERROR_ALREADY_INITIALIZED;
uint64_t flags = PRISM_BACKEND_SUPPORTS_SPEAK | PRISM_BACKEND_SUPPORTS_BRAILLE |
    PRISM_BACKEND_SUPPORTS_OUTPUT | PRISM_BACKEND_SUPPORTS_STOP;
const char* version = "0.18.3";
std::vector<std::string> calls;
std::vector<bool> interrupts;
PrismConfig config_init() { return {}; }
PrismContext* init(PrismConfig*) { return context_ok ? reinterpret_cast<PrismContext*>(1) : nullptr; }
void shutdown(PrismContext*) { ++shutdowns; }
PrismBackend* acquire(PrismContext*) { return backend_ok ? reinterpret_cast<PrismBackend*>(2) : nullptr; }
PrismError initialize(PrismBackend*) { return initialized; }
const char* name(PrismBackend*) { return "Fake"; }
uint64_t features(PrismBackend*) { return flags; }
PrismError output(PrismBackend*, const char* text, bool interrupt) { calls.push_back(std::string("output:") + text); interrupts.push_back(interrupt); return PRISM_OK; }
PrismError speak(PrismBackend*, const char* text, bool interrupt) { calls.push_back(std::string("speak:") + text); interrupts.push_back(interrupt); return PRISM_OK; }
PrismError braille(PrismBackend*, const char* text) { calls.push_back(std::string("braille:") + text); return PRISM_OK; }
PrismError stop(PrismBackend*) { ++stopped; return PRISM_OK; }
void free(PrismBackend*) { ++freed; }
const char* error(PrismError) { return "fake error"; }
const char* get_version() { return version; }
speech::prism::Api api() {
    speech::prism::Api a;
    a.config_init = config_init; a.init = init; a.shutdown = shutdown; a.registry_acquire_best = acquire;
    a.backend_initialize = initialize; a.backend_name = name; a.backend_get_features = features;
    a.backend_output = output; a.backend_speak = speak; a.backend_braille = braille;
    a.backend_stop = stop; a.backend_free = free; a.error_string = error; a.version_string = get_version;
    return a;
}
}
int main() {
    int failures = 0;
    auto expect = [&](bool okay, const char* what) { if (!okay) { ++failures; std::printf("FAIL %s\n", what); } };
    speech::prism::Backend backend;
    expect(backend.load(fake::api()), "already-initialized acquired backend is accepted");
    backend.output(L"Category: NPCs", true);
    backend.output(L"Soldier1", false);
    expect(fake::calls[fake::calls.size()-2] == "output:Category: NPCs" && fake::calls.back() == "output:Soldier1" &&
        fake::interrupts[fake::interrupts.size()-2] && !fake::interrupts.back(),
        "combined Prism output preserves category interruption then queued initial destination");
    expect(backend.has_braille(), "native braille capability is retained");
    expect(backend.output(L"Zażółć 日本 😀", true) == PRISM_OK, "UTF-16 output encodes as UTF-8");
    expect(fake::calls.back() == "output:Zażółć 日本 😀" && fake::interrupts.back(), "combined output and interrupt flag reach the API");
    expect(backend.output(L"dots", false, true) == PRISM_OK && fake::calls.back() == "braille:dots", "braille-only remains braille-only");
    expect(backend.stop() == PRISM_OK && fake::stopped == 1, "stop reaches the backend");
    fake::flags &= ~PRISM_BACKEND_SUPPORTS_OUTPUT;
    size_t before = fake::calls.size(); backend.output(L"both", false);
    expect(fake::calls.size() == before + 2 && fake::calls[before] == "speak:both" && fake::calls.back() == "braille:both", "no combined capability preserves separate speech and braille");
    fake::flags &= ~(PRISM_BACKEND_SUPPORTS_BRAILLE | PRISM_BACKEND_SUPPORTS_STOP);
    expect(!backend.has_braille() && backend.output(L"dots", false, true) == PRISM_ERROR_NOT_IMPLEMENTED,
        "unsupported braille is honest and does not produce speech");
    expect(backend.stop() == PRISM_ERROR_NOT_IMPLEMENTED, "unsupported stop is reported");
    expect(backend.output(std::wstring(1, wchar_t(0xD800)), false) == PRISM_ERROR_INVALID_UTF8, "invalid UTF-16 cannot become corrupted output");
    static speech::Queue queue;
    queue.say(L"same", false); queue.say(L"same", false);
    speech::Line line; before = fake::calls.size();
    while (queue.pop(line)) backend.output(line.text, line.interrupt);
    expect(fake::calls.size() == before + 2, "distinct identical-text events are both submitted");
    queue.say(L"old", false); queue.say(L"new", true); queue.braille(L"dots");
    expect(queue.pop(line) && std::wstring(line.text) == L"new" && line.interrupt, "intentional interrupt cancels pending older events");
    expect(queue.pop(line) && line.kind == speech::Kind::Braille, "braille remains ordered after speech");
    queue.say(L"pending", false); queue.silence();
    expect(queue.pop(line) && line.silence && !queue.pop(line), "silence cancels queued requests");
    backend.close(); backend.close();
    expect(fake::freed == 1 && fake::shutdowns == 1, "teardown releases backend before context exactly once");
    auto missing = fake::api(); missing.backend_output = nullptr;
    expect(!backend.load(missing), "missing exports fail closed");
    fake::version = "0.18.2"; expect(!backend.load(fake::api()), "incompatible version fails closed");
    fake::version = "0.18.3"; fake::context_ok = false;
    expect(!backend.load(fake::api()), "null initialization is handled");
    fake::context_ok = true; fake::backend_ok = false;
    expect(!backend.load(fake::api()) && fake::shutdowns == 2, "no backend releases its context");
    fake::backend_ok = true; fake::initialized = PRISM_ERROR_INTERNAL;
    expect(!backend.load(fake::api()) && fake::freed == 2 && fake::shutdowns == 3, "backend initialization failure releases all ownership");
    expect(backend.output(L"silent", false) == PRISM_ERROR_NOT_INITIALIZED, "unavailable service stays safe");
    std::printf("Prism: UTF-8, channels, queue, capabilities and lifecycle: %d failures\n", failures);
    return failures ? 1 : 0;
}
