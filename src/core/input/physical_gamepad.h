#pragma once
#include <cstdint>
namespace input::physical {
struct Pad {
    bool ready = false;
    bool available = false;
    int type = 0;
    uint32_t held = 0, down = 0, up = 0;
    uint8_t right = 0, right_down = 0, left = 0, left_down = 0;
    float lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;
    bool trigger_down = false;
};
void prepare(); // boot only: creates a parked SDK owner
void request(); // native owner: only publishes readiness, never starts SDL inline
Pad take();    // copied physical state; no SDK call or waiting
void shutdown();
}
