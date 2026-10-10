#pragma once
#include <string>
#include <vector>
#include "field_api.h"
#include "baked_router.h"

// The game-side glue of the baked-map router (baked_router.h): the live map, the player's
// cell and layer, the current vehicle, the owned auto-vehicles, the live blockers and the
// counter rule, turned into a request; the answer turned into the spoken route. Called by
// field::find_path_to / find_path_to_quick (navigation.cpp) before the game's own searcher,
// which stays the fallback for a map the baked data does not know.
//
// Modes (FF5's PathSearchMode, kept): Full = an explicit player action: live blockers, the
// hidden-passage retry, and on a world map boarding a parked ship or airship when walking
// alone cannot reach the target ("board the Ship", "land"). Quick = per entity / per tick:
// one reachability flood per player cell, no blockers, no boarding.
namespace field::baked_nav {

/// True when the baked data knows the current map (and its size agrees with the live map).
bool available(void* map_handle);

/// The route for the current map, or false (then the caller may try the game's searcher).
bool find(unity::Vec3 from, unity::Vec3 to, void* map_handle, void* player, bool full, PathInfo& out);

/// The player's cell and layer on the current map's baked grid (false off the field).
bool player_cell(void* map_handle, void* player, baked_route::Cell& out);
/// A world position as a baked cell on the current map (the layer is left at 1).
bool cell_at(void* map_handle, unity::Vec3 world, baked_route::Cell& out);
/// The centre of a baked cell in the map's local space (the inverse of cell_at).
unity::Vec3 world_of(void* map_handle, const baked_route::Cell& cell);

/// The cells a route may end on for a target at `target` (the counter rule for a service NPC,
/// else the cell and its neighbours), on every layer the map has.
std::vector<baked_route::Cell> goal_cells(void* map_handle, unity::Vec3 target);

/// "North 3, East 2, board the Ship, East 40, land, West 2" from a list of phases.
struct Phase {
    std::vector<baked_route::Cell> cells;
    std::wstring action_before;   // spoken before this phase's legs (empty = none)
};
std::wstring describe(const std::vector<Phase>& phases);

/// Forget the cached reachability flood (a scan or a map change).
void invalidate();

// --- across maps -----------------------------------------------------------------------------
/// A destination on any sub-map of this game, by map master id and baked cell.
struct MapGoal {
    int map_id = 0;
    baked_route::Cell cell;
    bool examine = false;   // the target is examined from a neighbouring cell (a switch, a clock)
};

/// The first leg of the shortest way from the player to `goal`, possibly on another map: the
/// route across the current map to the exit or warp to take (Dijkstra over the sub-map graph of
/// GotoMap exits and script warps, each arriving at the destination's PointIn), or the route to
/// the goal itself when it is on this map. `description` says what to do at the end of the leg
/// ("take the exit to Castle Cornelia 2F", "step on the warp"). False when unreachable.
struct CrossLeg {
    bool found = false;
    bool on_this_map = false;        // the goal itself is the end of this leg
    int next_map_id = 0;             // the map the exit / warp leads to (when !on_this_map)
    std::vector<baked_route::Cell> cells;
    std::wstring description;        // the spoken directions, action word included
    int steps_here = 0;              // steps of this leg
    int steps_total = 0;             // estimated steps to the goal across every map
};
CrossLeg find_across(void* map_handle, void* player, const MapGoal& goal);

/// The map master name of a baked map id, through the game ("Citadel of Trials 2F"), or empty.
std::wstring map_name_of(int map_id);

} // namespace field::baked_nav
