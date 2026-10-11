// The game-side glue of the baked-map router: see baked_nav.h.
//
// Cells: the game's MapRouteSearcher cell space, cx = floor(w/2 + x/16), cy = floor(h/2 - y/16)
// (FF1 FindPathTo; w, h = the sub-map's size in tiles), the player's layer = its game object's
// Unity layer - 9 (Bottom 0, Ground 1, Upper 2). A world map has no collision grid: the baked
// sub-map's size is used (the live IMapAccessor reports no collision size there).
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <climits>
#include <unordered_map>
#include "baked_nav.h"
#include "baked_map.h"
#include "field_internal.h"
#include "map_names.h"
#include "map_state.h"
#include "../game.h"
#include "../hooks.h"
#include "../log.h"
#include "../frame.h"
#include "../modtext.h"
#include "../say.h"
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
ref::Class kCollider{"Last.Entity.Field", "FieldColliderEntity"};
ref::Field fColliderEnable{kCollider, "enable"};
ref::Field fColliderId{kCollider, "collisionId"};
ref::Class kTrigger{"Last.Entity.Field", "EventTriggerEntity"};
ref::Field fTriggerPower{kTrigger, "<TriggerPower>k__BackingField"};
void invalidate_active();
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

void world_mode(baked_route::Request& req);

baked_route::GoalKind goal_kind(TargetKind kind)
{
    switch (kind) {
    case TargetKind::Interact: return baked_route::GoalKind::Interact;
    case TargetKind::Transition: return baked_route::GoalKind::Transition;
    case TargetKind::Spot: return baked_route::GoalKind::Spot;
    default: return baked_route::GoalKind::Infer;
    }
}

/// The goals for `target` under the player's movement rules now (baked_router.h goals_for).
baked_route::Goals goals_for(const baked::Submap& sm, const Cell& target, TargetKind kind)
{
    baked_route::Request movement;
    movement.submap = &sm;
    movement.passages = game::flags().has_hidden_passages;
    world_mode(movement);
    return baked_route::goals_for(movement, target, goal_kind(kind));
}

/// Direction to the interaction from the route's arrival cell.
std::wstring facing_text(const std::vector<Cell>& cells, const baked_route::Goals& goals)
{
    if (!goals.face || cells.empty() || goals.faced.empty()) return L"";
    const Cell& last = cells.back();
    const Cell* nearest = nullptr;
    int best = 1 << 30;
    for (const Cell& c : goals.faced) {
        const int d = std::abs(c.x - last.x) + std::abs(c.y - last.y);
        if (d < best) {
            best = d;
            nearest = &c;
        }
    }
    const int dir = baked_route::direction_towards(last, *nearest);
    if (dir < 0) return L"";
    return TF(L"face {0}", {T(direction_key(dir))});
}

/// Live obstacles for an explicit route, by what they are (user, 2026-10-11: say why a route
/// is blocked): a chest is a wall; a collision entity or a collision object that is active now
/// is a story gate (FF1's bridge before it is built, a locked door, "no entry until after the
/// event"); an NPC that stands still is a wall while it stands there; a wandering NPC is a
/// passing obstacle. Cells the route may end on are never obstacles. World maps: gates only.
struct Blockers {
    std::vector<Cell> chests, gates, fixed_npcs, moving_npcs, triggers;
    std::vector<baked_route::Request::Directional> directional;
    std::vector<Cell> hard(bool with_gates = true) const
    {
        std::vector<Cell> out = chests;
        if (with_gates) out.insert(out.end(), gates.begin(), gates.end());
        out.insert(out.end(), fixed_npcs.begin(), fixed_npcs.end());
        out.insert(out.end(), moving_npcs.begin(), moving_npcs.end());
        return out;
    }
};
constexpr int kObjCollisionEntity = 19, kObjEntity = 5, kObjAnimEntity = 10;   // MapConstants.ObjectType
constexpr int kFlagCollision = 1, kFlagWanders = 16;                           // bake field d

const baked::Object* baked_by_entity_id(const baked::Grids& g, int entity_id, baked::ObjType type, const Cell& live)
{
    if (entity_id <= 0) return nullptr;
    const baked::Object* best = nullptr;
    int nearest = INT_MAX;
    for (const baked::Object& o : g.objects)
        if (o.type == type && o.entity_id() == entity_id) {
            const int d = std::abs(o.tx - live.x) + std::abs(o.ty - live.y);
            if (d < nearest) { best = &o; nearest = d; }
        }
    return best;
}

Blockers live_blockers(const baked::Submap& sm, const baked::Grids& g, const std::vector<Cell>& goals)
{
    Blockers out;
    auto is_goal = [&](int x, int y) {
        for (const Cell& c : goals)
            if (c.x == x && c.y == y) return true;
        return false;
    };
    auto add_footprint = [&](std::vector<Cell>& into, const baked::Object* o, const Cell& live, bool exclude_goals = true) {
        if (!o) {
            if (!exclude_goals || !is_goal(live.x, live.y)) into.push_back(live);
            return;
        }
        for (int y = o->ty; y < o->ty + o->th; ++y)
            for (int x = o->tx; x < o->tx + o->tw; ++x)
                if (!exclude_goals || !is_goal(x, y)) into.push_back(Cell{live.layer, x, y});
    };
    for (void* fe : all_field_entities()) {
        if (!live_entity(fe) || !entity_is_active(fe)) continue;
        void* tr = unity::transform(fe);
        if (!tr) continue;
        void* property = entity_property(fe);
        const int type = object_type_of(property);
        const int id = property && r::fPropEntityId.get() >= 0 ? ref::rd<int32_t>(property, r::fPropEntityId) : 0;
        Cell c = cell_of(sm, unity::local_position(tr), game_object_layer(fe) - 9);
        if (c.layer < 0 || c.layer > 2 || sm.world) c.layer = -1;
        if (type == kObjCollisionEntity) {
            if (kCollider.is(fe) && !ref::rd<bool>(fe, fColliderEnable)) continue;
            void* transports = property ? ref::rd<void*>(property, r::fPropTargetTransportationIdList) : nullptr;
            const VehicleState vehicle = current_vehicle();
            bool applies = unity::list_count(transports) == 0;
            for (int i = 0; i < unity::list_count(transports); ++i)
                if (unity::list_value<int32_t>(transports, i) == (vehicle.valid ? vehicle.id : 1)) applies = true;
            if (!applies) continue;
            const auto* o = baked_by_entity_id(g, id, baked::ObjType::CollisionEntity, c);
            const int collision_type = kCollider.is(fe) ? ref::rd<int32_t>(fe, fColliderId) : (o ? o->a : 0);
            std::vector<Cell> cells;
            add_footprint(cells, o, c, false);
            for (const Cell& at : cells) {
                if (collision_type == 6 || collision_type == 7) out.directional.push_back({at, collision_type == 6 ? 1 : 2});
                else out.gates.push_back(at);
            }
        } else if (kTrigger.is(fe) && !entity_is_scenery(fe) && ref::rd<bool>(fe, fTriggerPower)) {
            const auto* o = baked_by_entity_id(g, id, baked::ObjType::Event, c);
            if (!o) o = baked_by_entity_id(g, id, baked::ObjType::Warp, c);
            add_footprint(out.triggers, o, c, false);
        } else if (type == kObjEntity || type == kObjAnimEntity) {
            const baked::Object* o = baked_by_entity_id(g, id, type == kObjEntity ? baked::ObjType::Entity : baked::ObjType::AnimEntity, c);
            if (o && (o->d & kFlagCollision)) add_footprint(out.gates, o, c, false);
        } else if (sm.world) {
            continue;
        } else if (r::kFieldTresureBox.is(fe)) {
            out.chests.push_back(c);
        } else if (r::kFieldNonPlayer.is(fe)) {
            const baked::Object* o = baked_by_entity_id(g, id, baked::ObjType::NPC, c);
            if (!o) o = baked_by_entity_id(g, id, baked::ObjType::ShopNPC, c);
            if (o && (o->d & 2)) c.layer = -1;
            const bool wanders = !o || (o->d & kFlagWanders);
            (wanders ? out.moving_npcs : out.fixed_npcs).push_back(c);
        }
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

/// A goal counts as reached when the flood reaches it or a cell beside it: a goal cell closed
/// on the grid (a town tile, an exit on a wall) is entered from its neighbour, as the router
/// enters it, and the cached flood was made for another target's goals.
bool any_reachable(const baked_route::Reach& reach, const std::vector<Cell>& goals)
{
    static const int kSide[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    for (const Cell& goal : goals) {
        if (reach.reachable(goal)) return true;
        for (const auto& s : kSide)
            if (reach.reachable(Cell{goal.layer, goal.x + s[0], goal.y + s[1]})) return true;
    }
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

/// The vehicle that would carry the player to the goals on a world map: the cheapest kind
/// first (the auto-ridden canoe or snowcraft, then a ship or submarine, then an airship). A
/// vehicle the party has (its auto-vehicles, the vehicles parked on this map) is named with
/// the game's own name; one it does not have yet is "vehicle" (user, 2026-10-11: name only
/// owned vehicles; "Requires vehicle" means the player does not own it yet). Empty when none
/// reaches the goals.
std::wstring vehicle_required(const baked::Grids& g, const baked_route::Request& req)
{
    std::vector<int> owned = owned_auto_vehicle_groups();
    for (const VehicleOnMap& v : vehicles_on_map())
        if (const baked::TransportRow* row = baked::transport(v.transportation_id)) owned.push_back(row->group);
    auto rank = [](int type) {
        switch (type) {
        case kTransportContent: return 0;
        case kTransportShip: return 1;
        case kTransportSubmarine: return 2;
        default: return 3;
        }
    };
    std::vector<const baked::TransportRow*> rows;
    for (const baked::TransportRow& row : baked::transports())
        if (row.type != 0 && row.type != 1 && row.group > 1 && row.group <= static_cast<int>(g.transport.size()))
            rows.push_back(&row);
    std::stable_sort(rows.begin(), rows.end(), [&](const baked::TransportRow* a, const baked::TransportRow* b) {
        return rank(a->type) < rank(b->type);
    });
    std::vector<int> tried;
    for (const baked::TransportRow* row : rows) {
        if (std::find(tried.begin(), tried.end(), row->group) != tried.end()) continue;
        tried.push_back(row->group);
        baked_route::Request with = req;
        with.blocked.clear();
        with.auto_groups.push_back(row->group);
        if (any_reachable(baked_route::flood(with), req.goals)) {
            if (std::find(owned.begin(), owned.end(), row->group) == owned.end()) return T(L"vehicle");
            std::wstring name = vehicle_display_name(row->id, row->type, baked::str(row->message));
            return name.empty() ? T(L"vehicle") : name;
        }
    }
    return L"";
}

/// Why no route was found, when the router knows: the story gates alone in the way, else the
/// NPCs that stand still, else (a world map) the vehicle that would reach the target.
std::wstring no_route_reason(const baked::Submap& sm, const baked::Grids& g, const baked_route::Request& req, const Blockers& live)
{
    if (!live.gates.empty() || !live.directional.empty() || !live.fixed_npcs.empty() || !live.moving_npcs.empty()) {
        baked_route::Request without = req;
        without.blocked = live.hard(false);
        without.directional.clear();
        if ((!live.gates.empty() || !live.directional.empty()) && baked_route::find(without).found) return T(L"Story gated");
        without.blocked = live.chests;
        without.blocked.insert(without.blocked.end(), live.gates.begin(), live.gates.end());
        without.directional = req.directional;
        if ((!live.fixed_npcs.empty() || !live.moving_npcs.empty()) && baked_route::find(without).found) return T(L"Blocked by an NPC");
    }
    if (sm.world && req.vehicle_group == 0) {
        const std::wstring vehicle = vehicle_required(g, req);
        if (!vehicle.empty()) return TF(L"Requires {0}", {vehicle});
    }
    return L"";
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
    return goals_for(*sm, cell_of(*sm, target, 1), TargetKind::Infer).cells;
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
    for (size_t pi = 0; pi < phases.size(); ++pi) {
        const Phase& p = phases[pi];
        if (!p.action_before.empty()) parts.push_back(p.action_before);
        bool whole = true;
        for (const baked_route::Leg& leg : baked_route::legs(p.cells)) {
            if (spoken >= cap) {
                whole = false;
                break;
            }
            parts.push_back(text::num(leg.count) + L" " + T(direction_key(leg.dir)));
            ++spoken;
        }
        if (!whole) break;
        if (!p.action_after.empty()) parts.push_back(p.action_after);
        if (route_progress::at_interaction(phases, pi) && !p.arrival_facing.empty()) parts.push_back(p.arrival_facing);
    }
    if (parts.empty()) return T(L"No movement needed");
    std::wstring out = text::join(parts, L", ");
    if (total_legs > spoken) out += L", " + TF(L"and {0} more", {text::num(static_cast<long long>(total_legs - spoken))});
    return out;
}

void invalidate()
{
    if (g_cache) g_cache->submap = nullptr;
    invalidate_active();
}

// --- across maps -----------------------------------------------------------------------------
namespace {

/// An exit or warp of a sub-map, as the cross-map graph sees it.
struct Edge {
    const baked::Object* object;
    int dest_map_id;
    int dest_point;
    bool warp;        // a script warp (step-on or examined), else a GotoMap exit
};

std::vector<Edge> edges_of(const baked::Grids& g)
{
    std::vector<Edge> out;
    for (const baked::Object& o : g.objects) {
        if (o.type == baked::ObjType::GotoMap && o.a > 0) out.push_back(Edge{&o, o.a, o.b, false});
        else if (o.type == baked::ObjType::Warp && o.a > 0) out.push_back(Edge{&o, o.a, o.b, true});
    }
    return out;
}

/// The PointIn of a sub-map with this point id (the arrival cell of an exit), or null.
const baked::Object* point_in(const baked::Submap& sm, int point, map_state::Resolver& state)
{
    const auto& g = baked::grids(sm);
    const baked::Object* first = nullptr;
    for (const baked::Object& o : g.objects) {
        if (o.type != baked::ObjType::PointIn) continue;
        if (!state.active(sm, o)) continue;
        if (o.a == point) return &o;
        if (!first) first = &o;
    }
    return point <= 0 ? first : nullptr;
}

/// Distance from a flood to the nearest cell from which `o` is entered: the object's own
/// cells, or (a closed exit cell, a town tile) a neighbouring open cell plus one.
int distance_to_object(const baked_route::Request& request, const baked_route::Reach& reach, const baked::Object& o, int layer, bool world)
{
    int best = -1;
    for (int y = o.ty; y < o.ty + o.th; ++y) {
        for (int x = o.tx; x < o.tx + o.tw; ++x) {
            const int l = world ? layer : o.layer;
            bool blocked = false;
            for (const Cell& c : request.blocked)
                if (c.x == x && c.y == y && (c.layer < 0 || c.layer == l)) { blocked = true; break; }
            if (blocked) continue; // A closed exit is enterable; a solid NPC/gate over it is not.
            int d = reach.distance(Cell{l, x, y});
            if (d < 0) {
                static const int kSide[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
                for (const auto& s : kSide) {
                    if (!baked_route::direction_allowed(request, Cell{l, x+s[0], y+s[1]}, Cell{l,x,y})) continue;
                    const int dn = reach.distance(Cell{l, x + s[0], y + s[1]});
                    if (dn >= 0 && (d < 0 || dn + 1 < d)) d = dn + 1;
                }
            }
            if (d >= 0 && (best < 0 || d < best)) best = d;
        }
    }
    return best;
}

baked_route::Request walk_request(const baked::Submap& sm, const Cell& start)
{
    baked_route::Request req;
    req.submap = &sm;
    req.start = start;
    req.passages = game::flags().has_hidden_passages;
    if (sm.world) req.auto_groups = owned_auto_vehicle_groups();
    return req;
}

constexpr int kMaxVisitedMaps = 400;

} // namespace

std::wstring map_name_of(int map_id) { return resolve_map_name_by_id(map_id); }

CrossLeg find_across(void* map_handle, void* player, const MapGoal& goal, bool with_gates)
{
    CrossLeg out;
    const baked::Submap* here = current_submap(map_handle);
    const baked::Submap* there = baked::submap_by_map_id(goal.map_id);
    if (!here || !there || !player) return out;
    return hooks::guarded_r("BakedNav.FindAcross", out, [&] {
        Cell start;
        if (!player_cell(map_handle, player, start)) return out;
        const baked::Grids& gh = baked::grids(*here);
        if (!baked::in_bounds(gh, start.x, start.y)) return out;
        // The live obstacles of this map (only this map has live state): a story gate closes
        // the cells it stands on, and an exit it stands on is sealed (Kashuan Keep's door).
        const Blockers live = live_blockers(*here, gh, {});
        map_state::Resolver state;
        auto sealed = [&](const baked::Object& o) {
            if (with_gates) for (const Cell& c : live.gates)
                if (o.contains(c.x, c.y)) return true;
            return false;
        };

        // The goal's cells on its own map.
        std::vector<Cell> goal_cells = goal.cells;
        if (goal_cells.empty()) {
            const baked::Grids& gt = baked::grids(*there);
            Cell gc = goal.cell;
            if (goal.examine) {
                static const int kSide[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
                for (const auto& s : kSide) {
                    Cell c{gc.layer, gc.x + s[0], gc.y + s[1]};
                    if (baked::in_bounds(gt, c.x, c.y)) goal_cells.push_back(c);
                }
            }
            if (goal_cells.empty()) goal_cells.push_back(gc);
            if (there->world)
                for (Cell& c : goal_cells) c.layer = 1;
        }

        // Dijkstra over (sub-map, arrival cell): the whole map is flooded from the arrival once.
        struct State { int submap; Cell at; int cost; int parent; const baked::Object* via; int via_dest; };
        std::vector<State> states;
        std::vector<int> order;   // indices sorted by cost on each pop (small graphs)
        states.push_back(State{baked::submap_index(here), start, 0, -1, nullptr, 0});
        std::vector<bool> done;
        int goal_state = -1;
        int goal_steps = 0;
        int visited = 0;
        while (visited < kMaxVisitedMaps) {
            // pop the cheapest unfinished state
            int best = -1;
            for (size_t i = 0; i < states.size(); ++i) {
                if (i < done.size() && done[i]) continue;
                if (best < 0 || states[i].cost < states[static_cast<size_t>(best)].cost) best = static_cast<int>(i);
            }
            if (best < 0) break;
            if (done.size() < states.size()) done.resize(states.size(), false);
            done[static_cast<size_t>(best)] = true;
            ++visited;
            const State cur = states[static_cast<size_t>(best)];
            const baked::Submap* sm = baked::submap_at(cur.submap);
            if (!sm) continue;
            const baked::Grids& g = baked::grids(*sm);
            baked_route::Request req = walk_request(*sm, cur.at);
            if (sm == here) {
                req.blocked = live.hard(with_gates);
                if (with_gates) req.directional = live.directional;
                req.live_hazards = true;
                req.hazards = live.triggers;
            } else map_state::add_obstacles(state, *sm, req, with_gates);
            // Keep destinations behind a visible event available; warn on the actual leg.
            req.ignore_hazards = true;
            baked_route::Reach reach = baked_route::flood(req);

            if (sm == there) {
                int d = -1;
                for (const Cell& c : goal_cells) {
                    int dc = reach.distance(c);
                    if (dc < 0 && !goal.examine) {
                        // a closed goal cell (a chest, a switch): from beside it
                        static const int kSide[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
                        for (const auto& s : kSide) {
                            const int dn = reach.distance(Cell{c.layer, c.x + s[0], c.y + s[1]});
                            if (dn >= 0 && (dc < 0 || dn + 1 < dc)) dc = dn + 1;
                        }
                    }
                    if (dc >= 0 && (d < 0 || dc < d)) d = dc;
                }
                if (d >= 0) {
                    goal_state = best;
                    goal_steps = cur.cost + d;
                    break;
                }
            }
            for (const Edge& e : edges_of(g)) {
                if (!state.active(*sm, *e.object)) continue;
                const baked::Submap* dest = baked::submap_by_map_id(e.dest_map_id);
                if (!dest) continue;
                if (sm == here && sealed(*e.object)) continue;
                const int d = distance_to_object(req, reach, *e.object, cur.at.layer, sm->world);
                if (d < 0) continue;
                const baked::Object* arrival = point_in(*dest, e.dest_point, state);
                if (!arrival) continue;
                Cell at{arrival->layer, arrival->tx, arrival->ty};
                if (dest->world) at.layer = 1;
                const int cost = cur.cost + d + 1;
                // keep the cheapest arrival per (map, cell)
                bool known = false;
                for (size_t i = 0; i < states.size(); ++i) {
                    if (states[i].submap == baked::submap_index(dest) && states[i].at == at) {
                        known = true;
                        if (cost < states[i].cost && !(i < done.size() && done[i])) {
                            states[i].cost = cost;
                            states[i].parent = best;
                            states[i].via = e.object;
                            states[i].via_dest = e.dest_map_id;
                        }
                        break;
                    }
                }
                if (!known) states.push_back(State{baked::submap_index(dest), at, cost, best, e.object, e.dest_map_id});
            }
        }
        if (goal_state < 0) return out;

        // Walk back to the first hop taken from the start state.
        int first = goal_state;
        while (first >= 0 && states[static_cast<size_t>(first)].parent > 0) first = states[static_cast<size_t>(first)].parent;
        std::vector<Phase> phases;
        baked_route::Request req = walk_request(*here, start);
        if (goal_state == 0) {
            // On this map: route to the goal itself.
            req.goals = goal_cells;
            if (!goal.examine && goal.cells.empty()) {
                std::vector<Cell> around = goals_for(*here, goal.cell, TargetKind::Infer).cells;
                req.goals.insert(req.goals.end(), around.begin(), around.end());
            }
            req.blocked = live.hard(with_gates);
            if (with_gates) req.directional = live.directional;
            req.live_hazards = true;
            req.hazards = live.triggers;
            baked_route::Route route = baked_route::find(req);
            if (!route.found && !req.hazards.empty()) { req.ignore_hazards = true; route = baked_route::find(req); }
            if (!route.found) return out;
            Phase p;
            p.cells = route.cells;
            phases.push_back(p);
            out.on_this_map = true;
            out.phases = phases;
            out.description = describe(phases);
            if (req.ignore_hazards) out.description = T(L"Event trigger on the route") + L", " + out.description;
            out.cells = route.cells;
            out.steps_here = route.steps();
            out.steps_total = goal_steps;
            out.found = true;
            return out;
        }
        const State& hop = states[static_cast<size_t>(first)];
        if (!hop.via) return out;
        const baked::Object& o = *hop.via;
        for (int y = o.ty; y < o.ty + o.th; ++y)
            for (int x = o.tx; x < o.tx + o.tw; ++x)
                req.goals.push_back(Cell{here->world ? start.layer : o.layer, x, y});
        req.blocked = live.hard(with_gates);
        if (with_gates) req.directional = live.directional;
        req.live_hazards = true;
        req.hazards = live.triggers;
        baked_route::Route route = baked_route::find(req);
        if (!route.found && !req.hazards.empty()) { req.ignore_hazards = true; route = baked_route::find(req); }
        if (!route.found) return out;
        Phase p;
        p.cells = route.cells;
        std::wstring dest_name = map_name_of(hop.via_dest);
        if (o.type == baked::ObjType::Warp) {
            p.action_after = o.d == 1 ? T(L"step on the warp") : T(L"examine the warp");
        } else {
            p.action_after = dest_name.empty() ? T(L"take the exit") : TF(L"take the exit to {0}", {dest_name});
        }
        phases.push_back(p);
        out.found = true;
        out.on_this_map = false;
        out.next_map_id = hop.via_dest;
        out.cells = route.cells;
        out.phases = phases;
        out.description = describe(phases);
        if (req.ignore_hazards) out.description = T(L"Event trigger on the route") + L", " + out.description;
        out.steps_here = route.steps();
        out.steps_total = goal_steps;
        return out;
    });
}

// --- the active route (user, 2026-10-11) -----------------------------------------------------
// A route is planned once and followed to its end. Asking again for the same target while
// standing on the planned route speaks the rest of that route from the player's cell; the
// search runs again only for another target, another map, or a player who has left the route.
// Before this every query re-planned from the current cell, and a villager standing in a
// street at planning time (a live blocker) flipped the plan between the two ways round a
// block mid-walk: "West 4, South 10, East 4 ..." and four steps later "East 4, North 1, East 2".
namespace {

struct ActiveRoute {
    bool valid = false;
    int map_id = 0;               // the map the route was planned on (the player's)
    Cell target;                  // the target cell the route was planned to (x, y)
    int goal_map_id = 0;          // the target's map: map_id, or another floor (route_to_goal)
    int next_map_id = 0;          // the route goes on there after its last leg (0 = ends at the target)
    std::vector<Phase> phases;
};
ActiveRoute* g_active = nullptr;
void invalidate_active() { if (g_active) g_active->valid = false; }

bool same_target(const ActiveRoute& r, int map_id, const Cell& target, int goal_map_id)
{
    return r.valid && r.map_id == map_id && r.goal_map_id == goal_map_id && r.target.x == target.x && r.target.y == target.y;
}

void set_active(int map_id, const Cell& target, int goal_map_id, const std::vector<Phase>& phases, int next_map_id)
{
    if (!g_active) g_active = new ActiveRoute();
    g_active->valid = true;
    g_active->map_id = map_id;
    g_active->target = target;
    g_active->goal_map_id = goal_map_id;
    g_active->next_map_id = next_map_id;
    g_active->phases = phases;
}

/// The rest of the active route from the player's cell, when the cell lies on it (the last
/// occurrence, nearest the goal). A phase's action word is kept only at the phase's first cell.
bool remaining_from(const ActiveRoute& r, const Cell& at, std::vector<Phase>& out)
{
    return route_progress::remaining(r.phases, at, out);
}

void fill_result(const baked::Submap& sm, const std::vector<Phase>& phases, PathInfo& out)
{
    out.success = true;
    out.error.clear();
    out.world_path.clear();
    out.step_count = 0;
    for (const Phase& p : phases) {
        std::vector<Vec3> w = world_path_of(sm, p.cells);
        out.world_path.insert(out.world_path.end(), w.begin(), w.end());
        out.step_count += p.cells.empty() ? 0 : static_cast<int>(p.cells.size()) - 1;
    }
    out.description = describe(phases);
}

} // namespace

bool find(Vec3 from, Vec3 to, void* map_handle, void* player, bool full, TargetKind kind, bool with_vehicles, PathInfo& out)
{
    const baked::Submap* sm = current_submap(map_handle);
    if (!sm) return false;
    return hooks::guarded_r("BakedNav.Find", false, [&] {
        const baked::Grids& g = baked::grids(*sm);
        if (g.width <= 0 || g.height <= 0) return false;

        baked_route::Request req;
        req.submap = sm;
        req.start = cell_of(*sm, from, sm->world || !player ? 1 : player_layer(player));
        const Cell target = cell_of(*sm, to, 1);
        const baked_route::Goals goals = goals_for(*sm, target, kind);
        req.goals = goals.cells;
        world_mode(req);
        if (!baked::in_bounds(g, req.start.x, req.start.y) || req.goals.empty()) {
            out.error = L"No path found";
            return true;   // handled: the game's searcher would not do better off the grid
        }

        if (!full) {
            // Events are actionable obstacles, not reasons to hide every destination beyond
            // them. Explicit routing evaluates active triggers and gives the warning.
            req.ignore_hazards = true;
            // "Current map with vehicles": the vehicles the party has on this map carry it too
            // (their groups join walking, as the auto-ridden canoe does).
            if (with_vehicles && sm->world && req.vehicle_group == 0) {
                for (const VehicleOnMap& v : vehicles_on_map())
                    if (const baked::TransportRow* row = baked::transport(v.transportation_id))
                        if (row->type != kTransportContent && row->type != 1 && row->group > 1) req.auto_groups.push_back(row->group);
            }
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

        const Blockers live = live_blockers(*sm, g, req.goals);
        req.blocked = live.hard();
        req.directional = live.directional;
        req.live_hazards = true;
        req.hazards = live.triggers;
        auto annotate_events = [&](const std::vector<Phase>& phases) {
            for (const Phase& p : phases) for (const Cell& c : p.cells) for (const Cell& event : live.triggers)
                if (c.x == event.x && c.y == event.y && (event.layer < 0 || c.layer == event.layer)) {
                    out.description = T(L"Event trigger on the route") + L", " + out.description;
                    return;
                }
        };
        // The route already planned to this target, while the player is still on it.
        const int map_id = current_map_id();
        if (g_active && same_target(*g_active, map_id, target, map_id)) {
            std::vector<Phase> rest;
            if (remaining_from(*g_active, req.start, rest)) {
                bool blocked = false;
                for (const Phase& phase : rest) for (size_t i = 1; i < phase.cells.size(); ++i) {
                    if (!baked_route::direction_allowed(req, phase.cells[i-1], phase.cells[i])) blocked = true;
                    for (const Cell& obstacle : req.blocked) {
                        const Cell& c = phase.cells[i];
                        if (c.x == obstacle.x && c.y == obstacle.y && (obstacle.layer < 0 || c.layer == obstacle.layer)) blocked = true;
                    }
                }
                if (blocked) g_active->valid = false;
                else {
                fill_result(*sm, rest, out);
                annotate_events(rest);
                out.next_map_id = g_active->next_map_id;
                LOGF("[BakedNav] route kept: at (%d,%d) L%d, %d steps left to (%d,%d)", req.start.x, req.start.y,
                     req.start.layer, out.step_count, target.x, target.y);
                return true;
                }
            }
        }

        // Every solid live NPC is a wall at its current position. Native Stamp movement
        // changes facing without leaving the tile (Dreadnought captains); even a travelling
        // NPC may not be crossed just because the real detour is longer than six cells.
        baked_route::Route route = baked_route::find(req);
        if (!route.found && game::flags().has_hidden_passages && !sm->world) {
            baked_route::Request with = req;
            with.passages = true;
            route = baked_route::find(with);
        }
        if (!route.found && !live.triggers.empty()) {
            auto through_events = req;
            through_events.ignore_hazards = true;
            if (game::flags().has_hidden_passages && !sm->world) through_events.passages = true;
            route = baked_route::find(through_events);
        }
        std::vector<Phase> phases;
        if (route.found) {
            Phase p;
            p.cells = route.cells;
            phases.push_back(p);
        } else if (sm->world && req.vehicle_group == 0 && boarding_route(*sm, g, req, req.goals, phases)) {
            // boarding found a way
        }
        std::wstring reason;
        int next_map = 0;
        if (phases.empty() && !sm->world) {
            // Cut off on this floor: through another part of the map and back (user,
            // 2026-10-11: Kashuan Keep's 2F stairs are reached through the other 1F part and
            // back in at its upper door). The leg ends at the exit to take; the player asks
            // again on the other side. When only a story gate (the sealed door) stands in the
            // way of that, say so.
            MapGoal mg;
            mg.map_id = map_id;
            mg.cell = target;
            mg.cells = req.goals;
            CrossLeg leg = find_across(map_handle, player, mg, true);
            if (leg.found && !leg.on_this_map) {
                phases = leg.phases;
                next_map = leg.next_map_id;
            } else if (!live.gates.empty() && find_across(map_handle, player, mg, false).found) {
                reason = T(L"Story gated");
            }
        }
        if (phases.empty()) {
            out.error = L"No path found";
            out.reason = reason.empty() ? no_route_reason(*sm, g, req, live) : reason;
            if (g_active && same_target(*g_active, map_id, target, map_id)) g_active->valid = false;
            LOGF("[BakedNav] no route: (%d,%d) L%d -> (%d,%d) on map %d, %zu goal cells; %zu chests, %zu gates, %zu fixed and %zu moving NPCs; %ls",
                 req.start.x, req.start.y, req.start.layer, target.x, target.y, map_id, req.goals.size(),
                 live.chests.size(), live.gates.size(), live.fixed_npcs.size(), live.moving_npcs.size(),
                 out.reason.empty() ? L"no reason known" : out.reason.c_str());
            return true;
        }
        if (phases.back().action_after.empty()) phases.back().arrival_facing = facing_text(phases.back().cells, goals);
        set_active(map_id, target, map_id, phases, next_map);
        fill_result(*sm, phases, out);
        annotate_events(phases);
        out.next_map_id = next_map;
        LOGF("[BakedNav] route planned: (%d,%d) L%d -> (%d,%d) on map %d: %d steps, %zu phase(s); %zu chests, %zu gates, %zu fixed and %zu moving NPCs",
             req.start.x, req.start.y, req.start.layer, target.x, target.y, map_id, out.step_count, phases.size(),
             live.chests.size(), live.gates.size(), live.fixed_npcs.size(), live.moving_npcs.size());
        return true;
    });
}

namespace {
double g_follow_failed_at = 0;
constexpr double kFollowRetrySeconds = 2.0;

/// The beacon's next corner on `rest` from `at`: the end of the first leg; a phase of one cell
/// (standing beside the vehicle to board) steers at the next phase's first cell.
void corner_of(const baked::Submap& sm, const std::vector<Phase>& rest, const Cell& at, Corner& out)
{
    int steps_left = 0;
    for (const Phase& p : rest) steps_left += p.cells.empty() ? 0 : static_cast<int>(p.cells.size()) - 1;
    Cell corner = at;
    int to_corner = 0;
    for (size_t pi = 0; pi < rest.size(); ++pi) {
        const Phase& p = rest[pi];
        if (p.cells.size() >= 2) {
            const std::vector<baked_route::Leg> legs = baked_route::legs(p.cells);
            to_corner = legs.empty() ? static_cast<int>(p.cells.size()) - 1 : legs.front().count;
            corner = p.cells[static_cast<size_t>(to_corner)];
            break;
        }
        if (pi + 1 < rest.size() && !rest[pi + 1].cells.empty()) {
            corner = rest[pi + 1].cells.front();
            to_corner = 1;
            break;
        }
    }
    out.found = true;
    out.arrived = steps_left == 0;
    out.point = world_of_cell(sm, corner);
    out.steps_to_corner = to_corner;
    out.steps_left = steps_left;
}
} // namespace

bool follow(void* map_handle, void* player, Vec3 target, TargetKind kind, Corner& out)
{
    out = Corner{};
    const baked::Submap* sm = current_submap(map_handle);
    if (!sm || !player) return false;
    return hooks::guarded_r("BakedNav.Follow", false, [&] {
        void* tr = unity::transform(player);
        if (!tr) return false;
        const Vec3 from = unity::local_position(tr);
        const Cell at = cell_of(*sm, from, sm->world ? 1 : player_layer(player));
        const Cell target_cell = cell_of(*sm, target, 1);
        const int map_id = current_map_id();
        std::vector<Phase> rest;
        bool have = g_active && same_target(*g_active, map_id, target_cell, map_id) && remaining_from(*g_active, at, rest);
        if (!have) {
            // No route yet, or the player has left it: plan one silently. A target without a
            // route is retried every two seconds, not on every ping.
            const double now = frame::now();
            if (g_follow_failed_at > 0 && now - g_follow_failed_at < kFollowRetrySeconds) return false;
            PathInfo info;
            if (!find(from, target, map_handle, player, true, kind, false, info) || !info.success) {
                g_follow_failed_at = now;
                return false;
            }
            g_follow_failed_at = 0;
            if (!g_active || !remaining_from(*g_active, at, rest)) return false;
        }
        corner_of(*sm, rest, at, out);
        return true;
    });
}

// --- other floors (user, 2026-10-11) ---------------------------------------------------------
// "Current map" lists what is on the other floors too, and a route to one of them (or to a
// target cut off on this floor) goes on after the transition: the destination is remembered and
// the next leg is spoken from where the player arrives, until a leg ends at it. Before this the
// player was told which exit to take and then lost the destination, which was no longer in the
// list on the other side.
namespace {

struct Floor { const baked::Submap* submap; int map_id; };

/// The sub-maps reachable from `here` through exits and warps without crossing a world map,
/// each with the map id it was reached by (the exit's, so the floor's own title), in
/// breadth-first order; `here` itself is not included.
std::vector<Floor> connected_floors(const baked::Submap* here, int here_map_id, map_state::Resolver& state)
{
    std::vector<Floor> out;
    if (!here || here->world) return out;
    std::vector<int> seen{baked::submap_index(here)};
    std::vector<Floor> queue{Floor{here, here_map_id}};
    for (size_t qi = 0; qi < queue.size() && queue.size() < static_cast<size_t>(kMaxVisitedMaps); ++qi) {
        const Floor f = queue[qi];
        for (const Edge& e : edges_of(baked::grids(*f.submap))) {
            if (!state.active(*f.submap, *e.object)) continue;
            const baked::Submap* dest = baked::submap_by_map_id(e.dest_map_id);
            if (!dest || dest->world) continue;
            const int idx = baked::submap_index(dest);
            if (std::find(seen.begin(), seen.end(), idx) != seen.end()) continue;
            seen.push_back(idx);
            queue.push_back(Floor{dest, e.dest_map_id});
            out.push_back(Floor{dest, e.dest_map_id});
        }
    }
    return out;
}

struct Pending {
    bool valid = false;
    MapGoal goal;
    TargetKind kind = TargetKind::Infer;
    std::wstring name;
};
Pending* g_pending = nullptr;

} // namespace

std::vector<RemoteObject> connected_objects(void* map_handle)
{
    std::vector<RemoteObject> out;
    const baked::Submap* here = current_submap(map_handle);
    if (!here || here->world) return out;
    return hooks::guarded_r("BakedNav.ConnectedObjects", out, [&] {
        map_state::Resolver state;
        const std::vector<Floor> floors = connected_floors(here, current_map_id(), state);
        auto in_group = [&](int map_id) {
            const baked::Submap* s = baked::submap_by_map_id(map_id);
            if (!s) return false;
            if (s == here) return true;
            for (const Floor& f : floors)
                if (f.submap == s) return true;
            return false;
        };
        for (const Floor& f : floors) {
            const baked::Grids& g = baked::grids(*f.submap);
            for (const baked::Object& o : g.objects) {
                if (!state.active(*f.submap, o) || state.collected(*f.submap, o)) continue;
                RemoteObject r;
                r.map_id = f.map_id;
                r.cell = Cell{o.layer, o.tx, o.ty};
                r.type = o.type;
                r.raw_name = baked::str(o.name);
                r.group = baked::str(o.group);
                switch (o.type) {
                case baked::ObjType::NPC:
                case baked::ObjType::ShopNPC:
                    if (!(o.d & (4 | 8))) continue;   // neither script nor action: scenery (the live rule)
                    r.shop = o.type == baked::ObjType::ShopNPC;
                    break;
                case baked::ObjType::TreasureBox:
                    r.content_id = o.chest_content_id();
                    break;
                case baked::ObjType::SavePoint:
                    break;
                case baked::ObjType::GotoMap:
                    if (o.a <= 0 || in_group(o.a)) continue;   // stairs between the group's own floors
                    r.dest_map_id = o.a;
                    break;
                default:
                    continue;
                }
                out.push_back(r);
            }
        }
        LOGF("[BakedNav] other floors of map %d: %zu floors, %zu objects", current_map_id(), floors.size(), out.size());
        return out;
    });
}

bool route_to_goal(void* map_handle, void* player, const MapGoal& goal_in, TargetKind kind, const std::wstring& name,
                   PathInfo& out)
{
    out = PathInfo{};
    const baked::Submap* here = current_submap(map_handle);
    const baked::Submap* there = baked::submap_by_map_id(goal_in.map_id);
    if (!here || !there || !player) {
        out.error = L"No path found";
        return false;
    }
    return hooks::guarded_r("BakedNav.RouteToGoal", false, [&] {
        const int map_id = current_map_id();
        MapGoal goal = goal_in;
        if (goal.cells.empty()) goal.cells = goals_for(*there, goal.cell, kind).cells;
        if (goal.cells.empty()) {
            out.error = L"No path found";
            clear_pending();
            return true;
        }
        if (goal.map_id == map_id) {
            // This floor: the ordinary route (kept while it is walked, the facing at its end);
            // it goes through another floor and back only when this floor is cut (find).
            void* tr = unity::transform(player);
            if (!tr) return false;
            if (!find(unity::local_position(tr), world_of_cell(*here, goal.cell), map_handle, player, true, kind, false, out))
                return false;
            if (out.success && out.next_map_id > 0) remember_destination(goal, kind, name);
            else clear_pending();
            return true;
        }
        CrossLeg leg = find_across(map_handle, player, goal, true);
        if (!leg.found) {
            out.error = L"No path found";
            const Blockers live = live_blockers(*here, baked::grids(*here), {});
            if (!live.gates.empty() && find_across(map_handle, player, goal, false).found) out.reason = T(L"Story gated");
            clear_pending();
            LOGF("[BakedNav] no route to map %d (%d,%d) from map %d; %ls", goal.map_id, goal.cell.x, goal.cell.y, map_id,
                 out.reason.empty() ? L"no reason known" : out.reason.c_str());
            return true;
        }
        const int next = leg.on_this_map ? 0 : leg.next_map_id;
        set_active(map_id, goal.cell, goal.map_id, leg.phases, next);
        fill_result(*here, leg.phases, out);
        out.description = leg.description;
        out.next_map_id = next;
        if (next > 0) remember_destination(goal, kind, name);
        else clear_pending();
        LOGF("[BakedNav] route to map %d (%d,%d) from map %d: %d steps here, %d in all, %ls", goal.map_id, goal.cell.x,
             goal.cell.y, map_id, leg.steps_here, leg.steps_total, next > 0 ? L"goes on" : L"ends there");
        return true;
    });
}

bool pending_goal(MapGoal& goal, TargetKind& kind, std::wstring& name)
{
    if (!g_pending || !g_pending->valid) return false;
    goal = g_pending->goal;
    kind = g_pending->kind;
    name = g_pending->name;
    return true;
}

void remember_destination(const MapGoal& goal, TargetKind kind, const std::wstring& name)
{
    if (!g_pending) g_pending = new Pending();
    g_pending->valid = true;
    g_pending->goal = goal;
    g_pending->kind = kind;
    g_pending->name = name;
}

void clear_pending()
{
    if (g_pending) g_pending->valid = false;
}

void continue_pending()
{
    if (!g_pending || !g_pending->valid) return;
    void* map = field::map_handle();
    void* pl = field::player();
    if (!map || !pl) return;
    hooks::guarded("BakedNav.ContinuePending", [&] {
        const Pending p = *g_pending;   // route_to_goal re-remembers or clears it
        PathInfo info;
        if (!route_to_goal(map, pl, p.goal, p.kind, p.name, info)) {
            clear_pending();
            return;
        }
        std::wstring line = p.name.empty() ? std::wstring() : p.name + L": ";
        if (info.success && !info.description.empty()) line += info.description;
        else line += !info.reason.empty() ? info.reason : T(L"No path");
        ffpr::say(line, false);
    });
}

bool follow_goal(void* map_handle, void* player, const MapGoal& goal_in, TargetKind kind, Corner& out)
{
    out = Corner{};
    const baked::Submap* sm = current_submap(map_handle);
    const baked::Submap* there = baked::submap_by_map_id(goal_in.map_id);
    if (!sm || !there || !player) return false;
    const int map_id = current_map_id();
    if (goal_in.map_id == map_id) return follow(map_handle, player, world_of_cell(*sm, goal_in.cell), kind, out);
    return hooks::guarded_r("BakedNav.FollowGoal", false, [&] {
        void* tr = unity::transform(player);
        if (!tr) return false;
        const Cell at = cell_of(*sm, unity::local_position(tr), sm->world ? 1 : player_layer(player));
        std::vector<Phase> rest;
        bool have = g_active && same_target(*g_active, map_id, goal_in.cell, goal_in.map_id) && remaining_from(*g_active, at, rest);
        if (!have) {
            const double now = frame::now();
            if (g_follow_failed_at > 0 && now - g_follow_failed_at < kFollowRetrySeconds) return false;
            MapGoal goal = goal_in;
            if (goal.cells.empty()) goal.cells = goals_for(*there, goal.cell, kind).cells;
            CrossLeg leg = goal.cells.empty() ? CrossLeg{} : find_across(map_handle, player, goal, true);
            if (!leg.found) {
                g_follow_failed_at = now;
                return false;
            }
            g_follow_failed_at = 0;
            set_active(map_id, goal.cell, goal.map_id, leg.phases, leg.on_this_map ? 0 : leg.next_map_id);
            if (!remaining_from(*g_active, at, rest)) return false;
        }
        corner_of(*sm, rest, at, out);
        return true;
    });
}

} // namespace field::baked_nav
