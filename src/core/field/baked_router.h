#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "baked_map.h"

// The baked-map router: A* over one sub-map's baked grids, the mod-side replacement for the
// game's MapRouteSearcher (a 64x64 window, about 31 tiles; user decision, 2026-10-10). Pure
// data: no game pointers, so tools/route_check.cpp runs it offline against data/ffN/maps.bin.
//
// Movement model (what the game does, confirmed in the FF2 decompile of 2026-10-10):
//  * 4-connected steps, one tile each; a cell is open when its collision id is 0, and a
//    hidden-passage cell is open to the player (never to the game's searcher) when asked;
//  * layer changes happen by stepping on a ToLayer cell (the object's own layer -> its target
//    layer, and back); the step is taken when the cell is open on either layer;
//  * world maps have no collision grid: on foot the player moves where transportation group 1
//    is open; a vehicle moves where its group's layer is open; the auto-ridden vehicles
//    (FF1-FF3 canoe, FF2 snowcraft: type 5 "Content") extend walking with their groups once
//    the player owns them;
//  * live blockers (NPCs, closed objects) are passed in by the caller.
namespace field::baked_route {

struct Cell {
    int layer = 1, x = 0, y = 0;
    bool operator==(const Cell& o) const { return layer == o.layer && x == o.x && y == o.y; }
};

struct Request {
    const baked::Submap* submap = nullptr;
    Cell start;
    std::vector<Cell> goals;          // reaching any one ends the search
    bool passages = false;            // hidden passages open
    int vehicle_group = 0;            // riding: that group's world layer; 0 = on foot
    std::vector<int> auto_groups;     // on foot: groups ridden automatically when owned
    std::vector<Cell> blocked;        // live blockers; layer -1 = every layer
    bool goals_open = true;           // a goal cell may be entered although it is closed: the
                                      // game opens a GotoMap entity's cells for the route
                                      // (IMapAccessor.CombinedGotoMapEntityCurrentMappingData);
                                      // a world-map town tile is closed to walking otherwise
    int max_expansions = 0;           // 0 = whole map
};

struct Route {
    bool found = false;
    std::vector<Cell> cells;          // start .. goal, one tile per step
    int steps() const { return cells.empty() ? 0 : static_cast<int>(cells.size()) - 1; }
};

/// A* for the shortest route. Empty `goals` = not found.
Route find(const Request& r);

/// Every cell reachable from `start` under the request's rules (goals ignored): one BFS, then
/// O(1) tests, for the pathfinding filter (one flood per scan instead of one search per entity).
struct Reach {
    const baked::Submap* submap = nullptr;
    int width = 0, height = 0;
    std::vector<uint16_t> dist[3];    // 0xFFFF = unreachable
    bool reachable(const Cell& c) const;
    int distance(const Cell& c) const;   // -1 when unreachable
};
Reach flood(const Request& r);

/// Cell tests shared with the callers: open for the request's movement rules.
bool open(const Request& r, const baked::Grids& g, const Cell& c);
/// The layer the player is on after stepping on `c` (ToLayer objects), else c.layer.
int layer_after(const baked::Grids& g, int from_layer, int x, int y);

/// "North 3, East 2": consecutive same-direction steps merged; count plus direction, no
/// "steps", no totals (user rule). The direction words are the caller's (mod text); this
/// returns direction indices: 0 north, 1 east, 2 south, 3 west, with counts.
struct Leg { int dir; int count; };
std::vector<Leg> legs(const std::vector<Cell>& cells);

} // namespace field::baked_route
