// Ports FF5 Patches/TimerPatches.cs (Timer_Update_Patch, TimerHelper) and its two key bindings
// from Core/InputManager.cs (T: announce timers, Shift+F5: freeze/resume), gated on
// game::flags().timer_freeze.
//
// Timer.Update is detoured ONLY while the freeze is on, as in the C#, which patched and unpatched
// it at runtime because a permanent trampoline disturbed IL2CPP event processing (the Pyramid 5F
// serpent event loop). The hook registry queues detours for one boot-time freeze and has no
// runtime enable/disable, so this file uses MinHook directly (src/core/minhook_dyn.h): one
// mh::create on the first freeze, then mh::enable / mh::disable on each toggle. Last.Timer.Timer
// .Update has its own RVA in FF5 (0x49EED0, one method) and no core file hooks it.
// This is the one exception in the module to "hooks go through hooks::attach".
#include <cstdint>
#include <string>
#include "ff5_internal.h"
#include "../../core/core.h"
#include "../../core/game.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/minhook_dyn.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/unity.h"
#include "../../core/field/object_cache.h"
#include "../../core/input/input.h"

namespace ffpr::ff5 {
namespace {

constexpr uint8_t k5 = game::kFF5;

ref::Class  kTimer{"Last.Timer", "Timer", k5};
ref::Method mTimerUpdate{kTimer, "Update", 1};

ref::Class  kScreenTimer{"Last.UI", "ScreenTimerController", k5};
ref::Field  fScreenTimerView{kScreenTimer, "view"};
ref::Class  kScreenTimerView{"Last.UI", "ScreenTimerView", k5};
ref::Field  fPlayTimeMinuteText{kScreenTimerView, "playTimeMinuteText"};
ref::Field  fPlayTimeSecondText{kScreenTimerView, "playTimeSecondText"};
ref::Field  fCanvasGroup{kScreenTimerView, "canvasGroup"};
ref::Class  kCanvasGroup{"UnityEngine", "CanvasGroup", k5};
ref::Method mCanvasGroupAlpha{kCanvasGroup, "get_alpha", 0};

ref::Class  kFieldTimer{"Last.UI.Common.Map", "FieldGrobalTimer", k5};
ref::Field  fFieldTimerKeyText{kFieldTimer, "keyText"};
ref::Field  fFieldTimerTouchText{kFieldTimer, "touchText"};

bool g_frozen = false;
bool g_hook_created = false;
void* g_update_target = nullptr;

using Update_t = bool (*)(void* self, float dt, const void* mi);
Update_t o_Update = nullptr;

/// Timer_Update_Patch.Prefix: while frozen, skip the update (only installed while frozen).
bool d_Update(void* self, float dt, const void* mi)
{
    if (g_frozen) return false;
    return o_Update ? o_Update(self, dt, mi) : false;
}

bool apply_freeze(bool frozen)
{
    if (!mh::available()) return false;
    if (!g_hook_created) {
        g_update_target = mTimerUpdate.ptr();
        if (!g_update_target) return false;
        if (mh::create(g_update_target, reinterpret_cast<void*>(&d_Update), reinterpret_cast<void**>(&o_Update)) != MH_OK)
            return false;
        g_hook_created = true;
    }
    return (frozen ? mh::enable(g_update_target) : mh::disable(g_update_target)) == MH_OK;
}

/// TimerHelper.ToggleTimerFreeze.
void toggle_timer_freeze()
{
    g_frozen = !g_frozen;
    bool ok = hooks::guarded_r("FF5.TimerHelper.ToggleTimerFreeze", false, [] { return apply_freeze(g_frozen); });
    if (!ok) {
        LOGF("FF5 timers: Timer.Update freeze hook could not be %s", g_frozen ? "enabled" : "disabled");
        if (g_frozen) g_frozen = false;   // nothing is frozen
        ffpr::say(T(L"Error toggling timer freeze"), true);
        return;
    }
    ffpr::say(g_frozen ? T(L"Timers frozen") : T(L"Timers resumed"), true);
}

/// int.TryParse (invariant digits, optional sign and surrounding whitespace).
bool try_parse_int(const std::wstring& s, int& out)
{
    std::wstring t = text::trim(s);
    if (t.empty()) return false;
    size_t i = 0;
    bool neg = false;
    if (t[0] == L'+' || t[0] == L'-') {
        neg = t[0] == L'-';
        i = 1;
    }
    if (i >= t.size()) return false;
    long long v = 0;
    for (; i < t.size(); ++i) {
        if (t[i] < L'0' || t[i] > L'9') return false;
        v = v * 10 + (t[i] - L'0');
        if (v > 2147483648LL) return false;
    }
    if (neg) v = -v;
    if (v > 2147483647LL || v < -2147483648LL) return false;
    out = static_cast<int>(v);
    return true;
}

/// FormatTimeString.
std::wstring format_time(const std::wstring& minutes, const std::wstring& seconds)
{
    std::wstring result;
    int min = 0, sec = 0;
    if (!minutes.empty() && try_parse_int(minutes, min) && min > 0) {
        result += text::num(min);
        result += min == 1 ? L" " + T(L"minute") : L" " + T(L"minutes");
    }
    if (!seconds.empty() && try_parse_int(seconds, sec)) {
        if (!result.empty()) result += L" ";
        result += text::num(sec);
        result += sec == 1 ? L" " + T(L"second") : L" " + T(L"seconds");
    }
    if (result.empty()) {
        if (!minutes.empty()) result += minutes;
        if (!minutes.empty() && !seconds.empty()) result += L":";
        if (!seconds.empty()) result += seconds;
    }
    return result;
}

/// FormatFieldTimerString.
std::wstring format_field_timer(const std::wstring& s)
{
    size_t colon = s.find(L':');
    if (colon != std::wstring::npos && s.find(L':', colon + 1) == std::wstring::npos)
        return format_time(s.substr(0, colon), s.substr(colon + 1));
    return TF(L"Timer: {0}", {s});
}

/// TimerHelper.AnnounceActiveTimers.
void announce_active_timers()
{
    std::wstring announcement;
    int count = 0;
    for (void* timer : field::cache::get_all(kScreenTimer)) {
        void* view = timer ? ref::rd<void*>(timer, fScreenTimerView) : nullptr;
        if (!view) continue;
        void* group = ref::rd<void*>(view, fCanvasGroup);
        if (!group || !(ref::call<float>(mCanvasGroupAlpha, group) > 0.0f)) continue;
        std::wstring minutes = text_of(ref::rd<void*>(view, fPlayTimeMinuteText));
        std::wstring seconds = text_of(ref::rd<void*>(view, fPlayTimeSecondText));
        if (minutes.empty() && seconds.empty()) continue;
        if (count > 0) announcement += L". ";
        announcement += format_time(minutes, seconds);
        ++count;
    }
    for (void* timer : field::cache::get_all(kFieldTimer)) {
        if (!timer || !unity::active_in_hierarchy(timer)) continue;
        std::wstring s = text_of(ref::rd<void*>(timer, fFieldTimerKeyText));
        if (s.empty()) s = text_of(ref::rd<void*>(timer, fFieldTimerTouchText));
        if (s.empty()) continue;
        if (count > 0) announcement += L". ";
        announcement += format_field_timer(s);
        ++count;
    }
    ffpr::say(count > 0 ? announcement : T(L"No active timers"), true);
}

int install()
{
    if (!game::flags().timer_freeze) return 0;
    actions::define("TimerHelper.ToggleTimerFreeze", [] { toggle_timer_freeze(); });
    actions::define("TimerHelper.AnnounceActiveTimers", [] {
        bool ok = hooks::guarded_r("FF5.TimerHelper.AnnounceActiveTimers", false, [] {
            announce_active_timers();
            return true;
        });
        if (!ok) ffpr::say(T(L"Error reading timers"), true);
    });
    queries::define("TimerHelper.TimersFrozen", [] { return g_frozen; });
    input::add_binding(input::Key::F5, input::Mod::Shift, input::Context::Global, "TimerHelper.ToggleTimerFreeze",
                       "Toggle timer freeze");
    input::add_binding(input::Key::T, input::Mod::None, input::Context::Global, "TimerHelper.AnnounceActiveTimers",
                       "Announce active timers");
    return 0;
}

core::Install s_install{"FF5.Timers", &install, 505, game::kFF5};

} // namespace
} // namespace ffpr::ff5
