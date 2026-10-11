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

// Ordinary exits, vehicle pads and layer switches are never story-event hazards.
inline bool step_event(const baked::Object& object) {
    return (object.type == baked::ObjType::Event && (object.a != 0 || object.b != 0)) ||
           (object.type == baked::ObjType::Warp && object.d == 1);
}
inline bool crosses_event(const std::vector<Cell>& path, const std::vector<Cell>& events) {
    // The initial cell is already occupied; only subsequent displacement can trigger one.
    for (size_t i = 1; i < path.size(); ++i) for (const Cell& event : events)
        if (path[i].x == event.x && path[i].y == event.y &&
            (event.layer < 0 || path[i].layer == event.layer)) return true;
    return false;
}

struct Request {
    const baked::Submap* submap = nullptr;
    Cell start;
    std::vector<Cell> goals;          // reaching any one ends the search
    bool passages = false;            // hidden passages open
    int vehicle_group = 0;            // riding: that group's world layer; 0 = on foot
    std::vector<int> auto_groups;     // on foot: groups ridden automatically when owned
    std::vector<Cell> blocked;        // live blockers; layer -1 = every layer
    struct Directional { Cell cell; int axis; }; // 1 blocks north/south, 2 blocks east/west
    std::vector<Directional> directional;
    bool ignore_hazards = false;     // filter reachability includes destinations behind events
    bool live_hazards = false;       // current map: replace baked variants with active triggers
    std::vector<Cell> hazards;
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

/// A* for the shortest route with turns priced (BDFFHD's model; user decision 2026-10-11):
/// a change of direction costs 1.5 steps, so an open room is crossed in two long runs and not
/// in a staircase of single steps, and a turn is traded for at most one extra step. Empty
/// `goals` = not found.
Route find(const Request& r);

/// How a route may end at its target (user, 2026-10-11: "pinpoint", never "within 1 step").
///  Interact    an NPC, chest, switch: end on a neighbouring cell the target is used from (a
///              service NPC: its counter's RelayInteraction tile) and say which way to face;
///  Transition  an exit, a town tile, a warp pad: end ON it, on a cell the player can really
///              step onto (world maps: the terrain attribute the current transport crosses;
///              Salamand's two side tiles stand on wall attribute 19 and are never goals);
///  Spot        a waypoint, a vehicle, a save point: end on the cell itself;
///  Infer       by what is baked at the target cell (an exit or warp pad: Transition; a talk
///              object: Interact; else Spot).
enum class GoalKind { Infer, Interact, Transition, Spot };

struct Goals {
    std::vector<Cell> cells;      // where the route may end (empty = nothing to route to)
    bool face = false;            // an interaction: the facing is spoken at the end
    std::vector<Cell> faced;      // the target's own cells, which the final cell faces
};

/// The goal cells for `target` under the request's movement rules (its submap, transport and
/// passages; `start` is not used). `accept_bits` are the NPC accept_action_direction bits in
/// direction order north, east, south, west (0 = ignore the mask).
Goals goals_for(const Request& movement, const Cell& target, GoalKind kind);

/// The direction index (0 north, 1 east, 2 south, 3 west) from `from` towards `to`, by the
/// larger axis delta (ties: the vertical). -1 when the cells coincide.
int direction_towards(const Cell& from, const Cell& to);

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
bool direction_allowed(const Request& r, const Cell& from, const Cell& to);
/// The layer the player is on after stepping on `c` (ToLayer objects), else c.layer.
int layer_after(const baked::Grids& g, int from_layer, int x, int y);

/// "North 3, East 2": consecutive same-direction steps merged; count plus direction, no
/// "steps", no totals (user rule). The direction words are the caller's (mod text); this
/// returns direction indices: 0 north, 1 east, 2 south, 3 west, with counts.
struct Leg { int dir; int count; };
std::vector<Leg> legs(const std::vector<Cell>& cells);

} // namespace field::baked_route
