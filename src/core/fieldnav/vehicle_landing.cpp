// Ports FF1 Patches/VehicleLandingPatches.cs: both of its classes,
//  * VehicleLandingPatches - MapUIManager.SwitchLandable postfix ("Can land");
//  * MovementSpeechPatches - FieldPlayer.GetOn / GetOff postfixes ("On ship", "On foot").
//
// FF1 has no landing-ping sound: its "landing" feedback is the spoken "Can land" below
// (sound::play_landing_ping is FF5's later addition and is not called here).
//
// FieldPlayer.GetOn differs per game: GetOn(int typeId, bool isBackground) in FF1, FF2, FF4;
// GetOn(int typeId, float uniqueSpeedScale, bool isBackground) in FF3, FF5, FF6. Both detours
// only read typeId. No hook here is folded (one RVA hit each in every dump.cs).
//
// FF6 (no landing or boarding speech in its own mod) gets FF1's: its TransportationType 5 is an
// internal marker there (Content, skipped by FF6's vehicle list), not a canoe, so it has no name.
#include <cstdint>
#include <string>
#include "move_state.h"
#include "vehicle_landing.h"
#include "../core.h"
#include "../hooks.h"
#include "../log.h"
#include "../modtext.h"
#include "../refs.h"
#include "../say.h"
#include "../field/field_api.h"

namespace ffpr::fieldnav::vehicle_landing {
namespace {

// --- VehicleLandingPatches ------------------------------------------------------------------

ref::Class  kMapUIManager{"Last.Map", "MapUIManager"};
ref::Method mSwitchLandable{kMapUIManager, "SwitchLandable", 1};

bool g_last_landable_state = false;

using SwitchLandable_t = void (*)(void* self, bool landable, const void* mi);
SwitchLandable_t o_SwitchLandable = nullptr;

void d_SwitchLandable(void* self, bool landable, const void* mi)
{
    o_SwitchLandable(self, landable, mi);
    hooks::guarded("VehicleLanding.SwitchLandable", [&] {
        // Only announce when in a vehicle (not on foot)
        if (move_state::is_on_foot())
            return;

        // Only announce when entering a landable zone (false -> true)
        if (landable && !g_last_landable_state)
            ffpr::say(T(L"Can land"), false);

        g_last_landable_state = landable;
    });
}

// --- MovementSpeechPatches ------------------------------------------------------------------

// MapConstants.TransportationType values (identical in all five games).
constexpr int kTransportPlayer = 1;
constexpr int kTransportShip = 2;
constexpr int kTransportPlane = 3;          // airship
constexpr int kTransportContent = 5;        // canoe in FF1
constexpr int kTransportSubmarine = 6;
constexpr int kTransportLowFlying = 7;
constexpr int kTransportSpecialPlane = 8;
constexpr int kTransportYellowChocobo = 9;
constexpr int kTransportBlackChocobo = 10;

ref::Class  kFieldPlayer2{"Last.Entity.Field", "FieldPlayer", game::kFF1 | game::kFF2 | game::kFF4};
ref::Class  kFieldPlayer3{"Last.Entity.Field", "FieldPlayer", game::kFF3 | game::kFF5 | game::kFF6};
ref::Class  kFieldPlayer{"Last.Entity.Field", "FieldPlayer"};
ref::Method mGetOn2{kFieldPlayer2, "GetOn", 2};     // (int typeId, bool isBackground)
ref::Method mGetOn3{kFieldPlayer3, "GetOn", 3};     // (int typeId, float uniqueSpeedScale, bool isBackground)
ref::Method mGetOff{kFieldPlayer, "GetOff", 2};     // (int typeId, int layer)

/// Human-readable name for a TransportationType, or empty (FF1 mapping).
std::wstring transportation_name(int type_id)
{
    switch (type_id) {
    case kTransportShip: return T(L"ship");
    case kTransportPlane: return T(L"airship");
    case kTransportContent:                              // FF1: canoe uses the Content slot
        return game::is(game::Id::FF6) ? std::wstring() : T(L"canoe");
    case kTransportSubmarine: return T(L"submarine");
    case kTransportLowFlying: return T(L"airship");      // low-flying mode of the airship
    case kTransportSpecialPlane: return T(L"airship");
    case kTransportYellowChocobo: return T(L"chocobo");
    case kTransportBlackChocobo: return T(L"chocobo");
    default: return std::wstring();
    }
}

/// The boarded vehicle's spoken name: the game's own (field::vehicle_display_name, from the
/// TransportationController's current vehicle, which the game switches before GetOn) when it
/// names the vehicle of this type; else FF1's per-type words. "On canoe" for FF2's snowcraft
/// (both are type 5) ended here on 2026-10-10.
std::wstring boarded_vehicle_name(int type_id)
{
    ::field::VehicleState v = ::field::current_vehicle();
    if (v.valid && v.type == type_id) {
        std::wstring name = ::field::vehicle_game_name(v.id, v.message_id);
        if (!name.empty()) return name;
    }
    return transportation_name(type_id);
}

/// GetOn postfix body: boarding AND disembarking. FF1 uses GetOn(vehicle) to board and
/// GetOn(1 = Player) to return to walking.
void on_get_on(int type_id)
{
    if (type_id == kTransportPlayer) {
        // Only announce if we were previously in a vehicle
        if (!move_state::is_on_foot()) {
            move_state::set_on_foot();
            ffpr::say(T(L"On foot"), true);
        }
        return;
    }

    std::wstring vehicle_name = boarded_vehicle_name(type_id);
    if (!vehicle_name.empty()) {
        std::wstring announcement = TF(L"On {0}", {vehicle_name});
        move_state::set_vehicle_state(type_id, vehicle_name);
        ffpr::say(announcement, true);
    }
}

using GetOn2_t = void (*)(void* self, int32_t type_id, bool is_background, const void* mi);
using GetOn3_t = void (*)(void* self, int32_t type_id, float unique_speed_scale, bool is_background, const void* mi);
using GetOff_t = void (*)(void* self, int32_t type_id, int32_t layer, const void* mi);
GetOn2_t o_GetOn2 = nullptr;
GetOn3_t o_GetOn3 = nullptr;
GetOff_t o_GetOff = nullptr;

void d_GetOn2(void* self, int32_t type_id, bool is_background, const void* mi)
{
    o_GetOn2(self, type_id, is_background, mi);
    hooks::guarded("MovementSpeech.GetOn", [&] { on_get_on(type_id); });
}

void d_GetOn3(void* self, int32_t type_id, float unique_speed_scale, bool is_background, const void* mi)
{
    o_GetOn3(self, type_id, unique_speed_scale, is_background, mi);
    hooks::guarded("MovementSpeech.GetOn", [&] { on_get_on(type_id); });
}

void d_GetOff(void* self, int32_t type_id, int32_t layer, const void* mi)
{
    o_GetOff(self, type_id, layer, mi);
    hooks::guarded("MovementSpeech.GetOff", [&] {
        std::wstring vehicle_name = transportation_name(type_id);
        // A vehicle the game named (boarding said "On <name>") counts as known too.
        const bool was_on_vehicle = !move_state::is_on_foot();
        move_state::set_on_foot();

        // Only announce "On foot" if we were on a known vehicle
        if (!vehicle_name.empty() || was_on_vehicle)
            ffpr::say(T(L"On foot"), true);
    });
}

int install()
{
    int n = 0;
    if (hooks::attach(mSwitchLandable, &d_SwitchLandable, &o_SwitchLandable)) ++n;
    else LOGF("[Landing] Could not hook MapUIManager.SwitchLandable");

    bool get_on = false;
    if (kFieldPlayer2.applies()) get_on = hooks::attach(mGetOn2, &d_GetOn2, &o_GetOn2);
    else if (kFieldPlayer3.applies()) get_on = hooks::attach(mGetOn3, &d_GetOn3, &o_GetOn3);
    if (get_on) ++n;
    else LOGF("[MoveState] Could not hook FieldPlayer.GetOn");

    if (hooks::attach(mGetOff, &d_GetOff, &o_GetOff)) ++n;
    else LOGF("[MoveState] Could not hook FieldPlayer.GetOff");
    return n;
}
core::Install s_install{"VehicleLanding", &install, 120};

} // namespace

void reset_landing_state() { g_last_landable_state = false; }

void reset_movement_state() { move_state::reset(); }

} // namespace ffpr::fieldnav::vehicle_landing
