#pragma once

namespace ffpr::fieldnav::autowalk {
// A leg ends on its native trigger cell. Keep the final destination while the game takes
// over, and resume only after the expected map, player input and fade are ready.
class Transition {
    int expected_ = 0;
    bool waiting_ = false, arrived_ = false;
    double deadline_ = 0;
public:
    void set_next(int next) { expected_ = next; waiting_ = arrived_ = false; deadline_ = 0; }
    bool begin(double now) {
        if (expected_ <= 0) return false;
        if (!waiting_) deadline_ = now + 30.0;
        waiting_ = true;
        return true;
    }
    bool map_ready(int map, double now) {
        if (!waiting_ || map != expected_) return false;
        arrived_ = true;
        deadline_ = now + 30.0;
        return true;
    }
    bool waiting() const { return waiting_; }
    bool can_resume(bool native_ready) const { return waiting_ && arrived_ && native_ready; }
    bool expired(double now) const { return waiting_ && now > deadline_; }
    void clear() { set_next(0); }
};
}
