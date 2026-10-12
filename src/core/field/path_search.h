#pragma once
#include "field_api.h"

// Baked-map route modes, without changing field_api.h.
//
//   field::find_path_to        = Full baked routing for explicit player actions.
//   field::find_path_to_quick  = Quick baked routing for per-tick or per-entity callers.
namespace field {

/// `with_vehicles`: on a world map the target also counts as reachable when a vehicle the party
/// has here (a parked ship or airship, the auto-ridden canoe) carries the player to it: the
/// "Current map with vehicles" filter mode (user, 2026-10-11).
PathInfo find_path_to_quick(unity::Vec3 from, unity::Vec3 to, void* map_handle, void* player = nullptr,
                            TargetKind kind = TargetKind::Infer, bool with_vehicles = false);

} // namespace field
