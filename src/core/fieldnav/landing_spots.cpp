// Ports FF5 Field/FieldNavigationHelper.cs (GetNearbyLandingSpots, CheckAdjacentLanding) and
// Field/Routing/RoutingAdapter.cs (TryGetCurrentTransportId).
//
// CheckLandingList / CheckOkList index the vehicle list by TransportationInfo.Id (not by its
// TransportationType): no current vehicle means no answer, rather than another vehicle's list.
// A call that throws skips its direction, as the C# catch did (ref::call would return false
// for CheckOkList, which reads as "land" - so these calls report failure instead).
#include <cstdint>
#include "landing_spots.h"
#include "watercraft.h"
#include "../game.h"
#include "../field/field_api.h"
#include "../field/baked_map.h"
#include "../hooks.h"
#include "../log.h"
#include "../refs.h"
#include "../unity.h"
#include "../field/object_cache.h"

namespace ffpr::fieldnav::landing_spots {
namespace {

using unity::Vec2;
using unity::Vec3;

ref::Class kFieldMap{"", "FieldMap"};
ref::Field fFieldController{kFieldMap, "fieldController"};

ref::Class kFieldController{"Last.Map", "FieldController"};
ref::Field fTransportation{kFieldController, "transportation"};
ref::Method mConvertWorldPositionToCellPosition{kFieldController, "ConvertWorldPositionToCellPosition", 1};
ref::Method mGetCellAttribute{kFieldController, "GetCellAttribute", 1};

ref::Class kTransportationController{"Last.Map", "TransportationController"};
ref::Method mGetCurrentTransportation{kTransportationController, "get_CurrentTransportation", 0};
ref::Method mCheckLandingList{kTransportationController, "CheckLandingList", 2};
ref::Method mCheckOkList{kTransportationController, "CheckOkList", 2};

ref::Class kTransportationInfo{"Last.Map", "TransportationInfo"};
ref::Method mInfoGetId{kTransportationInfo, "get_Id", 0};

constexpr float kTileSize = 16.0f;   // GameConstants.TILE_SIZE

/// A compiled IL2CPP instance method, called with a success flag: false when the method is
/// absent or it threw/faulted (the C# catch).
template <typename R, typename... A>
bool try_call(ref::Method& m, void* self, R& out, A... args)
{
    void* fp = m.ptr();
    if (!fp || !self) return false;
    il2cpp::Method* mi = m.get();
    try {
        using F = R (*)(void*, A..., const void*);
        out = reinterpret_cast<F>(fp)(self, args..., mi);
        return true;
    } catch (...) {
        return false;
    }
}

bool g_warned = false;

/// RoutingAdapter.TryGetCurrentTransportId: TransportationController.CurrentTransportation.Id.
bool current_transport_id(void* transport_controller, int32_t& id)
{
    id = -1;
    void* current = nullptr;
    if (!try_call(mGetCurrentTransportation, transport_controller, current)) {
        if (!g_warned) {
            g_warned = true;
            LOGF("[Routing] CurrentTransportation read failed; landing pings stay silent");
        }
        return false;
    }
    if (!current) return false;
    return try_call(mInfoGetId, current, id);
}

/// A legal shore must accept walking, reject the currently ridden watercraft, and satisfy
/// its explicit landing list when it has one. Non-sailable walls are never landing spots.
void check_adjacent(void* field_controller, void* transport_controller, int32_t transport_id, Vec3 player_world,
                    Vec3 offset, sound::Direction direction, std::vector<sound::Direction>& out)
{
    Vec3 adjacent_world{player_world.x + offset.x, player_world.y + offset.y, player_world.z + offset.z};
    Vec3 cell{};
    if (!try_call(mConvertWorldPositionToCellPosition, field_controller, cell, adjacent_world)) return;
    int32_t attribute = 0;
    if (!try_call(mGetCellAttribute, field_controller, attribute, Vec2{cell.x, cell.y})) return;

    bool in_landing_list = false;
    bool in_ok_list = false,walking=false;
    if (!try_call(mCheckLandingList, transport_controller, in_landing_list, transport_id, attribute)) return;
    if (!try_call(mCheckOkList, transport_controller, in_ok_list, transport_id, attribute)) return;
    if (!try_call(mCheckOkList, transport_controller, walking, int32_t(1), attribute)) return;
    const auto* row=::field::baked::transport(transport_id);
    if (!row) return; // unknown landing policy must not announce a wall as a shore
    if (legal_shore(walking,in_ok_list,in_landing_list,row->landing_group>0)) out.push_back(direction);
}

} // namespace

bool riding_watercraft() {
    const auto v=::field::current_vehicle();
    return v.valid && watercraft(game::number(),v.id,v.type);
}

void nearby(void* field_player, std::vector<sound::Direction>& out)
{
    out.clear();
    if (!field_player || !riding_watercraft()) return;

    hooks::guarded("LandingPing.GetNearbyLandingSpots", [&] {
        void* tr = unity::transform(field_player);
        if (!tr) return;

        void* field_map = ::field::cache::get_or_refresh(kFieldMap);
        void* field_controller = field_map ? ref::rd<void*>(field_map, fFieldController) : nullptr;
        if (!field_controller) return;
        void* transport_controller = ref::rd<void*>(field_controller, fTransportation);
        if (!transport_controller) return;

        int32_t transport_id = -1;
        if (!current_transport_id(transport_controller, transport_id)) return;

        Vec3 pos = unity::position(tr);   // transform.position (world), as FF5
        check_adjacent(field_controller, transport_controller, transport_id, pos, Vec3{0, kTileSize, 0},
                       sound::Direction::North, out);
        check_adjacent(field_controller, transport_controller, transport_id, pos, Vec3{0, -kTileSize, 0},
                       sound::Direction::South, out);
        check_adjacent(field_controller, transport_controller, transport_id, pos, Vec3{kTileSize, 0, 0},
                       sound::Direction::East, out);
        check_adjacent(field_controller, transport_controller, transport_id, pos, Vec3{-kTileSize, 0, 0},
                       sound::Direction::West, out);
    });
}

} // namespace ffpr::fieldnav::landing_spots
