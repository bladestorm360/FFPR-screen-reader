#pragma once
#include "../unity.h"

// Small helpers shared by the fieldnav files (waypoints, audio loops, teleport).
//
// Ports the parts of FF1 Core/EntityNavigationManager.cs (EnsureFieldContext) and
// Field/FilterContext.cs (player localPosition) that the waypoint code calls. The entity area
// owns the originals; field_api.h does not declare them, so fieldnav keeps its own copy.
namespace ffpr::fieldnav {

/// FieldPlayer.transform.localPosition (FF1 uses localPosition for every navigation read).
/// False, and `out` = zero, when there is no field player.
bool player_local_position(unity::Vec3& out);

/// UnityEngine.Vector3.Distance.
float distance(unity::Vec3 a, unity::Vec3 b);

/// UnityEngine.Vector3 == Vector3.zero (Unity's approximate equality).
bool is_zero(unity::Vec3 v);

/// EntityNavigationManager.EnsureFieldContext: false (and "Not on map" spoken when the map or
/// player is missing) unless the player is on an active field map with no menu open.
bool ensure_field_context();

} // namespace ffpr::fieldnav
