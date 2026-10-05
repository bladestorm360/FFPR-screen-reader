#pragma once

// Ports FF1 Patches/VehicleLandingPatches.cs (VehicleLandingPatches + MovementSpeechPatches).
// The hooks install themselves; these are the two public resets of the C# classes (FF1 never
// calls them; kept for parity).
namespace ffpr::fieldnav::vehicle_landing {

/// VehicleLandingPatches.ResetState: forget the last "landable" state.
void reset_landing_state();
/// MovementSpeechPatches.ResetState: MoveStateHelper.ResetState.
void reset_movement_state();

} // namespace ffpr::fieldnav::vehicle_landing
