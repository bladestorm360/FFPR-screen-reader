#pragma once
#include "input.h"
#include <vector>

// Physical input stays physical. Only mod command dispatch uses this catalog.
namespace input::bindings {
struct Entry {
    std::string id;
    std::wstring label;
    bool controller = false;
    int scope = 0, layout = -1;
    int primary = -1, modifier = 0, default_primary = -1, default_modifier = 0;
};
int add(const char* id, const char* label, bool controller, int scope, int primary, int modifier = 0, int layout = -1);
const std::vector<Entry>& entries();
std::wstring value(int index);
std::wstring input_name(bool controller, int primary);
std::wstring scope_name(int scope);
bool set(int index, int primary, int modifier, std::wstring& conflict);
void reset(int index);
void reset_all();
bool pressed(int index);
bool held(int index);
bool keyboard_in_use(Context context);
void context(Context context);
bool remapped_keyboard_in_use();
bool keyboard_action_pressed(const char* action);
void begin_frame();
bool pad(const char* id, bool release = false);
bool pad_held(const char* id);
bool consumed(int physical_input);
bool navigation_request_pressed();
bool navigation_controller_input(int physical_input);
void keyboard_allowed(bool allowed);
// Editor is hosted inside the speech-only mod menu; capture suppresses router dispatch.
void capture(int index);
bool capturing();
bool update_capture();
void cancel_capture();
constexpr int kLeftTrigger = kButtonCount, kRightTrigger = kButtonCount + 1;
constexpr int kLeftUp = kButtonCount + 2, kLeftDown = kButtonCount + 3,
    kLeftLeft = kButtonCount + 4, kLeftRight = kButtonCount + 5,
    kRightUp = kButtonCount + 6, kRightDown = kButtonCount + 7,
    kRightLeft = kButtonCount + 8, kRightRight = kButtonCount + 9;
constexpr int kPadInputs = kButtonCount + 10;
}
