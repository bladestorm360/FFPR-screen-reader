#pragma once
#include "../frame.h"

namespace field {
// Engine reads stay on their native owner. Limit their elapsed cost, not the
// number of cheap entries: a four-slot cap turned a world scan into seconds.
class CaptureBudget {
public:
    CaptureBudget() : until_(frame::now() + 0.0005) {}
    bool available() const { return frame::now() < until_; }
private:
    double until_;
};
}
