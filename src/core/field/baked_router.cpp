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
        bool ok = open(r, g, moved);
        if (!ok && nl != from.layer && open(r, g, n)) ok = true;
        if (!ok && r.goals_open && is_goal_cell(r, moved)) ok = true;   // a closed goal cell (town tile)
        if (!ok || blocked_by_caller(r, moved)) continue;
        // A step-on warp or trapdoor is crossed only as the goal itself.
        if (!g.hazard.empty() && g.hazard[static_cast<size_t>(n.y) * g.width + n.x] && !is_goal_cell(r, moved)) continue;
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
        out.cells.push_back(start);
        return out;
    }

    std::vector<int32_t> cost(static_cast<size_t>(total), -1);
    std::vector<int32_t> came(static_cast<size_t>(total), -1);
    using Item = std::pair<int, int>;   // f, node
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> frontier;
    const int s = node_of(g, start);
    cost[static_cast<size_t>(s)] = 0;
    frontier.push({heuristic(start), s});
    int expansions = 0;
    int goal_node = -1;

    while (!frontier.empty()) {
        Item top = frontier.top();
        frontier.pop();
        const int node = top.second;
        const Cell cur = cell_of(g, node);
        const int gcost = cost[static_cast<size_t>(node)];
        if (top.first - heuristic(cur) > gcost) continue;   // stale entry
        if (is_goal(cur)) {
            goal_node = node;
            break;
        }
        if (r.max_expansions > 0 && ++expansions > r.max_expansions) break;
        neighbours(r, g, cur, [&](const Cell& next) {
            const int n = node_of(g, next);
            const int ng = gcost + 1;
            if (cost[static_cast<size_t>(n)] < 0 || ng < cost[static_cast<size_t>(n)]) {
                cost[static_cast<size_t>(n)] = ng;
                came[static_cast<size_t>(n)] = node;
                frontier.push({ng + heuristic(next), n});
            }
            return true;
        });
    }
    if (goal_node < 0) return out;

    std::vector<Cell> path;
    for (int n = goal_node; n >= 0; n = came[static_cast<size_t>(n)]) {
        path.push_back(cell_of(g, n));
        if (n == s) break;
    }
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

std::vector<Leg> legs(const std::vector<Cell>& cells)
{
    std::vector<Leg> out;
    for (size_t i = 1; i < cells.size(); ++i) {
        const int dx = cells[i].x - cells[i - 1].x;
        const int dy = cells[i].y - cells[i - 1].y;
        int dir;
        if (dy < 0) dir = 0;        // north (tile rows grow downward)
        else if (dx > 0) dir = 1;   // east
        else if (dy > 0) dir = 2;   // south
        else dir = 3;               // west
        if (!out.empty() && out.back().dir == dir) ++out.back().count;
        else out.push_back(Leg{dir, 1});
    }
    return out;
}

} // namespace field::baked_route
