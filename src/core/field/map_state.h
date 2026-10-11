#pragma once
#include "baked_map.h"
#include "baked_router.h"
#include "../json.h"
#include <array>
#include <map>
#include <optional>

namespace field::map_state {
// Private copied state for navigation. Projection never writes native flags or runs scripts.
struct Flags {
    std::array<std::optional<int>, 1024> scenario, treasure;
    std::array<std::optional<int>, 16> global;
};
struct Selection { bool known = true; std::wstring group; };
Selection project(const json::Value& record, const Flags& flags);
Flags snapshot();
void observe_current(int map_id, std::wstring group);

class Resolver {
public:
    Resolver();
    explicit Resolver(Flags flags, const json::Value& records) : flags_(std::move(flags)), records_(records) {}
    // Unknown variants are excluded from destinations and included as possible obstacles.
    bool active(const baked::Submap& sm, const baked::Object& object, bool obstacle = false);
    bool collected(const baked::Submap& sm, const baked::Object& object) const;
    bool known(const baked::Submap& sm);
private:
    Selection selection(const baked::Submap& sm);
    Flags flags_;
    const json::Value& records_;
    std::map<int, Selection> cache_;
};
void add_obstacles(Resolver& resolver, const baked::Submap& sm, baked_route::Request& request, bool gates = true);
}
