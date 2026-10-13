// Ports FF1 Utils/MoveStateHelper.cs.
#include "move_state.h"
#include "../core.h"
#include "../modtext.h"
#include "../say.h"
#include "../field/field_api.h"
#include "../system/system.h"

namespace ffpr::fieldnav::move_state {
namespace {
// MapConstants.TransportationType values (identical in all five games; FF3/FF5 add 11, 12).
constexpr int kTransportShip = 2;
constexpr int kTransportPlane = 3;          // airship
constexpr int kTransportContent = 5;        // canoe in FF1
constexpr int kTransportSubmarine = 6;
constexpr int kTransportLowFlying = 7;
constexpr int kTransportSpecialPlane = 8;
constexpr int kTransportYellowChocobo = 9;
constexpr int kTransportBlackChocobo = 10;

int g_move_state = kWalk;
int g_transportation_type = 0;
int g_last_announced_state = -1;
std::wstring* g_vehicle_name = nullptr;   // heap: no static destructor at process exit

std::wstring& vehicle_name_storage()
{
    if (!g_vehicle_name) g_vehicle_name = new std::wstring();
    return *g_vehicle_name;
}

int transport_type_to_move_state(int transportation_type)
{
    switch (transportation_type) {
    case kTransportShip: return kShip;
    case kTransportPlane: return kAirship;
    case kTransportContent: return kCanoe;          // FF1: canoe uses the Content slot
    case kTransportSubmarine: return kShip;         // treated like the ship
    case kTransportLowFlying: return kLowFlying;
    case kTransportSpecialPlane: return kAirship;
    case kTransportYellowChocobo:
    case kTransportBlackChocobo: return kCanoe;     // any chocobo maps to canoe (FF1)
    default: return kWalk;
    }
}

int install()
{
    queries::define("MoveStateHelper.IsOnFoot", [] { return is_on_foot(); });
    queries::define("MoveStateHelper.IsControllingShip", [] { return is_controlling_ship(); });
    queries::define("MoveStateHelper.IsInCanoe", [] { return is_in_canoe(); });
    queries::define("MoveStateHelper.IsControllingAirship", [] { return is_controlling_airship(); });
    queries::define_text("MoveStateHelper.GetMoveStateName", [] { return current_name(); });
    return 0;
}
core::Install s_install{"MoveStateHelper", &install, 90};
} // namespace

void set_vehicle_state(int transportation_type, const std::wstring& name)
{
    g_transportation_type = transportation_type;
    g_move_state = transport_type_to_move_state(transportation_type);
    vehicle_name_storage() = name;
}

void set_on_foot()
{
    g_transportation_type = 0;
    g_move_state = kWalk;
    vehicle_name_storage().clear();
}

const std::wstring& vehicle_name() { return vehicle_name_storage(); }

std::wstring current_name()
{
    // Explicit V/controller query: loading a boarded save need not emit GetOn.
    // Read the same ready native transportation owner as the route snapshot.
    if (system::is_field_navigation_ready()) {
        const auto vehicle = ::field::current_vehicle();
        if (vehicle.valid) {
            if (vehicle.type <= 1) return move_state_name(kWalk);
            auto name = ::field::vehicle_display_name(vehicle.id, vehicle.type, vehicle.message_id);
            if (!name.empty()) return name;
            return move_state_name(transport_type_to_move_state(vehicle.type));
        }
    }
    // The game's own name for the boarded vehicle ("Snowcraft", not the state slot's "Canoe").
    if (!is_on_foot() && !vehicle_name_storage().empty()) return vehicle_name_storage();
    return move_state_name(current());
}

void announce_state_change(int previous_state, int new_state)
{
    std::wstring announcement;
    bool has = false;

    if (new_state == kShip) {
        announcement = T(L"On ship");
        has = true;
    } else if (new_state == kCanoe) {
        announcement = T(L"On canoe");
        has = true;
    } else if (new_state == kAirship || new_state == kLowFlying) {
        announcement = T(L"On airship");
        has = true;
    } else if ((previous_state == kShip || previous_state == kCanoe ||
                previous_state == kAirship || previous_state == kLowFlying) &&
               (new_state == kWalk || new_state == kDush)) {
        announcement = T(L"On foot");
        has = true;
    }

    if (has) {
        g_last_announced_state = new_state;
        ffpr::say(announcement, true);
    }
}

int current() { return g_move_state; }

bool is_controlling_ship() { return current() == kShip; }

bool is_on_foot()
{
    int state = current();
    return state == kWalk || state == kDush;
}

bool is_in_canoe() { return current() == kCanoe; }

bool is_controlling_airship()
{
    int state = current();
    return state == kAirship || state == kLowFlying;
}

float pathfinding_multiplier()
{
    switch (current()) {
    case kWalk:
    case kDush: return 1.0f;        // baseline (on foot)
    case kShip: return 2.5f;        // 2.5x scope for ship
    case kCanoe: return 1.5f;       // moderate increase for canoe
    case kAirship:
    case kLowFlying: return 1.0f;   // airship uses a different navigation system
    default: return 1.0f;
    }
}

std::wstring move_state_name(int move_state)
{
    switch (move_state) {
    case kWalk: return T(L"Walking");
    case kDush: return T(L"Dashing");
    case kShip: return T(L"Ship");
    case kAirship: return T(L"Airship");
    case kLowFlying: return T(L"Low Flying");
    case kCanoe: return T(L"Canoe");
    case kGimmick: return T(L"Gimmick");
    case kUnique: return T(L"Unique");
    default: return T(L"Unknown");
    }
}

void reset()
{
    g_move_state = kWalk;
    g_transportation_type = 0;
    g_last_announced_state = -1;
    vehicle_name_storage().clear();
}

} // namespace ffpr::fieldnav::move_state
