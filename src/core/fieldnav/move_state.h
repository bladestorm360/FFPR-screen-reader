#pragma once
#include <string>

// Ports FF1 Utils/MoveStateHelper.cs: the player's movement state (walking, ship, canoe,
// airship), cached from the FieldPlayer.GetOn / GetOff hooks in vehicle_landing.cpp.
// Explicit vehicle-name queries use the ready native transportation owner, so
// a loaded boarded save does not require a new boarding event.
//
// Other areas read it without this header through queries:
//   "MoveStateHelper.IsOnFoot", "MoveStateHelper.IsControllingShip",
//   "MoveStateHelper.IsInCanoe", "MoveStateHelper.IsControllingAirship"   (bool)
//   "MoveStateHelper.GetMoveStateName"   (text: the name of the current state, as
//                                         GlobalHotkeyHandler.AnnounceCurrentVehicle speaks it)
namespace ffpr::fieldnav::move_state {

// FieldPlayerConstants.MoveState values (identical in all five games).
constexpr int kWalk = 0;
constexpr int kDush = 1;       // dash
constexpr int kAirship = 2;
constexpr int kShip = 3;
constexpr int kLowFlying = 4;
constexpr int kCanoe = 5;      // FF1 has the canoe in the Chocobo slot
constexpr int kGimmick = 6;
constexpr int kUnique = 7;

/// Boarding (GetOn hook): TransportationType -> MoveState. `name` is the vehicle's spoken name
/// (field::vehicle_display_name), kept for the V key; empty = unknown (the state's own word).
void set_vehicle_state(int transportation_type, const std::wstring& name = std::wstring());
/// Disembarking (GetOn(Player) / GetOff hooks).
void set_on_foot();
/// The boarded vehicle's name as given to set_vehicle_state, or empty on foot / unknown.
const std::wstring& vehicle_name();
/// What the V key speaks: the vehicle's name when known, else move_state_name(current()).
std::wstring current_name();

/// MoveStateHelper.AnnounceStateChange (kept public as in FF1; nothing calls it there).
void announce_state_change(int previous_state, int new_state);

int current();
bool is_controlling_ship();
bool is_on_foot();
bool is_in_canoe();
bool is_controlling_airship();
float pathfinding_multiplier();
std::wstring move_state_name(int move_state);

/// MoveStateHelper.ResetState.
void reset();

} // namespace ffpr::fieldnav::move_state
