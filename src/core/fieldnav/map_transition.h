#pragma once
#include <string>
#include <string_view>

// Ports FF1 Patches/MapTransitionPatches.cs and Utils/LocationMessageTracker.cs.
//
// The map change is detected FF1's way: a postfix on SubSceneManagerMainGame.ChangeState for
// the ChangeMap / FieldReady / Player states reads the current map id. A new id clears stuck
// menu flags, fires events::Kind::MapChanged, asks the entity area to rescan
// (field::request_rescan) and speaks "Entering {map}" (queued). The map name comes from the
// entity area through the text query "MapNameResolver.GetCurrentMapName".
//
// Published for other areas:
//   query "MapTransitionPatches.IsScreenFading"            (FadeManager.IsFadeFinish() == false)
//   query "LocationMessageTracker.InMapTransition"
//   text  "LocationMessageTracker.LastMapTransition"
namespace ffpr::fieldnav::map_transition {

/// True while the screen is fading (FadeManager.Instance.IsFadeFinish() is false). Read when
/// asked, as FF1 did (the wall-tone loop reads it every 100 ms).
bool is_screen_fading();

/// MapTransitionPatches.ResetMapTracking (no caller in FF1): resets the location tracker.
/// FF1 also cleared EntityTranslator's untranslated-name log; that belongs to the entity area.
void reset_map_tracking();

} // namespace ffpr::fieldnav::map_transition

namespace ffpr::fieldnav::location_message {

/// LocationMessageTracker.SetLastMapTransition: record "Entering X" and mark the transition.
void set_last_map_transition(const std::wstring& message);
/// LocationMessageTracker.ShouldAnnounceFadeMessage: false for a fade/system message that
/// repeats the last transition ("Cornelia 1F" after "Entering Cornelia 1F"), or that looks like
/// a bare location name when no transition fired (opening the menu). For the fade-message
/// reader (FF1 ScrollMessagePatches).
bool should_announce_fade_message(std::wstring_view message);
/// LocationMessageTracker.Reset (every scene load).
void reset();

bool in_map_transition();
const std::wstring& last_map_transition();

} // namespace ffpr::fieldnav::location_message
