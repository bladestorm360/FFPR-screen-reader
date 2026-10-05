// Ports FF1 Patches/MessageWindowPatches.cs: NPC dialogue and system messages, page by page as
// each page is shown, with the speaker's name when it changes; R repeats the last page.
#include <string>
#include <vector>
#include "core.h"
#include "dedup.h"
#include "hooks.h"
#include "log.h"
#include "modtext.h"
#include "refs.h"
#include "say.h"
#include "text.h"
#include "unity.h"
#include "dialogue/dialogue.h"

namespace ffpr::dialogue {
namespace {

ref::Class kMwm{"Last.Message", "MessageWindowManager"};
ref::Field fMessageList{kMwm, "messageList"};          // List<string>
ref::Field fNewPageLineList{kMwm, "newPageLineList"};  // List<int>
ref::Field fSpeakerValue{kMwm, "spekerValue"};         // (sic) the game's spelling
ref::Field fCurrentPageNumber{kMwm, "currentPageNumber"};
ref::Method mSetContent{kMwm, "SetContent", 1};
ref::Method mPlayingInit{kMwm, "PlayingInit", 0};
ref::Method mClose{kMwm, "Close", 0};

// Dedup contexts (FF1 lastAnnouncedPageIndex / lastAnnouncedSpeaker). The Dialogue scope is reset
// by events::Kind::DialogueClosed, which Close fires (FF1 ClearState); a new message resets the
// page context (FF1 SetContent).
constexpr const char* kCtxPage = "Dialogue.Page";
constexpr const char* kCtxSpeaker = "Dialogue.Speaker";

std::vector<std::wstring> g_message_list;   // currentMessageList (cleaned lines)
std::vector<int> g_page_starts;             // currentPageBreaks (line index where each page starts)
int g_last_page = -1;                       // lastAnnouncedPageIndex (also for the repeat)
std::wstring g_current_speaker;             // currentSpeaker
std::wstring g_last_speaker;                // lastAnnouncedSpeaker (also for the repeat)
bool g_in_dialogue = false;                 // IsInDialogue

// CleanMessage: strip icon markup, newlines to spaces, collapse double spaces, trim.
std::wstring clean_message(const std::wstring& message)
{
    if (message.empty()) return message;
    std::wstring clean = text::trim(text::strip_markup(message));
    for (wchar_t& c : clean)
        if (c == L'\n' || c == L'\r') c = L' ';
    std::wstring out;
    out.reserve(clean.size());
    for (wchar_t c : clean) {
        if (c == L' ' && !out.empty() && out.back() == L' ') continue;
        out.push_back(c);
    }
    return text::trim(out);
}

// GetPageText: the page's non-blank lines joined by spaces ("" when none).
std::wstring page_text(int page)
{
    if (page < 0 || page >= static_cast<int>(g_page_starts.size())) return {};
    const int start = g_page_starts[static_cast<size_t>(page)];
    const int end = (page + 1 < static_cast<int>(g_page_starts.size())) ? g_page_starts[static_cast<size_t>(page) + 1]
                                                                       : static_cast<int>(g_message_list.size());
    std::wstring out;
    for (int i = start; i < end && i < static_cast<int>(g_message_list.size()); ++i) {
        const std::wstring& line = g_message_list[static_cast<size_t>(i)];
        if (text::is_blank(line)) continue;
        if (!out.empty()) out += L" ";
        out += line;
    }
    return out;
}

// ClearState
void clear_state()
{
    g_message_list.clear();
    g_page_starts.clear();
    g_last_page = -1;
    g_current_speaker.clear();
    g_last_speaker.clear();
    g_in_dialogue = false;
}

// SetContent_Postfix: store the lines and page starts for page-by-page reading.
void set_content_postfix(void* self)
{
    void* list = ref::rd<void*>(self, fMessageList);
    if (!list) {
        LOGF("[MessageWindow] messageList pointer is null");
        return;
    }
    const int count = unity::list_count(list);
    if (count == 0) return;

    g_message_list.clear();
    for (int i = 0; i < count; ++i) {
        void* s = unity::list_at(list, i);
        g_message_list.push_back(s ? clean_message(ref::str(s)) : std::wstring());
    }

    // newPageLineList holds each page's ENDING line index (inclusive); convert to START indices.
    g_page_starts.clear();
    void* breaks = ref::rd<void*>(self, fNewPageLineList);
    const int nbreaks = breaks ? unity::list_count(breaks) : 0;
    g_page_starts.push_back(0);   // the first page always starts at line 0
    for (int i = 0; i < nbreaks; ++i) {
        const int next_start = unity::list_value<int32_t>(breaks, i) + 1;
        if (next_start < static_cast<int>(g_message_list.size())) g_page_starts.push_back(next_start);
    }

    // Reset page tracking.
    g_last_page = -1;
    dedup::reset(kCtxPage);
    g_in_dialogue = true;
}

// PlayingInit_Postfix: each page as it starts displaying (the last page included).
void playing_init_postfix(void* self)
{
    if (!g_in_dialogue || g_page_starts.empty()) return;

    const int page = ref::rd<int32_t>(self, fCurrentPageNumber);
    if (page < 0 || page >= static_cast<int>(g_page_starts.size())) return;

    std::wstring text_ = page_text(page);
    if (text::is_blank(text_)) return;
    if (!dedup::index(kCtxPage, page, dedup::Scope::Dialogue)) return;

    // The speaker, prepended when it changed.
    const std::wstring speaker = ref::rd_str(self, fSpeakerValue);
    if (!text::is_blank(speaker)) g_current_speaker = clean_message(speaker);
    if (!g_current_speaker.empty() && dedup::text(kCtxSpeaker, g_current_speaker, dedup::Scope::Dialogue)) {
        text_ = g_current_speaker + L": " + text_;
        g_last_speaker = g_current_speaker;
    }

    g_last_page = page;
    ffpr::say(text_, false);
}

using SetContent_t = void (*)(void* self, void* content_list, const void* mi);
using Void_t = void (*)(void* self, const void* mi);
SetContent_t o_SetContent;
Void_t o_PlayingInit;
Void_t o_Close;

void d_SetContent(void* self, void* content_list, const void* mi)
{
    o_SetContent(self, content_list, mi);
    hooks::guarded("MessageWindow.SetContent", [&] { set_content_postfix(self); });
}

void d_PlayingInit(void* self, const void* mi)
{
    o_PlayingInit(self, mi);
    hooks::guarded("MessageWindow.PlayingInit", [&] { playing_init_postfix(self); });
}

void d_Close(void* self, const void* mi)
{
    o_Close(self, mi);
    hooks::guarded("MessageWindow.Close", [] {
        clear_state();
        events::fire(events::Kind::DialogueClosed);   // resets the Dialogue dedup scope
    });
}

int install()
{
    int n = 0;
    if (hooks::attach(mSetContent, &d_SetContent, &o_SetContent)) ++n;
    if (hooks::attach(mPlayingInit, &d_PlayingInit, &o_PlayingInit)) ++n;
    if (hooks::attach(mClose, &d_Close, &o_Close)) ++n;

    queries::define("MessageWindowPatches.IsInDialogue", [] { return g_in_dialogue; });
    actions::define("MessageWindowPatches.RepeatLastDialogue", [] { repeat_last_dialogue(); });
    return n;
}

core::Install s_install{"MessageWindow", &install, 100};

} // namespace

bool in_dialogue() { return g_in_dialogue; }

// RepeatLastDialogue: re-speak the last announced page, with its speaker when known.
void repeat_last_dialogue()
{
    if (!g_in_dialogue || g_last_page < 0 || g_last_page >= static_cast<int>(g_page_starts.size())) {
        ffpr::say(T(L"Nothing to repeat"), true);
        return;
    }
    std::wstring text_ = page_text(g_last_page);
    if (text::is_blank(text_)) {
        ffpr::say(T(L"Nothing to repeat"), true);
        return;
    }
    if (!g_last_speaker.empty()) text_ = g_last_speaker + L": " + text_;
    ffpr::say(text_, true);
}

} // namespace ffpr::dialogue
