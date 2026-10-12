#pragma once
#include "input.h"

// Shared between the input area's own files only. Other areas use input.h.
namespace input {
// Called after controller routing; retains consumed gestures until release.
void update_native_input_ownership();

// SDL3 button indices (SDL3.cs constants; identical to the SDL3 header's enum values).
constexpr int kSouth = 0, kEast = 1, kWest = 2, kNorth = 3, kBack = 4, kGuide = 5, kStart = 6,
              kLeftStick = 7, kRightStick = 8, kLeftShoulder = 9, kRightShoulder = 10,
              kDpadUp = 11, kDpadDown = 12, kDpadLeft = 13, kDpadRight = 14;

namespace gamepad {
/// First frame: signals the prepared SDL owner without waiting or loading inline.
void initialize();
/// Keyboard poll, copied physical snapshot, native-owner Unity gamepad guard.
void update();
void shutdown();

bool dpad_up_pressed();
bool dpad_down_pressed();
bool dpad_left_pressed();
bool dpad_right_pressed();
bool rstick_up_pressed();
bool rstick_down_pressed();
bool rstick_left_pressed();
bool rstick_right_pressed();
bool lstick_up_pressed();
bool lstick_down_pressed();
bool lstick_left_pressed();
bool lstick_right_pressed();
bool lstick_up_held();
bool lstick_down_held();
bool lstick_left_held();
bool lstick_right_held();
} // namespace gamepad

namespace router {
/// ControllerRouter.Update(gameContext), once per frame from the input tick.
void update(Context game_context);
void reset();
/// ControllerRouter.NotifyKeyboardInput.
void notify_keyboard_input();
bool is_button_consumed(int button);
bool is_stick_pulse_down(int button);
bool is_stick_pulse_up(int button);
} // namespace router

namespace global_hotkeys {
/// GlobalHotkeyHandler.AnnounceCurrentVehicle.
void announce_current_vehicle();
/// GlobalHotkeyHandler.HandleItemDetailsKey.
void handle_item_details_key();
} // namespace global_hotkeys

} // namespace input
