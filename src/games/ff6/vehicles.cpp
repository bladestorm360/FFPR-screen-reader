// FF6's vehicle words for the core's V key (MoveStateHelper.GetMoveStateName). FF6's mod had no
// move-state helper; the core's boarding speech and V key (src/core/fieldnav/vehicle_landing.cpp,
// move_state.cpp) now run in FF6, with FF1's names. FF1 keeps its canoe in the Chocobo move
// state, which FF6 uses for its chocobos (TransportationType 9 and 10 both map to it), so FF6
// names that state "Chocobo", as FF4 does (src/games/ff4/vehicles.cpp). Game content only; the
// other states keep the core's words.
//
// The vehicle names of the entity list (FF6 Field/NavigableEntity.cs VehicleEntity
// .GetVehicleName: the vehicle's own message id, then FF6's table) are in the core's per-game
// table, src/core/field/navigable_entity.cpp.
#include <string>
#include "../../core/core.h"
#include "../../core/game.h"
#include "../../core/modtext.h"
#include "../../core/fieldnav/move_state.h"

namespace ffpr::ff6 {
namespace {

namespace ms = ffpr::fieldnav::move_state;

/// MoveStateHelper.GetMoveStateName with FF6's word for the chocobo state.
std::wstring move_state_name()
{
    int state = ms::current();
    if (state == ms::kCanoe) return T(L"Chocobo");   // FieldPlayerConstants.MoveState.Chocobo = 5
    return ms::move_state_name(state);
}

int install()
{
    queries::define_text("MoveStateHelper.GetMoveStateName", [] { return move_state_name(); });
    return 0;
}

core::Install s_install{"FF6.Vehicles", &install, 540, game::kFF6};

} // namespace
} // namespace ffpr::ff6
