// Ports FF1 Core/ControllerLabels.cs.
//
// Button names for the connected controller type. SDL names buttons by position (SOUTH is the
// bottom face button), so the label follows the markings of each controller family.
#include <SDL3/SDL_gamepad.h>
#include "input_internal.h"
#include "../core.h"
#include "../modtext.h"

namespace input {
namespace {

enum class Family { Xbox360, XboxOne, PS3, PS4, PS5, Nintendo, Generic };

Family family()
{
    switch (gamepad_type()) {
    case SDL_GAMEPAD_TYPE_XBOX360: return Family::Xbox360;
    case SDL_GAMEPAD_TYPE_XBOXONE: return Family::XboxOne;
    case SDL_GAMEPAD_TYPE_PS3: return Family::PS3;
    case SDL_GAMEPAD_TYPE_PS4: return Family::PS4;
    case SDL_GAMEPAD_TYPE_PS5: return Family::PS5;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        return Family::Nintendo;
    default:
        return Family::XboxOne;   // the most common modern controller
    }
}

bool is_ps(Family f) { return f == Family::PS3 || f == Family::PS4 || f == Family::PS5; }

// Text queries for other areas (FF1 ConfigMenuPatches reads the live bound button's label).
struct ButtonQuery { const char* name; int button; };
constexpr ButtonQuery kButtonQueries[] = {
    {"ControllerLabels.GetButtonLabel.South", kSouth},
    {"ControllerLabels.GetButtonLabel.East", kEast},
    {"ControllerLabels.GetButtonLabel.West", kWest},
    {"ControllerLabels.GetButtonLabel.North", kNorth},
    {"ControllerLabels.GetButtonLabel.Back", kBack},
    {"ControllerLabels.GetButtonLabel.Guide", kGuide},
    {"ControllerLabels.GetButtonLabel.Start", kStart},
    {"ControllerLabels.GetButtonLabel.LeftStick", kLeftStick},
    {"ControllerLabels.GetButtonLabel.RightStick", kRightStick},
    {"ControllerLabels.GetButtonLabel.LeftShoulder", kLeftShoulder},
    {"ControllerLabels.GetButtonLabel.RightShoulder", kRightShoulder},
    {"ControllerLabels.GetButtonLabel.DpadUp", kDpadUp},
    {"ControllerLabels.GetButtonLabel.DpadDown", kDpadDown},
    {"ControllerLabels.GetButtonLabel.DpadLeft", kDpadLeft},
    {"ControllerLabels.GetButtonLabel.DpadRight", kDpadRight},
};

int install()
{
    for (const ButtonQuery& q : kButtonQueries) {
        const int b = q.button;
        queries::define_text(q.name, [b] { return button_label(b); });
    }
    queries::define_text("ControllerLabels.GetLeftTriggerLabel", [] { return left_trigger_label(); });
    queries::define_text("ControllerLabels.GetRightTriggerLabel", [] { return right_trigger_label(); });
    queries::define_text("ControllerLabels.GetLeftStickClickLabel", [] { return left_stick_click_label(); });
    queries::define_text("ControllerLabels.GetRightStickClickLabel", [] { return right_stick_click_label(); });
    return 0;
}
core::Install s_install{"ControllerLabels", &install, 60};

} // namespace

std::wstring button_label(int button)
{
    const Family f = family();
    switch (button) {
    case kSouth:
        if (is_ps(f)) return T(L"Cross");
        return f == Family::Nintendo ? L"B" : L"A";
    case kEast:
        if (is_ps(f)) return T(L"Circle");
        return f == Family::Nintendo ? L"A" : L"B";
    case kWest:
        if (is_ps(f)) return T(L"Square");
        return f == Family::Nintendo ? L"Y" : L"X";
    case kNorth:
        if (is_ps(f)) return T(L"Triangle");
        return f == Family::Nintendo ? L"X" : L"Y";
    case kBack:
        switch (f) {
        case Family::Xbox360: return L"Back";
        case Family::XboxOne: return T(L"View");
        case Family::PS3: return L"Select";
        case Family::PS4: return L"Share";
        case Family::PS5: return L"Create";
        case Family::Nintendo: return T(L"Minus");
        default: return T(L"View");
        }
    case kStart:
        switch (f) {
        case Family::Xbox360: return L"Start";
        case Family::XboxOne: return T(L"Menu");
        case Family::PS3: return L"Start";
        case Family::PS4:
        case Family::PS5: return L"Options";
        case Family::Nintendo: return T(L"Plus");
        default: return T(L"Menu");
        }
    case kLeftShoulder:
        if (is_ps(f)) return L"L1";
        return f == Family::Nintendo ? L"L" : L"LB";
    case kRightShoulder:
        if (is_ps(f)) return L"R1";
        return f == Family::Nintendo ? L"R" : L"RB";
    case kLeftStick:
        return is_ps(f) ? L"L3" : L"LS";
    case kRightStick:
        return is_ps(f) ? L"R3" : L"RS";
    case kDpadUp: return T(L"D-pad Up");
    case kDpadDown: return T(L"D-pad Down");
    case kDpadLeft: return T(L"D-pad Left");
    case kDpadRight: return T(L"D-pad Right");
    case kGuide:
        switch (f) {
        case Family::Xbox360: return L"Guide";
        case Family::XboxOne: return L"Xbox";
        case Family::PS3:
        case Family::PS4:
        case Family::PS5: return L"PS";
        case Family::Nintendo: return T(L"Home");
        default: return L"Xbox";
        }
    default:
        return TF(L"Button {0}", {std::to_wstring(button)});
    }
}

std::wstring left_trigger_label()
{
    const Family f = family();
    if (is_ps(f)) return L"L2";
    return f == Family::Nintendo ? L"ZL" : L"LT";
}

std::wstring right_trigger_label()
{
    const Family f = family();
    if (is_ps(f)) return L"R2";
    return f == Family::Nintendo ? L"ZR" : L"RT";
}

// "L3"/"R3" reads as a click for PlayStation/Xbox/generic; Nintendo has no L3/R3 convention.
std::wstring left_stick_click_label()
{
    return family() == Family::Nintendo ? T(L"L Stick Click") : L"L3";
}

std::wstring right_stick_click_label()
{
    return family() == Family::Nintendo ? T(L"R Stick Click") : L"R3";
}

std::wstring controller_type_name(int type)
{
    switch (type) {
    case SDL_GAMEPAD_TYPE_XBOX360: return L"Xbox 360";
    case SDL_GAMEPAD_TYPE_XBOXONE: return L"Xbox One";
    case SDL_GAMEPAD_TYPE_PS3: return L"PlayStation 3";
    case SDL_GAMEPAD_TYPE_PS4: return L"PlayStation 4";
    case SDL_GAMEPAD_TYPE_PS5: return L"PlayStation 5";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO: return L"Switch Pro";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT: return L"Joy-Con Left";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT: return L"Joy-Con Right";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return L"Joy-Con Pair";
    case SDL_GAMEPAD_TYPE_STANDARD: return L"Standard";
    default: return L"Unknown";
    }
}

} // namespace input
