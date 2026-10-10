#pragma once
#include <string>
#include <vector>

// The system area: generic cursor reading and menu-state clearing, save/load, the main-game
// state, the controls display, the item/equipment/magic command bars and the field menu.
// Ports FF1 Core/FFI_ScreenReaderMod.cs (ManualPatches, TryPatchCursorNavigation,
// ClearOtherMenuStates, ClearAllMenuStates, ClearMenuFlagsForMapTransition),
// Patches/CursorNavigationPatches.cs, Menus/MenuTextDiscovery.cs, Patches/SaveLoadPatches.cs,
// Patches/SaveListPatches.cs, Menus/SaveSlotReader.cs, Patches/GameStatePatches.cs,
// Menus/KeyHelpReader.cs, Patches/CommandBarPatches.cs, Menus/CommandBarReader.cs and
// Patches/FieldMenuPatches.cs.
//
// Published names (for areas that do not include this header):
//   queries  "GameStatePatches.IsFieldPlayerState", "KeyHelpReader.IsScreenActive",
//            "CommandBarPatches.IsCommandBarActive", "SaveLoadMenuState.IsActive",
//            "SaveLoadMenuState.ShouldSuppress",
//            "SaveListController_SetActive_Patch.ShouldReadSaveSlot"
//   actions  "KeyHelpReader.NavigateNext", "KeyHelpReader.NavigatePrevious",
//            "KeyHelpReader.JumpToTop", "KeyHelpReader.JumpToBottom",
//            "KeyHelpReader.AnnounceKeyHelp", "KeyHelpReader.CloseControlsHelp",
//            "GameStatePatches.ResetMainGameState", "Mod.ClearAllMenuStates",
//            "Mod.ClearMenuFlagsForMapTransition", "Mod.ClearOtherMenuStates.<Menu>" for <Menu> in
//            Config, BattleCommand, BattleTarget, BattleItem, BattleMagic, Item, Equip, Magic,
//            Status, StatusDetails, Shop (FFI_ScreenReaderMod.ClearOtherMenuStates("<Menu>"))
// Main thread only.
namespace ffpr::system {

// --- MenuTextDiscovery ------------------------------------------------------------------
/// MenuTextDiscovery.WaitAndReadCursor: one frame later, find the cursor's text and speak it
/// with its "(n of N)" position.
void wait_and_read_cursor(void* cursor);

// --- FFI_ScreenReaderMod / ManualPatches -------------------------------------------------
/// FFI_ScreenReaderMod.ClearOtherMenuStates(exceptMenu) ("Config", "Item", "Shop", ...).
void clear_other_menu_states(const std::string& except_menu);
/// FFI_ScreenReaderMod.ClearAllMenuStates / ManualPatches.ClearAllMenuStates.
void clear_all_menu_states();
/// FFI_ScreenReaderMod.ClearMenuFlagsForMapTransition (also runs on events::Kind::MapChanged).
void clear_menu_flags_for_map_transition();

// --- SaveLoadPatches / SaveListPatches / SaveSlotReader -----------------------------------
/// SaveLoadMenuState.ShouldSuppress: a save/load confirmation popup is up.
bool save_load_should_suppress();
/// SaveListController_SetActive_Patch.ShouldReadSaveSlot.
bool should_read_save_slot();
/// SaveSlotReader.TryReadSaveSlot: the slot under the cursor, or "" (count = slot count or -1).
std::wstring try_read_save_slot(void* cursor_transform, int cursor_index, int& count);

// --- FieldMenuPatches ---------------------------------------------------------------------
/// FieldMenuReader.TryGetFieldCommandCount: field-menu commands when `cursor` is that menu's
/// cursor, else -1.
int field_command_count(void* cursor);

// --- GameStatePatches -----------------------------------------------------------------------
/// GameStatePatches.IsFieldPlayerState: the main game is in its Player state.
bool is_field_player_state();
/// The main game is in a field state (Init, ChangeMap, FieldReady, Player, Transport) or its
/// state is not known yet: no menu, shop or battle screen can have focus. Menu readers whose
/// deferred reads can be reached by the game's UI pre-load (the command bars) skip when true.
/// Query "GameStatePatches.IsFieldState".
bool is_field_state();
/// GameStatePatches.ResetMainGameState (FF1: on the "Title" scene load).
void reset_main_game_state();

// --- KeyHelpReader --------------------------------------------------------------------------
/// KeyHelpReader.OpenControlsHelp: the config area's Gamepad/Keyboard Controls pop-up opened
/// (`owner` = the ConfigKeysSettingController, `entries` = the rendered "action: key" lines).
void open_controls_help(void* owner, const std::vector<std::wstring>& entries);
/// KeyHelpReader.CloseControlsHelp.
void close_controls_help();
/// KeyHelpReader.IsScreenActive (drives the KeyHelp key context).
bool key_help_screen_active();

// --- CommandBarPatches / CommandBarReader -----------------------------------------------------
/// CommandBarPatches.IsCommandBarActive.
bool is_command_bar_active();
/// CommandBarReader.Announce(name) / Announce(name, index, count).
void announce_command(const std::wstring& name);
void announce_command(const std::wstring& name, int index, int count);
/// CommandBarReader name maps (Last.Defaine.ShopCommandId, Last.Defaine.UI.ItemCommandId,
/// Last.UI.EquipmentCommandId, Last.Defaine.UI.AbilityCommandId, Last.Defaine.MenuCommandId).
/// "" for an id without a name.
std::wstring shop_command_name(int id);
std::wstring item_command_name(int id);
std::wstring equipment_command_name(int id);
std::wstring ability_command_name(int id);
std::wstring menu_command_name(int id);

} // namespace ffpr::system
