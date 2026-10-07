#pragma once
// The bestiary (Extra Library), from the title Extras and from the in-game config menu.
// Ports FF1 Patches/BestiaryPatches.cs, Menus/BestiaryReader.cs and
// Menus/BestiaryNavigationReader.cs. All six games: every class, field and method used is in
// all six dump.cs, and SubSceneManagerExtraLibrary.State / LibraryMenuController.State have the
// same values in all six. Only the main-game library states differ (handled below).
//
// Published names (for areas that do not include this header):
//   queries  "BestiaryStateTracker.IsInBestiary", "BestiaryStateTracker.IsInList",
//            "BestiaryStateTracker.IsInDetail", "BestiaryStateTracker.IsInFormation",
//            "BestiaryStateTracker.IsInMap", "BestiaryNavigationTracker.IsNavigationActive",
//            "BestiaryNavigationTracker.ValidateState", "BestiaryNavigationTracker.IsActive" (both),
//            "BestiaryNavigationReader.IsActive", "ConfigBestiaryStateHandler.WasInConfigBestiary"
//   actions  "BestiaryNavigationReader.NavigateNext", ".NavigatePrevious", ".JumpToTop",
//            ".JumpToBottom", ".JumpToNextGroup", ".JumpToPreviousGroup", ".ReadCurrentStat",
//            "ConfigBestiaryStateHandler.HandleExit",
//            "ConfigBestiaryStateHandler.HandleStateChange.MenuLibraryUi",
//            "ConfigBestiaryStateHandler.HandleStateChange.MenuLibraryInfo"
// Main thread only.
#include <string>
#include <vector>

namespace ffpr::bestiary {

// --- BestiaryReader -------------------------------------------------------------------------

/// Groups for stat navigation, in the UI's order.
enum class StatGroup { MonsterData, Status, Options, Items, Properties };

/// One navigable "Label: Value" entry of the detail view.
struct StatEntry {
    std::wstring label;
    std::wstring value;
    StatGroup group;
    std::wstring to_string() const { return label + L": " + value; }
};

/// "042: Goblin" ("???" for an unseen monster). `picture_book_data` = PictureBookData.
std::wstring read_list_entry(void* picture_book_data);
/// "Bestiary. Encountered: X of Y" for a List<PictureBookData>.
std::wstring read_encounter_summary(void* list);
/// The detail view's visible stats (LibraryInfoContent) plus the dropped items from master data.
std::vector<StatEntry> build_stat_buffer(void* content, void* monster_data);
/// "Formation N: name, name" for a Last.Data.Master.MonsterParty.
std::wstring read_formation(int formation_index, void* party);
/// The monster's habitat name at `map_index` (MonsterData.HabitatNameList).
std::wstring read_map_name(void* monster_data, int map_index);

// --- BestiaryNavigationReader ---------------------------------------------------------------
void nav_initialize(const std::vector<StatEntry>& entries);
void nav_reset();
/// Populated buffer and the tracker active.
bool nav_is_active();
void navigate_next();
void navigate_previous();
void jump_to_next_group();
void jump_to_previous_group();
void jump_to_top();
void jump_to_bottom();
void read_current_stat();

// --- BestiaryNavigationTracker --------------------------------------------------------------
struct Tracker {
    bool is_navigation_active = false;
    /// Last.OutGame.Library.MonsterData under the cursor or in the detail view.
    void* current_monster_data = nullptr;
    /// The KeyInput LibraryInfoController; validate_state re-checks it with unity::alive.
    void* active_controller = nullptr;

    /// Also resets the navigation buffer (BestiaryNavigationReader.Reset).
    void reset();
    bool validate_state() const;
};
Tracker& tracker();

// --- BestiaryStateTracker -------------------------------------------------------------------
// SubSceneManagerExtraLibrary.State: Init 0, List 1, Field 2, Dungeon 3, Info 4, ArTop 5,
// ArBattle 6, GotoTitle 7 (all six games).
bool is_in_bestiary();
bool is_in_list();
bool is_in_detail();
bool is_in_formation();
bool is_in_map();
void clear_state();

// --- ConfigBestiaryStateHandler (the bestiary opened from the in-game config menu) ----------
/// SubSceneManagerMainGame.ChangeState postfix, with the state value. MenuLibraryUi (list) and
/// MenuLibraryInfo (detail) are 19/20 in FF1, 18/19 in FF2, 17/18 in FF3-FF6; other values
/// are ignored, as in FF1.
void config_handle_state_change(int main_game_state);
/// The same, by meaning: list = MenuLibraryUi, else MenuLibraryInfo.
void config_handle_library_state(bool list);
void config_handle_exit();
bool was_in_config_bestiary();

} // namespace ffpr::bestiary
