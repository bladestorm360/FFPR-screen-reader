#pragma once
#include <string>

// The cursor move the generic menu reader is handling. Ports the arguments of FF1
// ManualPatches.CursorNavigation_Postfix (Core/FFI_ScreenReaderMod.cs) and of the reads it starts
// (MenuTextDiscovery.WaitAndReadCursor), for the per-menu areas that the generic reader calls
// through queries and actions (which take no arguments).
//
// Inside such a query or action, cursor::current() is the cursor being handled; outside one it
// is empty (cursor == nullptr). The generic reader calls, while it is set:
//   queries  "NewGamePatches.IsHandlingCursor", "JobSelectionPatches.IsHandlingCursor",
//            "ConfigMenuState.ShouldSuppress", "BattleCommandState.ShouldSuppress",
//            "BattleTargetState.ShouldSuppress", "BattleItemMenuState.ShouldSuppress",
//            "BattleMagicMenuState.ShouldSuppress", "ShopMenuTracker.ValidateState",
//            "ItemMenuState.ShouldSuppress", "EquipMenuState.ShouldSuppress",
//            "MagicMenuState.ShouldSuppress", "StatusMenuState.ShouldSuppress",
//            "StatusDetailsState.ShouldSuppress", "BestiaryStateTracker.IsInBestiary",
//            "MusicPlayerStateTracker.IsInMusicPlayer", "GalleryStateTracker.IsInGallery",
//            and every query added with add_suppressor();
//   actions  "NewGamePatches.ClearHandlingFlag", "JobSelectionPatches.ClearHandlingFlag";
//   text     "CharacterSelectionReader.TryReadCharacterSelection" (one frame after the move,
//            from MenuTextDiscovery): return the announcement or "", and put the C# `out count`
//            in current().out_count (leave it -1 when there is no list).
namespace cursor {

struct CursorEvent {
    void* cursor = nullptr;      // Last.UI.Cursor (the hooked NextIndex/PrevIndex/Skip*Index __instance)
    int index = -1;              // cursor.Index after the move
    void* instance = nullptr;    // the hooked method's __instance (the same Cursor)
    void* transform = nullptr;   // cursor.transform
    std::wstring path;           // "grandparent/parent/cursor" GameObject names (the C# cursorPath)
    mutable int out_count = -1;  // a text query reports its list size here (the C# `out int count`)
};

/// The cursor being handled (empty outside the generic reader's calls). Main thread.
const CursorEvent& current();

/// A game module's own screens: the generic reader stays silent while this query is true.
/// Checked after FF1's own list, before the default read. Boot or main thread.
void add_suppressor(const char* query_name);

} // namespace cursor
