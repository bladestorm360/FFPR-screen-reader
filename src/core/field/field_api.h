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
    std::wstring reason;          // spoken when there is no route and the router knows why:
                                  // "Requires Ship", "Story gated", "Blocked by an NPC"
    int step_count = 0;
    std::wstring description;
    std::vector<unity::Vec3> world_path;
    int next_map_id = 0;          // the route continues on this map after its last leg ("take
                                  // the exit to ..."): the target is cut off on this floor or
                                  // lies on another one (baked_nav); 0 = the route ends at the target
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
    int remote_map_id = 0;        // on another floor of this map (the baked data; user,
                                  // 2026-10-11): that floor's map id, 0 = on this map. The
                                  // position is then meaningless; route by the cell below.
    int cell_layer = 1, cell_x = 0, cell_y = 0;
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
/// What a route's target is, for where the route ends (baked_router.h GoalKind): an
/// interaction (NPC, chest, switch) is reached from a neighbouring cell and then faced; a
/// transition (exit, town tile, warp pad) is stepped on; a spot (waypoint, vehicle, save point)
/// is stood on. Infer decides by what is baked at the target cell.
enum class TargetKind { Infer = 0, Interact, Transition, Spot };
PathInfo find_path_to(unity::Vec3 from, unity::Vec3 to, void* map_handle, void* player = nullptr,
                      TargetKind kind = TargetKind::Infer);
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

// --- vehicles ----------------------------------------------------------------------------
/// TransportationController.CurrentTransportation: the vehicle the player is in (or the
/// player row, id 1). The game switches it before FieldPlayer.GetOn runs (FF2
/// FieldController.ChangeTransportation, 2026-10-10), so GetOn hooks may read it.
struct VehicleState {
    bool valid = false;
    int id = 0;                 // transportation master id (FF2: 2 canoe, 5 snowcraft)
    int type = 0;               // MapConstants.TransportationType
    std::wstring message_id;    // the master's message_id ("MSG_SYSTEM_200"), or empty
};
VehicleState current_vehicle();

/// The vehicle's name for speech, one rule for every game and every reader (the boarding
/// line, the V key, the Vehicles category): the game's own text first - the master's
/// message_id, else the game module's key-item message for a row without one
/// (game::Flags::vehicle_message_id) - and the mod's per-type words only as the fallback.
/// `transportation_id` 0 = unknown (type fallback only). Empty when nothing names it.
std::wstring vehicle_display_name(int transportation_id, int type, const std::wstring& message_id);
/// The game's own name only (the first two steps of vehicle_display_name); empty when the
/// game has none, so a caller can keep its own per-type words for those.
std::wstring vehicle_game_name(int transportation_id, const std::wstring& message_id);

// --- selection ---------------------------------------------------------------------------
Target selected();
/// Event-bound asynchronous rescan after native map readiness.
void request_rescan();

} // namespace field
