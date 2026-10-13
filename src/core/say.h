#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "speech_timing.h"

// The mod's speech front end. Every reader speaks through say(); speech.h is the backend.
//
// Interrupt once for a new explicit input/context. Follow-ups within that event queue
// behind its headline; use say_sequence for ready parts and QueuedSpeechScope for
// known mod-only callback continuations. Dialogue and battle event chains queue.
// Cleaning belongs to the speech worker, so readers pass the displayed text as it is.
//
// No text deduplication happens here (shared rules: identical text can belong to distinct
// events). Readers that must suppress a repeated EVENT use dedup.h.
namespace ffpr {
/// Native-owner scope for known mod-only callback continuations. Their output
/// queues behind the answer/heading already emitted by the owning input event.
/// Never use around native game operations or on background workers.
class QueuedSpeechScope {
public:
    QueuedSpeechScope();
    ~QueuedSpeechScope();
    QueuedSpeechScope(const QueuedSpeechScope&) = delete;
};
void say_parts(std::vector<std::wstring> parts, bool interrupt = true);
/// Headline, initial focus/details: one ordered event with a single interruption.
void say_sequence(std::vector<std::wstring> parts, bool interrupt = true);
void say_sequence(std::vector<std::wstring> parts, bool interrupt, speech::Timing timing);

void say(std::wstring_view text, bool interrupt = true);
void say(std::wstring_view text, bool interrupt, speech::Timing timing);
/// Queue behind whatever is speaking (interrupt = false).
inline void say_queued(std::wstring_view text) { say(text, false); }
/// Stop speech now.
void silence();
/// The last line spoken through say() (for "repeat"). Main thread.
const std::wstring& last_spoken();

} // namespace ffpr
