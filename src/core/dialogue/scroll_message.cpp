// Ports FF1 Patches/ScrollMessagePatches.cs: fade messages (location banners, system lines),
// line-fade messages (game over, story text) and the scrolling intro/ending text, paced to the
// visual scroll.
#include <memory>
#include <string>
#include <vector>
#include "core.h"
#include "dedup.h"
#include "frame.h"
#include "hooks.h"
#include "log.h"
#include "refs.h"
#include "say.h"
#include "text.h"
#include "unity.h"
#include "dialogue/dialogue.h"
#include "fieldnav/map_transition.h"

namespace ffpr::dialogue {
namespace {

ref::Class kFadeManager{"Last.Message", "FadeMessageManager"};
ref::Class kLineFadeController{"Last.UI.Message", "LineFadeMessageWindowController"};
ref::Class kScrollManager{"Last.Message", "ScrollMessageManager"};
ref::Method mFadePlay{kFadeManager, "Play", 9};
ref::Method mLineFadeSetData{kLineFadeController, "SetData", 5};
ref::Method mScrollPlay{kScrollManager, "Play", 7};

// FF1 lastScrollMessage: one slot shared by the three hooks, never reset (Session scope). It holds
// the raw message, compared before cleaning.
constexpr const char* kCtxSystemMessage = "System.Message";

// Message.Replace("\n"," ").Replace("\r"," "), collapse double spaces, Trim.
std::wstring flatten(const std::wstring& message)
{
    std::wstring s = message;
    for (wchar_t& c : s)
        if (c == L'\n' || c == L'\r') c = L' ';
    std::wstring out;
    out.reserve(s.size());
    for (wchar_t c : s) {
        if (c == L' ' && !out.empty() && out.back() == L' ') continue;
        out.push_back(c);
    }
    return text::trim(out);
}

// FadeMessageManager.Play(string message, int fontSize, Color32 color, float fadeinTime,
// float fadeoutTime, float waitTime, bool isCenterAnchor, float postionX, float postionY)
void fade_play_postfix(void* message_str)
{
    const std::wstring message = ref::str(message_str);
    if (message.empty()) return;
    if (!dedup::text(kCtxSystemMessage, message, dedup::Scope::Session)) return;

    const std::wstring clean = flatten(message);
    // Skip a banner that repeats the map transition's "Entering X" (LocationMessageTracker,
    // owned by the map-transition area).
    if (!fieldnav::location_message::should_announce_fade_message(clean)) return;
    ffpr::say(clean, false);
}

// LineFadeMessageWindowController.SetData(List<string> messages, ...): some paths (game over)
// bypass the managers and call the controller directly.
void line_fade_set_data_postfix(void* messages)
{
    if (!messages) return;
    const int count = unity::list_count(messages);
    if (count == 0) return;

    std::wstring combined;
    for (int i = 0; i < count; ++i) {
        std::wstring line = ref::str(unity::list_at(messages, i));
        if (line.empty()) continue;
        if (!combined.empty()) combined += L" ";
        combined += line;
    }
    if (combined.empty()) return;
    if (!dedup::text(kCtxSystemMessage, combined, dedup::Scope::Session)) return;

    const std::wstring clean = flatten(combined);
    // Interrupt for the defeat message so it is heard over a preceding KO message.
    const bool is_defeat = text::contains(text::lower(clean), L"defeated");
    ffpr::say(clean, is_defeat);
}

// SpeakScrollLinesWithTiming: the first line at once, then one line per (scrollTime / (lines+1))
// seconds, matching the linear visual scroll. Blank lines are skipped without a wait.
struct ScrollRun {
    std::vector<std::wstring> lines;
    size_t next = 0;
    double delay = 0;
};

void speak_next_scroll_line(const std::shared_ptr<ScrollRun>& run)
{
    while (run->next < run->lines.size()) {
        const std::wstring line = text::trim(run->lines[run->next++]);
        if (line.empty()) continue;
        ffpr::say(line, false);
        if (run->next < run->lines.size())
            frame::after_seconds(run->delay, [run] {
                hooks::guarded("ScrollMessage.Line", [&] { speak_next_scroll_line(run); });
            }, "ScrollMessage.Line");
        return;
    }
}

// ScrollMessageManager.Play(ScrollType type, string message, float scrollTime, int fontSize,
// Color32 color, TextAnchor anchor, Rect margin)
void scroll_play_postfix(void* message_str, float scroll_time)
{
    const std::wstring message = ref::str(message_str);
    if (message.empty()) return;
    if (!dedup::text(kCtxSystemMessage, message, dedup::Scope::Session)) return;

    // Stop any previous scroll announcement.
    frame::cancel_label("ScrollMessage.Line");

    auto run = std::make_shared<ScrollRun>();
    // Split by newlines, dropping empty entries.
    std::wstring cur;
    for (wchar_t c : message) {
        if (c == L'\n' || c == L'\r') {
            if (!cur.empty()) run->lines.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) run->lines.push_back(cur);
    run->delay = static_cast<double>(scroll_time) / static_cast<double>(run->lines.size() + 1);
    speak_next_scroll_line(run);
}

// Value-type arguments exactly as IL2CPP passes them: Color32 (4 bytes) in a register, Rect
// (16 bytes) by reference to a caller copy, enums as int32.
struct Rect16 { float x, y, w, h; };

using FadePlay_t = void (*)(void* self, void* message, int32_t font_size, uint32_t color, float fade_in,
                            float fade_out, float wait, bool center, float pos_x, float pos_y, const void* mi);
using LineFadeSetData_t = void (*)(void* self, void* messages, uint32_t color, float fade_in, float fade_out,
                                   float wait, const void* mi);
using ScrollPlay_t = void (*)(void* self, int32_t type, void* message, float scroll_time, int32_t font_size,
                              uint32_t color, int32_t anchor, Rect16 margin, const void* mi);
FadePlay_t o_FadePlay;
LineFadeSetData_t o_LineFadeSetData;
ScrollPlay_t o_ScrollPlay;

void d_FadePlay(void* self, void* message, int32_t font_size, uint32_t color, float fade_in, float fade_out,
                float wait, bool center, float pos_x, float pos_y, const void* mi)
{
    o_FadePlay(self, message, font_size, color, fade_in, fade_out, wait, center, pos_x, pos_y, mi);
    hooks::guarded("FadeMessageManager.Play", [&] { fade_play_postfix(message); });
}

void d_LineFadeSetData(void* self, void* messages, uint32_t color, float fade_in, float fade_out, float wait,
                       const void* mi)
{
    o_LineFadeSetData(self, messages, color, fade_in, fade_out, wait, mi);
    hooks::guarded("LineFadeMessageWindowController.SetData", [&] { line_fade_set_data_postfix(messages); });
}

void d_ScrollPlay(void* self, int32_t type, void* message, float scroll_time, int32_t font_size, uint32_t color,
                  int32_t anchor, Rect16 margin, const void* mi)
{
    o_ScrollPlay(self, type, message, scroll_time, font_size, color, anchor, margin, mi);
    hooks::guarded("ScrollMessageManager.Play", [&] { scroll_play_postfix(message, scroll_time); });
}

int install()
{
    int n = 0;
    if (hooks::attach(mFadePlay, &d_FadePlay, &o_FadePlay)) ++n;
    if (hooks::attach(mLineFadeSetData, &d_LineFadeSetData, &o_LineFadeSetData)) ++n;
    if (hooks::attach(mScrollPlay, &d_ScrollPlay, &o_ScrollPlay)) ++n;
    return n;
}

core::Install s_install{"ScrollMessage", &install, 100};

} // namespace
} // namespace ffpr::dialogue
