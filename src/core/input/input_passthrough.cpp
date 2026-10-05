// Ports FF1 Patches/InputPassthroughPatches.cs.
//
// Postfixes on System.Input.InputSystemManager GetKeyDown / GetKey / GetKeyUp (the
// InputActionType overloads) and GetAnyKey:
//  1. while the mod owns input (mod mode, mod menu, text input, confirmation dialog) the game
//     gets no input at all, keyboard included;
//  2. otherwise the SDL controller state is fed to the game for every button the router did
//     not consume.
// One RVA each in all five dump.cs (no folded bodies). InputActionType is identical in all five.
#include <cstdint>
#include "input_internal.h"
#include "../core.h"
#include "../hooks.h"
#include "../refs.h"

namespace input {
namespace {

ref::Class  kInputSystemManager{"System.Input", "InputSystemManager"};
ref::Method mGetKeyDown{kInputSystemManager, "GetKeyDown", 1};
ref::Method mGetKey{kInputSystemManager, "GetKey", 1};
ref::Method mGetKeyUp{kInputSystemManager, "GetKeyUp", 1};
ref::Method mGetAnyKey{kInputSystemManager, "GetAnyKey", 0};

// InputSystemManager.InputActionType
constexpr int32_t kActionEnter = 0;
constexpr int32_t kActionCancel = 1;
constexpr int32_t kActionShortcut = 2;
constexpr int32_t kActionMenu = 3;
constexpr int32_t kActionUp = 4;
constexpr int32_t kActionDown = 5;
constexpr int32_t kActionLeft = 6;
constexpr int32_t kActionRight = 7;
constexpr int32_t kActionSwitchLeft = 8;
constexpr int32_t kActionSwitchRight = 9;
constexpr int32_t kActionPageUp = 10;
constexpr int32_t kActionPageDown = 11;
constexpr int32_t kActionStart = 12;
constexpr int32_t kActionStickL = 13;
constexpr int32_t kActionStickR = 14;

using Check = bool (*)(int button);

/// SDL button behind a stick-click action, or -1 for any other action.
int stick_button_for(int32_t action)
{
    return action == kActionStickL ? kLeftStick : action == kActionStickR ? kRightStick : -1;
}

bool pass(int button, Check check)
{
    return !router::is_button_consumed(button) && check(button);
}

/// Directional passthrough depends on the context:
///  - active field: the left stick only (the D-pad and right stick belong to the mod);
///  - elsewhere: D-pad and left stick (both navigate menus).
bool check_directional(int32_t action, Check check, bool use_edge)
{
    int dpad = 0;
    bool lstick = false;
    switch (action) {
    case kActionUp:
        dpad = kDpadUp;
        lstick = use_edge ? gamepad::lstick_up_pressed() : gamepad::lstick_up_held();
        break;
    case kActionDown:
        dpad = kDpadDown;
        lstick = use_edge ? gamepad::lstick_down_pressed() : gamepad::lstick_down_held();
        break;
    case kActionLeft:
        dpad = kDpadLeft;
        lstick = use_edge ? gamepad::lstick_left_pressed() : gamepad::lstick_left_held();
        break;
    case kActionRight:
        dpad = kDpadRight;
        lstick = use_edge ? gamepad::lstick_right_pressed() : gamepad::lstick_right_held();
        break;
    default:
        return false;
    }
    if (field_active()) return lstick;
    return check(dpad) || lstick;
}

bool check_sdl(int32_t action, Check check, bool use_direction_edge)
{
    switch (action) {
    case kActionEnter: return pass(kSouth, check);
    case kActionCancel: return pass(kEast, check);
    case kActionShortcut: return pass(kWest, check);
    case kActionMenu: return pass(kNorth, check);
    case kActionUp:
    case kActionDown:
    case kActionLeft:
    case kActionRight:
        return check_directional(action, check, use_direction_edge);
    case kActionSwitchLeft: return pass(kLeftShoulder, check);
    case kActionSwitchRight: return pass(kRightShoulder, check);
    case kActionPageUp:
        // Left trigger: the mod's pathfind key on the active field, the game's elsewhere.
        if (field_active()) return false;
        return left_trigger() > 0.5f;
    case kActionPageDown:
        return right_trigger() > 0.5f;
    case kActionStart:
        return false;   // never: the mod menu owns Start
    case kActionStickL: return pass(kLeftStick, check);
    case kActionStickR: return pass(kRightStick, check);
    default:
        return false;
    }
}

// A field stick click reaches the game as a synthetic press once it resolves as a lone click
// (ControllerRouter.UpdateStickClicks); the raw button is consumed until then.
bool sdl_key_down(int32_t action)
{
    return check_sdl(action, &button_pressed, true) || router::is_stick_pulse_down(stick_button_for(action));
}

bool sdl_key_held(int32_t action)
{
    return check_sdl(action, &button_held, false) || router::is_stick_pulse_down(stick_button_for(action));
}

bool sdl_key_released(int32_t action)
{
    return check_sdl(action, &button_released, true) || router::is_stick_pulse_up(stick_button_for(action));
}

using GetKeyX_t = bool (*)(void* self, int32_t type, const void* mi);
using GetAnyKey_t = bool (*)(void* self, const void* mi);
GetKeyX_t o_GetKeyDown = nullptr;
GetKeyX_t o_GetKey = nullptr;
GetKeyX_t o_GetKeyUp = nullptr;
GetAnyKey_t o_GetAnyKey = nullptr;

/// The shared postfix body: full suppression first (it must also block keyboard-only players,
/// so it runs before the gamepad check), then keep a true keyboard result, then inject SDL.
template <typename F>
bool postfix(bool result, F&& sdl)
{
    if (suppress_game_input()) return false;
    if (!gamepad_available()) return result;
    if (result) return result;
    return sdl();
}

bool d_GetKeyDown(void* self, int32_t type, const void* mi)
{
    const bool r = o_GetKeyDown(self, type, mi);
    return hooks::guarded_r("InputPassthrough.GetKeyDown", r, [&] { return postfix(r, [&] { return sdl_key_down(type); }); });
}

bool d_GetKey(void* self, int32_t type, const void* mi)
{
    const bool r = o_GetKey(self, type, mi);
    return hooks::guarded_r("InputPassthrough.GetKey", r, [&] { return postfix(r, [&] { return sdl_key_held(type); }); });
}

bool d_GetKeyUp(void* self, int32_t type, const void* mi)
{
    const bool r = o_GetKeyUp(self, type, mi);
    return hooks::guarded_r("InputPassthrough.GetKeyUp", r, [&] { return postfix(r, [&] { return sdl_key_released(type); }); });
}

bool d_GetAnyKey(void* self, const void* mi)
{
    const bool r = o_GetAnyKey(self, mi);
    return hooks::guarded_r("InputPassthrough.GetAnyKey", r, [&] {
        return postfix(r, [] {
            // Any button held that the router did not consume.
            for (int i = 0; i < kButtonCount; ++i)
                if (button_held(i) && !router::is_button_consumed(i)) return true;
            return false;
        });
    });
}

int install()
{
    int n = 0;
    n += hooks::attach(mGetKeyDown, &d_GetKeyDown, &o_GetKeyDown) ? 1 : 0;
    n += hooks::attach(mGetKey, &d_GetKey, &o_GetKey) ? 1 : 0;
    n += hooks::attach(mGetKeyUp, &d_GetKeyUp, &o_GetKeyUp) ? 1 : 0;
    n += hooks::attach(mGetAnyKey, &d_GetAnyKey, &o_GetAnyKey) ? 1 : 0;
    LOGF("[InputPassthrough] Patched InputSystemManager (%d of 4: GetKeyDown/GetKey/GetKeyUp/GetAnyKey)", n);
    return n;
}
core::Install s_install{"InputPassthrough", &install, 60};

} // namespace
} // namespace input
