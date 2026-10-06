// FF4's vehicle words for the core's boarding speech and the V key: ports the name tables of FF4
// Patches/MovementSpeechPatches.cs (GetTransportationName) and Utils/MoveStateHelper.cs
// (GetMoveStateName). The core's tables are FF1's (ship, canoe, airship); FF4's ship slot is the
// hovercraft and its chocobo slot holds chocobos, not a canoe (game content).
//
// FieldPlayer.GetOn / GetOff: the core's subscribers are switched off for FF4 in module.cpp
// configure() (hooks::disable), and these detours replace them with the same behaviour (FF1's
// GetOn / GetOff logic in src/core/fieldnav/vehicle_landing.cpp) and FF4's words. They attach by
// address because the disable list applies to every attach by name.
#include <string>
#include "ff4.h"
#include "../../core/core.h"
#include "../../core/hooks.h"
#include "../../core/modtext.h"
#include "../../core/say.h"
#include "../../core/fieldnav/move_state.h"

namespace ffpr::ff4 {
namespace {

namespace ms = ffpr::fieldnav::move_state;

ref::Class  kFieldPlayer{"Last.Entity.Field", "FieldPlayer", game::kFF4};
ref::Method mGetOn{kFieldPlayer, "GetOn", 2};    // (int typeId, bool isBackground)
ref::Method mGetOff{kFieldPlayer, "GetOff", 2};  // (int typeId, int layer)

// MapConstants.TransportationType (FF4 MoveStateHelper constants).
constexpr int kTransportPlayer = 1;
constexpr int kTransportShip = 2;           // hovercraft
constexpr int kTransportPlane = 3;          // Enterprise
constexpr int kTransportLowFlying = 7;
constexpr int kTransportSpecialPlane = 8;   // Falcon / Lunar Whale
constexpr int kTransportYellowChocobo = 9;
constexpr int kTransportBlackChocobo = 10;

/// GetTransportationName (FF4). Empty for a type FF4 did not announce (the submarine).
std::wstring transportation_name(int type_id)
{
    switch (type_id) {
    case kTransportShip: return T(L"hovercraft");
    case kTransportPlane: return T(L"Enterprise");
    case kTransportSpecialPlane: return T(L"airship");   // the C# tried the Falcon / Lunar Whale name first
    case kTransportLowFlying: return T(L"black chocobo");
    case kTransportYellowChocobo: return T(L"yellow chocobo");
    case kTransportBlackChocobo: return T(L"black chocobo");
    default: return {};
    }
}

/// The core's on_get_on with FF4's words.
void on_get_on(int type_id)
{
    if (type_id == kTransportPlayer) {
        if (!ms::is_on_foot()) {
            ms::set_on_foot();
            ffpr::say(T(L"On foot"), true);
        }
        return;
    }
    std::wstring name = transportation_name(type_id);
    if (name.empty()) return;
    ms::set_vehicle_state(type_id);
    ffpr::say(TF(L"On {0}", {name}), true);
}

using GetOn_t = void (*)(void* self, int32_t type_id, bool is_background, const void* mi);
using GetOff_t = void (*)(void* self, int32_t type_id, int32_t layer, const void* mi);
GetOn_t o_GetOn;
GetOff_t o_GetOff;

void d_GetOn(void* self, int32_t type_id, bool is_background, const void* mi)
{
    o_GetOn(self, type_id, is_background, mi);
    hooks::guarded("FF4 FieldPlayer.GetOn", [&] { on_get_on(type_id); });
}

void d_GetOff(void* self, int32_t type_id, int32_t layer, const void* mi)
{
    o_GetOff(self, type_id, layer, mi);
    hooks::guarded("FF4 FieldPlayer.GetOff", [&] {
        std::wstring name = transportation_name(type_id);
        ms::set_on_foot();
        if (!name.empty()) ffpr::say(T(L"On foot"), true);
    });
}

/// MoveStateHelper.GetMoveStateName (FF4): the V key.
std::wstring move_state_name()
{
    int state = ms::current();
    if (state == ms::kShip) return T(L"Hovercraft");
    if (state == ms::kCanoe) return T(L"Chocobo");   // MOVE_STATE_CHOCOBO = 5
    return ms::move_state_name(state);
}

int install()
{
    int n = 0;
    if (hooks::attach_addr(mGetOn.ptr(), "FF4 FieldPlayer.GetOn(2)", reinterpret_cast<void*>(&d_GetOn),
                           reinterpret_cast<void**>(&o_GetOn))) ++n;
    if (hooks::attach_addr(mGetOff.ptr(), "FF4 FieldPlayer.GetOff(2)", reinterpret_cast<void*>(&d_GetOff),
                           reinterpret_cast<void**>(&o_GetOff))) ++n;
    queries::define_text("MoveStateHelper.GetMoveStateName", [] { return move_state_name(); });
    return n;
}

core::Install s_install{"FF4.Vehicles", &install, 540, game::kFF4};

} // namespace
} // namespace ffpr::ff4
