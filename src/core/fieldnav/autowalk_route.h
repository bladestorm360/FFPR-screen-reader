#pragma once
#include "../field/route_progress.h"
#include <cmath>
#include <utility>

// Data-only follower. It advances only after a completed native tile move, never by time
// or interpolated transform position. The caller owns native movement and context checks.
namespace ffpr::fieldnav::autowalk {
using Cell = field::baked_route::Cell;
using Phase = field::route_progress::Phase;
enum class Result { Move, Arrived, Action, OffRoute };
struct Step {
    Result result = Result::OffRoute;
    Cell next;
    int x = 0, y = 0; // native axis: positive y is north
    std::wstring instruction;
    bool goal_edge = false;
    int face_direction = -1;
};
class Route {
    std::vector<Phase> phases_;
    size_t phase_ = 0, cell_ = 0;
    int width_ = 0, height_ = 0;
    bool wraps_ = false;
    static int delta(int d, int size, bool wraps) {
        if (wraps && size > 0) {
            if (d > size / 2) d -= size;
            if (d < -size / 2) d += size;
        }
        return d;
    }
public:
    void set(std::vector<Phase> phases, int width, int height, bool wraps) {
        phases_ = std::move(phases); phase_ = cell_ = 0;
        width_ = width; height_ = height; wraps_ = wraps;
    }
    Step next(const Cell& at) {
        while (phase_ < phases_.size()) {
            const Phase& p = phases_[phase_];
            if (p.cells.empty()) return {};
            // Boarding/landing can change position to the next phase's first cell. Give the
            // action before requiring that displacement; it belongs to the player.
            if (cell_ == 0 && !p.action_before.empty())
                return {Result::Action, {}, 0, 0, p.action_before};
            while (cell_ + 1 < p.cells.size() && at == p.cells[cell_ + 1]) ++cell_;
            if (!(at == p.cells[cell_])) return {};
            if (cell_ + 1 < p.cells.size()) {
                const Cell& to = p.cells[cell_ + 1];
                int dx = delta(to.x - at.x, width_, wraps_);
                int dy = delta(to.y - at.y, height_, wraps_);
                if (std::abs(dx) + std::abs(dy) != 1) return {}; // never jump/teleport
                return {Result::Move, to, dx, -dy, {}, cell_ + 2 == p.cells.size()};
            }
            if (p.manual_after) return {Result::Action, {}, 0, 0, p.action_after, false, p.arrival_direction};
            if (phase_ + 1 == phases_.size())
                return {Result::Arrived, {}, 0, 0, p.arrival_facing, false, p.arrival_direction};
            ++phase_; cell_ = 0;
        }
        return {};
    }
};
}
