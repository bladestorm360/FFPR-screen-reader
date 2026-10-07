// Ports FF6 Patches/OperaPatches.cs (OperaController_Update_Patch): during the opera house's
// button prompts, "Success" when the player presses in time, "Failed" when the prompt passes,
// "Out of time" when the interactive timer runs out.
//
// The C# polled three OperaController flags in a postfix on OperaController.Update (every
// frame). In FF6's GameAssembly.dll that Update only services the limit timer; the flags are
// written elsewhere (offline disassembly, 2026-10-07), and this file hooks those writers:
//   clickedInteractiveIconButton = true   only in OnClickInteractiveIcon() (the touch button's
//                                          callback, and called directly by the opera event
//                                          coroutine on a key press);
//   isFailedInteractiveIconButton = true  only in the PlayOperaEvent coroutine's MoveNext;
//   isInteractiveTimeup = true            only in the StartOperaInteractiveTimer coroutine's
//                                          MoveNext (it runs twice per prompt).
// Each detour compares the flag before and after the original, which is the C#'s
// "changed from false to true" test at the moment it happens. The coroutine steps run only
// while the opera event plays. All three bodies have one RVA each in FF6's dump.cs.
// As in the C#, nothing is spoken unless OperaController.isPlaying.
#include <cstdint>
#include "../../core/core.h"
#include "../../core/game.h"
#include "../../core/hooks.h"
#include "../../core/il2cpp.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"

namespace ffpr::ff6 {
namespace {

constexpr uint8_t k6 = game::kFF6;

ref::Class  kOperaController{"Last.FF6.Opera", "OperaController", k6};
ref::Field  fIsPlaying{kOperaController, "isPlaying"};
ref::Field  fClicked{kOperaController, "clickedInteractiveIconButton"};
ref::Field  fFailed{kOperaController, "isFailedInteractiveIconButton"};
ref::Field  fTimeup{kOperaController, "isInteractiveTimeup"};
ref::Method mOnClickInteractiveIcon{kOperaController, "OnClickInteractiveIcon", 0};

// <>4__this of each coroutine state machine (resolved at install).
int32_t g_event_this = -1;
int32_t g_timer_this = -1;

bool flag(void* opera, ref::Field& f) { return opera && ref::rd<bool>(opera, f); }
bool playing(void* opera) { return flag(opera, fIsPlaying); }

using OnClick_t = void (*)(void* self, const void* mi);
OnClick_t o_OnClickInteractiveIcon = nullptr;

void d_OnClickInteractiveIcon(void* self, const void* mi)
{
    bool before = hooks::guarded_r("FF6.Opera.OnClickInteractiveIcon.prefix", true, [&] { return flag(self, fClicked); });
    o_OnClickInteractiveIcon(self, mi);
    hooks::guarded("FF6.Opera.OnClickInteractiveIcon", [&] {
        if (!before && flag(self, fClicked) && playing(self)) ffpr::say(T(L"Success"));
    });
}

using MoveNext_t = bool (*)(void* self, const void* mi);
MoveNext_t o_EventMoveNext = nullptr;
MoveNext_t o_TimerMoveNext = nullptr;

void* opera_of(void* state_machine, int32_t this_offset)
{
    return il2cpp::read_field<void*>(state_machine, this_offset);
}

bool d_EventMoveNext(void* self, const void* mi)
{
    void* opera = opera_of(self, g_event_this);
    bool before = hooks::guarded_r("FF6.Opera.PlayOperaEvent.prefix", true, [&] { return flag(opera, fFailed); });
    bool result = o_EventMoveNext(self, mi);
    hooks::guarded("FF6.Opera.PlayOperaEvent", [&] {
        if (!before && flag(opera, fFailed) && playing(opera)) ffpr::say(T(L"Failed"));
    });
    return result;
}

bool d_TimerMoveNext(void* self, const void* mi)
{
    void* opera = opera_of(self, g_timer_this);
    bool before = hooks::guarded_r("FF6.Opera.StartOperaInteractiveTimer.prefix", true, [&] { return flag(opera, fTimeup); });
    bool result = o_TimerMoveNext(self, mi);
    hooks::guarded("FF6.Opera.StartOperaInteractiveTimer", [&] {
        if (!before && flag(opera, fTimeup) && playing(opera)) ffpr::say(T(L"Out of time"));
    });
    return result;
}

/// A coroutine's MoveNext and its <>4__this offset, by the coroutine's name.
void* coroutine_move_next(const char* prefix, int32_t& this_offset)
{
    il2cpp::Class* outer = kOperaController.get();
    il2cpp::Class* sm = outer ? il2cpp::find_nested_prefix(outer, prefix) : nullptr;
    if (!sm) {
        LOGF("[Opera] coroutine %s not found", prefix);
        return nullptr;
    }
    this_offset = il2cpp::field_offset(sm, "<>4__this");
    il2cpp::Method* m = il2cpp::find_method(sm, "MoveNext", 0);
    if (!m || this_offset < 0) {
        LOGF("[Opera] coroutine %s: MoveNext or <>4__this missing", prefix);
        return nullptr;
    }
    return il2cpp::method_pointer(m);
}

int install()
{
    int n = 0;
    if (hooks::attach(mOnClickInteractiveIcon, &d_OnClickInteractiveIcon, &o_OnClickInteractiveIcon)) ++n;
    if (void* p = coroutine_move_next("<PlayOperaEvent>d__", g_event_this)) {
        if (hooks::attach_addr(p, "OperaController.PlayOperaEvent.MoveNext", reinterpret_cast<void*>(&d_EventMoveNext),
                               reinterpret_cast<void**>(&o_EventMoveNext))) ++n;
    }
    if (void* p = coroutine_move_next("<StartOperaInteractiveTimer>d__", g_timer_this)) {
        if (hooks::attach_addr(p, "OperaController.StartOperaInteractiveTimer.MoveNext",
                               reinterpret_cast<void*>(&d_TimerMoveNext), reinterpret_cast<void**>(&o_TimerMoveNext))) ++n;
    }
    if (n < 3) LOGF("[Opera] %d of 3 opera hooks installed", n);
    return n;
}

core::Install s_install{"FF6.Opera", &install, 510, game::kFF6};

} // namespace
} // namespace ffpr::ff6
