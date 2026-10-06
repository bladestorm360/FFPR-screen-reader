// Ports FF5 Field/Routing/BreadcrumbRouteChainer.cs and the parts of Field/Routing/RoutingAdapter.cs
// it uses (tile size, routing log lines). FF5's mod-side addition, moved into the core so every
// game has it (UNIFIED_MOD_PLAN.md Phase 3). Called by field::find_path_to (navigation.cpp) only
// after the game's own search, and the hidden-passage retry, found nothing.
//
// Reaches targets beyond MapRouteSearcher's ~31.5-tile window by chaining several of its
// searches together. Search takes an arbitrary start cell, so the chainer searches from virtual
// cells and stitches the results; layers, hidden passages, IgnoreRoute and collision entities
// stay the game's business. A* over breadcrumb nodes, not a greedy chain: backward bearings are
// candidates from the first expansion, which is what lets a route leave a room the "wrong" way.
//
// MapRouteSearcher is identical in all five dump.cs files (Search(IMapAccessor, Vector3,
// Vector3, bool); SearchHorizontalLimit = SearchVerticalLimit = 64; MapRouteSearchException).
// Outside its window Search throws; ref::call_static turns that into "no points" (FF5 SafeSearch).
//
// Not ported:
//  * PathSpaceNormalizer: FF5 measured in game (2026-07-26) that Search returns WORLD space; its
//    conversion branch was dead code. The core describes the plain search's points as world
//    space in every game (FF1's design), so chained segments are used as returned, the same way.
//  * RoutingAdapter.TryGetCurrentTransportId: landing pings (src/core/fieldnav/landing_spots.cpp).
//  * The vehicle route searcher: removed from FF5 on 2026-10-03 at the user's request.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "field_internal.h"
#include "../frame.h"
#include "../hooks.h"
#include "../log.h"

namespace field {
namespace {

using Vec3 = unity::Vec3;

struct Cell { int x = 0, y = 0; };

// --- Window geometry ----------------------------------------------------------------------
// MapRouteSearcher builds a 64x64 cell window, so roughly +/-31 cells from the start. Hops use a
// smaller radius: a hop that lands outside the window just fails, so the margin is insurance.
constexpr int kWindowRadius = 31;
constexpr int kHopRadii[] = {24, 16, 10};

/// Candidate bearings relative to the straight line to the target, in degrees. Probe ORDER
/// favours the target (cheap wins first); membership never does.
constexpr float kCandidateBearings[] = {0.0f, 30.0f, -30.0f, 60.0f, -60.0f, 90.0f, -90.0f, 135.0f, -135.0f, 180.0f};

// --- Budgets ------------------------------------------------------------------------------
constexpr int kMaxHops = 24;
constexpr double kBudgetSeconds = 0.400;   // BUDGET_MS 400

/// Nodes within this many cells of each other are the same node.
constexpr int kNodeQuantise = 8;

// CHAIN_WHEN_TARGET_WITHIN_WINDOW is false in FF5 (by explicit instruction): a target inside the
// window that the plain search could not reach is a genuine local block; "no route" is right.

/// RoutingAdapter.TileSizeInverse: 16 world units per cell (the core's tile size in every game).
constexpr float kTileSizeInverse = 0.0625f;
constexpr float kDeg2Rad = 0.0174532924f;   // Mathf.Deg2Rad
constexpr float kRad2Deg = 57.29578f;       // Mathf.Rad2Deg

/// Mathf.RoundToInt: round half to even (the default floating-point rounding mode).
int round_to_int(float v) { return static_cast<int>(std::nearbyint(v)); }

struct ChainContext {
    void* map_handle = nullptr;
    int map_width = 0;
    int map_height = 0;
    bool collision_enabled = false;
    float layer_z = 0;
    Cell start;
    Cell goal;
};

struct HopNode {
    Cell cell;
    int g_cost = 0;                     // real steps from the player
    int f_cost = 0;                     // g_cost + straight-line estimate
    int parent = -1;                    // index in the node arena; -1 = the start node
    std::vector<Vec3> path_from_parent; // world space, joint cell included
};

Cell world_to_cell(Vec3 world, int map_width, int map_height)
{
    return Cell{static_cast<int>(std::floor(map_width * 0.5f + world.x * kTileSizeInverse)),
                static_cast<int>(std::floor(map_height * 0.5f - world.y * kTileSizeInverse))};
}

int chebyshev_distance(Cell a, Cell b) { return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)); }

int heuristic(Cell a, Cell b) { return chebyshev_distance(a, b); }

int64_t quantise_key(Cell cell)
{
    int64_t qx = cell.x / kNodeQuantise;
    int64_t qy = cell.y / kNodeQuantise;
    return static_cast<int64_t>((static_cast<uint64_t>(qx) << 32) ^ (static_cast<uint64_t>(qy) & 0xFFFFFFFFull));
}

/// Breadcrumb candidates around a node: every bearing at every radius, target-first.
void generate_candidates(Cell from, Cell goal, std::vector<Cell>& out)
{
    out.clear();
    float base_angle = static_cast<float>(std::atan2(static_cast<double>(goal.y - from.y),
                                                     static_cast<double>(goal.x - from.x))) * kRad2Deg;
    for (int radius : kHopRadii) {
        for (float bearing : kCandidateBearings) {
            float rad = (base_angle + bearing) * kDeg2Rad;
            float c = static_cast<float>(std::cos(static_cast<double>(rad)));
            float s = static_cast<float>(std::sin(static_cast<double>(rad)));
            out.push_back(Cell{from.x + round_to_int(c * static_cast<float>(radius)),
                               from.y + round_to_int(s * static_cast<float>(radius))});
        }
    }
}

/// One plain MapRouteSearcher search between two cells. False when there is no path. Tries the
/// destination layer ladder the plain pathfinder uses (2, 1, 0), so stairs behave the same way.
bool try_search(const ChainContext& ctx, Cell from, Cell to, std::vector<Vec3>& out)
{
    if (to.x < 0 || to.y < 0 || to.x >= ctx.map_width || to.y >= ctx.map_height)
        return false;

    Vec3 start{static_cast<float>(from.x), static_cast<float>(from.y), ctx.layer_z};
    for (int dest_z = 2; dest_z >= 0; --dest_z) {
        Vec3 dest{static_cast<float>(to.x), static_cast<float>(to.y), static_cast<float>(dest_z)};
        // A throw (target outside the window) comes back as null: "continue" in the C#.
        void* points = ref::call_static<void*>(r::mSearch, ctx.map_handle, start, dest, ctx.collision_enabled);
        int n = points ? unity::list_count(points) : 0;
        if (n <= 0) continue;

        out.clear();
        out.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) out.push_back(unity::list_value<Vec3>(points, i));
        return true;
    }
    return false;
}

/// Walks the hop chain back to the player and concatenates the segments, dropping the
/// duplicated joint cell at each seam, so a straight run across a seam reads as one leg.
std::vector<Vec3> stitch(const std::vector<HopNode>& nodes, int goal_node, Vec3 player_world_pos)
{
    std::vector<int> segments;
    for (int n = goal_node; n >= 0 && nodes[static_cast<size_t>(n)].parent >= 0;
         n = nodes[static_cast<size_t>(n)].parent)
        segments.push_back(n);
    std::reverse(segments.begin(), segments.end());

    std::vector<Vec3> stitched;
    for (int s : segments) {
        const std::vector<Vec3>& segment = nodes[static_cast<size_t>(s)].path_from_parent;
        size_t start_index = stitched.empty() ? 0 : 1;   // skip the repeated joint cell
        for (size_t i = start_index; i < segment.size(); ++i) stitched.push_back(segment[i]);
    }
    if (stitched.empty()) stitched.push_back(player_world_pos);
    return stitched;
}

int elapsed_ms(double started) { return static_cast<int>((frame::now() - started) * 1000.0); }

/// RunHopSearch. True with the stitched world path; false when no route was found (both
/// failure kinds - no path proved, stopped early - are told apart in the log only).
bool run_hop_search(const ChainContext& ctx, Vec3 player_world_pos, std::vector<Vec3>& out_path)
{
    const double started = frame::now();

    // Region bound: a bound on WHERE the search may look, never on whether each hop makes
    // progress (a route that must back out of a room regresses for several hops).
    int span_to_goal = chebyshev_distance(ctx.start, ctx.goal);
    int region_bound = std::max(kWindowRadius, round_to_int(static_cast<float>(span_to_goal) * 1.5f)) + 32;

    std::vector<HopNode> nodes;                   // arena; the open list holds indices
    std::vector<int> open;
    std::unordered_map<int64_t, int> closed;      // quantised cell -> best g cost
    std::unordered_set<int64_t> enclosed;         // nodes with no exit in any direction

    HopNode start_node;
    start_node.cell = ctx.start;
    start_node.f_cost = heuristic(ctx.start, ctx.goal);
    nodes.push_back(std::move(start_node));
    open.push_back(0);

    int hops = 0;
    bool budget_hit = false;
    int reached_goal = -1;
    std::vector<Cell> candidates;
    std::vector<Vec3> segment;

    while (!open.empty()) {
        if (hops >= kMaxHops || frame::now() - started > kBudgetSeconds) {
            budget_hit = true;
            break;
        }

        // Pop lowest F (the first one on a tie).
        size_t best = 0;
        for (size_t i = 1; i < open.size(); ++i)
            if (nodes[static_cast<size_t>(open[i])].f_cost < nodes[static_cast<size_t>(open[best])].f_cost) best = i;
        const int current = open[best];
        open.erase(open.begin() + static_cast<std::ptrdiff_t>(best));
        const Cell current_cell = nodes[static_cast<size_t>(current)].cell;
        const int current_g = nodes[static_cast<size_t>(current)].g_cost;
        const bool current_is_start = nodes[static_cast<size_t>(current)].parent < 0;

        const int64_t key = quantise_key(current_cell);
        auto seen = closed.find(key);
        if (seen != closed.end() && seen->second <= current_g) continue;
        closed[key] = current_g;

        hops++;

        // Can we finish from here in one plain search?
        if (try_search(ctx, current_cell, ctx.goal, segment)) {
            HopNode goal_node;
            goal_node.cell = ctx.goal;
            goal_node.g_cost = current_g + std::max(0, static_cast<int>(segment.size()) - 1);
            goal_node.parent = current;
            goal_node.path_from_parent = segment;
            nodes.push_back(std::move(goal_node));
            reached_goal = static_cast<int>(nodes.size()) - 1;
            break;
        }

        // Otherwise expand candidate breadcrumbs in every direction.
        bool any_exit = false;
        generate_candidates(current_cell, ctx.goal, candidates);
        for (const Cell& candidate : candidates) {
            if (chebyshev_distance(candidate, ctx.start) > region_bound) continue;

            const int64_t ckey = quantise_key(candidate);
            if (ckey == key) continue;
            if (enclosed.count(ckey)) continue;

            if (!try_search(ctx, current_cell, candidate, segment)) continue;

            any_exit = true;

            int g = current_g + std::max(0, static_cast<int>(segment.size()) - 1);
            auto known = closed.find(ckey);
            if (known != closed.end() && known->second <= g) continue;

            HopNode node;
            node.cell = candidate;
            node.g_cost = g;
            node.f_cost = g + heuristic(candidate, ctx.goal);
            node.parent = current;
            node.path_from_parent = segment;
            nodes.push_back(std::move(node));
            open.push_back(static_cast<int>(nodes.size()) - 1);
        }

        if (!any_exit) {
            // Nothing could leave this node in ANY direction: it is walled in. Never probe it
            // again; if it is where the player stands, there is nothing to search at all.
            enclosed.insert(key);
            if (current_is_start) {
                LOGF("[Routing] Start node is enclosed - no route without a vehicle");
                return false;
            }
        }
    }

    if (reached_goal >= 0) {
        out_path = stitch(nodes, reached_goal, player_world_pos);
        LOGF("[Routing] Breadcrumb route: %d hops, %zu points in %dms", hops, out_path.size(), elapsed_ms(started));
        return true;
    }

    // No route. Which kind matters for the log, not for what is spoken.
    if (budget_hit) {
        LOGF("[Routing] Breadcrumb search stopped early after %d hops, %dms (limit: %s) - reachability unknown",
             hops, elapsed_ms(started), hops >= kMaxHops ? "hop cap" : "wall clock");
        return false;
    }
    LOGF("[Routing] Breadcrumb search exhausted after %d hops, %dms - no route exists within the candidate model",
         hops, elapsed_ms(started));
    return false;
}

} // namespace

bool try_breadcrumb_chain(unity::Vec3 player_world_pos, unity::Vec3 target_world_pos, void* map_handle,
                          void* player, std::vector<unity::Vec3>& out_path)
{
    if (!map_handle || !player) return false;

    return hooks::guarded_r("BreadcrumbRouteChainer.TryChain", false, [&] {
        int map_width = call_virtual_int(r::mGetCollisionLayerWidth, map_handle);
        int map_height = call_virtual_int(r::mGetCollisionLayerHeight, map_handle);
        if (map_width <= 0 || map_height <= 0 || map_width > 10000 || map_height > 10000)
            return false;

        ChainContext ctx;
        ctx.map_handle = map_handle;
        ctx.map_width = map_width;
        ctx.map_height = map_height;
        ctx.collision_enabled = ref::rd<bool>(player, r::fIsOnCollision);
        ctx.layer_z = static_cast<float>(game_object_layer(player) - 9);
        ctx.start = world_to_cell(player_world_pos, map_width, map_height);
        ctx.goal = world_to_cell(target_world_pos, map_width, map_height);

        // Inside the window the plain search is authoritative and the chainer stays dormant.
        int dx = std::abs(ctx.goal.x - ctx.start.x);
        int dy = std::abs(ctx.goal.y - ctx.start.y);
        bool beyond_window = dx > kWindowRadius || dy > kWindowRadius;
        if (!beyond_window) return false;

        return run_hop_search(ctx, player_world_pos, out_path);
    });
}

} // namespace field
