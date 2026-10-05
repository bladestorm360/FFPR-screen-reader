#pragma once
#include <memory>
#include <string>
#include <vector>
#include "../unity.h"

// The waypoint system, shared by the fieldnav files.
//
// Ports FF1 Field/WaypointEntity.cs (Entity, categories), Core/WaypointManager.cs (store),
// Core/WaypointNavigator.cs (nav). Core/WaypointController.cs and Core/Handlers/WaypointHandler.cs
// are in waypoint_controller.cpp (the "WaypointHandler.*" actions).
//
// Waypoints persist where the C# mod kept them, in the C# format:
// <game folder>\UserData\waypoints.json (AppDomain.BaseDirectory under MelonLoader is the game
// folder), so each game's existing waypoints load unchanged.
namespace ffpr::fieldnav::waypoints {

/// WaypointCategory. Stored as an int because C#'s Enum.TryParse also accepted numbers.
enum Category : int { All = 0, Landmarks = 1, Docks = 2, LandingZones = 3 };
constexpr int kCategoryCount = 4;

/// WaypointEntity: immutable; a rename makes a new one (as in C#), so identity changes.
struct Entity {
    std::wstring id;
    std::wstring name;
    unity::Vec3 position{};
    std::wstring map_id;
    int category = LandingZones;

    /// "Name (Category) (3.5 steps north)".
    std::wstring format_description(unity::Vec3 player_pos) const;
};
using EntityPtr = std::shared_ptr<const Entity>;

/// WaypointEntity.GetCategoryDisplayName: "Landmark" / "Dock" / "Landing Site" / "Waypoint".
std::wstring category_display_name(int category);
/// WaypointEntity.GetCategoryNames()[category]: "All" / "Landmarks" / "Docks" / "Landing Sites".
std::wstring category_name(int category);

// --- WaypointManager --------------------------------------------------------------------
namespace store {
void load();
void save();
std::vector<EntityPtr> for_map(const std::wstring& map_id);
std::vector<EntityPtr> for_category(const std::wstring& map_id, int category);
EntityPtr add(const std::wstring& name, unity::Vec3 position, const std::wstring& map_id,
              int category = LandingZones);
bool remove(const std::wstring& waypoint_id);
bool rename(const std::wstring& waypoint_id, const std::wstring& new_name);
int clear_map(const std::wstring& map_id);
int count_for_map(const std::wstring& map_id);
/// WaypointManager.GetNextWaypointName: "Waypoint {n}" (no caller in FF1).
std::wstring next_waypoint_name(const std::wstring& map_id);
const std::wstring& file_path();
} // namespace store

// --- WaypointNavigator --------------------------------------------------------------------
namespace nav {
/// SelectedWaypoint, or null.
EntityPtr selected();
int count();
int current_category();
void refresh_list(const std::wstring& map_id);
EntityPtr cycle_next();
EntityPtr cycle_previous();
std::wstring cycle_next_category(const std::wstring& map_id);
std::wstring cycle_previous_category(const std::wstring& map_id);
std::wstring format_current_waypoint();
std::wstring category_announcement();
void clear_selection();
} // namespace nav

} // namespace ffpr::fieldnav::waypoints
