#pragma once
// The game's own config screen: the in-game Config menu, the title-screen Options menu, the
// language dropdown, the keyboard/gamepad remap screen and its Controls pop-up.
// Ports FF1 Patches/ConfigMenuPatches.cs and Menus/ConfigMenuReader.cs. All five games (every
// class, field and method used is in all five dump.cs). FF3 and FF5 later moved their slider
// reads to Unity's Slider.onValueChanged; this keeps FF1's SetSliderValue approach.
//
// Published names (for areas that do not include this header):
//   queries  "ConfigMenuState.IsActive", "ConfigMenuState.ShouldSuppress"
//   actions  "ConfigMenuState.ResetState",
//            "ConfigController_SetActive_Patch.ReannounceFocusedConfigOption" (popup closed over
//            the config menu), "ConfigController_SetActive_Patch.ArmReannounceAfterLibrary"
//            (bestiary exit), "ConfigController_SetActive_Patch.OnMenuResumedAfterLibrary"
//            (the library-return fade-in callback)
// Also subscribes events::Kind::SceneLoaded -> ConfigMenuState.ResetState (FF1 OnSceneLoaded).
// Main thread only.
#include <string>

namespace ffpr::config {

// --- ConfigMenuState ------------------------------------------------------------------------
bool is_active();
void set_active(bool active);
/// The generic cursor reader stays silent: the remap screen is up, or the config menu is open
/// (validated: a stale flag with no live ConfigActualDetailsControllerBase is reset).
bool should_suppress();
void reset_state();

// --- ConfigController_SetActive_Patch -------------------------------------------------------
/// A popup over the config menu closed: re-read the focused row now.
void reannounce_focused_config_option();
/// Bestiary exit: arm the re-read for when the config menu is back on screen.
void arm_reannounce_after_library();
/// Library-return fade-in finished: consume the arm.
void on_menu_resumed_after_library();

// --- ConfigMenuReader -----------------------------------------------------------------------
/// The value of a KeyInput ConfigCommandController ("On", "50%", "English"); empty when none.
std::wstring find_config_value_from_controller(void* controller);
/// The value of a Touch ConfigCommandController (arrow text or slider percentage).
std::wstring value_from_touch_command(void* command);
/// The game's current language as an English display name ("English"), empty when unknown.
std::wstring current_language_display_name();
/// A UnityEngine.UI.Slider as "N%" over its range; BGM / SFX rows (by setting name) as the raw
/// 1-10 value. `setting_name` null = no name.
std::wstring slider_percentage(void* slider, const std::wstring* setting_name = nullptr);
/// Not blank, not an arrow glyph, not an editor placeholder.
bool is_valid_config_value(const std::wstring& value);
/// The first valid active UI.Text under an arrow-select root (GameObject), or empty.
std::wstring arrow_text_under(void* root);
/// A UnityEngine.UI.Dropdown's options[value].text, or empty (out of range / missing).
std::wstring dropdown_selected_text(void* dropdown);

} // namespace ffpr::config
