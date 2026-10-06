// Ports FF4 Patches/TimerPatches.cs (Timer_Update_Patch and TimerHelper): T reads the on-screen
// countdowns (escape timers), Shift+T freezes and resumes them.
//
// Timer.Update is the game's own per-frame call; the detour only skips it while the freeze is on
// (OPEN_ISSUES.md "Timers and per-frame work deliberately kept": the game's timing, as in FF5).
#include <string>
#include "ff4.h"
#include "../../core/core.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/unity.h"
#include "../../core/input/input.h"

namespace ffpr::ff4 {
namespace {

ref::Class  kTimer{"Last.Timer", "Timer", game::kFF4};
ref::Method mTimerUpdate{kTimer, "Update", 1};

ref::Class  kScreenTimer{"Last.UI", "ScreenTimerController", game::kFF4};
ref::Field  fScreenTimerView{kScreenTimer, "view"};
ref::Class  kScreenTimerView{"Last.UI", "ScreenTimerView", game::kFF4};
ref::Field  fMinuteText{kScreenTimerView, "playTimeMinuteText"};
ref::Field  fSecondText{kScreenTimerView, "playTimeSecondText"};
ref::Field  fCanvasGroup{kScreenTimerView, "canvasGroup"};
ref::Class  kCanvasGroup{"UnityEngine", "CanvasGroup", game::kFF4};
ref::Method mGetAlpha{kCanvasGroup, "get_alpha", 0};

ref::Class  kFieldTimer{"Last.UI.Common.Map", "FieldGrobalTimer", game::kFF4};
ref::Field  fKeyText{kFieldTimer, "keyText"};
ref::Field  fTouchText{kFieldTimer, "touchText"};

bool g_timers_frozen = false;

using Update_t = bool (*)(void* self, float delta_time, const void* mi);
Update_t o_TimerUpdate;
bool d_TimerUpdate(void* self, float delta_time, const void* mi)
{
    // Prefix returning false: the original is skipped, __result stays false.
    if (g_timers_frozen) return false;
    return o_TimerUpdate(self, delta_time, mi);
}

std::wstring text_of(void* text_component)
{
    return text_component ? text::trim(unity::ui_text(text_component)) : std::wstring();
}

bool parse_int(const std::wstring& s, int& out)
{
    if (s.empty()) return false;
    try {
        size_t used = 0;
        out = std::stoi(s, &used);
        return used == s.size();
    } catch (...) {
        return false;
    }
}

/// TimerHelper.FormatTimeString
std::wstring format_time(const std::wstring& minutes, const std::wstring& seconds)
{
    std::wstring result;
    int min = 0;
    if (parse_int(minutes, min) && min > 0)
        result = text::num(min) + L" " + (min == 1 ? T(L"minute") : T(L"minutes"));
    int sec = 0;
    if (parse_int(seconds, sec)) {
        if (!result.empty()) result += L" ";
        result += text::num(sec) + L" " + (sec == 1 ? T(L"second") : T(L"seconds"));
    }
    if (result.empty()) {
        result = minutes;
        if (!minutes.empty() && !seconds.empty()) result += L":";
        result += seconds;
    }
    return result;
}

/// TimerHelper.FormatFieldTimerString ("5:30" style).
std::wstring format_field_timer(const std::wstring& timer_text)
{
    size_t colon = timer_text.find(L':');
    if (colon != std::wstring::npos && timer_text.find(L':', colon + 1) == std::wstring::npos)
        return format_time(timer_text.substr(0, colon), timer_text.substr(colon + 1));
    return TF(L"Timer: {0}", {timer_text});
}

/// TimerHelper.AnnounceActiveTimers
void announce_active_timers()
{
    std::wstring announcement;
    int count = 0;

    for (void* timer : unity::find_objects(kScreenTimer)) {
        void* view = ref::rd<void*>(timer, fScreenTimerView);
        if (!view) continue;
        void* group = ref::rd<void*>(view, fCanvasGroup);
        if (!group || !(ref::call<float>(mGetAlpha, group) > 0.0f)) continue;
        std::wstring minutes = text_of(ref::rd<void*>(view, fMinuteText));
        std::wstring seconds = text_of(ref::rd<void*>(view, fSecondText));
        if (minutes.empty() && seconds.empty()) continue;
        if (count > 0) announcement += L". ";
        announcement += format_time(minutes, seconds);
        ++count;
    }

    for (void* timer : unity::find_objects(kFieldTimer)) {
        if (!timer || !unity::active_in_hierarchy(timer)) continue;
        std::wstring t = text_of(ref::rd<void*>(timer, fKeyText));
        if (t.empty()) t = text_of(ref::rd<void*>(timer, fTouchText));
        if (t.empty()) continue;
        if (count > 0) announcement += L". ";
        announcement += format_field_timer(t);
        ++count;
    }

    ffpr::say(count > 0 ? announcement : T(L"No active timers"), true);
}

/// TimerHelper.ToggleTimerFreeze
void toggle_timer_freeze()
{
    g_timers_frozen = !g_timers_frozen;
    ffpr::say(g_timers_frozen ? T(L"Timers frozen") : T(L"Timers resumed"), true);
}

int install()
{
    int n = hooks::attach(mTimerUpdate, &d_TimerUpdate, &o_TimerUpdate) ? 1 : 0;
    actions::define("TimerHelper.AnnounceActiveTimers", [] {
        hooks::guarded("TimerHelper.AnnounceActiveTimers", [] { announce_active_timers(); });
    });
    actions::define("TimerHelper.ToggleTimerFreeze", [] { toggle_timer_freeze(); });
    queries::define("TimerHelper.TimersFrozen", [] { return g_timers_frozen; });
    input::add_binding(input::Key::T, input::Mod::None, input::Context::Global, "TimerHelper.AnnounceActiveTimers",
                       "Announce active timers");
    input::add_binding(input::Key::T, input::Mod::Shift, input::Context::Global, "TimerHelper.ToggleTimerFreeze",
                       "Toggle timer freeze");
    return n;
}

core::Install s_install{"FF4.Timers", &install, 530, game::kFF4};

} // namespace
} // namespace ffpr::ff4
