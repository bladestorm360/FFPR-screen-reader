// Ports FF1 Field/FieldNavigationHelper.cs (GetAllFieldEntities, VehicleTypeMap, map exits,
// walls, walkable directions, FindPathTo, path description, directions, distance and steps),
// FF3 Field/HiddenPassageRouting.cs (FF3, FF4, FF5: game::flags().has_hidden_passages), and
// FFI_ScreenReaderMod.GetFieldPlayer / GetCurrentMapId.
//
// From FF5 Field/FieldNavigationHelper.cs (all five games): PathSearchMode (find_path_to = Full,
// find_path_to_quick = Quick, path_search.h), the breadcrumb fallback after a failed search
// (breadcrumb_chain.cpp) and DescribeRoute's six-leg cap with "and N more" for chained routes.
// A route the game's own search finds is described as FF1 described it (every leg).
//
// Per-game differences kept from each game's own mod: Transportation.ModelList skips types 0
// and 1 in FF1 (the canoe, type 5, is skipped by the vehicle detector), and 0, 1, 4 (Symbol)
// and 5 (Content) in FF2-FF5. FF4 and FF5 also keep each vehicle's message id (its official
// name). The C# pointer offsets (TransportationController 0x18, Transportation 0x18,
// FieldController 0x120, FootEvent 0x10) are the fields infoData, modelList, footEvent and
// stepOnTriggerList, resolved by name.
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include "field_api.h"
#include "field_internal.h"
#include "object_cache.h"
#include "path_search.h"
#include "../game.h"
#include "../hooks.h"
#include "../il2cpp.h"
#include "../log.h"
#include "../modtext.h"
#include "../text.h"

namespace field {
namespace {

struct VehicleInfo { int type; std::wstring message_id; };
/// FieldNavigationHelper.VehicleTypeMap. Entries for objects no longer on the map are dropped
/// at each scan, so a reused address can never be taken for a vehicle.
std::unordered_map<void*, VehicleInfo> g_vehicle_types;

using Vec3 = unity::Vec3;

int floor_to_int(float v) { return static_cast<int>(std::floor(v)); }

Vec3 cell_of(int map_width, int map_height, Vec3 world, float z)
{
    return Vec3{static_cast<float>(floor_to_int(map_width * 0.5f + world.x * 0.0625f)),
                static_cast<float>(floor_to_int(map_height * 0.5f - world.y * 0.0625f)), z};
}

/// MapRouteSearcher.Search(mapHandle, start, dest, collisionEnabled): the path list or null.
void* route_search(void* map_handle, Vec3 start, Vec3 dest, bool collision)
{
    return ref::call_static<void*>(r::mSearch, map_handle, start, dest, collision);
}

bool has_points(void* list) { return list && unity::list_count(list) > 0; }

bool skip_transport_type(int type)
{
    if (type == 0 || type == 1) return true;   // None, Player
    // FF2-FF5's mods also skip Symbol (4) and Content (5), internal markers there.
    if (!game::is(game::Id::FF1) && (type == 4 || type == 5)) return true;
    return false;
}

Vec3 normalized(Vec3 v)
{
    float mag = static_cast<float>(std::sqrt(static_cast<double>(v.x * v.x + v.y * v.y + v.z * v.z)));
    if (mag > 1e-5f) return Vec3{v.x / mag, v.y / mag, v.z / mag};
    return Vec3{0, 0, 0};
}

/// GetCardinalDirectionName: the direction of one path segment.
std::wstring cardinal_direction_name(Vec3 dir)
{
    if (std::fabs(dir.x) > 0.4f && std::fabs(dir.y) > 0.4f) {
        if (dir.y > 0 && dir.x > 0) return T(L"Northeast");
        if (dir.y > 0 && dir.x < 0) return T(L"Northwest");
        if (dir.y < 0 && dir.x > 0) return T(L"Southeast");
        if (dir.y < 0 && dir.x < 0) return T(L"Southwest");
    }
    if (std::fabs(dir.y) > std::fabs(dir.x)) return dir.y > 0 ? T(L"North") : T(L"South");
    if (std::fabs(dir.x) > 0.1f) return dir.x > 0 ? T(L"East") : T(L"West");
    return T(L"Unknown");
}

/// The path as straight runs, "North 3": consecutive steps in the same direction merge (FF1
/// DescribePath's loop = FF5 BuildLegs). On a breadcrumb route this also merges the runs across
/// the seams between chained searches ("North 89", not "North 24, North 30, North 35").
std::vector<std::wstring> route_legs(const std::vector<Vec3>& path)
{
    std::vector<std::wstring> segments;
    Vec3 current{0, 0, 0};
    int steps = 0;
    for (size_t i = 1; i < path.size(); ++i) {
        Vec3 dir = normalized(Vec3{path[i].x - path[i - 1].x, path[i].y - path[i - 1].y, path[i].z - path[i - 1].z});
        if (vec_distance(dir, current) < 0.1f) {
            steps++;
        } else {
            if (steps > 0) segments.push_back(cardinal_direction_name(current) + L" " + text::num(steps));
            current = dir;
            steps = 1;
        }
    }
    if (steps > 0) segments.push_back(cardinal_direction_name(current) + L" " + text::num(steps));
    return segments;
}

std::wstring join_legs(const std::vector<std::wstring>& segments, size_t count)
{
    std::wstring out;
    for (size_t i = 0; i < count && i < segments.size(); ++i) {
        if (i) out += L", ";
        out += segments[i];
    }
    return out;
}

/// DescribePath: "North 3, East 2".
std::wstring describe_path(const std::vector<Vec3>& path)
{
    if (path.size() < 2) return T(L"No movement needed");
    std::vector<std::wstring> segments = route_legs(path);
    return join_legs(segments, segments.size());
}

/// FF5 FieldNavigationHelper.MAX_SPOKEN_LEGS: legs spoken before a chained route is summarised.
/// A long route hugs terrain and runs to dozens of short legs; the player walks what they heard
/// and asks again, and the search re-runs from the new position.
constexpr size_t kMaxSpokenLegs = 6;

/// FF5 DescribeRoute, for breadcrumb routes: at most six legs, then "and N more".
std::wstring describe_chained_route(const std::vector<Vec3>& path)
{
    std::vector<std::wstring> legs = path.size() < 2 ? std::vector<std::wstring>{} : route_legs(path);
    if (legs.empty()) return T(L"No movement needed");
    size_t spoken = std::min(kMaxSpokenLegs, legs.size());
    std::wstring out = join_legs(legs, spoken);
    size_t remaining = legs.size() - spoken;
    if (remaining > 0)
        out += L", " + TF(L"and {0} more", {text::num(static_cast<long long>(remaining))});
    return out;
}

/// The target cell on each destination layer (2, 1, 0), then each of its eight neighbours
/// (FF3 SearchTargetAndNeighbours = FF1 FindPathTo's search loop).
void* search_target_and_neighbours(void* map_handle, Vec3 start_cell, Vec3 dest_cell, Vec3 target_world,
                                   int map_width, int map_height, bool collision)
{
    void* path = nullptr;
    for (int z = 2; z >= 0; --z) {
        dest_cell.z = static_cast<float>(z);
        path = route_search(map_handle, start_cell, dest_cell, collision);
        if (has_points(path)) return path;
    }
    static const Vec3 kAdjacent[] = {
        {0, 16, 0},     // north
        {16, 0, 0},     // east
        {0, -16, 0},    // south
        {-16, 0, 0},    // west
        {16, 16, 0},    // northeast
        {16, -16, 0},   // southeast
        {-16, -16, 0},  // southwest
        {-16, 16, 0},   // northwest
    };
    for (const Vec3& off : kAdjacent) {
        Vec3 adjacent{target_world.x + off.x, target_world.y + off.y, target_world.z + off.z};
        Vec3 cell = cell_of(map_width, map_height, adjacent, 0);
        for (int z = 2; z >= 0; --z) {
            cell.z = static_cast<float>(z);
            path = route_search(map_handle, start_cell, cell, collision);
            if (has_points(path)) return path;
        }
    }
    return path;
}

// --- HiddenPassageRouting (FF3 Field/HiddenPassageRouting.cs) -------------------------------
//
// The game routes on MapModel.CurrentMappingData, which FieldController.SetupCurrentMappingData
// builds as CollisionMappingData AND HiddenPassageMappingData, so every hidden-passage cell is
// closed to MapRouteSearcher although the player walks it (movement uses the collision layer
// alone). When the normal search finds nothing, FindPathTo runs it once more with the route
// grid swapped for the collision grid, then puts it back. Maps without passages are untouched.

bool g_passage_logged = false;

void* map_model(void* map_handle)
{
    if (!r::kFieldController.is(map_handle)) {
        if (!g_passage_logged) {
            g_passage_logged = true;
            LOGF("[Routing] Hidden-passage routing unavailable (map handle is not a FieldController); "
                 "targets behind secret passages will report no path");
        }
        return nullptr;
    }
    void* manager = ref::rd<void*>(map_handle, r::fMapManager);
    return manager ? ref::rd<void*>(manager, r::fCurrentMapModel) : nullptr;
}

template <typename F>
void* search_with_passages_open(void* map_handle, F&& search)
{
    void* model = map_model(map_handle);
    if (!model) return nullptr;
    void* passages = ref::rd<void*>(model, r::fHiddenPassageMappingData);
    void* collision = ref::rd<void*>(model, r::fCollisionMappingData);
    void* current = ref::rd<void*>(model, r::fCurrentMappingData);
    if (!passages || !collision || !current) return nullptr;

    ref::call_void(r::mSetCurrentMappingData, model, collision);
    void* result = hooks::guarded_r("Field.SearchWithPassagesOpen", static_cast<void*>(nullptr),
                                    [&] { return search(); });
    ref::call_void(r::mSetCurrentMappingData, model, current);
    return result;
}

/// IsAdjacentTileBlocked: no route from the player's cell to the next cell = a wall.
bool adjacent_tile_blocked(void* player, Vec3 player_pos, Vec3 direction)
{
    return hooks::guarded_r("Field.IsAdjacentTileBlocked", false, [&] {
        void* controller = cache::get_or_refresh(r::kFieldPlayerController);
        if (!controller) return false;
        void* map_handle = ref::rd<void*>(controller, r::fMapHandle);
        if (!map_handle) return false;

        int width = call_virtual_int(r::mGetCollisionLayerWidth, map_handle);
        int height = call_virtual_int(r::mGetCollisionLayerHeight, map_handle);
        if (width <= 0 || height <= 0 || width > 10000 || height > 10000) return false;

        Vec3 start = cell_of(width, height, player_pos, static_cast<float>(game_object_layer(player) - 9));
        Vec3 target{player_pos.x + direction.x, player_pos.y + direction.y, player_pos.z + direction.z};
        Vec3 dest = cell_of(width, height, target, start.z);

        bool collision = ref::rd<bool>(player, r::fIsOnCollision);
        void* path = route_search(map_handle, start, dest, collision);
        return !has_points(path);
    });
}

/// CheckPositionWalkable: FieldController.IsCanMoveToDestPosition(player, ref position).
bool position_walkable(void* player, Vec3 position)
{
    void* map = cache::get(r::kFieldMap);
    void* fc = map ? ref::rd<void*>(map, r::fFieldController) : nullptr;
    if (!fc) return false;
    return ref::call<bool>(r::mIsCanMoveToDestPosition, fc, player, &position);
}

} // namespace

// --- internal ------------------------------------------------------------------------------

void* player_controller() { return cache::get_or_refresh(r::kFieldPlayerController); }
void* field_map() { return cache::get_or_refresh(r::kFieldMap); }

bool vehicle_type_of(void* fe, int& type, std::wstring& message_id)
{
    auto it = g_vehicle_types.find(fe);
    if (it == g_vehicle_types.end()) return false;
    type = it->second.type;
    message_id = it->second.message_id;
    return true;
}

// --- field_api.h ---------------------------------------------------------------------------

void* player()
{
    // FFI_ScreenReaderMod.GetFieldPlayer: the cached controller, then a refreshed one.
    if (void* pc = cache::get(r::kFieldPlayerController))
        if (void* p = ref::rd<void*>(pc, r::fFieldPlayer)) return p;
    if (void* pc = cache::refresh(r::kFieldPlayerController))
        if (void* p = ref::rd<void*>(pc, r::fFieldPlayer)) return p;
    return nullptr;
}

void* map_handle()
{
    void* pc = player_controller();
    return pc ? ref::rd<void*>(pc, r::fMapHandle) : nullptr;
}

unity::Vec3 player_position()
{
    void* p = player();
    void* tr = p ? unity::transform(p) : nullptr;
    return tr ? unity::local_position(tr) : unity::Vec3{};
}

int current_map_id()
{
    // FFI_ScreenReaderMod.GetCurrentMapId: FieldMapProvisionInformation.Instance.CurrentMapId.
    void* info = ref::call_static<void*>(r::mProvisionGetInstance);
    return info ? ref::rd<int32_t>(info, r::fProvisionCurrentMapId) : -1;
}

std::vector<void*> all_field_entities()
{
    std::vector<void*> results;
    std::unordered_set<void*> seen;
    auto add = [&](void* fe) {
        if (fe && seen.insert(fe).second) results.push_back(fe);
    };

    hooks::guarded("FieldNavigationHelper.GetAllFieldEntities", [&] {
        void* map = field_map();
        if (!map) return;
        void* fc = ref::rd<void*>(map, r::fFieldController);
        if (!fc) return;

        void* entity_list = ref::rd<void*>(fc, r::fEntityList);
        int n = unity::list_count(entity_list);
        for (int i = 0; i < n; ++i) add(unity::list_at(entity_list, i));

        // Transportation entities (NeedInteractiveList) and Transportation.ModelList
        if (void* transportation = ref::rd<void*>(fc, r::fTransportation)) {
            hooks::guarded("FieldNavigationHelper.NeedInteractiveList", [&] {
                void* list = ref::call<void*>(r::mNeedInteractiveList, transportation);
                int count = unity::list_count(list);
                for (int i = 0; i < count; ++i) {
                    void* item = unity::list_at(list, i);
                    if (item && r::kFieldEntity.is(item)) add(item);
                }
            });

            hooks::guarded("FieldNavigationHelper.ModelList", [&] {
                void* info_data = ref::rd<void*>(transportation, r::fInfoData);
                void* model_list = info_data ? ref::rd<void*>(info_data, r::fModelList) : nullptr;
                if (!model_list) return;
                std::vector<void*> infos;
                unity::dict_for_each(model_list, [](uint64_t, uint64_t v, void* s) {
                    static_cast<std::vector<void*>*>(s)->push_back(reinterpret_cast<void*>(v));
                }, &infos, 4, 8);
                bool want_message = game::is(game::Id::FF4) || game::is(game::Id::FF5);
                for (void* info : infos) {
                    if (!info) continue;
                    bool enabled = ref::call<bool>(r::mInfoGetEnable, info);
                    int type = ref::call<int32_t>(r::mInfoGetType, info);
                    if (skip_transport_type(type) || !enabled) continue;
                    void* map_object = ref::call<void*>(r::mInfoGetMapObject, info);
                    if (map_object && !seen.count(map_object)) {
                        add(map_object);
                        VehicleInfo vi{type, {}};
                        if (want_message) vi.message_id = ref::call_str(r::mInfoGetMessageId, info);
                        g_vehicle_types[map_object] = vi;
                    }
                }
            });
        }

        // FootEvent.stepOnTriggerList: step-on triggers kept apart from the entity list
        // (e.g. the airship-raising tile)
        hooks::guarded("FieldNavigationHelper.FootEvent", [&] {
            void* foot = ref::rd<void*>(fc, r::fFootEvent);
            void* dict = foot ? ref::rd<void*>(foot, r::fStepOnTriggerList) : nullptr;
            if (!dict) return;
            std::vector<void*> triggers;
            unity::dict_for_each(dict, [](uint64_t, uint64_t v, void* s) {
                static_cast<std::vector<void*>*>(s)->push_back(reinterpret_cast<void*>(v));
            }, &triggers, 4, 8);
            for (void* t : triggers)
                if (t && r::kFieldEntity.is(t)) add(t);
        });
    });

    for (auto it = g_vehicle_types.begin(); it != g_vehicle_types.end();) {
        if (!seen.count(it->first)) it = g_vehicle_types.erase(it);
        else ++it;
    }
    return results;
}

bool is_direction_near_map_exit(unity::Vec3 player_pos, unity::Vec3 dir, const std::vector<unity::Vec3>& exits)
{
    const float tolerance = 12.0f;   // 3/4 tile
    if (exits.empty()) return false;
    Vec3 adjacent{player_pos.x + dir.x, player_pos.y + dir.y, player_pos.z + dir.z};
    for (const Vec3& e : exits) {
        // 2D distance only (X/Y), ignoring Z (collision layer)
        float dx = adjacent.x - e.x, dy = adjacent.y - e.y;
        if (std::sqrt(dx * dx + dy * dy) <= tolerance) return true;
    }
    return false;
}

std::wstring walkable_directions(void* player_obj, void* map)
{
    // FF1's own text (not translated in FF1 either).
    if (!player_obj || !map) return L"Cannot check directions";
    Vec3 pos = unity::position(player_obj);
    const float step = 16.0f;
    std::vector<std::wstring> dirs;
    if (position_walkable(player_obj, Vec3{pos.x, pos.y + step, pos.z})) dirs.push_back(L"North");
    if (position_walkable(player_obj, Vec3{pos.x, pos.y - step, pos.z})) dirs.push_back(L"South");
    if (position_walkable(player_obj, Vec3{pos.x + step, pos.y, pos.z})) dirs.push_back(L"East");
    if (position_walkable(player_obj, Vec3{pos.x - step, pos.y, pos.z})) dirs.push_back(L"West");
    if (dirs.empty()) return L"STUCK - No walkable directions!";
    std::wstring out;
    for (size_t i = 0; i < dirs.size(); ++i) {
        if (i) out += L", ";
        out += dirs[i];
    }
    return out;
}

WallDistances nearby_walls(void* player_obj)
{
    WallDistances w;
    void* tr = player_obj ? unity::transform(player_obj) : nullptr;
    if (!tr) return w;
    Vec3 pos = unity::local_position(tr);
    // GetWallDistance: 0 = the adjacent tile is blocked, -1 = no wall adjacent
    w.north = adjacent_tile_blocked(player_obj, pos, Vec3{0, 16, 0}) ? 0 : -1;
    w.south = adjacent_tile_blocked(player_obj, pos, Vec3{0, -16, 0}) ? 0 : -1;
    w.east = adjacent_tile_blocked(player_obj, pos, Vec3{16, 0, 0}) ? 0 : -1;
    w.west = adjacent_tile_blocked(player_obj, pos, Vec3{-16, 0, 0}) ? 0 : -1;
    return w;
}

namespace {

/// FindPathTo(..., mode). `allow_chain` = PathSearchMode.Full (FF5).
PathInfo find_path(unity::Vec3 from, unity::Vec3 to, void* map, void* player_obj, bool allow_chain)
{
    PathInfo info;
    if (!map) {
        info.error = L"Map handle not available";
        return info;
    }

    bool ok = hooks::guarded_r("FieldNavigationHelper.FindPathTo", false, [&] {
        int width = call_virtual_int(r::mGetCollisionLayerWidth, map);
        int height = call_virtual_int(r::mGetCollisionLayerHeight, map);

        // Unreasonable dimensions: likely a stale map handle
        if (width <= 0 || height <= 0 || width > 10000 || height > 10000) {
            LOGF("[Pathfinding] Invalid map dimensions: %dx%d - mapHandle may be stale", width, height);
            info.error = L"Invalid map dimensions - try rescanning";
            return false;
        }

        Vec3 start = cell_of(width, height, from, 0);
        Vec3 dest = cell_of(width, height, to, 0);
        if (player_obj) start.z = static_cast<float>(game_object_layer(player_obj) - 9);

        void* path = nullptr;
        if (player_obj) {
            bool collision = ref::rd<bool>(player_obj, r::fIsOnCollision);
            path = search_target_and_neighbours(map, start, dest, to, width, height, collision);

            // Secret passages are closed on the game's route grid but open to the player:
            // retry once with them open (FF3-FF5; no-op on maps without hidden passages).
            if (!has_points(path) && collision && game::flags().has_hidden_passages) {
                path = search_with_passages_open(map, [&] {
                    return search_target_and_neighbours(map, start, dest, to, width, height, collision);
                });
            }
        } else {
            path = ref::call_static<void*>(r::mSearchSimple, map, start, dest);
        }

        if (!has_points(path)) {
            // FF5: the plain search came up empty. Only now, and only for an explicit player
            // action (Full), chain several searches to reach past the game's ~31.5-tile window.
            // The chainer stays dormant for a target inside the window or without a player.
            std::vector<Vec3> chained;
            if (allow_chain && player_obj && try_breadcrumb_chain(from, to, map, player_obj, chained)) {
                info.world_path = std::move(chained);
                info.step_count = info.world_path.empty() ? 0 : static_cast<int>(info.world_path.size()) - 1;
                info.description = describe_chained_route(info.world_path);
                return true;
            }
            info.error = L"No path found";
            return false;
        }

        int n = unity::list_count(path);
        info.world_path.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) info.world_path.push_back(unity::list_value<Vec3>(path, i));
        info.step_count = n > 0 ? n - 1 : 0;
        info.description = describe_path(info.world_path);
        return true;
    });

    info.success = ok;
    if (!ok && info.error.empty()) info.error = L"Pathfinding error";
    return info;
}

} // namespace

PathInfo find_path_to(unity::Vec3 from, unity::Vec3 to, void* map, void* player_obj)
{
    return find_path(from, to, map, player_obj, true);
}

PathInfo find_path_to_quick(unity::Vec3 from, unity::Vec3 to, void* map, void* player_obj)
{
    return find_path(from, to, map, player_obj, false);
}

std::wstring direction(unity::Vec3 from, unity::Vec3 to) { return direction_name(from, to); }

float distance(unity::Vec3 from, unity::Vec3 to) { return vec_distance(from, to); }

float distance_to_steps(float d) { return d / 16.0f; }

std::wstring simple_path_description(unity::Vec3 from, unity::Vec3 to)
{
    float d = distance(from, to);
    std::wstring dir = direction(from, to);
    float steps = distance_to_steps(d);
    std::wstring label = std::fabs(steps - 1.0f) < 0.1f ? T(L"step") : T(L"steps");
    // "{steps:F0}": rounded half away from zero, as .NET formats
    double rounded = std::round(static_cast<double>(steps));
    return text::num(static_cast<long long>(rounded)) + L" " + label + L" " + dir;
}

} // namespace field
