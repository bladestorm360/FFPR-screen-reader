#include <windows.h>
#include "say.h"
#include "speech.h"
#include "text.h"

namespace ffpr {
namespace {
std::wstring g_last;
std::shared_ptr<const speech::Parts> g_last_parts;
int g_queued_scope = 0; // same native owner as repeat state
}

void say(std::wstring_view raw, bool interrupt)
{
    say(raw, interrupt, {});
}
void say(std::wstring_view raw, bool interrupt, speech::Timing timing)
{
    if (raw.empty()) return;
    // Owner-thread repeat state. Cleaning/chunking belongs to the speech worker.
    g_last.assign(raw);
    g_last_parts.reset();
    speech::request_load();
    speech::say(g_last.c_str(), interrupt && !g_queued_scope, timing);
}

void silence() { speech::silence(); }

void say_parts(std::vector<std::wstring> parts, bool interrupt) {
    if (parts.empty()) return;
    g_last.clear();
    g_last_parts = std::make_shared<const speech::Parts>(std::move(parts));
    speech::request_load(); speech::say_parts(g_last_parts, interrupt && !g_queued_scope);
}

void say_sequence(std::vector<std::wstring> parts, bool interrupt) {
    say_sequence(std::move(parts), interrupt, {});
}
void say_sequence(std::vector<std::wstring> parts, bool interrupt, speech::Timing timing) {
    if (parts.empty()) return;
    g_last.clear();
    g_last_parts = std::make_shared<const speech::Parts>(std::move(parts));
    speech::request_load(); speech::say_sequence(g_last_parts, interrupt && !g_queued_scope, timing);
}

QueuedSpeechScope::QueuedSpeechScope() { ++g_queued_scope; }
QueuedSpeechScope::~QueuedSpeechScope() { --g_queued_scope; }

const std::wstring& last_spoken() {
    // Explicit inspection only; automatic producers never join buffers here.
    if (g_last_parts) g_last = text::join(*g_last_parts, L". ");
    return g_last;
}

} // namespace ffpr
