#pragma once
#include "field_api.h"

// FF5 FieldNavigationHelper.PathSearchMode, without changing field_api.h.
//
//   field::find_path_to        = PathSearchMode.Full: the game's search, the hidden-passage
//                                retry, then breadcrumb chaining past the game's ~31-tile window
//                                (FF5's addition, all five games). For explicit player actions:
//                                announce the selected entity, pathfind to a waypoint.
//   field::find_path_to_quick  = PathSearchMode.Quick: the same without chaining. For callers
//                                that run per tick or per entity (the beacon loop, the
//                                pathfinding filter), which must never pay for chaining (up to
//                                400 ms per target).
namespace field {

/// `with_vehicles`: on a world map the target also counts as reachable when a vehicle the party
/// has here (a parked ship or airship, the auto-ridden canoe) carries the player to it: the
/// "Current map with vehicles" filter mode (user, 2026-10-11).
PathInfo find_path_to_quick(unity::Vec3 from, unity::Vec3 to, void* map_handle, void* player = nullptr,
                            TargetKind kind = TargetKind::Infer, bool with_vehicles = false);

} // namespace field
