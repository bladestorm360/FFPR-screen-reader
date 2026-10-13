// The baked-map router: see baked_router.h.
#include <algorithm>
#include <cstdlib>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include "baked_router.h"

namespace field::baked_route {
namespace {

constexpr int kDirs[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};   // north, east, south, west

/// Dense node index: layer * w * h + y * w + x.
inline int node_of(const baked::Grids& g, const Cell& c) { return (c.layer * g.height + c.y) * g.width + c.x; }
inline Cell cell_of(const baked::Grids& g, int node)
{
    const int per_layer = g.width * g.height;
    Cell c;
    c.layer = node / per_layer;
    const int rem = node % per_layer;
    c.y = rem / g.width;
    c.x = rem % g.width;
    return c;
}

bool blocked_by_caller(const Request& r, const Cell& c)
{
    for (const Cell& b : r.blocked)
        if (b.x == c.x && b.y == c.y && (b.layer < 0 || b.layer == c.layer)) return true;
    return false;
}

/// A goal cell of the request (a world map has one layer: the layer is ignored there).
bool is_goal_cell(const Request& r, const Cell& c)
{
    const bool world = r.submap && r.submap->world;
    for (const Cell& goal : r.goals)
        if (goal.x == c.x && goal.y == c.y && (world || goal.layer == c.layer)) return true;
    return false;
}

/// The ToLayer object at (x, y) whose own layer or target layer is `layer`, if any.
const baked::Object* to_layer_at(const baked::Grids& g, int layer, int x, int y)
{
    for (const baked::Object& o : g.objects) {
        if (o.type != baked::ObjType::ToLayer || !o.contains(x, y)) continue;
        if (o.layer == layer || o.a == layer) return &o;
    }
    return nullptr;
}

} // namespace

int layer_after(const baked::Grids& g, int from_layer, int x, int y)
{
    const baked::Object* o = to_layer_at(g, from_layer, x, y);
    if (!o) return from_layer;
    if (o->layer == from_layer && o->a >= 0 && o->a <= 2) return o->a;
    if (o->a == from_layer) return o->layer;
    return from_layer;
}

bool open(const Request& r, const baked::Grids& g, const Cell& c)
{
    if (!baked::in_bounds(g, c.x, c.y)) return false;
    if (r.submap && r.submap->world) {
        if (r.vehicle_group > 0) return baked::vehicle_open(g, r.vehicle_group, c.x, c.y);
        if (baked::vehicle_open(g, 1, c.x, c.y)) return true;
        for (int grp : r.auto_groups)
            if (baked::vehicle_open(g, grp, c.x, c.y)) return true;
        return false;
    }
    return baked::walkable(g, c.layer, c.x, c.y, r.passages);
}

bool direction_allowed(const Request& r, const Cell& from, const Cell& to)
{
    for (const auto& gate : r.directional)
        if (gate.cell.x == to.x && gate.cell.y == to.y &&
            (gate.cell.layer < 0 || gate.cell.layer == to.layer) &&
            ((gate.axis == 1 && from.y != to.y) || (gate.axis == 2 && from.x != to.x))) return false;
    return true;
}

namespace {

/// Expands `from` in the four directions; `visit(next)` returns false to stop.
template <typename F>
void neighbours(const Request& r, const baked::Grids& g, const Cell& from, F&& visit)
{
    for (const auto& d : kDirs) {
        Cell n{from.layer, from.x + d[0], from.y + d[1]};
        if (!baked::in_bounds(g, n.x, n.y)) continue;
        // A ToLayer cell moves the player to its other layer; the step counts when the cell is
        // open on either layer (the collision grids of a stairway overlap at its ends).
        const int nl = r.submap && r.submap->world ? from.layer : layer_after(g, from.layer, n.x, n.y);
        Cell moved{nl, n.x, n.y};
        if (!direction_allowed(r, from, moved)) continue;
        bool ok = open(r, g, moved);
        if (!ok && nl != from.layer && open(r, g, n)) ok = true;
        if (!ok && r.goals_open && is_goal_cell(r, moved)) ok = true;   // a closed goal cell (town tile)
        if (!ok || blocked_by_caller(r, moved)) continue;
        // A step-on warp or trapdoor is crossed only as the goal itself.
        bool hazard = false;
        if (!r.ignore_hazards) {
            if (r.live_hazards) {
                for (const Cell& c : r.hazards)
                    if (c.x == moved.x && c.y == moved.y && (c.layer < 0 || c.layer == moved.layer)) { hazard = true; break; }
            } else hazard = !g.hazard.empty() && g.hazard[static_cast<size_t>(n.y) * g.width + n.x];
        }
        if (hazard && !is_goal_cell(r, moved)) continue;
        if (!visit(moved)) return;
    }
}

} // namespace

Route find(const Request& r)
{
    Route out;
    if (!r.submap || r.goals.empty()) return out;
    const baked::Grids& g = baked::grids(*r.submap);
    if (g.width <= 0 || g.height <= 0) return out;
    if (!baked::in_bounds(g, r.start.x, r.start.y)) return out;

    const int layers = r.submap->world ? 1 : 3;
    const int total = layers * g.width * g.height;
    Cell start = r.start;
    if (r.submap->world) start.layer = 0;
    std::vector<Cell> goals = r.goals;
    if (r.submap->world)
        for (Cell& c : goals) c.layer = 0;

    auto is_goal = [&](const Cell& c) {
        for (const Cell& goal : goals)
            if (goal == c) return true;
        return false;
    };
    auto heuristic = [&](const Cell& c) {
        int best = 1 << 30;
        for (const Cell& goal : goals) best = std::min(best, std::abs(goal.x - c.x) + std::abs(goal.y - c.y));
        return best;
    };

    if (is_goal(start)) {
        out.found = true;
        // World searches normalize their internal layer to zero. Published paths
        // still use the caller's layer, including adjacent zero-movement arrival.
        out.cells.push_back(r.start);
        return out;
    }

    // Turns are priced (BDFFHD navgrid.cpp, kTurnPenaltyWorld = 1.5 steps): the search state
    // is the cell plus the direction it was entered by, a step costs kStep and a change of
    // direction kTurn. Without this a 4-connected search has no reason to prefer two long runs
    // over a staircase of single steps across an open room, and spoke "West 1, South 1, West 1,
    // South 1 ..." (user, 2026-10-11). Ties break on the state index: the same route every time.
    constexpr int kStep = 10;
    constexpr int kTurn = 15;
    const int states = total * 4;
    std::vector<int32_t> cost(static_cast<size_t>(states), -1);
    std::vector<int32_t> came(static_cast<size_t>(states), -1);   // -1 = reached from the start
    using Item = std::pair<int, int>;   // f, state (node * 4 + direction)
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> frontier;
    auto h = [&](const Cell& c) { return heuristic(c) * kStep; };
    auto relax = [&](const Cell& next, int dir, int ng, int from_state) {
        const int st = node_of(g, next) * 4 + dir;
        if (cost[static_cast<size_t>(st)] < 0 || ng < cost[static_cast<size_t>(st)]) {
            cost[static_cast<size_t>(st)] = ng;
            came[static_cast<size_t>(st)] = from_state;
            frontier.push({ng + h(next), st});
        }
    };
    // The start has no direction: its exits cost a step each, never a turn.
    neighbours(r, g, start, [&](const Cell& next) {
        relax(next, direction_towards(start, next), kStep, -1);
        return true;
    });
    int expansions = 0;
    int goal_state = -1;

    while (!frontier.empty()) {
        if (r.cancelled && r.cancelled()) return out;
        Item top = frontier.top();
        frontier.pop();
        const int st = top.second;
        const int dir = st % 4;
        const Cell cur = cell_of(g, st / 4);
        const int gcost = cost[static_cast<size_t>(st)];
        if (top.first - h(cur) > gcost) continue;   // stale entry
        if (is_goal(cur)) {
            goal_state = st;
            break;
        }
        if (r.max_expansions > 0 && ++expansions > r.max_expansions) break;
        neighbours(r, g, cur, [&](const Cell& next) {
            const int nd = direction_towards(cur, next);
            relax(next, nd, gcost + kStep + (nd != dir ? kTurn : 0), st);
            return true;
        });
    }
    if (goal_state < 0) return out;

    std::vector<Cell> path;
    for (int st = goal_state; st >= 0; st = came[static_cast<size_t>(st)]) path.push_back(cell_of(g, st / 4));
    path.push_back(start);
    std::reverse(path.begin(), path.end());
    if (r.submap->world)
        for (Cell& c : path) c.layer = r.start.layer;
    out.found = !path.empty() && path.front().x == start.x && path.front().y == start.y;
    out.cells = std::move(path);
    return out;
}

bool Reach::reachable(const Cell& c) const { return distance(c) >= 0; }

int Reach::distance(const Cell& c) const
{
    const int layer = submap && submap->world ? 0 : c.layer;
    if (layer < 0 || layer > 2 || c.x < 0 || c.y < 0 || c.x >= width || c.y >= height) return -1;
    if (dist[layer].empty()) return -1;
    const uint16_t d = dist[layer][static_cast<size_t>(c.y) * width + c.x];
    return d == 0xFFFF ? -1 : d;
}

Reach flood(const Request& r)
{
    Reach out;
    out.submap = r.submap;
    if (!r.submap) return out;
    const baked::Grids& g = baked::grids(*r.submap);
    out.width = g.width;
    out.height = g.height;
    if (g.width <= 0 || g.height <= 0 || !baked::in_bounds(g, r.start.x, r.start.y)) return out;
    const int layers = r.submap->world ? 1 : 3;
    for (int l = 0; l < layers; ++l) out.dist[l].assign(static_cast<size_t>(g.width) * g.height, 0xFFFF);

    Cell start = r.start;
    if (r.submap->world) start.layer = 0;
    auto& d0 = out.dist[start.layer];
    d0[static_cast<size_t>(start.y) * g.width + start.x] = 0;
    std::queue<Cell> q;
    q.push(start);
    while (!q.empty()) {
        if (r.cancelled && r.cancelled()) return Reach{};
        Cell cur = q.front();
        q.pop();
        const uint16_t dc = out.dist[cur.layer][static_cast<size_t>(cur.y) * g.width + cur.x];
        neighbours(r, g, cur, [&](const Cell& next) {
            uint16_t& dn = out.dist[next.layer][static_cast<size_t>(next.y) * g.width + next.x];
            if (dn == 0xFFFF) {
                dn = static_cast<uint16_t>(std::min(0xFFFE, dc + 1));
                q.push(next);
            }
            return true;
        });
    }
    return out;
}

int direction_towards(const Cell& from, const Cell& to)
{
    const int dx = to.x - from.x;
    const int dy = to.y - from.y;   // tile rows grow downward: dy < 0 is north
    if (dx == 0 && dy == 0) return -1;
    if (std::abs(dy) >= std::abs(dx)) return dy < 0 ? 0 : 2;
    return dx > 0 ? 1 : 3;
}

int facing_direction(const Cell& from, const Goals& goals)
{
    if (!goals.face || goals.faced.empty()) return -1;
    const Cell* nearest = nullptr;
    int best = 1 << 30;
    for (const Cell& c : goals.faced) {
        const int distance = std::abs(c.x - from.x) + std::abs(c.y - from.y);
        if (distance < best) { best = distance; nearest = &c; }
    }
    return direction_towards(from, *nearest);
}

std::vector<Leg> legs(const std::vector<Cell>& cells)
{
    std::vector<Leg> out;
    for (size_t i = 1; i < cells.size(); ++i) {
        const int dir = direction_towards(cells[i - 1], cells[i]);
        if (dir < 0) continue;
        if (!out.empty() && out.back().dir == dir) ++out.back().count;
        else out.push_back(Leg{dir, 1});
    }
    return out;
}

// --- goals ------------------------------------------------------------------------------------
namespace {

bool is_talk(baked::ObjType t)
{
    switch (t) {
    case baked::ObjType::NPC:
    case baked::ObjType::ShopNPC:
    case baked::ObjType::Entity:
    case baked::ObjType::AnimEntity:
    case baked::ObjType::TreasureBox:
        return true;
    default:
        return false;
    }
}

const baked::Object* object_at(const baked::Grids& g, const Cell& c, bool (*pred)(const baked::Object&))
{
    for (const baked::Object& o : g.objects)
        if (pred(o) && o.contains(c.x, c.y)) return &o;
    return nullptr;
}

bool talk_pred(const baked::Object& o) { return is_talk(o.type); }
bool exit_pred(const baked::Object& o) { return o.type == baked::ObjType::GotoMap && o.a > 0; }
bool step_warp_pred(const baked::Object& o) { return o.type == baked::ObjType::Warp && o.d == 1; }
bool examine_warp_pred(const baked::Object& o) { return o.type == baked::ObjType::Warp && o.d != 1; }

void footprint_of(const baked::Object& o, std::vector<Cell>& out, int layer)
{
    for (int y = o.ty; y < o.ty + o.th; ++y)
        for (int x = o.tx; x < o.tx + o.tw; ++x) out.push_back(Cell{layer, x, y});
}

bool contains_cell(const std::vector<Cell>& cells, int x, int y)
{
    for (const Cell& c : cells)
        if (c.x == x && c.y == y) return true;
    return false;
}

void push_unique(std::vector<Cell>& cells, const Cell& c)
{
    for (const Cell& e : cells)
        if (e == c) return;
    cells.push_back(c);
}

/// The transport groups the player moves with on a world map: the vehicle ridden, else walking
/// (group 1) with the owned auto-vehicles.
std::vector<int> movement_groups(const Request& r)
{
    if (r.vehicle_group > 0) return {r.vehicle_group};
    std::vector<int> groups{1};
    groups.insert(groups.end(), r.auto_groups.begin(), r.auto_groups.end());
    return groups;
}

/// A world-map cell the player can really step onto: its terrain attribute is one the current
/// transport crosses (TransportationInfo.CheckOkList: okList[attribute - 1]). The route layer is
/// not this test: it closes every town tile so routes do not run through towns, and a town's
/// decorative side tiles (attribute 19) are closed in both.
bool world_enterable(const Request& r, const baked::Grids& g, int x, int y)
{
    if (!baked::in_bounds(g, x, y)) return false;
    const int attr = baked::attribute(g, x, y);
    for (int grp : movement_groups(r))
        if (baked::group_crosses(grp, attr)) return true;
    return false;
}

/// The cell on every layer it is open on; `fallback_layer` when it is open on none (-1 = skip).
void add_open_layers(const Request& r, const baked::Grids& g, int x, int y, int fallback_layer, std::vector<Cell>& out)
{
    const bool world = r.submap && r.submap->world;
    if (!baked::in_bounds(g, x, y)) return;
    bool any = false;
    const int layers = world ? 1 : 3;
    for (int l = 0; l < layers; ++l) {
        Cell c{world ? 1 : l, x, y};
        if (!world && g.collision[l].empty()) continue;
        if (open(r, g, c)) {
            push_unique(out, c);
            any = true;
        }
    }
    if (!any && fallback_layer >= 0) push_unique(out, Cell{fallback_layer, x, y});
}

constexpr int kSide[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};   // north, east, south, west

/// accept_action_direction: the sides an NPC is spoken to from, one bit per side; 15 = every
/// side (all but ~40 NPCs across the six games, 2026-10-11). FF2's
/// FieldMapObjectAnimDefault.IsAcceptActionDirection (0x180C4AC90, decompiled read-only
/// 2026-10-11) tests the action vector against Vector2.up -> bit 1, right -> bit 2, down ->
/// bit 4, left -> bit 8 (zero always passes), and the dumps place those NPCs with the excluded
/// side against a wall (Gestahl, mask 4, wall to the north: the player stands south), so the
/// bit is the side of the NPC the player stands on. A mask that leaves no open side is ignored
/// rather than making the NPC unreachable.
constexpr int kAcceptBitOfSide[4] = {1, 2, 4, 8};   // north, east, south, west

} // namespace

Goals goals_for(const Request& r, const Cell& target, GoalKind kind)
{
    Goals out;
    if (!r.submap) return out;
    const baked::Grids& g = baked::grids(*r.submap);
    if (!baked::in_bounds(g, target.x, target.y)) return out;
    const bool world = r.submap->world;

    const baked::Object* talk = object_at(g, target, talk_pred);
    const baked::Object* exit = object_at(g, target, exit_pred);
    const baked::Object* pad = object_at(g, target, step_warp_pred);
    const baked::Object* warp = object_at(g, target, examine_warp_pred);
    if (kind == GoalKind::Infer) {
        if (exit || pad) kind = GoalKind::Transition;
        else if (talk || warp) kind = GoalKind::Interact;
        else kind = GoalKind::Spot;
    }

    if (kind == GoalKind::Transition) {
        // The exit's own cells, with those of its sibling tiles (the same destination and
        // arrival point within two cells: a town's arc of three tiles is one entrance).
        std::vector<Cell> footprint;
        const baked::Object* anchor = exit ? exit : pad;
        const int layer = anchor ? anchor->layer : target.layer;
        if (anchor) {
            footprint_of(*anchor, footprint, world ? target.layer : layer);
            if (exit) {
                for (const baked::Object& o : g.objects) {
                    if (!exit_pred(o) || &o == exit || o.a != exit->a || o.b != exit->b) continue;
                    if (std::abs(o.tx - exit->tx) > 2 || std::abs(o.ty - exit->ty) > 2) continue;
                    for (int y = o.ty; y < o.ty + o.th; ++y)
                        for (int x = o.tx; x < o.tx + o.tw; ++x) push_unique(footprint, Cell{world ? target.layer : o.layer, x, y});
                }
            }
        } else {
            footprint.push_back(target);
        }
        if (world) {
            for (const Cell& c : footprint)
                if (world_enterable(r, g, c.x, c.y)) out.cells.push_back(c);
            if (out.cells.empty()) out.cells = footprint;   // nothing crossable: let the game decide
        } else {
            // Entered although closed (the game opens GotoMap cells for a route), on the exit's
            // layer and on any other layer the cell is open on.
            for (const Cell& c : footprint) {
                push_unique(out.cells, c);
                add_open_layers(r, g, c.x, c.y, -1, out.cells);
            }
        }
        return out;
    }

    if (kind == GoalKind::Interact) {
        out.face = true;
        const baked::Object* object = talk ? talk : warp;
        if (object) footprint_of(*object, out.faced, world ? target.layer : object->layer);
        else out.faced.push_back(target);
        // The game's own counter tile: a service NPC's link_object_id names the RelayInteraction
        // the player stands on to talk across the counter (every game, 2026-10-10). NPCs only:
        // a chest's link_object_id is its own object id (FF1/FF2 chests: 100) and names nothing.
        const bool npc = talk && (talk->type == baked::ObjType::NPC || talk->type == baked::ObjType::ShopNPC);
        if (npc && !world && talk->a > 0) {
            for (const baked::Object& o : g.objects) {
                if (o.type == baked::ObjType::RelayInteraction && o.a == talk->a) {
                    out.cells.push_back(Cell{o.layer, o.tx, o.ty});
                    return out;
                }
            }
        }
        // NPCs, chests and examinable objects all carry accept_action_direction (bake field b).
        const int mask = talk && talk->b > 0 ? talk->b : 15;
        std::vector<Cell> allowed, any_side;
        for (const Cell& c : out.faced) {
            for (int d = 0; d < 4; ++d) {
                const int x = c.x + kSide[d][0], y = c.y + kSide[d][1];
                if (contains_cell(out.faced, x, y)) continue;
                std::vector<Cell> here;
                add_open_layers(r, g, x, y, -1, here);
                for (const Cell& h : here) {
                    push_unique(any_side, h);
                    if (mask == 0 || (mask & kAcceptBitOfSide[d])) push_unique(allowed, h);
                }
            }
        }
        out.cells = allowed.empty() ? any_side : allowed;
        if (out.cells.empty()) {
            // Boxed in on every side: the diagonals, then the target's own cells (entered as a goal).
            static const int kDiag[4][2] = {{1, -1}, {1, 1}, {-1, 1}, {-1, -1}};
            for (const Cell& c : out.faced)
                for (const auto& d : kDiag) add_open_layers(r, g, c.x + d[0], c.y + d[1], -1, out.cells);
            if (out.cells.empty()) out.cells = out.faced;
        }
        return out;
    }

    // Spot: the cell itself where it is open; a closed cell (a parked ship on water, a trigger
    // on a closed tile) is entered as the goal on the target's layer.
    add_open_layers(r, g, target.x, target.y, target.layer, out.cells);
    return out;
}

} // namespace field::baked_route
