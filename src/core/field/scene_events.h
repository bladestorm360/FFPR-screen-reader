#pragma once
#include <string>

// The scene-loaded event of FF1 Core/FFI_ScreenReaderMod.OnSceneLoaded.
//
// The C# mod subscribed to SceneManager.sceneLoaded. Natively the field area detours the
// method that raises it, UnityEngine.SceneManagement.SceneManager.Internal_SceneLoaded (one
// RVA in every game), and after the game's own handlers have run it:
//   1. records the scene names below,
//   2. clears the GameObjectCache (field/object_cache.h), as FF1 did here,
//   3. fires events::Kind::SceneLoaded.
//
// There is one rescan path: fieldnav (audio_loops.cpp) subscribes to SceneLoaded and, 0.5 s
// later (FF1's DelayedInitialScan), calls field::request_rescan() and restarts the audio
// loops; fieldnav also resets the LocationMessageTracker there. The rest of FF1's
// OnSceneLoaded belongs to other areas, which subscribe the same way and read the names here:
// the title press-prompt arm (scene "Title"), the battle-exit clear (previous name contains
// "Battle", current does not), ConfigMenuState/KeyHelp resets, MovementSoundPatches.ResetState
// and GameStatePatches.ResetMainGameState (scene "Title").
namespace field {

/// The scene that has just loaded (valid inside SceneLoaded handlers and after).
const std::wstring& scene_name();
/// The scene loaded before it (FF1's previousSceneName at the moment of the event).
const std::wstring& previous_scene_name();

} // namespace field
