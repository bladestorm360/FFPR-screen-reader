#pragma once
#include <vector>
#include "../audio/sound.h"

// Ports FF5 Field/FieldNavigationHelper.cs GetNearbyLandingSpots / CheckAdjacentLanding and
// Field/Routing/RoutingAdapter.cs TryGetCurrentTransportId: the test behind the landing pings
// (audio_loops.cpp). FF5's addition, in the core for every game.
//
// The game names it reads (FieldController.ConvertWorldPositionToCellPosition / GetCellAttribute,
// TransportationController.CurrentTransportation / CheckLandingList / CheckOkList,
// TransportationInfo.Id) exist with the same signatures in all five dump.cs files.
namespace ffpr::fieldnav::landing_spots {

/// The neighbouring tiles (north, south, east, west of the player) where the current vehicle can
/// land or dock: the tile's attribute is in the vehicle's landing list, or not in its OK list
/// (a tile a ship cannot sail on is land). `out` is cleared first; empty when there is no field,
/// no current vehicle, or a read fails (a failed direction is skipped).
void nearby(void* field_player, std::vector<sound::Direction>& out);

} // namespace ffpr::fieldnav::landing_spots
