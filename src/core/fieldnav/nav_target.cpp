// Ports FF1 Core/NavigationTargetTracker.cs.
#include "nav_target.h"
#include "../core.h"

namespace ffpr::fieldnav::nav_target {
namespace {
Kind g_last = Kind::None;

int install()
{
    actions::define("NavigationTargetTracker.MarkEntity", [] { mark_entity(); });
    actions::define("NavigationTargetTracker.MarkWaypoint", [] { mark_waypoint(); });
    actions::define("NavigationTargetTracker.Clear", [] { clear(); });
    queries::define("NavigationTargetTracker.IsEntity", [] { return g_last == Kind::Entity; });
    queries::define("NavigationTargetTracker.IsWaypoint", [] { return g_last == Kind::Waypoint; });
    return 0;
}
core::Install s_install{"NavigationTargetTracker", &install, 90};
} // namespace

Kind last_kind() { return g_last; }
void mark_entity() { g_last = Kind::Entity; }
void mark_waypoint() { g_last = Kind::Waypoint; }
void clear() { g_last = Kind::None; }

} // namespace ffpr::fieldnav::nav_target
