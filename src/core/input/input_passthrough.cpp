// Ports FF1 Patches/InputPassthroughPatches.cs.
//
// Prefix ownership on both native input APIs, then ordinary SDL passthrough.
// System.Input.InputSystemManager GetKeyDown / GetKey / GetKeyUp (the
// InputActionType overloads) and GetAnyKey:
//  1. while the mod owns input (mod mode, mod menu, text input, confirmation dialog) the game
//     gets no input at all, keyboard included;
//  2. otherwise the SDL controller state is fed to the game for every button the router did
//     not consume.
// Result controllers bypass the manager via InputListener/KeyConfig in all six games.
// See docs/re/RESULTS.md for caller paths and verified listener signatures.
#include <cstdint>
#include "input_internal.h"
#include "bindings.h"
#include "../core.h"
#include "../hooks.h"
#include "../refs.h"
#include "../fieldnav/autowalk.h"

namespace input {
namespace {

ref::Class  kInputSystemManager{"System.Input", "InputSystemManager"};
ref::Method mGetKeyDown{kInputSystemManager, "GetKeyDown", 1};
ref::Method mGetKey{kInputSystemManager, "GetKey", 1};
ref::Method mGetKeyUp{kInputSystemManager, "GetKeyUp", 1};
ref::Method mGetAnyKey{kInputSystemManager, "GetAnyKey", 0};
ref::Class kListener{"System.Input", "InputListener"};
ref::Method mListenerDown{kListener, "GetKeyDown", 4}, mListenerHeld{kListener, "GetKey", 3}, mListenerUp{kListener, "GetKeyUp", 3};
ref::Method mAnyHeld{kListener, "AnyKey", 1}, mAnyDown{kListener, "AnyKeyDown", 0}, mAnyUp{kListener, "AnyKeyUp", 0};
ref::Class kInputValue{"System.Input", "InputValue"};
ref::Field fKeyName{kInputValue, "Name"};
bool g_owned_buttons[kButtonCount]{};

bool native_input_blocked()
{
    if (suppress_game_input()) return true;
    if(bindings::remapped_keyboard_in_use())return true;
    if (queries::is("ReviewBuffer.IsInformation")) {
        // Arrows review the copied comparison. WASD, native confirm/cancel and
        // the description/page toggle keep their ordinary menu ownership.
        if(bindings::keyboard_in_use(Context::Information))return true;
        for (int i = 0; i < kButtonCount; ++i)
            if (g_owned_buttons[i] && (button_held(i) || button_released(i))) return true;
    }
    if (!queries::is("ReviewBuffer.IsResults")) return false;
    if(bindings::keyboard_in_use(Context::Results))return true;
    for (int i = 0; i < kButtonCount; ++i)
        if (g_owned_buttons[i] && (button_held(i) || button_released(i))) return true;
    return false;
}

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
    int stick=bindings::kLeftUp;
    if(action==kActionDown)stick=bindings::kLeftDown;
    if(action==kActionLeft)stick=bindings::kLeftLeft;
    if(action==kActionRight)stick=bindings::kLeftRight;
    return pass(dpad,check) || (lstick&&!bindings::consumed(stick));
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
        return !bindings::consumed(bindings::kLeftTrigger) && left_trigger() > 0.5f;
    case kActionPageDown:
        return !bindings::consumed(bindings::kRightTrigger) && right_trigger() > 0.5f;
    case kActionStart:
        return pass(kStart,check);
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
    if (native_input_blocked()) return false; // before native key checks or side effects
    const bool r = o_GetKeyDown(self, type, mi) || ffpr::fieldnav::autowalk::virtual_key(type);
    return hooks::guarded_r("InputPassthrough.GetKeyDown", r, [&] { return postfix(r, [&] { return sdl_key_down(type); }); });
}

bool d_GetKey(void* self, int32_t type, const void* mi)
{
    if (native_input_blocked()) return false;
    const bool r = o_GetKey(self, type, mi) || ffpr::fieldnav::autowalk::virtual_key(type);
    return hooks::guarded_r("InputPassthrough.GetKey", r, [&] { return postfix(r, [&] { return sdl_key_held(type); }); });
}

bool d_GetKeyUp(void* self, int32_t type, const void* mi)
{
    if (native_input_blocked()) return false;
    const bool r = o_GetKeyUp(self, type, mi) || ffpr::fieldnav::autowalk::virtual_key(type, true);
    return hooks::guarded_r("InputPassthrough.GetKeyUp", r, [&] { return postfix(r, [&] { return sdl_key_released(type); }); });
}

bool any_sdl(Check check)
{
    for (int i = 0; i < kButtonCount; ++i)
        if (pass(i, check)) return true;
    return false;
}
bool d_GetAnyKey(void* self, const void* mi)
{
    if (native_input_blocked()) return false;
    const bool r = o_GetAnyKey(self, mi);
    return hooks::guarded_r("InputPassthrough.GetAnyKey", r, [&] {
        return postfix(r, [] { return any_sdl(&button_held); });
    });
}

// InputListener reads KeyConfig directly. AnyKeyDown/Up inline GetKey checks,
// so guard all six entry points rather than relying on the manager or one wrapper.
using ListenerDown = bool (*)(void*, void*, int32_t, bool, bool, const void*);
using ListenerKey = bool (*)(void*, void*, int32_t, bool, const void*);
using ListenerAny = bool (*)(void*, int32_t, const void*);
ListenerDown o_listener_down;
ListenerKey o_listener_held, o_listener_up;
ListenerAny o_any_held;
GetAnyKey_t o_any_down, o_any_up;

int listener_action(void* key)
{
    const auto name = ref::rd_str(key, fKeyName);
    for (auto [label, action] : {std::pair{L"Action", kActionEnter}, {L"Cancel", kActionCancel},
            {L"Menu", kActionMenu}, {L"Shortcut", kActionShortcut},
            {L"Up", kActionUp}, {L"Down", kActionDown}, {L"Left", kActionLeft}, {L"Right", kActionRight},
            {L"SwitchLeft", kActionSwitchLeft}, {L"SwitchRight", kActionSwitchRight},
            {L"PageUp", kActionPageUp}, {L"PageDown", kActionPageDown}, {L"Start", kActionStart},
            {L"StickL", kActionStickL}, {L"StickR", kActionStickR}})
        if (name == label) return action;
    return -1;
}
template <typename F>
bool result_listener_passthrough(bool native, int32_t device, F&& read_sdl)
{
    // Preserve other native screens. Results need SDL too because this caller
    // bypasses the manager, and Unity gamepad devices are disabled by the mod.
    if (native || (!queries::is("ReviewBuffer.IsResults") && !queries::is("ReviewBuffer.IsInformation")) ||
        (device != 1 && device != 3)) return native;
    return gamepad_available() && read_sdl();
}
bool d_listener_down(void* self, void* key, int32_t device, bool mouse, bool ignore, const void* mi)
{
    if (native_input_blocked()) return false;
    const bool r = o_listener_down(self, key, device, mouse, ignore, mi) ||
        (ffpr::fieldnav::autowalk::virtual_input_active() && ffpr::fieldnav::autowalk::virtual_key(listener_action(key)));
    return result_listener_passthrough(r, device, [&] { return sdl_key_down(listener_action(key)); });
}
bool d_listener_held(void* self, void* key, int32_t device, bool ignore, const void* mi)
{
    if (native_input_blocked()) return false;
    const bool r = o_listener_held(self, key, device, ignore, mi) ||
        (ffpr::fieldnav::autowalk::virtual_input_active() && ffpr::fieldnav::autowalk::virtual_key(listener_action(key)));
    // BattleMenuController.IsEscapeInput reads held SwitchLeft/Right directly through
    // InputListener, bypassing the manager. Unity pads are disabled while SDL owns them.
    // Forward only these two native actions in battle; mod mode/modal ownership above
    // still consumes them, and the game retains escape eligibility and timing.
    if (!r && (device == 1 || device == 3) && gamepad_available()
        && queries::is("BattleStateHelper.IsInBattle")) {
        const int action = listener_action(key);
        if (action == kActionSwitchLeft || action == kActionSwitchRight)
            return sdl_key_held(action);
    }
    return result_listener_passthrough(r, device, [&] { return sdl_key_held(listener_action(key)); });
}
bool d_listener_up(void* self, void* key, int32_t device, bool ignore, const void* mi)
{
    if (native_input_blocked()) return false;
    const bool r = o_listener_up(self, key, device, ignore, mi) ||
        (ffpr::fieldnav::autowalk::virtual_input_active() && ffpr::fieldnav::autowalk::virtual_key(listener_action(key), true));
    return result_listener_passthrough(r, device, [&] { return sdl_key_released(listener_action(key)); });
}
bool d_any_held(void* self, int32_t device, const void* mi)
{
    if (native_input_blocked()) return false;
    const bool r = o_any_held(self, device, mi);
    return result_listener_passthrough(r, device, [] { return any_sdl(&button_held); });
}
bool d_any_down(void* self, const void* mi)
{
    if (native_input_blocked()) return false;
    const bool r = o_any_down(self, mi);
    return result_listener_passthrough(r, 3, [] { return any_sdl(&button_pressed); });
}
bool d_any_up(void* self, const void* mi)
{
    if (native_input_blocked()) return false;
    const bool r = o_any_up(self, mi);
    return result_listener_passthrough(r, 3, [] { return any_sdl(&button_released); });
}

int install()
{
    int n = 0;
    n += hooks::attach(mGetKeyDown, &d_GetKeyDown, &o_GetKeyDown) ? 1 : 0;
    n += hooks::attach(mGetKey, &d_GetKey, &o_GetKey) ? 1 : 0;
    n += hooks::attach(mGetKeyUp, &d_GetKeyUp, &o_GetKeyUp) ? 1 : 0;
    n += hooks::attach(mGetAnyKey, &d_GetAnyKey, &o_GetAnyKey) ? 1 : 0;
    n += hooks::attach(mListenerDown, &d_listener_down, &o_listener_down) ? 1 : 0;
    n += hooks::attach(mListenerHeld, &d_listener_held, &o_listener_held) ? 1 : 0;
    n += hooks::attach(mListenerUp, &d_listener_up, &o_listener_up) ? 1 : 0;
    n += hooks::attach(mAnyHeld, &d_any_held, &o_any_held) ? 1 : 0;
    n += hooks::attach(mAnyDown, &d_any_down, &o_any_down) ? 1 : 0;
    n += hooks::attach(mAnyUp, &d_any_up, &o_any_up) ? 1 : 0;
    LOGF("[InputPassthrough] Patched manager/listener input ownership (%d of 10)", n);
    return n;
}
core::Install s_install{"InputPassthrough", &install, 60};

} // namespace
void update_native_input_ownership()
{
    const bool owned = suppress_game_input();
    for (int i = 0; i < kButtonCount; ++i)
        g_owned_buttons[i] = (owned || g_owned_buttons[i]) && (button_held(i) || button_released(i));
}
} // namespace input
