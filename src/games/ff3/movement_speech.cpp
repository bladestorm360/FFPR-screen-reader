// Ports FF3 Patches/MovementSpeechPatches.cs and the FF3 differences of Utils/MoveStateHelper.cs
// (GetMoveStateName's "Chocobo", TransportationToMoveState's Boko).
//
// Boarding and leaving vehicles: "On ship", "On chocobo", "On magical armor", "On foot".
// The core (src/core/fieldnav/vehicle_landing.cpp) ports FF1's GetOn/GetOff postfixes with FF1's
// vehicle table, which is wrong for FF3: TransportationType 5 (Content) is FF1's canoe but an
// intermediate cinematic state in FF3, and FF3's Boko (11) and Magical Armor (12) have no name.
// module.cpp therefore switches off the core's "FieldPlayer.GetOn" / "FieldPlayer.GetOff" for
// FF3, and this file installs FF3's three hooks instead: FieldController.ChangeTransportation
// (FF3's primary hook) plus GetOn / GetOff (attached by address: the names are disabled). The
// shared move state (footsteps, "Can land", pathfinding, V key) is still the core's
// move_state, fed with FF3's mapping.
//
// Parity notes:
//  * Wording is FF1's and FF3's ("On {0}", "On foot"); speech interrupts as in FF1 (the core).
//    FF3 queued these lines - drift, FF1 kept.
//  * lastTransportationId / lastAnnouncedTransportId (DEDUP_INVENTORY FF3 trackers #27-28) are a
//    transition state shared by three hooks, kept as they were; their reset methods had no caller.
//  * Boko feeds the core move state as a chocobo (FF3 mapped Boko to MOVE_STATE_CHOCOBO, the
//    same slot as the core's kCanoe); the V key says "Chocobo" for that state in FF3.
// ChangeTransportation (0x27A090), GetOn (0xF17A00), GetOff (0xF17710) are unique in FF3.
#include <cstdint>
#include <string>
#include "../../core/core.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/field/field_api.h"
#include "../../core/fieldnav/move_state.h"

namespace ffpr::ff3 {
namespace {

namespace ms = ffpr::fieldnav::move_state;

ref::Class kFieldController{"Last.Map", "FieldController", game::kFF3};
ref::Method mChangeTransportation{kFieldController, "ChangeTransportation", 4};
ref::Class kFieldPlayer{"Last.Entity.Field", "FieldPlayer", game::kFF3};
ref::Method mGetOn{kFieldPlayer, "GetOn", 3};    // (int typeId, float uniqueSpeedScale, bool isBackground)
ref::Method mGetOff{kFieldPlayer, "GetOff", 2};  // (int typeId, int layer)

// MapConstants.TransportationType (FF3).
constexpr int kNone = 0;
constexpr int kPlayer = 1;
constexpr int kShip = 2;
constexpr int kPlane = 3;
constexpr int kSymbol = 4;
constexpr int kContent = 5;
constexpr int kSubmarine = 6;
constexpr int kLowFlying = 7;
constexpr int kSpecialPlane = 8;
constexpr int kYellowChocobo = 9;
constexpr int kBlackChocobo = 10;
constexpr int kBoko = 11;
constexpr int kMagicalArmor = 12;

int g_last_transportation = kPlayer;   // lastTransportationId
int g_last_announced = -1;             // lastAnnouncedTransportId

std::wstring transportation_name(int id)
{
    switch (id) {
    case kShip: return T(L"ship");
    case kPlane:
    case kLowFlying:
    case kSpecialPlane: return T(L"airship");
    case kSubmarine: return T(L"submarine");
    case kYellowChocobo:
    case kBlackChocobo:
    case kBoko: return T(L"chocobo");
    case kMagicalArmor: return T(L"magical armor");
    default: return {};
    }
}

bool is_intermediate(int id) { return id == kNone || id == kSymbol || id == kContent; }
bool is_vehicle(int id) { return id != kNone && id != kPlayer && id != kSymbol && id != kContent; }

/// MoveStateHelper.SetVehicleState with FF3's table (Boko is a chocobo).
void set_vehicle_state(int id, const std::wstring& name = std::wstring())
{
    ms::set_vehicle_state(id == kBoko ? kYellowChocobo : id, name);
}

/// The game's own name for the vehicle the TransportationController now holds, when that is
/// the vehicle asked about (by master id or by type): the master's message_id, or FF3's
/// key-item name for the canoe (module.cpp). Empty = use the per-type words.
std::wstring game_name(int id_or_type, bool by_master_id, int& type_out)
{
    ::field::VehicleState v = ::field::current_vehicle();
    if (!v.valid) return {};
    if (by_master_id ? v.id != id_or_type : v.type != id_or_type) return {};
    type_out = v.type;
    return ::field::vehicle_game_name(v.id, v.message_id);
}

// ChangeTransportation_Postfix. `id` is the transportation MASTER id (the C# compared it with
// the TransportationType constants; since 2026-10-10 the type comes from the controller's
// current vehicle when it is this id, and the name is the game's own).
void on_change_transportation(int id)
{
    if (id == g_last_announced) return;
    const int previous = g_last_transportation;
    g_last_transportation = id;
    if (previous == -1) return;

    int type = id;   // the C# reading, kept when the controller does not know this id
    std::wstring name = game_name(id, true, type);
    if (is_intermediate(type)) return;   // cinematics and transitions
    if (name.empty()) name = transportation_name(type);

    std::wstring announcement;
    if (!is_vehicle(previous) && is_vehicle(type)) {
        if (!name.empty()) {
            announcement = TF(L"On {0}", {name});
            set_vehicle_state(type, name);
        }
    } else if (is_vehicle(previous) && id == kPlayer) {
        announcement = T(L"On foot");
        ms::set_on_foot();
    }
    if (announcement.empty()) return;
    g_last_announced = id;
    ffpr::say(announcement, true);
}

// GetOn_Postfix (`id` is the TransportationType here)
void on_get_on(int id)
{
    if (id == g_last_announced) return;
    int type = id;
    std::wstring name = game_name(id, false, type);
    if (name.empty()) name = transportation_name(id);
    if (name.empty()) return;
    set_vehicle_state(id, name);
    g_last_announced = id;
    g_last_transportation = id;
    ffpr::say(TF(L"On {0}", {name}), true);
}

// GetOff_Postfix
void on_get_off(int id)
{
    if (g_last_announced == kPlayer) return;
    const std::wstring name = transportation_name(id);
    ms::set_on_foot();
    g_last_announced = kPlayer;
    g_last_transportation = kPlayer;
    if (!name.empty()) ffpr::say(T(L"On foot"), true);
}

/// MoveStateHelper.GetMoveStateName for FF3: the game's name of the boarded vehicle when known;
/// else the chocobo slot is "Chocobo" (FF1: "Canoe").
std::wstring move_state_name()
{
    if (!ms::is_on_foot() && !ms::vehicle_name().empty()) return ms::vehicle_name();
    const int state = ms::current();
    return state == ms::kCanoe ? T(L"Chocobo") : ms::move_state_name(state);
}

using Change_t = void (*)(void* self, int32_t id, bool change_collision, bool is_background, int32_t view_type,
                          const void* mi);
using GetOn_t = void (*)(void* self, int32_t id, float speed_scale, bool is_background, const void* mi);
using GetOff_t = void (*)(void* self, int32_t id, int32_t layer, const void* mi);
Change_t o_Change;
GetOn_t o_GetOn;
GetOff_t o_GetOff;

void d_Change(void* self, int32_t id, bool change_collision, bool is_background, int32_t view_type, const void* mi)
{
    o_Change(self, id, change_collision, is_background, view_type, mi);
    hooks::guarded("FF3.MovementSpeech.ChangeTransportation", [&] { on_change_transportation(id); });
}

void d_GetOn(void* self, int32_t id, float speed_scale, bool is_background, const void* mi)
{
    o_GetOn(self, id, speed_scale, is_background, mi);
    hooks::guarded("FF3.MovementSpeech.GetOn", [&] { on_get_on(id); });
}

void d_GetOff(void* self, int32_t id, int32_t layer, const void* mi)
{
    o_GetOff(self, id, layer, mi);
    hooks::guarded("FF3.MovementSpeech.GetOff", [&] { on_get_off(id); });
}

int install()
{
    int n = 0;
    n += hooks::attach(mChangeTransportation, &d_Change, &o_Change) ? 1 : 0;
    // The core's GetOn/GetOff hooks are disabled for FF3 (module.cpp): attach by address.
    if (kFieldPlayer.applies()) {
        n += hooks::attach_addr(mGetOn.ptr(), "FF3.FieldPlayer.GetOn", reinterpret_cast<void*>(&d_GetOn),
                                reinterpret_cast<void**>(&o_GetOn)) ? 1 : 0;
        n += hooks::attach_addr(mGetOff.ptr(), "FF3.FieldPlayer.GetOff", reinterpret_cast<void*>(&d_GetOff),
                                reinterpret_cast<void**>(&o_GetOff)) ? 1 : 0;
    }
    if (n != 3) LOGF("[FF3 MoveState] only %d of 3 vehicle hooks", n);

    // Replaces the core's text query (this install runs after it).
    queries::define_text("MoveStateHelper.GetMoveStateName", [] { return move_state_name(); });
    return n;
}

core::Install s_install{"FF3.MovementSpeech", &install, 504, game::kFF3};

} // namespace
} // namespace ffpr::ff3
