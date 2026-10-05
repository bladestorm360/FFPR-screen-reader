#pragma once
#include <string>
#include <vector>
#include "../unity.h"

// The field-navigation contract between the entity area (src/core/field: EntityScanner,
// FieldNavigationHelper, EntityNavigationManager, MapNameResolver) and its users (waypoints,
// audio loops, map transitions, vehicle landing, input). The entity area implements every
// function here; the others call them. Main thread only. All functions are safe off the field
// (they return empty / invalid results).
//
// Ports FF1 Field/FieldNavigationHelper.cs (public API) and the entity-selection parts of
// Core/FFI_ScreenReaderMod.cs and Core/EntityNavigationManager.cs.
namespace field {

/// FieldNavigationHelper.PathInfo
struct PathInfo {
    bool success = false;
    std::wstring error;
    int step_count = 0;
    std::wstring description;
    std::vector<unity::Vec3> world_path;
};

/// FieldNavigationHelper.WallDistances (tiles to the nearest wall; -1 = none in range)
struct WallDistances {
    int north = -1, south = -1, east = -1, west = -1;
};

/// The entity the player has selected with the entity keys (EntityScanner.CurrentEntity).
struct Target {
    bool valid = false;
    unity::Vec3 position{};
    std::wstring name;            // as announced
    const void* identity = nullptr;   // the game object, as an identity only
    int category = 0;             // the scanner's category index
};

// --- player and map ---------------------------------------------------------------------
/// The live FieldPlayer (FFI_ScreenReaderMod.GetFieldPlayer), or null off the field.
void* player();
/// The field's map accessor (IMapAccessor used by FieldNavigationHelper), or null.
void* map_handle();
unity::Vec3 player_position();
/// FFI_ScreenReaderMod.GetCurrentMapId: the current map id, or -1.
int current_map_id();
/// World positions of the current map's exits (for wall tones near exits).
std::vector<unity::Vec3> map_exit_positions();

// --- FieldNavigationHelper ---------------------------------------------------------------
PathInfo find_path_to(unity::Vec3 from, unity::Vec3 to, void* map_handle, void* player = nullptr);
bool is_direction_near_map_exit(unity::Vec3 player_pos, unity::Vec3 direction,
                                const std::vector<unity::Vec3>& exits);
std::wstring walkable_directions(void* player, void* map_handle);
WallDistances nearby_walls(void* player);
std::wstring direction(unity::Vec3 from, unity::Vec3 to);
float distance(unity::Vec3 from, unity::Vec3 to);
float distance_to_steps(float distance);
std::wstring simple_path_description(unity::Vec3 from, unity::Vec3 to);
/// Every FieldEntity on the map (FieldNavigationHelper.GetAllFieldEntities).
std::vector<void*> all_field_entities();

// --- selection ---------------------------------------------------------------------------
Target selected();
/// The rescan the C# mod runs after a scene load / map change (DelayedInitialScan).
void request_rescan();

} // namespace field
