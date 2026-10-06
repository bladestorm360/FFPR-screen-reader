#pragma once
// The status area: the character-selection list of the Status menu, the status details screen
// with its stat navigation, and the shared character/master-data helpers they read.
// Ports FF1 Patches/StatusMenuPatches.cs, Menus/StatusDetailsReader.cs,
// Menus/StatusNavigationReader.cs, Menus/StatusStatDefinitions.cs and
// Core/Handlers/StatusNavigationHandler.cs. The config screen (config.h) lives in this folder too.
//
// Published names (for areas that do not include this header):
//   queries  "StatusMenuState.IsActive", "StatusMenuState.ShouldSuppress",
//            "StatusDetailsState.IsActive", "StatusDetailsState.ShouldSuppress",
//            "StatusNavigationTracker.IsNavigationActive", "StatusNavigationTracker.ValidateState",
//            "StatusNavigationTracker.IsActive" (both: the StatusNavigationHandler / KeyContext.Status gate)
//   actions  "StatusNavigationReader.NavigateNext", "StatusNavigationReader.NavigatePrevious",
//            "StatusNavigationReader.JumpToTop", "StatusNavigationReader.JumpToBottom",
//            "StatusNavigationReader.JumpToNextGroup", "StatusNavigationReader.JumpToPreviousGroup",
//            "StatusNavigationReader.ReadCurrentStat", "StatusMenuState.ResetState"
//
// Games: the character-selection reader and the menu-transition clears run in all five games.
// The status details screen is FF1's (Serial.FF1 StatusDetailsController and FF1's stat list);
// FF2-FF5 have their own controllers and stat sets (see status_navigation.cpp).
// Main thread only.
#include <string>
#include "../refs.h"

namespace ffpr::status {

// --- StatusNavigationTracker (Menus/StatusStatDefinitions.cs) -------------------------------
struct Tracker {
    bool is_navigation_active = false;
    int current_stat_index = 0;
    /// OwnedCharacterData of the shown character (party data: lives as long as the party).
    void* current_character_data = nullptr;
    /// The KeyInput StatusDetailsController; validate_state re-checks it with unity::alive.
    void* active_controller = nullptr;

    void reset();
    /// IsNavigationActive, character data, and the controller alive and active in the hierarchy.
    bool validate_state() const;
};
Tracker& tracker();

// --- StatusMenuState / StatusDetailsState ---------------------------------------------------
bool status_menu_active();
void reset_status_menu_state();
bool status_details_active();

// --- StatusDetailsReader --------------------------------------------------------------------
void set_current_character_data(void* data);
void clear_current_character_data();
void* get_current_character_data();
/// "Name. Level N. HP: a / b" for the reader's current character (empty when none).
std::wstring read_status_details(void* controller);

// --- StatusNavigationReader -----------------------------------------------------------------
void initialize_stat_list();
void navigate_next();
void navigate_previous();
void jump_to_next_group();
void jump_to_previous_group();
void jump_to_top();
void jump_to_bottom();
void read_current_stat();

// --- characters (Last.Data.User.OwnedCharacterData / Last.Data.PlayerCharacterParameter) ----
std::wstring character_name(void* data);
int character_job_id(void* data);
int character_current_exp(void* data);
int character_next_exp(void* data);
/// The character's PlayerCharacterParameter, or null.
void* character_parameter(void* data);
int param_confirmed_level(void* param);
/// ConfirmedLevel, or BaseLevel when that read failed (FF1's try/catch fallback).
int param_level_or_base(void* param);
int param_current_hp(void* param);
int param_max_hp(void* param);
/// CurrentMpCountList[level] (FF1 spell charges), 0 when the level is absent.
int param_mp_count(void* param, int level);
int param_power(void* param);
int param_agility(void* param);
int param_vitality(void* param);
int param_intelligence(void* param);
int param_luck(void* param);
int param_attack(void* param);
int param_accuracy_rate(void* param);
int param_defense(void* param);
int param_evasion_rate(void* param);

/// StatusMenuHelpers.GetCurrentJobName: the localized name of the character's job, or empty.
std::wstring current_job_name(void* data);

// --- game data (game_data.cpp) --------------------------------------------------------------
/// MessageManager.Instance.GetMessage(id, isReplace); empty on any miss.
std::wstring game_message(const std::wstring& mes_id, bool is_replace = true);
/// MasterManager.GetList<T>(): the Dictionary<int, T> of one master type, or null.
void* master_list(ref::Class& type);
/// MasterManager.GetList<T>()[id], or null when the list or the id is missing.
void* master_entry(ref::Class& type, int id);
/// Whether a Dictionary<int, int> holds `key`, and its value.
bool int_dict_get(void* dict, int key, int& value);

} // namespace ffpr::status
