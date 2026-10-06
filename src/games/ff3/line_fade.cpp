// Ports FF3 Patches/LineFadeMessagePatches.cs (LineFadeMessageTracker) and the two line-fade
// postfixes of FF3 Patches/ScrollMessagePatches.cs (LineFadeController_SetData_Postfix,
// LineFadeController_PlayInit_Postfix).
//
// FF3 tells its story text through Last.UI.Message.LineFadeMessageWindowController, one line
// fading in after another. FF3 spoke each line as the game shows it: SetData stores the lines,
// each PlayInit (once per line) speaks the next one, queued. FF1 (and the core) speak all lines
// at once from SetData. Two readers must not speak the same lines, so module.cpp switches off
// the core's "LineFadeMessageWindowController.SetData" hook for FF3 and this file replaces it
// (attached by address, since a disabled name cannot be attached).
//
// SetData also cleared the dialogue's last announced speaker (DialogueTracker
// .ClearLastAnnouncedSpeaker), so the next dialogue page re-announces its speaker: here that is a
// reset of the core's "Dialogue.Speaker" dedup context.
// SetData (0x4EAD60) and PlayInit (0x4EA920) are unique in FF3's dump.cs.
#include <cstdint>
#include <string>
#include <vector>
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/unity.h"

namespace ffpr::ff3 {
namespace {

ref::Class kLineFade{"Last.UI.Message", "LineFadeMessageWindowController", game::kFF3};
ref::Method mSetData{kLineFade, "SetData", 5};
ref::Method mPlayInit{kLineFade, "PlayInit", 0};

std::vector<std::wstring> g_lines;   // storedMessages
size_t g_next = 0;                   // currentLineIndex

void set_messages(void* messages)
{
    g_lines.clear();
    g_next = 0;
    const int count = unity::list_count(messages);
    for (int i = 0; i < count; ++i) g_lines.push_back(ref::str(unity::list_at(messages, i)));
}

void announce_next_line()
{
    if (g_next >= g_lines.size()) return;
    const std::wstring& line = g_lines[g_next];
    if (!text::is_blank(line)) ffpr::say(text::trim(line), false);
    ++g_next;
}

// SetData(List<string> messages, Color32 color, float fadeInTime, float fadeOutTime, float waitTime)
using SetData_t = void (*)(void* self, void* messages, uint32_t color, float fade_in, float fade_out, float wait,
                           const void* mi);
using Void_t = void (*)(void* self, const void* mi);
SetData_t o_SetData;
Void_t o_PlayInit;

void d_SetData(void* self, void* messages, uint32_t color, float fade_in, float fade_out, float wait, const void* mi)
{
    o_SetData(self, messages, color, fade_in, fade_out, wait, mi);
    hooks::guarded("FF3.LineFade.SetData", [&] {
        set_messages(messages);
        dedup::reset("Dialogue.Speaker");
    });
}

void d_PlayInit(void* self, const void* mi)
{
    o_PlayInit(self, mi);
    hooks::guarded("FF3.LineFade.PlayInit", [&] { announce_next_line(); });
}

int install()
{
    int n = 0;
    // The core's hook on this method is disabled for FF3 (module.cpp): attach by address.
    if (kLineFade.applies() && hooks::attach_addr(mSetData.ptr(), "FF3.LineFadeMessageWindowController.SetData",
                                                  reinterpret_cast<void*>(&d_SetData),
                                                  reinterpret_cast<void**>(&o_SetData)))
        ++n;
    n += hooks::attach(mPlayInit, &d_PlayInit, &o_PlayInit) ? 1 : 0;
    if (n != 2) LOGF("[FF3 LineFade] only %d of 2 hooks - story text lines may be silent", n);
    return n;
}

core::Install s_install{"FF3.LineFade", &install, 503, game::kFF3};

} // namespace
} // namespace ffpr::ff3
