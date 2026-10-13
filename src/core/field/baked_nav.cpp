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
#include <atomic>
#include <unordered_map>
#include "baked_nav.h"
#include "baked_map.h"
#include "field_internal.h"
#include "map_names.h"
#include "map_state.h"
#include "entity_translator.h"
#include "../game.h"
#include "../hooks.h"
#include "../log.h"
#include "../frame.h"
#include "../background.h"
#include "../worker_job.h"
#include "capture_budget.h"
#include "../system/system.h"
#include "../modtext.h"
#include "../say.h"
#include "../text.h"
#include "../fieldnav/move_state.h"

namespace field::baked_nav {
namespace {

using baked_route::Cell;
using unity::Vec3;

struct ActiveRoute {
    bool valid = false;
    int map_id = 0;               // the map the route was planned on (the player's)
    Cell target;                  // the target cell the route was planned to (x, y)
    int goal_map_id = 0;          // the target's map: map_id, or another floor (route_to_goal)
    int next_map_id = 0;          // the route goes on there after its last leg (0 = ends at the target)
    int transportation_id = 0;
    std::vector<Phase> phases;
};

struct Pending {
    bool valid = false;
    MapGoal goal;
    TargetKind kind = TargetKind::Infer;
    std::wstring name;
};
std::shared_ptr<const ActiveRoute> g_published_route; // native-owner handle to immutable worker data

struct RouteSnapshot;
thread_local RouteSnapshot* g_route_snapshot = nullptr;
const baked::Submap* snapshot_submap();
int planning_map_id();
int snapshot_layer();
Vec3 snapshot_position();
VehicleState planning_vehicle();
std::vector<int> planning_auto_groups();
std::vector<VehicleOnMap> planning_vehicles();
std::wstring planning_vehicle_name(int id, int type, const std::wstring& message);
map_state::Resolver planning_state();
void cancel_route_work();
void cancel_follow_work();
void cancel_quick_work();
bool planning_cancelled();
bool quick_query(unity::Vec3 from, unity::Vec3 to, void* map, void* player, TargetKind kind, bool with_vehicles, PathInfo& out);


constexpr float kTile = 16.0f;
constexpr int kLongRouteSpeechThreshold = 31; // speech brevity only; searches have no distance cap
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

std::atomic<bool> g_prepared{false};
bool g_load_started = false; // boot/main-thread request only; all decoding belongs to worker
bool g_loaded = false;
std::string* g_blob = nullptr;   // the embedded resource bytes: the reader keeps the pointer
std::unordered_map<int, bool>* g_size_warned = nullptr;

bool ensure_loaded()
{
    return g_prepared.load(std::memory_order_acquire) && g_loaded;
}

const baked::Submap* current_submap(void* map_handle)
{
    if (g_route_snapshot) return snapshot_submap();
    if (!ensure_loaded()) return nullptr;
    const int map_id = planning_map_id();
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
            LOGF("[BakedMap] map %d: live size %dx%d, baked %ls %dx%d: navigation data mismatch", map_id, w, h,
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
    if (g_route_snapshot) return snapshot_layer();
    const int layer = game_object_layer(player) - 9;
    return layer < 0 || layer > 2 ? 1 : layer;
}

const wchar_t* direction_key(int dir)
{
    switch (dir) {
    case 0: return L"North";
    case 1: return L"East";
    case 2: return L"South";
    case 3: return L"West";
    case 4: return L"Northeast";
    case 5: return L"Southeast";
    case 6: return L"Southwest";
    default: return L"Northwest";
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
    // Land destinations are reached after legally leaving the current vehicle.
    // Determine their actual walking footprint rather than a ship's fallback tile.
    if (sm.world && movement.vehicle_group > 0 &&
        (kind == TargetKind::Transition || !baked::group_crosses(movement.vehicle_group,baked::attribute(baked::grids(sm),target.x,target.y)))) {
        movement.vehicle_group = 0;
    }
    return baked_route::goals_for(movement, target, goal_kind(kind));
}

/// Direction to the interaction from the route's arrival cell.
std::wstring facing_text(const std::vector<Cell>& cells, const baked_route::Goals& goals)
{
    const int dir = cells.empty() ? -1 : baked_route::facing_direction(cells.back(), goals);
    if (dir < 0) return L"";
    return TF(L"face {0}", {T(direction_key(dir))});
}

void annotate_facing(Phase& phase, const baked_route::Goals& goals)
{
    phase.arrival_direction = phase.cells.empty() ? -1 : baked_route::facing_direction(phase.cells.back(), goals);
    phase.arrival_facing = facing_text(phase.cells, goals);
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
struct NativeObstacle {
    Cell cell;
    int type = 0, id = 0, collision = 0;
    bool collider = false, applies = true, enabled = true, trigger = false;
    bool chest = false, npc = false;
};
struct RouteSnapshot {
    double requested_at = 0, captured_at = 0;
    int map_id = 0, layer = 1;
    const baked::Submap* submap = nullptr; // immutable prepared bake, process lifetime
    Vec3 position;
    VehicleState vehicle;
    std::vector<int> auto_groups;
    std::vector<VehicleOnMap> vehicles; // every .game is null before handoff
    std::unordered_map<int, std::wstring> vehicle_names;
    std::vector<NativeObstacle> obstacles;
    map_state::Resolver state;
    std::wstring locale;
    MapGoal goal;
    TargetKind kind = TargetKind::Infer;
    std::wstring name;
    ActiveRoute active;
    std::shared_ptr<const ActiveRoute> previous_active;
    Pending pending;
    bool keep_destination = true;
    std::shared_ptr<std::atomic<uint64_t>> epoch;
    uint64_t generation = 0;
    explicit RouteSnapshot(map_state::Resolver copied) : state(std::move(copied)) {}
};
const baked::Submap* snapshot_submap() { return g_route_snapshot->submap; }
int planning_map_id() { return g_route_snapshot ? g_route_snapshot->map_id : field::current_map_id(); }
int snapshot_layer() { return g_route_snapshot->layer; }
Vec3 snapshot_position() { return g_route_snapshot->position; }
VehicleState planning_vehicle() { return g_route_snapshot ? g_route_snapshot->vehicle : field::current_vehicle(); }
std::vector<int> planning_auto_groups() { return g_route_snapshot ? g_route_snapshot->auto_groups : field::owned_auto_vehicle_groups(); }
std::vector<VehicleOnMap> planning_vehicles() { return g_route_snapshot ? g_route_snapshot->vehicles : field::vehicles_on_map(); }
std::wstring planning_vehicle_name(int id, int type, const std::wstring& message) {
    if (!g_route_snapshot) return field::vehicle_display_name(id, type, message);
    const auto name = g_route_snapshot->vehicle_names.find(id);
    return name == g_route_snapshot->vehicle_names.end() ? std::wstring() : name->second;
}
map_state::Resolver planning_state() { return g_route_snapshot ? g_route_snapshot->state : map_state::Resolver(); }
bool planning_cancelled() {
    return g_route_snapshot && g_route_snapshot->epoch &&
        g_route_snapshot->epoch->load() != g_route_snapshot->generation;
}
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
    if (g_route_snapshot) {
        // Classification, bake matching and footprints are pure worker computation.
        for (const auto& raw : g_route_snapshot->obstacles) {
            Cell c = raw.cell;
            if (raw.type == kObjCollisionEntity) {
                if (!raw.enabled || !raw.applies) continue;
                const auto* o = baked_by_entity_id(g, raw.id, baked::ObjType::CollisionEntity, c);
                const int collision = raw.collider ? raw.collision : (o ? o->a : 0);
                std::vector<Cell> cells; add_footprint(cells, o, c, false);
                for (const Cell& at : cells) {
                    if (collision == 6 || collision == 7) out.directional.push_back({at, collision == 6 ? 1 : 2});
                    else out.gates.push_back(at);
                }
            } else if (raw.trigger) {
                const auto* o = baked_by_entity_id(g, raw.id, baked::ObjType::Event, c);
                if (!o) o = baked_by_entity_id(g, raw.id, baked::ObjType::Warp, c);
                add_footprint(out.triggers, o, c, false);
            } else if (raw.type == kObjEntity || raw.type == kObjAnimEntity) {
                const auto* o = baked_by_entity_id(g, raw.id, raw.type == kObjEntity ? baked::ObjType::Entity : baked::ObjType::AnimEntity, c);
                if (o && (o->d & kFlagCollision)) add_footprint(out.gates, o, c, false);
            } else if (!sm.world && raw.chest) out.chests.push_back(c);
            else if (!sm.world && raw.npc) {
                const auto* o = baked_by_entity_id(g, raw.id, baked::ObjType::NPC, c);
                if (!o) o = baked_by_entity_id(g, raw.id, baked::ObjType::ShopNPC, c);
                if (o && (o->d & 2)) c.layer = -1;
                ((!o || (o->d & kFlagWanders)) ? out.moving_npcs : out.fixed_npcs).push_back(c);
            }
        }
        return out;
    }
    // Live capture belongs to request_route; worker callers always have a snapshot.

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
thread_local ReachCache* g_cache = nullptr;

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
    req.vehicle_type = req.landing_group = 0;
    req.auto_groups.clear();
    if (!req.submap || !req.submap->world) return;
    req.auto_groups = planning_auto_groups();
    VehicleState v = planning_vehicle();
    if (v.valid && v.type != 1 && v.type != 0) {
        if (const baked::TransportRow* row = baked::transport(v.id)) {
            if (row->type != kTransportContent) {
                req.vehicle_group = row->group;
                req.vehicle_type = row->type; req.landing_group = row->landing_group;
                return;
            }
        }
    }
}

/// The vehicle that would carry the player to the goals on a world map: the cheapest kind
/// first (the auto-ridden canoe or snowcraft, then a ship or submarine, then an airship). A
/// vehicle the party has (its auto-vehicles, the vehicles parked on this map) is named with
/// the game's own name; one it does not have yet is "vehicle" (user, 2026-10-11: name only
/// owned vehicles; "Requires vehicle" means the player does not own it yet). Empty when none
/// reaches the goals.
std::wstring vehicle_required(const baked::Grids& g, const baked_route::Request& req)
{
    std::vector<int> owned = planning_auto_groups();
    for (const VehicleOnMap& v : planning_vehicles())
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
            std::wstring name = planning_vehicle_name(row->id, row->type, baked::str(row->message));
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

// Pure worker reachability after legally leaving the already-ridden vehicle.
// Seed all reachable shores once; never flood the whole map separately per shore.
baked_route::Reach land_after_ride(const baked_route::Request& riding, const baked_route::Reach& ride)
{
    if (!riding.submap || !riding.submap->world || riding.vehicle_group <= 0 || !ride.submap) return {};
    const auto& g = baked::grids(*riding.submap);
    auto foot = riding;
    foot.vehicle_group = 0; foot.vehicle_type = foot.landing_group = 0;
    foot.goals.clear();
    const bool sails = riding.vehicle_type == kTransportShip || riding.vehicle_type == kTransportSubmarine;
    std::vector<Cell> starts;
    std::vector<int> costs;
    constexpr int sides[4][2] = {{0,-1},{1,0},{0,1},{-1,0}};
    for (int y=0; y<g.height; ++y) {
        if (planning_cancelled() || (riding.cancelled && riding.cancelled())) return {};
        for (int x=0; x<g.width; ++x) {
            const Cell shore{riding.start.layer,x,y};
            if (!baked_route::open(foot,g,shore) ||
                (riding.landing_group > 0 && !baked::group_lands(riding.landing_group,baked::attribute(g,x,y)))) continue;
            int cost = !sails ? ride.distance(shore) : -1;
            if (sails) for (const auto& side : sides) {
                const Cell water{riding.start.layer,(x+side[0]+g.width)%g.width,(y+side[1]+g.height)%g.height};
                const int distance = ride.distance(water);
                if (distance>=0 && baked_route::direction_allowed(foot,water,shore) && (cost<0 || distance+1<cost)) cost=distance+1;
            }
            if (cost>=0) { starts.push_back(shore); costs.push_back(cost); }
        }
    }
    return baked_route::flood(foot,starts,costs);
}

/// Full mode on a world map, on foot, when walking (with the auto-vehicles) cannot reach the
/// target: board a parked ship, submarine or airship. Walk to a cell beside the vehicle, ride
/// to a cell from which the target's walkable area is entered (a ship: a water cell beside a
/// land cell of that area the ship may stop at; an airship: a land cell of that area it may
/// land on), then walk. The shortest total over the vehicles the party has on this map.
bool boarding_route(const baked::Submap& sm, const baked::Grids& g, const baked_route::Request& walk,
                    const std::vector<Cell>& goals, std::vector<Phase>& phases)
{
    const bool riding = walk.vehicle_group > 0;
    auto foot = walk;
    foot.vehicle_group = 0; foot.vehicle_type = foot.landing_group = 0;
    foot.goals_open = true;
    std::vector<VehicleOnMap> vehicles;
    if (riding) {
        const auto current = planning_vehicle();
        VehicleOnMap vehicle;
        vehicle.transportation_id = current.id; vehicle.type = current.type;
        vehicle.message_id = current.message_id; vehicle.pos = snapshot_position();
        if (current.valid) vehicles.push_back(std::move(vehicle));
    } else vehicles = planning_vehicles();
    if (vehicles.empty()) return false;

    // The target's own walkable area (where the last leg walks).
    baked_route::Request from_target = foot;
    from_target.goals.clear();
    from_target.start = goals.front();
    baked_route::Reach land = baked_route::flood(from_target, goals);

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
        baked_route::Request to_vehicle = foot;
        to_vehicle.goals.clear();
        static const int kSide[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
        for (const auto& d : kSide) {
            Cell c{walk.start.layer, vcell.x + d[0], vcell.y + d[1]};
            c.x = (c.x + g.width) % g.width; c.y = (c.y + g.height) % g.height;
            if (baked_route::open(foot, g, c)) to_vehicle.goals.push_back(c);
        }
        baked_route::Route leg1;
        if (!riding) {
            if (to_vehicle.goals.empty()) continue;
            leg1 = baked_route::find(to_vehicle);
            if (!leg1.found) continue;
        }

        // 2. Ride to where the target's area is entered.
        baked_route::Request ride;
        ride.cancelled = &planning_cancelled;
        ride.submap = &sm;
        ride.vehicle_group = group;
        ride.start = vcell;
        ride.goals_open = false;
        ride.blocked = walk.blocked; ride.directional = walk.directional;
        ride.live_hazards = walk.live_hazards; ride.hazards = walk.hazards;
        ride.ignore_hazards = walk.ignore_hazards;
        const auto reachable_ride = baked_route::flood(ride);
        int best_landing = INT_MAX;
        Cell land_start;
        for (int y = 0; y < g.height; ++y) {
            if (planning_cancelled()) return false;
            for (int x = 0; x < g.width; ++x) {
                Cell here{walk.start.layer, x, y};
                if (!land.reachable(here) || !baked_route::open(foot, g, here)) continue;
                const int attr = baked::attribute(g, x, y);
                if (sails) {
                    // A water cell beside this land cell, where the ship may stop.
                    for (const auto& d : kSide) {
                        Cell w{walk.start.layer, x + d[0], y + d[1]};
                        w.x = (w.x + g.width) % g.width; w.y = (w.y + g.height) % g.height;
                        const int distance = reachable_ride.distance(w);
                        if (distance < 0 || !baked::vehicle_open(g, group, w.x, w.y)) continue;
                        if (row->landing_group > 0 && !baked::group_lands(row->landing_group, attr)) continue;
                        if (!baked_route::direction_allowed(foot, w, here)) continue;
                        const int cost = distance + 1 + land.distance(here);
                        if (cost < best_landing) { best_landing = cost; ride.goals = {w}; land_start = here; }
                    }
                } else {
                    const int distance = reachable_ride.distance(here);
                    if (distance < 0 || !baked::vehicle_open(g, group, x, y)) continue;
                    if (row->landing_group > 0 && !baked::group_lands(row->landing_group, attr)) continue;
                    const int cost = distance + land.distance(here);
                    if (cost < best_landing) { best_landing = cost; ride.goals = {here}; land_start = here; }
                }
            }
        }
        if (ride.goals.empty()) continue;
        baked_route::Route leg2 = baked_route::find(ride);
        if (!leg2.found) continue;

        // 3. Walk from the landing to the target. A ship's stop is a water cell: the first step
        //    of this leg walks off it onto the land cell beside it (a start cell is never
        //    tested for openness, only the cells entered).
        baked_route::Request last = foot;
        const Cell stop = leg2.cells.back();
        last.start = land_start;
        baked_route::Route leg3 = baked_route::find(last);
        if (!leg3.found) continue;
        if (sails) leg3.cells.insert(leg3.cells.begin(), stop);

        const int total = leg1.steps() + leg2.steps() + leg3.steps();
        if (total >= best_total) continue;
        best_total = total;
        best.clear();
        std::wstring name = planning_vehicle_name(v.transportation_id, v.type, v.message_id);
        if (name.empty()) name = T(L"Vehicle");
        Phase p1;
        p1.cells = leg1.cells;
        Phase p2;
        p2.cells = leg2.cells;
        if (!riding) {
            p2.action_before = TF(L"board the {0}", {name});
            p2.transportation_id = v.transportation_id;
            p2.vehicle_before = sails ? route_progress::VehicleAction::WalkOn : route_progress::VehicleAction::ConfirmBoard;
            const auto approach = leg1.cells.back();
            p2.action_cell = approach;
            int dx = vcell.x - approach.x, dy = vcell.y - approach.y;
            if (std::abs(dx) > 1) dx = dx > 0 ? -1 : 1;
            if (std::abs(dy) > 1) dy = dy > 0 ? -1 : 1;
            p2.vehicle_direction = dy < 0 ? 0 : dx > 0 ? 1 : dy > 0 ? 2 : 3;
            if (sails) p2.cells.insert(p2.cells.begin(), approach);
        }
        Phase p3;
        p3.cells = leg3.cells;
        p3.action_before = T(L"land");
        p3.transportation_id = v.transportation_id;
        p3.action_cell = stop;
        p3.vehicle_before = sails ? route_progress::VehicleAction::WalkOff :
            game::is(game::Id::FF6) && (row->type == 3 || row->type == 8) ?
                route_progress::VehicleAction::BackLand : route_progress::VehicleAction::ConfirmLand;
        if (!riding) best.push_back(p1);
        best.push_back(p2);
        best.push_back(p3);
    }
    if (best.empty()) return false;
    phases = best;
    return true;
}

} // namespace

void preload()
{
    if (g_load_started) return;
    g_load_started = true;
    if (!background::post([] {
        const double started = frame::now();
        data::prepare_game_json();
        field::prepare_entity_translations();
        g_blob = new std::string(data::game_file("maps"));
        g_loaded = !g_blob->empty() && baked::load(g_blob->data(), g_blob->size());
        if (g_loaded) {
            // Decode once here. All readers, including worker projections, then use
            // immutable tables; map transitions never allocate/decode whole grids.
            for (int i = 0; i < baked::submap_count(); ++i) (void)baked::grids(*baked::submap_at(i));
        }
        LOGF("[BakedMap] %s prepared on data worker: %d maps, %.1f ms, loaded=%d",
             game::key(), g_loaded ? baked::submap_count() : 0, (frame::now() - started) * 1000, g_loaded);
        g_prepared.store(true, std::memory_order_release);
    })) {
        LOGF("[BakedMap] cannot start data worker; navigation preparation unavailable");
        g_load_started = false;
    }
}
bool prepared() { return g_prepared.load(std::memory_order_acquire); }
bool available(void* map_handle) { return current_submap(map_handle) != nullptr; }

bool player_cell(void* map_handle, void* player, Cell& out)
{
    const baked::Submap* sm = current_submap(map_handle);
    if (g_route_snapshot) { out = cell_of(*sm, snapshot_position(), snapshot_layer()); return true; }
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

std::wstring describe(const std::vector<Phase>& phases, bool full)
{
    const auto* map = snapshot_submap();
    const int width = map && map->world ? map->width : 0;
    const int height = map && map->world ? map->height : 0;
    std::vector<std::wstring> parts;
    size_t total_legs = 0;
    int total_steps = 0;
    for (const Phase& p : phases) {
        total_legs += baked_route::legs(p.cells,width,height).size();
        total_steps += p.cells.empty() ? 0 : static_cast<int>(p.cells.size()) - 1;
    }
    // A route the game's searcher could have given is spoken whole (FF1); a longer one is
    // capped as FF5 capped its chained routes: the player walks what was heard and asks again.
    const size_t cap = !full && total_steps > kLongRouteSpeechThreshold ? kMaxSpokenLegs : total_legs;
    size_t spoken = 0;
    for (size_t pi = 0; pi < phases.size(); ++pi) {
        const Phase& p = phases[pi];
        if (!p.action_before.empty()) parts.push_back(p.action_before);
        bool whole = true;
        for (const baked_route::Leg& leg : baked_route::legs(p.cells,width,height)) {
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
    cancel_route_work();
    cancel_follow_work();
    cancel_quick_work();
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
    req.cancelled = &planning_cancelled;
    req.submap = &sm;
    req.start = start;
    req.passages = game::flags().has_hidden_passages;
    if (sm.world) req.auto_groups = planning_auto_groups();
    return req;
}

constexpr int kMaxVisitedMaps = 400;

} // namespace

std::wstring map_name_of(int map_id) {
    // Native map-name resolution is a separate owner continuation. The token is
    // replaced by worker formatting before any route is published or spoken.
    if (g_route_snapshot) return L"\uE000" + std::to_wstring(map_id) + L"\uE001";
    return resolve_map_name_by_id(map_id);
}

CrossLeg find_across(void* map_handle, void* player, const MapGoal& goal, bool with_gates)
{
    CrossLeg out;
    const baked::Submap* here = current_submap(map_handle);
    const baked::Submap* there = baked::submap_by_map_id(goal.map_id);
    if (!here || !there || (!player && !g_route_snapshot)) return out;
    return hooks::guarded_r("BakedNav.FindAcross", out, [&] {
        Cell start;
        if (!player_cell(map_handle, player, start)) return out;
        const baked::Grids& gh = baked::grids(*here);
        if (!baked::in_bounds(gh, start.x, start.y)) return out;
        // The live obstacles of this map (only this map has live state): a story gate closes
        // the cells it stands on, and an exit it stands on is sealed (Kashuan Keep's door).
        const Blockers live = live_blockers(*here, gh, {});
        map_state::Resolver state = planning_state();
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
            if (planning_cancelled()) return out;
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
            annotate_facing(p, goals_for(*here, goal.cell, TargetKind::Infer));
            phases.push_back(p);
            out.on_this_map = true;
            out.phases = phases;
            out.description = describe(phases);
            if (baked_route::crosses_event(route.cells, req.hazards))
                out.description = T(L"Event trigger on the route") + L", " + out.description;
            out.cells = route.cells;
            out.steps_here = route.steps();
            out.steps_total = goal_steps;
            out.found = true;
            return out;
        }
        const State& hop = states[static_cast<size_t>(first)];
        if (!hop.via) return out;
        const baked::Object& o = *hop.via;
        const bool examine_warp = o.type == baked::ObjType::Warp && o.d != 1;
        const auto warp_goals = goals_for(*here, Cell{o.layer, o.tx, o.ty}, TargetKind::Interact);
        if (examine_warp) req.goals = warp_goals.cells;
        else for (int y = o.ty; y < o.ty + o.th; ++y)
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
            p.manual_after = o.d != 1;
            if (examine_warp) annotate_facing(p, warp_goals);
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
        if (baked_route::crosses_event(route.cells, req.hazards))
            out.description = T(L"Event trigger on the route") + L", " + out.description;
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


thread_local const ActiveRoute* g_active = nullptr;
void invalidate_active() {
    if (g_route_snapshot) g_route_snapshot->active.valid = false;
    else { g_active = nullptr; g_published_route.reset(); }
}

bool same_target(const ActiveRoute& r, int map_id, const Cell& target, int goal_map_id)
{
    return r.valid && r.map_id == map_id && r.goal_map_id == goal_map_id && r.target.x == target.x && r.target.y == target.y;
}

void set_active(int map_id, const Cell& target, int goal_map_id, const std::vector<Phase>& phases, int next_map_id)
{
    if (!g_route_snapshot) return; // only owned worker planning may construct routes
    auto& active = g_route_snapshot->active;
    active.valid = true; active.map_id = map_id; active.target = target;
    active.goal_map_id = goal_map_id; active.next_map_id = next_map_id; active.phases = phases;
    active.transportation_id = g_route_snapshot->vehicle.valid ? g_route_snapshot->vehicle.id : 0;
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
    out.full_description = describe(phases, true);
    out.walk_phases = phases;
    out.map_width = sm.width;
    out.map_height = sm.height;
    out.wraps = sm.world;
}

} // namespace

bool find(Vec3 from, Vec3 to, void* map_handle, void* player, bool full, TargetKind kind, bool with_vehicles, PathInfo& out)
{
    const baked::Submap* sm = current_submap(map_handle);
    if (!sm) return false;
    if (!g_route_snapshot) {
        if (!full) return quick_query(from, to, map_handle, player, kind, with_vehicles, out);
        out.reason = out.error = T(L"Navigation requires an asynchronous request");
        return true;
    }
    return hooks::guarded_r("BakedNav.Find", false, [&] {
        const baked::Grids& g = baked::grids(*sm);
        if (g.width <= 0 || g.height <= 0) return false;

        baked_route::Request req;
        req.cancelled = &planning_cancelled;
        req.submap = sm;
        req.start = cell_of(*sm, from, sm->world ? 1 : (g_route_snapshot ? snapshot_layer() : (!player ? 1 : player_layer(player))));
        const Cell target = cell_of(*sm, to, 1);
        const baked_route::Goals goals = goals_for(*sm, target, kind);
        req.goals = goals.cells;
        world_mode(req);
        if (req.vehicle_group > 0) req.goals_open = false;
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
                for (const VehicleOnMap& v : planning_vehicles())
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
            for (const Phase& p : phases)
                if (baked_route::crosses_event(p.cells, live.triggers)) {
                    out.description = T(L"Event trigger on the route") + L", " + out.description;
                    out.full_description = T(L"Event trigger on the route") + L", " + out.full_description;
                    return;
                }
        };
        // The route already planned to this target, while the player is still on it.
        const int map_id = planning_map_id();
        if (g_active && g_active->transportation_id == (g_route_snapshot->vehicle.valid ? g_route_snapshot->vehicle.id : 0) &&
            same_target(*g_active, map_id, target, map_id)) {
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
                if (blocked) invalidate_active();
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
        baked_route::Route route;
        const bool riding_goal = std::any_of(req.goals.begin(),req.goals.end(),[&](const Cell& goal) {
            return baked_route::open(req,g,goal);
        });
        // A land goal cannot terminate ship-only A*. Go straight to the owned
        // ride/landing/walk planner instead of exhausting the whole sea first.
        if (req.vehicle_group == 0 || riding_goal) route = baked_route::find(req);
        if (!route.found && game::flags().has_hidden_passages && !sm->world) {
            baked_route::Request with = req;
            with.passages = true;
            route = baked_route::find(with);
        }
        if (!route.found && !live.triggers.empty() && (req.vehicle_group == 0 || riding_goal)) {
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
        } else if (sm->world) {
            if (!boarding_route(*sm, g, req, req.goals, phases) && !live.triggers.empty()) {
                // The existing explicit-route policy may cross a live event with
                // a warning. Apply it to both vehicle/walking legs, without
                // repeating a ship-only search for an unreachable land goal.
                auto through_events = req;
                through_events.ignore_hazards = true;
                boarding_route(*sm, g, through_events, req.goals, phases);
            }
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
            if (g_active && same_target(*g_active, map_id, target, map_id)) invalidate_active();
            LOGF("[BakedNav] no route: (%d,%d) L%d -> (%d,%d) on map %d, %zu goal cells; %zu chests, %zu gates, %zu fixed and %zu moving NPCs; %ls",
                 req.start.x, req.start.y, req.start.layer, target.x, target.y, map_id, req.goals.size(),
                 live.chests.size(), live.gates.size(), live.fixed_npcs.size(), live.moving_npcs.size(),
                 out.reason.empty() ? L"no reason known" : out.reason.c_str());
            return true;
        }
        if (phases.back().action_after.empty()) annotate_facing(phases.back(), goals);
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
    Vec3 projected_corner{};
    bool project = false;
    for (size_t pi = 0; pi < rest.size(); ++pi) {
        const Phase& p = rest[pi];
        if (p.cells.size() >= 2) {
            const std::vector<baked_route::Leg> legs = baked_route::legs(p.cells,sm.world ? sm.width : 0,sm.world ? sm.height : 0);
            to_corner = legs.empty() ? static_cast<int>(p.cells.size()) - 1 : legs.front().count;
            corner = p.cells[static_cast<size_t>(to_corner)];
            if (sm.world && !legs.empty()) {
                // The beacon follows this leg's direction across an edge, not
                // the long Cartesian vector to the opposite side of the map.
                projected_corner = world_of_cell(sm,p.cells.front());
                const float length = to_corner * kTile;
                if (legs.front().dir == 0) projected_corner.y += length;
                else if (legs.front().dir == 1) projected_corner.x += length;
                else if (legs.front().dir == 2) projected_corner.y -= length;
                else projected_corner.x -= length;
                project = true;
            }
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
    out.point = project ? projected_corner : world_of_cell(sm, corner);
    out.steps_to_corner = to_corner;
    out.steps_left = steps_left;
}
enum class CornerState { Pending, Ready, Missing };
struct CornerSnapshot {
    std::shared_ptr<const ActiveRoute> route;
    const baked::Submap* submap = nullptr;
    Cell at, target;
    int map_id = 0, goal_map = 0;
};
struct CornerResult { CornerSnapshot input; Corner corner; bool found = false; };
worker_job::Owner g_follow_owner;
std::optional<CornerResult> g_corner;
std::optional<CornerSnapshot> g_corner_pending;
bool same_corner(const CornerSnapshot& a, const CornerSnapshot& b) {
    return a.route == b.route && a.submap == b.submap && a.at == b.at && a.target == b.target &&
        a.map_id == b.map_id && a.goal_map == b.goal_map;
}
void cancel_follow_work() { g_follow_owner.cancel(); g_corner.reset(); g_corner_pending.reset(); }
CornerResult build_corner(CornerSnapshot& input) {
    CornerResult result; result.input = input;
    std::vector<Phase> rest;
    if (input.route && same_target(*input.route, input.map_id, input.target, input.goal_map) &&
        remaining_from(*input.route, input.at, rest)) {
        corner_of(*input.submap, rest, input.at, result.corner);
        result.found = true;
    }
    return result;
}
CornerState cached_corner(const baked::Submap* sm, int map_id, const Cell& at,
                         const Cell& target, int goal_map, Corner& out) {
    if (!g_published_route || !same_target(*g_published_route, map_id, target, goal_map)) return CornerState::Missing;
    CornerSnapshot input{g_published_route, sm, at, target, map_id, goal_map};
    if (g_corner && same_corner(g_corner->input, input)) {
        out = g_corner->corner;
        return g_corner->found ? CornerState::Ready : CornerState::Missing;
    }
    if (g_corner_pending && same_corner(*g_corner_pending, input)) return CornerState::Pending;
    g_follow_owner.cancel(); g_corner_pending = input;
    worker_job::start(g_follow_owner, input, &build_corner, [](CornerResult result) {
        if (!g_corner_pending || !same_corner(*g_corner_pending, result.input)) return;
        g_corner = std::move(result); g_corner_pending.reset();
    }, background::Lane::Navigation);
    return CornerState::Pending;
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
        const int map_id = planning_map_id();
        const auto state = cached_corner(sm, map_id, at, target_cell, map_id, out);
        if (state == CornerState::Pending) return false;
        if (state == CornerState::Missing) {
            // No route yet, or the player has left it: plan one silently. A target without a
            // route is retried every two seconds, not on every ping.
            const double now = frame::now();
            if (g_follow_failed_at > 0 && now - g_follow_failed_at < kFollowRetrySeconds) return false;
            if (!request_pending()) {
                MapGoal goal; goal.map_id = map_id; goal.cell = target_cell;
                request_route(goal, kind, L"", [](PathInfo info) {
                    g_follow_failed_at = info.success ? 0 : frame::now();
                }, false);
            }
            return false;
        }
        return true;
    });
}

bool walk_edge(void* map_handle, void* player, const Cell& from, const Cell& to, bool goal_edge,
               route_progress::VehicleAction vehicle_action, int transportation_id)
{
    const baked::Submap* sm = current_submap(map_handle);
    if (!sm) return false; // missing bake is a data bug; do not approve an unknown edge
    const auto& g = baked::grids(*sm);
    baked_route::Request req;
    req.submap = sm;
    req.start = from;
    req.goals = {to};
    req.passages = game::flags().has_hidden_passages;
    world_mode(req);
    // Reuse the native movement collision query for this single edge. The game
    // owns moving NPCs, gates and directional collisions; never scan a map here.
    void* transform = unity::transform(player);
    if (!transform) return false;
    auto position = unity::local_position(transform);
    int dx = to.x - from.x, dy = to.y - from.y;
    if (sm->world && std::abs(dx) > 1) dx = dx > 0 ? -1 : 1;
    if (sm->world && std::abs(dy) > 1) dy = dy > 0 ? -1 : 1;
    position.x += dx * kTile; position.y -= dy * kTile;
    if (!native_position_walkable(player, position)) return false;
    // Stairs may be open on either the incoming or outgoing layer, just like the planner.
    const Cell incoming{from.layer, to.x, to.y};
    const bool open = baked_route::open(req, g, to) ||
        (from.layer != to.layer && baked_route::open(req, g, incoming));
    bool boundary = false;
    const auto* transport = baked::transport(transportation_id);
    if (sm->world && transport && std::abs(dx) + std::abs(dy) == 1) {
        auto foot = req; foot.vehicle_group = foot.vehicle_type = foot.landing_group = 0;
        if (vehicle_action == route_progress::VehicleAction::WalkOff && field::current_vehicle().id == transportation_id) {
            boundary = baked::vehicle_open(g, transport->group, from.x, from.y) && baked_route::open(foot, g, to) &&
                (transport->landing_group <= 0 || baked::group_lands(transport->landing_group, baked::attribute(g, to.x, to.y)));
        } else if (vehicle_action == route_progress::VehicleAction::WalkOn) {
            // The planned parked-vehicle edge was captured by the route worker.
            // Native collision above still owns boarding availability; do not
            // enumerate every live vehicle again on each input-axis callback.
            boundary = baked_route::open(foot, g, from) && baked::vehicle_open(g, transport->group, to.x, to.y);
        }
    }
    // Native exit/town cells can be closed in the static grid and explicitly entered as a
    // route goal. Keep that planner rule only for the final edge; live gates still veto it.
    return baked_route::edge_allowed(req, g, from, to) && (open || goal_edge || boundary);
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


thread_local Pending* g_pending = nullptr;

} // namespace

std::vector<RemoteObject> connected_objects(void* map_handle)
{
    const baked::Submap* here = current_submap(map_handle);
    if (!here || here->world) return {};
    map_state::Resolver state;
    return connected_objects_snapshot(planning_map_id(), state);
}
std::vector<RemoteObject> connected_objects_snapshot(int map_id, map_state::Resolver& state)
{
    std::vector<RemoteObject> out;
    if (!prepared()) return out;
    const baked::Submap* here = baked::submap_by_map_id(map_id);
    if (!here || here->world) return out;
    return hooks::guarded_r("BakedNav.ConnectedObjects", out, [&] {
        const std::vector<Floor> floors = connected_floors(here, map_id, state);
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
                if (!state.active(*f.submap, o)) continue; // Current map includes opened chests
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
                    r.scripted = (o.d & 4) != 0;
                    break;
                case baked::ObjType::TreasureBox:
                    r.opened = state.treasure_opened(*f.submap, o);
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
        LOGF("[BakedNav] other floors of map %d: %zu floors, %zu objects", map_id, floors.size(), out.size());
        return out;
    });
}

bool route_to_goal(void* map_handle, void* player, const MapGoal& goal_in, TargetKind kind, const std::wstring& name,
                   PathInfo& out)
{
    out = PathInfo{};
    const baked::Submap* here = current_submap(map_handle);
    const baked::Submap* there = baked::submap_by_map_id(goal_in.map_id);
    if (!here || !there || (!player && !g_route_snapshot)) {
        out.error = L"No path found";
        return false;
    }
    return hooks::guarded_r("BakedNav.RouteToGoal", false, [&] {
        const int map_id = planning_map_id();
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
            void* tr = g_route_snapshot ? nullptr : unity::transform(player);
            if (!tr && !g_route_snapshot) return false;
            if (!find(g_route_snapshot ? snapshot_position() : unity::local_position(tr), world_of_cell(*here, goal.cell), map_handle, player, true, kind, false, out))
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
        request_route(p.goal, p.kind, p.name, [name = p.name](PathInfo info) {
            std::wstring line = name.empty() ? std::wstring() : name + L": ";
            if (info.success && !info.description.empty()) line += info.description;
            else line += !info.reason.empty() ? info.reason : T(L"No path");
            ffpr::say(line, false);
        });
    });
}

bool follow_goal(void* map_handle, void* player, const MapGoal& goal_in, TargetKind kind, Corner& out)
{
    out = Corner{};
    const baked::Submap* sm = current_submap(map_handle);
    const baked::Submap* there = baked::submap_by_map_id(goal_in.map_id);
    if (!sm || !there || !player) return false;
    const int map_id = planning_map_id();
    if (goal_in.map_id == map_id) return follow(map_handle, player, world_of_cell(*sm, goal_in.cell), kind, out);
    return hooks::guarded_r("BakedNav.FollowGoal", false, [&] {
        void* tr = unity::transform(player);
        if (!tr) return false;
        const Cell at = cell_of(*sm, unity::local_position(tr), sm->world ? 1 : player_layer(player));
        const auto state = cached_corner(sm, map_id, at, goal_in.cell, goal_in.map_id, out);
        if (state == CornerState::Pending) return false;
        if (state == CornerState::Missing) {
            const double now = frame::now();
            if (g_follow_failed_at > 0 && now - g_follow_failed_at < kFollowRetrySeconds) return false;
            if (!request_pending()) request_route(goal_in, kind, L"", [](PathInfo info) {
                g_follow_failed_at = info.success ? 0 : frame::now();
            }, false);
            return false;
        }
        return true;
    });
}

namespace {
worker_job::Owner g_route_owner;
worker_job::Owner g_quick_owner;
struct QuickGrid {
    baked_route::Request movement;
    baked_route::Reach reach, passages, landed;
    int map_id = 0;
};
std::shared_ptr<const QuickGrid> g_quick_grid;
baked_route::Request g_quick_wanted;
bool g_quick_pending = false;
std::vector<std::function<void()>> g_quick_waiters;
void cancel_quick_work() {
    g_quick_owner.cancel(); g_quick_grid.reset(); g_quick_pending = false; g_quick_waiters.clear();
}
bool same_movement(const baked_route::Request& a, const baked_route::Request& b) {
    return a.submap == b.submap && a.start == b.start && a.vehicle_group == b.vehicle_group &&
        a.vehicle_type == b.vehicle_type && a.landing_group == b.landing_group && a.auto_groups == b.auto_groups;
}
baked_route::Request quick_movement(Vec3 from, void* map, void* player, bool with_vehicles) {
    baked_route::Request req;
    req.submap = current_submap(map);
    if (!req.submap) return req;
    req.start = cell_of(*req.submap, from, req.submap->world ? 1 : player_layer(player));
    req.ignore_hazards = true;
    world_mode(req);
    if (with_vehicles && req.submap->world && req.vehicle_group == 0)
        for (const auto& v : field::vehicles_on_map())
            if (const auto* row = baked::transport(v.transportation_id))
                if (row->type != kTransportContent && row->type != 1 && row->group > 1) req.auto_groups.push_back(row->group);
    return req;
}
void ensure_quick(baked_route::Request request, std::function<void()> ready = {}) {
    if (!request.submap) { if (ready) ready(); return; }
    if (g_quick_grid && same_movement(request, g_quick_grid->movement)) { if (ready) ready(); return; }
    if (ready) g_quick_waiters.push_back(std::move(ready));
    if (g_quick_pending && same_movement(request, g_quick_wanted)) return;
    g_quick_owner.cancel();
    g_quick_wanted = request; g_quick_pending = true;
    const int map_id = field::current_map_id();
    auto epoch = g_quick_owner.epoch(); const auto generation = epoch->load();
    request.cancelled = [epoch, generation] { return epoch->load() != generation; };
    struct Input { baked_route::Request request; int map_id; bool passages; };
    worker_job::start(g_quick_owner, Input{std::move(request), map_id, game::flags().has_hidden_passages},
        [](Input& input) {
            auto grid = std::make_shared<QuickGrid>();
            grid->map_id = input.map_id; grid->movement = input.request;
            grid->reach = baked_route::flood(input.request);
            grid->landed = land_after_ride(input.request,grid->reach);
            if (input.passages && !input.request.submap->world) {
                input.request.passages = true; grid->passages = baked_route::flood(input.request);
            }
            return grid;
        }, [](std::shared_ptr<QuickGrid> grid) {
            g_quick_pending = false;
            if (!grid || grid->map_id != field::current_map_id() || !ffpr::system::is_field_navigation_ready()) {
                g_quick_waiters.clear(); return;
            }
            g_quick_grid = std::move(grid);
            auto waiters = std::move(g_quick_waiters); g_quick_waiters.clear();
            for (auto& ready : waiters) ready();
        }, background::Lane::Navigation);
}
bool quick_query(Vec3 from, Vec3 to, void* map, void* player, TargetKind kind, bool with_vehicles, PathInfo& out) {
    auto req = quick_movement(from, map, player, with_vehicles);
    if (!req.submap) return false;
    ensure_quick(req);
    if (!g_quick_grid || !same_movement(req, g_quick_grid->movement)) { out.pending = true; return true; }
    const auto goals = goals_for(*req.submap, cell_of(*req.submap, to, 1), kind);
    out.success = any_reachable(g_quick_grid->reach, goals.cells) || any_reachable(g_quick_grid->passages, goals.cells) ||
        any_reachable(g_quick_grid->landed, goals.cells);
    if (!out.success) out.error = T(L"No path found");
    return true;
}
bool g_request_pending = false;
uint64_t g_request_serial = 0;
void cancel_route_work() {
    if (g_route_snapshot) return;
    ++g_request_serial; g_request_pending = false;
    g_route_owner.cancel(); frame::cancel_label("BakedNav.Capture");
}
struct SnapshotScope {
    RouteSnapshot* old;
    const ActiveRoute* active;
    Pending* pending;
    explicit SnapshotScope(RouteSnapshot& snapshot)
        : old(g_route_snapshot), active(g_active), pending(g_pending) {
        g_route_snapshot = &snapshot; g_active = &snapshot.active; g_pending = &snapshot.pending;
    }
    ~SnapshotScope() { g_route_snapshot = old; g_active = active; g_pending = pending; }
};
struct Planned {
    double requested_at = 0, capture_ms = 0, compute_ms = 0;
    VehicleState vehicle;
    PathInfo path;
    std::shared_ptr<ActiveRoute> active;
    Pending pending;
    int map_id = 0;
    Cell start;
    std::wstring locale;
    MapGoal goal;
    TargetKind kind = TargetKind::Infer;
    std::wstring name;
    bool keep_destination = true;
};
Planned plan_snapshot(RouteSnapshot& snapshot) {
    const double compute_started = frame::now();
    if (snapshot.previous_active) snapshot.active = *snapshot.previous_active;
    modtext::LocaleScope locale(snapshot.locale);
    SnapshotScope scope(snapshot);
    const Pending previous_pending = snapshot.pending;
    Planned result;
    result.requested_at = snapshot.requested_at;
    result.vehicle = snapshot.vehicle;
    result.capture_ms = (snapshot.captured_at - snapshot.requested_at) * 1000;
    result.map_id = snapshot.map_id;
    result.start = cell_of(*snapshot.submap, snapshot.position, snapshot.layer);
    result.locale = snapshot.locale;
    result.goal = snapshot.goal; result.kind = snapshot.kind;
    result.name = snapshot.name; result.keep_destination = snapshot.keep_destination;
    if (!route_to_goal(nullptr, nullptr, snapshot.goal, snapshot.kind, snapshot.name, result.path)) {
        result.path.reason = result.path.error = T(L"Navigation map data unavailable");
    }
    if (!snapshot.keep_destination) snapshot.pending = previous_pending;
    result.active = std::make_shared<ActiveRoute>(std::move(snapshot.active)); result.pending = std::move(snapshot.pending);
    result.compute_ms = (frame::now() - compute_started) * 1000;
    return result;
}
void resolve_plan_name(Planned& result, const std::wstring& name) {
    modtext::LocaleScope locale(result.locale);
    if (result.path.next_map_id <= 0) return;
    const auto token = L"\uE000" + std::to_wstring(result.path.next_map_id) + L"\uE001";
    const auto old_phrase = TF(L"take the exit to {0}", {token});
    const auto phrase = name.empty() ? T(L"take the exit") : TF(L"take the exit to {0}", {name});
    auto replace = [&](std::wstring& value) {
        size_t at = 0;
        while ((at = value.find(old_phrase, at)) != std::wstring::npos) {
            value.replace(at, old_phrase.size(), phrase); at += phrase.size();
        }
    };
    replace(result.path.description); replace(result.path.full_description);
    for (auto& phase : result.path.walk_phases) replace(phase.action_after);
    if (result.active) for (auto& phase : result.active->phases) replace(phase.action_after);
}
bool still_current(const Planned& result) {
    if (!ffpr::system::is_field_navigation_ready() || field::current_map_id() != result.map_id) return false;
    Cell now;
    if (!player_cell(field::map_handle(), field::player(), now) || !(now == result.start)) return false;
    const auto vehicle = field::current_vehicle();
    return vehicle.valid == result.vehicle.valid && (!vehicle.valid ||
        (vehicle.id == result.vehicle.id && vehicle.type == result.vehicle.type));
}
void commit_plan(Planned& result) {
    g_request_pending = false;
    if (!g_pending) g_pending = new Pending();
    g_published_route = std::move(result.active); g_active = g_published_route.get();
    *g_pending = std::move(result.pending);
    LOGF("[BakedNav] ready map %d: capture %.3f ms, compute %.3f ms, total %.3f ms",
        result.map_id, result.capture_ms, result.compute_ms, (frame::now() - result.requested_at) * 1000);
}
NativeObstacle capture_obstacle(void* fe, const RouteSnapshot& snapshot) {
    NativeObstacle out;
    void* tr = unity::transform(fe);
    void* property = entity_property(fe);
    out.type = object_type_of(property);
    out.id = property && r::fPropEntityId.get() >= 0 ? ref::rd<int32_t>(property, r::fPropEntityId) : 0;
    out.cell = cell_of(*snapshot.submap, unity::local_position(tr), game_object_layer(fe) - 9);
    if (out.cell.layer < 0 || out.cell.layer > 2 || snapshot.submap->world) out.cell.layer = -1;
    if (out.type == kObjCollisionEntity) {
        out.collider = kCollider.is(fe);
        out.enabled = !out.collider || ref::rd<bool>(fe, fColliderEnable);
        out.collision = out.collider ? ref::rd<int32_t>(fe, fColliderId) : 0;
        void* transports = property ? ref::rd<void*>(property, r::fPropTargetTransportationIdList) : nullptr;
        const int count = unity::list_count(transports);
        out.applies = count == 0;
        for (int i = 0; i < count; ++i)
            out.applies |= unity::list_value<int32_t>(transports, i) == (snapshot.vehicle.valid ? snapshot.vehicle.id : 1);
    }
    out.trigger = (out.type == 2 || out.type == 28) && kTrigger.is(fe)
        && !entity_is_scenery(fe) && ref::rd<bool>(fe, fTriggerPower);
    out.chest = r::kFieldTresureBox.is(fe); out.npc = r::kFieldNonPlayer.is(fe);
    return out;
}
}

void cancel_requests() { cancel_route_work(); cancel_follow_work(); }
bool request_pending() { return g_request_pending; }

struct ExitSnapshot {
    RouteSnapshot route;
    bool with_vehicles;
    explicit ExitSnapshot(bool vehicles, map_state::Resolver state = map_state::Resolver())
        : route(std::move(state)), with_vehicles(vehicles) {}
};
std::shared_ptr<ExitSnapshot> begin_exit_capture(bool with_vehicles, bool indoor) {
    void* pl = field::player();
    const auto* sm = current_submap(field::map_handle());
    if (!sm || !pl || !unity::transform(pl)) return {};
    if (!sm->world && !indoor) return {};
    auto capture = std::make_shared<ExitSnapshot>(with_vehicles);
    auto& snapshot = capture->route;
    snapshot.map_id = field::current_map_id(); snapshot.submap = sm;
    snapshot.position = unity::local_position(unity::transform(pl));
    snapshot.layer = sm->world ? 1 : player_layer(pl);
    snapshot.vehicle = field::current_vehicle();
    return capture;
}
void capture_exit_obstacle(ExitSnapshot& capture, void* entity) {
    if (live_entity(entity) && entity_is_active(entity) && unity::transform(entity))
        capture.route.obstacles.push_back(capture_obstacle(entity,capture.route));
}
void finish_exit_capture(ExitSnapshot& capture) {
    capture.route.auto_groups = field::owned_auto_vehicle_groups();
    if (capture.with_vehicles) {
        capture.route.vehicles = field::vehicles_on_map();
        for (auto& vehicle : capture.route.vehicles) vehicle.game = nullptr;
    }
}
bool exit_snapshot_world(const ExitSnapshot& capture) { return capture.route.submap->world; }
int exit_snapshot_vehicle(const ExitSnapshot& capture) { return capture.route.vehicle.valid ? capture.route.vehicle.id : 0; }
std::vector<int> exit_distances(ExitSnapshot& capture, const std::vector<ExitTarget>& targets,
                              std::function<bool()> cancelled) {
    auto& snapshot = capture.route;
    SnapshotScope scope(snapshot);
    const auto* sm = snapshot.submap;
    const auto& g = baked::grids(*sm);
    auto req = walk_request(*sm,cell_of(*sm,snapshot.position,snapshot.layer));
    world_mode(req);
    req.cancelled = cancelled;
    const auto live = live_blockers(*sm,g,{});
    req.blocked = live.hard(); req.directional = live.directional;
    req.live_hazards = true; req.hazards = live.triggers;
    const auto reach = baked_route::flood(req);
    const auto landed = land_after_ride(req,reach);
    auto foot = req; foot.vehicle_group = foot.vehicle_type = foot.landing_group = 0;
    std::vector<int> result(targets.size(),-1);
    for (size_t i=0; i<targets.size(); ++i) {
        if (cancelled && cancelled()) return result;
        const auto& target = targets[i];
        if (target.map_id > 0 && target.map_id != snapshot.map_id) {
            MapGoal goal; goal.map_id = target.map_id; goal.cell = target.cell;
            const auto leg = find_across(nullptr,nullptr,goal);
            if (leg.found) result[i] = leg.steps_total;
            continue;
        }
        const Cell at = cell_of(*sm,target.position,snapshot.layer);
        for (const auto& object : g.objects) {
            if (object.type != baked::ObjType::GotoMap || object.a != target.destination || !object.contains(at.x,at.y)) continue;
            if (!snapshot.state.active(*sm,object)) continue;
            // Exact trigger footprint: sibling arcs may improve a selected route,
            // but cannot make a decorative or unreachable list row eligible.
            auto movement = req;
            movement.goals.clear();
            for (int y=object.ty; y<object.ty+object.th; ++y) for (int x=object.tx; x<object.tx+object.tw; ++x) {
                if (baked_route::transition_enterable(movement,g,x,y)) movement.goals.push_back({sm->world ? 1 : object.layer,x,y});
            }
            int distance = baked_route::transition_distance(movement,reach);
            if (sm->world && req.vehicle_group > 0) {
                foot.goals.clear();
                for (int y=object.ty; y<object.ty+object.th; ++y) for (int x=object.tx; x<object.tx+object.tw; ++x)
                    if (baked_route::transition_enterable(foot,g,x,y)) foot.goals.push_back({1,x,y});
                const int land = baked_route::transition_distance(foot,landed);
                if (land >= 0 && (distance < 0 || land < distance)) distance = land;
            }
            if (distance < 0 && sm->world && capture.with_vehicles && req.vehicle_group == 0 && !movement.goals.empty()) {
                std::vector<Phase> phases;
                if (boarding_route(*sm,g,movement,movement.goals,phases)) {
                    distance = 0;
                    for (const auto& phase : phases) distance += std::max(0,static_cast<int>(phase.cells.size())-1);
                }
            }
            if (distance >= 0 && (result[i]<0 || distance<result[i])) result[i] = distance;
        }
    }
    return result;
}
void request_reachability(bool with_vehicles, std::function<void()> ready) {
    ensure_quick(quick_movement(field::player_position(), field::map_handle(), field::player(), with_vehicles),
        [with_vehicles, ready = std::move(ready)]() mutable {
            auto now = quick_movement(field::player_position(), field::map_handle(), field::player(), with_vehicles);
            if (now.submap && (!g_quick_grid || !same_movement(now, g_quick_grid->movement)))
                request_reachability(with_vehicles, std::move(ready));
            else ready();
        });
}
void request_route(MapGoal goal, TargetKind kind, std::wstring name,
                   std::function<void(PathInfo)> publish, bool keep_destination) {
    cancel_route_work();
    const uint64_t serial = g_request_serial;
    void* map = field::map_handle(); void* pl = field::player();
    const auto* submap = current_submap(map);
    if (!submap || !pl || !unity::transform(pl)) {
        PathInfo missing;
        missing.reason = missing.error = T(L"Navigation map data unavailable");
        publish(std::move(missing)); return;
    }
    auto snapshot = std::make_shared<RouteSnapshot>(map_state::Resolver());
    snapshot->requested_at = frame::now();
    snapshot->map_id = field::current_map_id(); snapshot->submap = submap;
    snapshot->position = unity::local_position(unity::transform(pl));
    snapshot->layer = submap->world ? 1 : player_layer(pl);
    snapshot->vehicle = field::current_vehicle();
    snapshot->locale = modtext::lang();
    snapshot->epoch = g_route_owner.epoch(); snapshot->generation = snapshot->epoch->load();
    snapshot->goal = std::move(goal); snapshot->kind = kind; snapshot->name = std::move(name);
    snapshot->keep_destination = keep_destination;
    snapshot->previous_active = g_published_route;
    if (g_pending) snapshot->pending = *g_pending;
    // Native pointers stay in this owner-thread capture closure, never in Snapshot/Work.
    auto entities = std::make_shared<EntityCursor>();
    g_request_pending = true;
    frame::every_frame([snapshot, entities, serial, publish = std::move(publish)]() mutable {
        if (serial != g_request_serial) return false;
        if (!ffpr::system::is_field_navigation_ready() || field::current_map_id() != snapshot->map_id) {
            cancel_route_work(); return false;
        }
        bool more = true;
        CaptureBudget budget;
        for (; budget.available();) {
            void* fe = nullptr;
            if (!(more = entities->next(fe))) break;
            if (!live_entity(fe) || !entity_is_active(fe) || !unity::transform(fe)) continue;
            snapshot->obstacles.push_back(capture_obstacle(fe, *snapshot));
        }
        if (more) return true;
        snapshot->auto_groups = field::owned_auto_vehicle_groups();
        snapshot->vehicles = field::vehicles_on_map();
        for (auto& vehicle : snapshot->vehicles) {
            vehicle.game = nullptr;
            snapshot->vehicle_names.emplace(vehicle.transportation_id,
                field::vehicle_display_name(vehicle.transportation_id, vehicle.type, vehicle.message_id));
        }
        for (const auto& row : baked::transports())
            if (std::find(snapshot->auto_groups.begin(), snapshot->auto_groups.end(), row.group) != snapshot->auto_groups.end())
                snapshot->vehicle_names.emplace(row.id, field::vehicle_display_name(row.id, row.type, baked::str(row.message)));
        snapshot->captured_at = frame::now();
        worker_job::start(g_route_owner, std::move(*snapshot), &plan_snapshot,
            [serial, publish = std::move(publish)](Planned result) mutable {
                if (serial != g_request_serial) return;
                if (!still_current(result)) {
                    g_request_pending = false;
                    if (result.map_id > 0 && result.map_id == field::current_map_id() && ffpr::system::is_field_navigation_ready())
                        request_route(std::move(result.goal), result.kind, std::move(result.name), std::move(publish), result.keep_destination);
                    else if (result.map_id == 0) {
                        PathInfo failure; failure.reason = failure.error = T(L"Navigation planner unavailable");
                        publish(std::move(failure));
                    }
                    return;
                }
                const auto name = result.path.next_map_id > 0 ? resolve_map_name_by_id(result.path.next_map_id) : std::wstring();
                if (result.path.next_map_id <= 0) {
                    // Same-floor routes already have their complete worker text.
                    // Do not add another queue/frame round trip to every request.
                    commit_plan(result);
                    publish(std::move(result.path));
                    return;
                }
                struct Finish { Planned result; std::wstring name; };
                worker_job::start(g_route_owner, Finish{std::move(result), name},
                    [](Finish& finish) { resolve_plan_name(finish.result, finish.name); return std::move(finish.result); },
                    [serial, publish = std::move(publish)](Planned finished) mutable {
                        if (serial != g_request_serial) return;
                        g_request_pending = false;
                        if (!still_current(finished)) {
                            if (finished.map_id == field::current_map_id() && ffpr::system::is_field_navigation_ready())
                                request_route(std::move(finished.goal), finished.kind, std::move(finished.name), std::move(publish), finished.keep_destination);
                            return;
                        }
                        commit_plan(finished);
                        publish(std::move(finished.path));
                    }, background::Lane::Navigation);
            }, background::Lane::Navigation);
        return false;
    }, "BakedNav.Capture", INT_MAX);
}

} // namespace field::baked_nav
