#pragma once

// Ports FF1 Core/NavigationTargetTracker.cs: whether the player last selected an entity or a
// waypoint. The audio beacon and the controller's left trigger route on it.
//
// Other areas reach it without this header through:
//   actions  "NavigationTargetTracker.MarkEntity" / ".MarkWaypoint" / ".Clear"
//   queries  "NavigationTargetTracker.IsEntity" / ".IsWaypoint"   (LastKind == ...)
namespace ffpr::fieldnav::nav_target {

enum class Kind { None, Entity, Waypoint };

Kind last_kind();
void mark_entity();
void mark_waypoint();
void clear();

} // namespace ffpr::fieldnav::nav_target
