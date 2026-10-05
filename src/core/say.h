#pragma once
#include <string>
#include <string_view>

// The mod's speech front end. Every reader speaks through say(); speech.h is the backend.
//
// Policy carried over from the C# mods: interrupt = true for hotkeys and anything the player
// asked for, false for game events that may follow one another (a dialogue page after the
// speaker's name). Markup is stripped here, so a reader passes the game's text as it is.
//
// No text deduplication happens here (shared rules: identical text can belong to distinct
// events). Readers that must suppress a repeated EVENT use dedup.h.
namespace ffpr {

void say(std::wstring_view text, bool interrupt = true);
/// Queue behind whatever is speaking (interrupt = false).
inline void say_queued(std::wstring_view text) { say(text, false); }
/// Stop speech now.
void silence();
/// The last line spoken through say() (for "repeat"). Main thread.
const std::wstring& last_spoken();

} // namespace ffpr
