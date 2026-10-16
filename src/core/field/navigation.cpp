// Ports FF1 Field/FieldNavigationHelper.cs (GetAllFieldEntities, VehicleTypeMap, map exits,
// walls, walkable directions, FindPathTo, path description, directions, distance and steps),
// FF3 Field/HiddenPassageRouting.cs (FF3, FF4, FF5: game::flags().has_hidden_passages), and
// FFI_ScreenReaderMod.GetFieldPlayer / GetCurrentMapId.
//
// Routing and walkability use the baked map exclusively. A missing bake is reported as a data
// error; MapRouteSearcher is never used as a fallback.
//
// Per-game differences kept from each game's own mod: Transportation.ModelList skips types 0
// and 1 in FF1 (the canoe, type 5, is skipped by the vehicle detector), and 0, 1, 4 (Symbol)
// and 5 (Content) in FF2-FF6. FF4, FF5 and FF6 also keep each vehicle's message id (its
// official name; FF6 Field/FieldNavigationHelper.cs PopulateVehicleTypeMap). The C# pointer offsets (TransportationController 0x18, Transportation 0x18,
// FieldController 0x120, FootEvent 0x10) are the fields infoData, modelList, footEvent and
// stepOnTriggerList, resolved by name.
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include "baked_map.h"
#include "baked_nav.h"
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

struct VehicleInfo { int type; int id; std::wstring message_id; int map_id; };
/// FieldNavigationHelper.VehicleTypeMap. Entries for objects no longer on the map are dropped
/// at each scan, so a reused address can never be taken for a vehicle. Since 2026-10-10 every
/// game keeps the master id and message id too (field::vehicle_display_name).
std::unordered_map<void*, VehicleInfo> g_vehicle_types;
std::vector<int> g_auto_groups;
int g_auto_map = 0;

using Vec3 = unity::Vec3;

bool skip_transport_type(int type)
{
    if (type == 0 || type == 1) return true;   // None, Player
    // FF2-FF5's mods also skip Symbol (4) and Content (5), internal markers there.
    if (!game::is(game::Id::FF1) && (type == 4 || type == 5)) return true;
    return false;
}

/// IsAdjacentTileBlocked: no route from the player's cell to the next cell = a wall.
bool adjacent_tile_blocked(void* player, Vec3 player_pos, Vec3 direction)
{
    return hooks::guarded_r("Field.IsAdjacentTileBlocked", false, [&] {
        void* controller = cache::get_or_refresh(r::kFieldPlayerController);
        if (!controller) return false;
        void* map_handle = ref::rd<void*>(controller, r::fMapHandle);
        if (!map_handle) return false;
        baked_route::Cell start, target;
        if (!baked_nav::player_cell(map_handle, player, start)) return false;
        Vec3 target_pos{player_pos.x + direction.x, player_pos.y + direction.y, player_pos.z + direction.z};
        if (!baked_nav::cell_at(map_handle, target_pos, target)) return false;
        target.layer = start.layer;
        return !baked_nav::walk_edge(map_handle, player, start, target, false);
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


void* field_map() { return cache::get_or_refresh(r::kFieldMap); }

bool vehicle_type_of(void* fe, int& type, int& transportation_id, std::wstring& message_id)
{
    auto it = g_vehicle_types.find(fe);
    if (it == g_vehicle_types.end() || it->second.map_id != current_map_id()) return false;
    type = it->second.type;
    transportation_id = it->second.id;
    message_id = it->second.message_id;
    return true;
}

std::vector<VehicleOnMap> vehicles_on_map()
{
    std::vector<VehicleOnMap> out;
    for (const auto& [fe, info] : g_vehicle_types) {
        if (info.map_id != current_map_id() || !live_entity(fe)) continue;
        void* tr = unity::transform(fe);
        if (!tr) continue;
        VehicleOnMap v;
        v.game = fe;
        v.type = info.type;
        v.transportation_id = info.id;
        v.message_id = info.message_id;
        v.pos = unity::local_position(tr);
        out.push_back(std::move(v));
    }
    return out;
}

std::vector<int> owned_auto_vehicle_groups()
{
    return g_auto_map == current_map_id() ? g_auto_groups : std::vector<int>();
}

// --- field_api.h vehicles --------------------------------------------------------------------

VehicleState current_vehicle()
{
    VehicleState v;
    hooks::guarded("Field.CurrentVehicle", [&] {
        void* map = field_map();
        void* fc = map ? ref::rd<void*>(map, r::fFieldController) : nullptr;
        void* transportation = fc ? ref::rd<void*>(fc, r::fTransportation) : nullptr;
        if (!transportation) return;
        void* info = ref::call<void*>(r::mGetCurrentTransportation, transportation);
        if (!info || !r::kTransportationInfo.is(info)) return;
        v.id = ref::call<int32_t>(r::mInfoGetId, info);
        v.type = ref::call<int32_t>(r::mInfoGetType, info);
        v.message_id = ref::call_str(r::mInfoGetMessageId, info);
        v.valid = true;
    });
    return v;
}

// --- field_api.h ---------------------------------------------------------------------------

void* player()
{
    return current_native_player();
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

bool native_position_walkable(void* pl, Vec3 position) { return position_walkable(pl, position); }

bool EntityCursor::next(void*& entity)
{
    entity = nullptr;
    void* map = field_map();
    void* fc = map ? ref::rd<void*>(map, r::fFieldController) : nullptr;
    if (!fc) return false;
    void* transportation = ref::rd<void*>(fc, r::fTransportation);
    void* info_data = transportation ? ref::rd<void*>(transportation, r::fInfoData) : nullptr;
    if (stage < 2) {
        void* list = ref::rd<void*>(fc, stage == 0 ? r::fEntityList : r::fColliderEntityList);
        if (index < unity::list_count(list)) entity = unity::list_at(list, index++);
        else { ++stage; index = 0; }
    } else if (stage == 2) {
        void* dict = info_data ? ref::rd<void*>(info_data, r::fModelList) : nullptr;
        if (index < unity::dict_count(dict)) {
            uint64_t key = 0, value = 0;
            if (unity::dict_entry(dict, index++, key, value, 4, 8)) {
                void* info = reinterpret_cast<void*>(value);
                if (info) {
                    void* object = ref::call<void*>(r::mInfoGetMapObject, info);
                    // NeedInteractiveList is only this model dictionary filtered by
                    // CheckNeedInteractive(id,true). Avoid allocating/walking its list.
                    if (object && ref::call<bool>(r::mCheckNeedInteractive, info_data, static_cast<int32_t>(key), true)
                        && r::kFieldEntity.is(object)) entity = object;
                    const int type = ref::call<int32_t>(r::mInfoGetType, info);
                    if (object && !skip_transport_type(type) && ref::call<bool>(r::mInfoGetEnable, info)) {
                        entity = object;
                        g_vehicle_types[object] = VehicleInfo{type, ref::call<int32_t>(r::mInfoGetId, info),
                            ref::call_str(r::mInfoGetMessageId, info), current_map_id()};
                    } else if (object) g_vehicle_types.erase(object);
                    if (type == 5 && ref::call<bool>(r::mInfoGetEnable, info)) {
                        if (const auto* row = baked::transport(ref::call<int32_t>(r::mInfoGetId, info)))
                            if (row->group > 0 && std::find(auto_groups.begin(), auto_groups.end(), row->group) == auto_groups.end())
                                auto_groups.push_back(row->group);
                    }
                }
            }
        } else { g_auto_map = current_map_id(); g_auto_groups = auto_groups; ++stage; index = 0; }
    } else if (stage == 3) {
        void* foot = ref::rd<void*>(fc, r::fFootEvent);
        void* dict = foot ? ref::rd<void*>(foot, r::fStepOnTriggerList) : nullptr;
        if (index < unity::dict_count(dict)) {
            uint64_t key = 0, value = 0;
            if (unity::dict_entry(dict, index++, key, value, 4, 8)) {
                void* item = reinterpret_cast<void*>(value);
                if (item && r::kFieldEntity.is(item)) entity = item;
            }
        } else { ++stage; index = 0; }
    } else {
        for (auto it = g_vehicle_types.begin(); it != g_vehicle_types.end();)
            if (!seen.count(it->first)) it = g_vehicle_types.erase(it); else ++it;
        return false;
    }
    // Retain the established cross-list inclusion guard, not speech deduplication.
    if (entity && !seen.insert(entity).second) entity = nullptr;
    return true;
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

/// FindPathTo(..., mode), using baked map data exclusively.
PathInfo find_path(unity::Vec3 from, unity::Vec3 to, void* map, void* player_obj, bool full, TargetKind kind,
                   bool with_vehicles)
{
    PathInfo info;
    if (!map) {
        info.error = L"Map handle not available";
        return info;
    }

    if (!baked_nav::find(from, to, map, player_obj, full, kind, with_vehicles, info)) {
        info.success = false;
        info.error = T(L"Navigation map data unavailable");
        info.reason = info.error;
    } else if (!info.success && info.error.empty()) {
        info.error = T(L"No path found");
    }
    return info;
}

} // namespace

PathInfo find_path_to(unity::Vec3 from, unity::Vec3 to, void* map, void* player_obj, TargetKind kind)
{
    return find_path(from, to, map, player_obj, true, kind, false);
}

PathInfo find_path_to_quick(unity::Vec3 from, unity::Vec3 to, void* map, void* player_obj, TargetKind kind, bool with_vehicles)
{
    return find_path(from, to, map, player_obj, false, kind, with_vehicles);
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
