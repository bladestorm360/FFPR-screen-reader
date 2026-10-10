// The game-side glue of the baked-map router: see baked_nav.h.
//
// Cells: the game's MapRouteSearcher cell space, cx = floor(w/2 + x/16), cy = floor(h/2 - y/16)
// (FF1 FindPathTo; w, h = the sub-map's size in tiles), the player's layer = its game object's
// Unity layer - 9 (Bottom 0, Ground 1, Upper 2). A world map has no collision grid: the baked
// sub-map's size is used (the live IMapAccessor reports no collision size there).
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include "baked_nav.h"
#include "baked_map.h"
#include "field_internal.h"
#include "../game.h"
#include "../hooks.h"
#include "../log.h"
#include "../modtext.h"
#include "../text.h"
#include "../fieldnav/move_state.h"

namespace field::baked_nav {
namespace {

using baked_route::Cell;
using unity::Vec3;

constexpr float kTile = 16.0f;
constexpr int kOldWindowSteps = 31;    // beyond the game's searcher: FF5's six-leg cap applies
constexpr size_t kMaxSpokenLegs = 6;
constexpr int kTransportContent = 5;   // auto-ridden vehicles (canoe, snowcraft)
constexpr int kTransportShip = 2;
constexpr int kTransportSubmarine = 6;

bool g_load_tried = false;
bool g_loaded = false;
std::string* g_blob = nullptr;   // the embedded resource bytes: the reader keeps the pointer
std::unordered_map<int, bool>* g_size_warned = nullptr;

bool ensure_loaded()
{
    if (g_load_tried) return g_loaded;
    g_load_tried = true;
    g_blob = new std::string(data::game_file("maps"));
    g_loaded = !g_blob->empty() && baked::load(g_blob->data(), g_blob->size());
    if (g_loaded)
        LOGF("[BakedMap] %s: %d sub-maps, %d vehicles", game::key(), baked::submap_count(),
             static_cast<int>(baked::transports().size()));
    else
        LOGF("[BakedMap] no baked maps for %s (%zu bytes): the game's route searcher is used", game::key(),
             g_blob->size());
    return g_loaded;
}

const baked::Submap* current_submap(void* map_handle)
{
    if (!ensure_loaded()) return nullptr;
    const int map_id = current_map_id();
    if (map_id <= 0) return nullptr;
    const baked::Submap* sm = baked::submap_by_map_id(map_id);
    if (!sm) return nullptr;
    // The live map's collision size must agree (a stale map handle, or the wrong sub-map).
    if (map_handle && !sm->world) {
        const int w = call_virtual_int(r::mGetCollisionLayerWidth, map_handle);
        const int h = call_virtual_int(r::mGetCollisionLayerHeight, map_handle);
        if (w > 0 && h > 0 && (w != sm->width || h != sm->height)) {
            if (!g_size_warned) g_size_warned = new std::unordered_map<int, bool>();
            if (!(*g_size_warned)[map_id]) {
                (*g_size_warned)[map_id] = true;
                LOGF("[BakedMap] map %d: live size %dx%d, baked %ls %dx%d: using the game's searcher", map_id, w, h,
                     baked::name_of(*sm).c_str(), sm->width, sm->height);
            }
            return nullptr;
        }
    }
    return sm;
}

Cell cell_of(const baked::Submap& sm, Vec3 world, int layer)
{
    Cell c;
    c.layer = layer;
    c.x = static_cast<int>(std::floor(sm.width * 0.5f + world.x / kTile));
    c.y = static_cast<int>(std::floor(sm.height * 0.5f - world.y / kTile));
    return c;
}

Vec3 world_of_cell(const baked::Submap& sm, const Cell& c)
{
    return Vec3{(c.x - sm.width * 0.5f) * kTile + kTile * 0.5f, (sm.height * 0.5f - c.y) * kTile - kTile * 0.5f, 0};
}

int player_layer(void* player)
{
    const int layer = game_object_layer(player) - 9;
    return layer < 0 || layer > 2 ? 1 : layer;
}

const wchar_t* direction_key(int dir)
{
    switch (dir) {
    case 0: return L"North";
    case 1: return L"East";
    case 2: return L"South";
    default: return L"West";
    }
}

/// The baked talk object (NPC, shopkeeper, chest ...) on a cell, if any: the counter rule.
const baked::Object* talk_object_at(const baked::Grids& g, int x, int y)
{
    for (const baked::Object& o : g.objects) {
        switch (o.type) {
        case baked::ObjType::NPC:
        case baked::ObjType::ShopNPC:
        case baked::ObjType::Entity:
        case baked::ObjType::AnimEntity:
        case baked::ObjType::TreasureBox:
            if (o.contains(x, y)) return &o;
            break;
        default: break;
        }
    }
    return nullptr;
}

const baked::Object* relay_with_id(const baked::Grids& g, int object_id)
{
    if (object_id <= 0) return nullptr;
    for (const baked::Object& o : g.objects)
        if (o.type == baked::ObjType::RelayInteraction && o.a == object_id) return &o;
    return nullptr;
}

std::vector<Cell> goals_for(const baked::Submap& sm, const baked::Grids& g, const Cell& target)
{
    std::vector<Cell> goals;
    const int layers = sm.world ? 1 : 3;
    auto add = [&](int x, int y) {
        for (int l = 0; l < layers; ++l) {
            if (!sm.world && g.collision[l].empty()) continue;
            goals.push_back(Cell{sm.world ? target.layer : l, x, y});
        }
    };
    // The game's own counter tile: a service NPC's link_object_id names the RelayInteraction the
    // player stands on to talk across the counter (every game, 2026-10-10).
    if (!sm.world) {
        if (const baked::Object* talk = talk_object_at(g, target.x, target.y)) {
            if (const baked::Object* relay = relay_with_id(g, talk->a)) {
                goals.push_back(Cell{relay->layer, relay->tx, relay->ty});
                return goals;
            }
        }
    }
    add(target.x, target.y);
    static const int kAround[8][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}, {1, -1}, {1, 1}, {-1, 1}, {-1, -1}};
    for (const auto& d : kAround) add(target.x + d[0], target.y + d[1]);
    return goals;
}

/// Live blockers for an explicit route: NPCs and closed objects with collision, except on
/// the target's own cell(s). Non-world maps only.
std::vector<Cell> live_blockers(const baked::Submap& sm, const std::vector<Cell>& goals)
{
    std::vector<Cell> out;
    if (sm.world) return out;
    for (void* fe : all_field_entities()) {
        if (!live_entity(fe) || !entity_is_active(fe)) continue;
        const bool npc = r::kFieldNonPlayer.is(fe);
        const bool chest = r::kFieldTresureBox.is(fe);
        if (!npc && !chest) continue;
        void* tr = unity::transform(fe);
        if (!tr) continue;
        Cell c = cell_of(sm, unity::local_position(tr), -1);
        bool is_goal = false;
        for (const Cell& goal : goals)
            if (goal.x == c.x && goal.y == c.y) is_goal = true;
        if (!is_goal) out.push_back(c);
    }
    return out;
}

// --- reachability cache (Quick mode) --------------------------------------------------------
struct ReachCache {
    const baked::Submap* submap = nullptr;
    Cell start;
    bool passages = false;
    int vehicle_group = 0;
    std::vector<int> auto_groups;
    baked_route::Reach reach;
};
ReachCache* g_cache = nullptr;

const baked_route::Reach& cached_flood(const baked_route::Request& req)
{
    if (!g_cache) g_cache = new ReachCache();
    if (g_cache->submap != req.submap || !(g_cache->start == req.start) || g_cache->passages != req.passages ||
        g_cache->vehicle_group != req.vehicle_group || g_cache->auto_groups != req.auto_groups) {
        g_cache->submap = req.submap;
        g_cache->start = req.start;
        g_cache->passages = req.passages;
        g_cache->vehicle_group = req.vehicle_group;
        g_cache->auto_groups = req.auto_groups;
        g_cache->reach = baked_route::flood(req);
    }
    return g_cache->reach;
}

bool any_reachable(const baked_route::Reach& reach, const std::vector<Cell>& goals)
{
    for (const Cell& goal : goals)
        if (reach.reachable(goal)) return true;
    return false;
}

/// The player's movement rules on a world map now: riding a vehicle (its group) or walking
/// with the owned auto-vehicles.
void world_mode(baked_route::Request& req)
{
    req.vehicle_group = 0;
    req.auto_groups.clear();
    if (!req.submap || !req.submap->world) return;
    VehicleState v = current_vehicle();
    if (v.valid && v.type != 1 && v.type != 0) {
        if (const baked::TransportRow* row = baked::transport(v.id)) {
            if (row->type != kTransportContent) {
                req.vehicle_group = row->group;
                return;
            }
        }
    }
    req.auto_groups = owned_auto_vehicle_groups();
}

std::vector<Vec3> world_path_of(const baked::Submap& sm, const std::vector<Cell>& cells)
{
    std::vector<Vec3> out;
    out.reserve(cells.size());
    for (const Cell& c : cells) out.push_back(world_of_cell(sm, c));
    return out;
}

/// Full mode on a world map, on foot, when walking (with the auto-vehicles) cannot reach the
/// target: board a parked ship, submarine or airship. Walk to a cell beside the vehicle, ride
/// to a cell from which the target's walkable area is entered (a ship: a water cell beside a
/// land cell of that area the ship may stop at; an airship: a land cell of that area it may
/// land on), then walk. The shortest total over the vehicles the party has on this map.
bool boarding_route(const baked::Submap& sm, const baked::Grids& g, const baked_route::Request& walk,
                    const std::vector<Cell>& goals, std::vector<Phase>& phases)
{
    std::vector<VehicleOnMap> vehicles = vehicles_on_map();
    if (vehicles.empty()) return false;

    // The target's own walkable area (where the last leg walks).
    baked_route::Request from_target = walk;
    from_target.goals.clear();
    from_target.blocked.clear();
    from_target.start = goals.front();
    baked_route::Reach land = baked_route::flood(from_target);

    int best_total = 1 << 30;
    std::vector<Phase> best;
    for (const VehicleOnMap& v : vehicles) {
        const baked::TransportRow* row = baked::transport(v.transportation_id);
        if (!row || row->type == kTransportContent || row->type == 1) continue;
        const int group = row->group;
        if (group < 1 || group > static_cast<int>(g.transport.size())) continue;
        const bool sails = row->type == kTransportShip || row->type == kTransportSubmarine;
        Cell vcell = cell_of(sm, v.pos, walk.start.layer);
        if (!baked::in_bounds(g, vcell.x, vcell.y)) continue;

        // 1. Walk to a cell beside the vehicle.
        baked_route::Request to_vehicle = walk;
        to_vehicle.goals.clear();
        static const int kSide[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
        for (const auto& d : kSide) {
            Cell c{walk.start.layer, vcell.x + d[0], vcell.y + d[1]};
            if (baked_route::open(walk, g, c)) to_vehicle.goals.push_back(c);
        }
        if (to_vehicle.goals.empty()) continue;
        baked_route::Route leg1 = baked_route::find(to_vehicle);
        if (!leg1.found) continue;

        // 2. Ride to where the target's area is entered.
        baked_route::Request ride;
        ride.submap = &sm;
        ride.vehicle_group = group;
        ride.start = vcell;
        ride.goals_open = false;
        for (int y = 0; y < g.height; ++y) {
            for (int x = 0; x < g.width; ++x) {
                Cell here{walk.start.layer, x, y};
                if (!land.reachable(here)) continue;
                const int attr = baked::attribute(g, x, y);
                if (sails) {
                    // A water cell beside this land cell, where the ship may stop.
                    for (const auto& d : kSide) {
                        Cell w{walk.start.layer, x + d[0], y + d[1]};
                        if (!baked::vehicle_open(g, group, w.x, w.y)) continue;
                        if (row->landing_group > 0 && !baked::group_lands(row->landing_group, attr)) continue;
                        ride.goals.push_back(w);
                    }
                } else {
                    if (!baked::vehicle_open(g, group, x, y)) continue;
                    if (row->landing_group > 0 && !baked::group_lands(row->landing_group, attr)) continue;
                    ride.goals.push_back(here);
                }
            }
        }
        if (ride.goals.empty()) continue;
        baked_route::Route leg2 = baked_route::find(ride);
        if (!leg2.found) continue;

        // 3. Walk from the landing to the target. A ship's stop is a water cell: the first step
        //    of this leg walks off it onto the land cell beside it (a start cell is never
        //    tested for openness, only the cells entered).
        baked_route::Request last = walk;
        last.blocked.clear();
        last.start = leg2.cells.back();
        baked_route::Route leg3 = baked_route::find(last);
        if (!leg3.found) continue;

        const int total = leg1.steps() + leg2.steps() + leg3.steps();
        if (total >= best_total) continue;
        best_total = total;
        best.clear();
        std::wstring name = vehicle_display_name(v.transportation_id, v.type, v.message_id);
        if (name.empty()) name = T(L"Vehicle");
        Phase p1;
        p1.cells = leg1.cells;
        Phase p2;
        p2.cells = leg2.cells;
        p2.action_before = TF(L"board the {0}", {name});
        Phase p3;
        p3.cells = leg3.cells;
        if (!sails) p3.action_before = T(L"land");
        best.push_back(p1);
        best.push_back(p2);
        best.push_back(p3);
    }
    if (best.empty()) return false;
    phases = best;
    return true;
}

} // namespace

bool available(void* map_handle) { return current_submap(map_handle) != nullptr; }

bool player_cell(void* map_handle, void* player, Cell& out)
{
    const baked::Submap* sm = current_submap(map_handle);
    if (!sm || !player) return false;
    void* tr = unity::transform(player);
    if (!tr) return false;
    out = cell_of(*sm, unity::local_position(tr), sm->world ? 1 : player_layer(player));
    return true;
}

bool cell_at(void* map_handle, Vec3 world, Cell& out)
{
    const baked::Submap* sm = current_submap(map_handle);
    if (!sm) return false;
    out = cell_of(*sm, world, 1);
    return true;
}

Vec3 world_of(void* map_handle, const Cell& cell)
{
    const baked::Submap* sm = current_submap(map_handle);
    return sm ? world_of_cell(*sm, cell) : Vec3{};
}

std::vector<Cell> goal_cells(void* map_handle, Vec3 target)
{
    const baked::Submap* sm = current_submap(map_handle);
    if (!sm) return {};
    return goals_for(*sm, baked::grids(*sm), cell_of(*sm, target, 1));
}

std::wstring describe(const std::vector<Phase>& phases)
{
    std::vector<std::wstring> parts;
    size_t total_legs = 0;
    int total_steps = 0;
    for (const Phase& p : phases) {
        total_legs += baked_route::legs(p.cells).size();
        total_steps += p.cells.empty() ? 0 : static_cast<int>(p.cells.size()) - 1;
    }
    // A route the game's searcher could have given is spoken whole (FF1); a longer one is
    // capped as FF5 capped its chained routes: the player walks what was heard and asks again.
    const size_t cap = total_steps > kOldWindowSteps ? kMaxSpokenLegs : total_legs;
    size_t spoken = 0;
    for (const Phase& p : phases) {
        if (!p.action_before.empty()) parts.push_back(p.action_before);
        for (const baked_route::Leg& leg : baked_route::legs(p.cells)) {
            if (spoken >= cap) break;
            parts.push_back(T(direction_key(leg.dir)) + L" " + text::num(leg.count));
            ++spoken;
        }
        if (spoken >= cap) break;
    }
    if (parts.empty()) return T(L"No movement needed");
    std::wstring out = text::join(parts, L", ");
    if (total_legs > spoken) out += L", " + TF(L"and {0} more", {text::num(static_cast<long long>(total_legs - spoken))});
    return out;
}

void invalidate()
{
    if (g_cache) g_cache->submap = nullptr;
}

bool find(Vec3 from, Vec3 to, void* map_handle, void* player, bool full, PathInfo& out)
{
    const baked::Submap* sm = current_submap(map_handle);
    if (!sm) return false;
    return hooks::guarded_r("BakedNav.Find", false, [&] {
        const baked::Grids& g = baked::grids(*sm);
        if (g.width <= 0 || g.height <= 0) return false;

        baked_route::Request req;
        req.submap = sm;
        req.start = cell_of(*sm, from, sm->world || !player ? 1 : player_layer(player));
        req.goals = goals_for(*sm, g, cell_of(*sm, to, 1));
        world_mode(req);
        if (!baked::in_bounds(g, req.start.x, req.start.y) || req.goals.empty()) {
            out.error = L"No path found";
            return true;   // handled: the game's searcher would not do better off the grid
        }

        if (!full) {
            // Quick: one flood per player cell; passages as a second flood only when needed.
            bool ok = any_reachable(cached_flood(req), req.goals);
            if (!ok && game::flags().has_hidden_passages && !sm->world) {
                baked_route::Request with = req;
                with.passages = true;
                ok = any_reachable(baked_route::flood(with), req.goals);
            }
            out.success = ok;
            if (!ok) out.error = L"No path found";
            return true;
        }

        req.blocked = live_blockers(*sm, req.goals);
        baked_route::Route route = baked_route::find(req);
        if (!route.found && !req.blocked.empty()) {
            // An NPC standing in the only corridor: route as if it were not there (it moves).
            baked_route::Request unblocked = req;
            unblocked.blocked.clear();
            route = baked_route::find(unblocked);
        }
        if (!route.found && game::flags().has_hidden_passages && !sm->world) {
            baked_route::Request with = req;
            with.passages = true;
            with.blocked.clear();
            route = baked_route::find(with);
        }
        std::vector<Phase> phases;
        if (route.found) {
            Phase p;
            p.cells = route.cells;
            phases.push_back(p);
        } else if (sm->world && req.vehicle_group == 0 && boarding_route(*sm, g, req, req.goals, phases)) {
            // boarding found a way
        } else {
            out.error = L"No path found";
            return true;
        }
        out.success = true;
        out.world_path.clear();
        out.step_count = 0;
        for (const Phase& p : phases) {
            std::vector<Vec3> w = world_path_of(*sm, p.cells);
            out.world_path.insert(out.world_path.end(), w.begin(), w.end());
            out.step_count += p.cells.empty() ? 0 : static_cast<int>(p.cells.size()) - 1;
        }
        out.description = describe(phases);
        return true;
    });
}

} // namespace field::baked_nav
