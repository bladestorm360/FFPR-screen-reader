// Ports FF1 Patches/ConfigMenuPatches.cs: ConfigMenuState, ConfigActualDetails_SelectCommand_Patch,
// ConfigActualDetails_SwitchArrowSelectType_Patch, ConfigCommandController_SetSliderValue_Patch,
// ConfigActualDetailsTouch_SwitchArrowType_Patch, ConfigActualDetailsTouch_SwitchSliderType_Patch,
// ConfigController_SetActive_Patch, OptionController_SetActive_Patch, OptionController_ShowConfig_Patch,
// OptionController_InitSelectLanguage_Patch and ConfigMenuPatches (controls remap screen, title
// language dropdown, Controls pop-up). All five games: every name is in all five dump.cs and no
// hooked body is folded (each RVA appears once).
//
// Not ported: ConfigCommandController_SetFocus_Patch.ResetTracking (an empty compatibility stub).
// Do NOT hook OptionController.UpdateSelectLanguage: its body is the shared empty stub.
#include <cstring>
#include <memory>
#include <SDL3/SDL_gamepad.h>
#include "config.h"
#include "status.h"
#include "../core.h"
#include "../dedup.h"
#include "../frame.h"
#include "../hooks.h"
#include "../il2cpp.h"
#include "../log.h"
#include "../modtext.h"
#include "../refs.h"
#include "../say.h"
#include "../text.h"
#include "../ui_util.h"
#include "../unity.h"
#include "../field/object_cache.h"
#include "../input/input.h"
#include "../system/system.h"

namespace ffpr::config {
namespace {

// --- game names -----------------------------------------------------------------------------

ref::Class  kDetailsKI{"Last.UI.KeyInput", "ConfigActualDetailsControllerBase"};
ref::Method mSelectCommand{kDetailsKI, "SelectCommand", 2};
ref::Method mSwitchArrowSelectType{kDetailsKI, "SwitchArrowSelectTypeProcess", 2};
ref::Field  fDetailsSelected{kDetailsKI, "<SelectedCommand>k__BackingField"};
ref::Field  fDetailsCommandList{kDetailsKI, "commandList"};

ref::Class  kCmdKI{"Last.UI.KeyInput", "ConfigCommandController"};
ref::Field  fCmdView{kCmdKI, "view"};
ref::Method mSetSliderValue{kCmdKI, "SetSliderValue", 1};
ref::Class  kViewKI{"Last.UI.KeyInput", "ConfigCommandView"};
ref::Field  fViewNameText{kViewKI, "nameText"};
ref::Field  fViewArrowRoot{kViewKI, "arrowSelectTypeRoot"};
ref::Field  fViewSlider{kViewKI, "slider"};

ref::Class  kDetailsTouch{"Last.UI.Touch", "ConfigActualDetailsControllerBase"};
ref::Method mTouchSwitchArrow{kDetailsTouch, "SwitchArrowTypeProcess", 2};
ref::Method mTouchSwitchSlider{kDetailsTouch, "SwitchSliderTypeProcess", 2};
ref::Class  kCmdTouch{"Last.UI.Touch", "ConfigCommandController"};
ref::Field  fCmdTouchView{kCmdTouch, "view"};
ref::Class  kViewTouch{"Last.UI.Touch", "ConfigCommandView"};
ref::Field  fTouchArrowRoot{kViewTouch, "arrowButtonTypeRoot"};
ref::Field  fTouchSliderRoot{kViewTouch, "sliderTypeRoot"};
ref::Class  kSlider{"UnityEngine.UI", "Slider"};

ref::Class  kConfigController{"Last.UI.KeyInput", "ConfigController"};
ref::Method mConfigSetActive{kConfigController, "SetActive", 1};
// The typed detailsController read avoids a FindObjectOfType ambiguity (the controller also
// holds a cheatSettingsController).
ref::Field  fConfigDetails{kConfigController, "detailsController"};

ref::Class  kOption{"Last.UI.KeyInput", "OptionController"};
ref::Method mOptionSetActive{kOption, "SetActive", 1};
ref::Method mOptionShowConfig{kOption, "ShowConfig", 0};
ref::Method mOptionInitSelectLanguage{kOption, "InitSelectLanguage", 0};
ref::Method mOptionSetDropDownItemFocus{kOption, "SetDropDownItemFocus", 0};
ref::Field  fOptionDetails{kOption, "configActualDetailsController"};
ref::Field  fOptionSelectedItem{kOption, "selectedItem"};
ref::Field  fOptionSelectedDropDown{kOption, "selectedDoropDown"};
ref::Field  fOptionDropdownItems{kOption, "dropdownItemList"};
ref::Class  kDropdownItem{"Last.UI.KeyInput", "CommandDropdownItemController"};
ref::Field  fDropdownItemView{kDropdownItem, "view"};
ref::Class  kDropdownItemView{"Last.UI.KeyInput", "CommandDropdownItemView"};
ref::Field  fDropdownItemLabel{kDropdownItemView, "labelText"};

ref::Class  kKeys{"Last.UI.KeyInput", "ConfigKeysSettingController"};
// SelectContent(int, CustomScrollView, Cursor, IEnumerable<ConfigControllCommandController>,
// WithinRangeType): the 5-argument navigation overload (a 2-argument one also exists).
ref::Method mKeysSelectContent{kKeys, "SelectContent", 5};
ref::Method mKeyboardSettingInit{kKeys, "KeyboardSettingInit", 0};
ref::Method mGamePadSettingInit{kKeys, "GamePadSettingInit", 0};
ref::Method mGamePadHelpInit{kKeys, "GamePadHelpInit", 0};
ref::Method mKeyboardHelpInit{kKeys, "KeyboardHelpInit", 0};
ref::Method mGamePadSelectInit{kKeys, "GamePadSelectInit", 0};
ref::Method mKeyboardSelectInit{kKeys, "KeyboardSelectInit", 0};
ref::Method mKeysClose{kKeys, "Close", 0};
// ChangeKeySetting(InputDeviceType, Key, KeyCode) and ChangeKeySetting(Key, InputActionType).
ref::Method mChangeKeySetting3{kKeys, "ChangeKeySetting", 3};
ref::Method mChangeKeySetting2{kKeys, "ChangeKeySetting", 2};
ref::Field  fKeysKeydata{kKeys, "keydata"};
ref::Field  fKeysSelectedCommand{kKeys, "selectedCommand"};
ref::Field  fKeysHelpList{kKeys, "helpContentList"};
ref::Field  fKeysKeyboardHelpList{kKeys, "keyboardHelpContentList"};

ref::Class  kCtrlCmd{"Last.UI.KeyInput", "ConfigControllCommandController"};
ref::Field  fCtrlMessageId{kCtrlCmd, "<MessageId>k__BackingField"};
ref::Field  fCtrlIsMouseKey{kCtrlCmd, "isMouseKey"};
ref::Field  fCtrlKey{kCtrlCmd, "key"};
ref::Field  fCtrlKeyboardIcon{kCtrlCmd, "keyboardIconController"};
ref::Field  fCtrlView{kCtrlCmd, "view"};
ref::Field  fCtrlMessageTexts{kCtrlCmd, "messageTexts"};
ref::Class  kCtrlView{"Last.UI.KeyInput", "ConfigControllCommandView"};
ref::Field  fCtrlNameTexts{kCtrlView, "nameTexts"};
ref::Field  fCtrlGamePadIconsRoot{kCtrlView, "gamePadIconsRoot"};
ref::Field  fCtrlKeyboardIconsRoot{kCtrlView, "keyboardIconsRoot"};
ref::Class  kIconCtrl{"Last.UI.KeyInput", "ConfigKeyIconController"};
ref::Field  fIconView{kIconCtrl, "view"};
ref::Class  kIconView{"Last.UI.KeyInput", "ConfigKeyIconView"};
ref::Field  fIconTextList{kIconView, "iconTextList"};
ref::Class  kKeyConfigData{"Last.Data", "KeyConfigData"};
ref::Method mGetGamePadDict{kKeyConfigData, "GetGamePadKeyConfigtDictionary", 0};

ref::Class  kImage{"UnityEngine.UI", "Image"};
ref::Method mImageSprite{kImage, "get_sprite", 0};

// --- dedup contexts (the existing FF1 checks, DEDUP_INVENTORY.md rows 12-15) -----------------
// Reset where FF1 reset them: the slider pair on config / title Options close, the dropdown
// label whenever its hook fires with the menu closed, the Touch slider row never.
constexpr const char* kCtxSliderRow = "Config.Slider.Row";          // lastSliderPtr (object)
constexpr const char* kCtxSlider = "Config.Slider";                 // lastPercentage (text)
constexpr const char* kCtxTouchSliderRow = "Config.TouchSlider.Row"; // lastTouchController (object)
constexpr const char* kCtxLanguageDropdown = "Config.LanguageDropdown"; // lastDropDownLabel (text)

// ConfigController_SetActive_Patch
bool g_pending_config_reannounce = false;
int g_reannounce_gen = 0;
constexpr int kReannounceMaxFrames = 10;

// ConfigActualDetails_SelectCommand_Patch initial-focus state
bool g_initial_focus_pending = false;
const char* g_initial_focus_source = "";
void* g_initial_focus_option = nullptr;

// ConfigCommandController_SetSliderValue_Patch.ResetTracking
void reset_slider_tracking()
{
    dedup::reset(kCtxSliderRow);
    dedup::reset(kCtxSlider);
}

bool is_list(void* obj)
{
    il2cpp::Class* k = obj ? il2cpp::object_class(obj) : nullptr;
    return k && std::strcmp(il2cpp::class_name(k), "List`1") == 0;
}

// --- ConfigActualDetails_SelectCommand_Patch ------------------------------------------------

// AnnounceConfigCommand: "Name: Value" (or "Name") plus its "(X of Y)" row position.
std::wstring announce_config_command(void* selected, int index, int count)
{
    if (!selected) return {};
    // Skip an inactive controller (scene loading).
    if (!unity::active_in_hierarchy(selected)) return {};
    void* view = ref::rd<void*>(selected, fCmdView);
    if (!view) return {};
    void* name_text = ref::rd<void*>(view, fViewNameText);
    if (!name_text) return {};
    std::wstring raw = unity::ui_text(name_text);
    if (text::is_blank(raw)) return {};
    std::wstring menu_text = text::trim(raw);

    // Template / placeholder values.
    if (menu_text == L"NewText" || menu_text == L"Text" || menu_text == L"Name" || menu_text == L"Label") return {};

    std::wstring value = find_config_value_from_controller(selected);
    std::wstring announcement = text::is_blank(value) ? menu_text : menu_text + L": " + value;
    announcement = ui::menu_position(announcement, index, count);
    ffpr::say(announcement, true);
    return announcement;
}

// AnnounceSelectedConfigCommand: the focused row of a ConfigActualDetailsControllerBase (the
// in-game config and the title Options menu both use this base). Position is best effort.
std::wstring announce_selected_config_command(void* instance)
{
    if (!instance) return {};
    int index = -1, count = -1;
    void* selected = ref::rd<void*>(instance, fDetailsSelected);
    void* list = ref::rd<void*>(instance, fDetailsCommandList);
    if (list && selected) {
        count = unity::list_count(list);
        for (int i = 0; i < count; ++i)
            if (unity::list_at(list, i) == selected) { index = i; break; }
    }
    return announce_config_command(ref::rd<void*>(instance, fDetailsSelected), index, count);
}

using SelectCommand_t = void (*)(void* self, void* cursor, int32_t type, const void* mi);
SelectCommand_t o_SelectCommand;
void d_SelectCommand(void* self, void* cursor, int32_t type, const void* mi)
{
    o_SelectCommand(self, cursor, type, mi);
    hooks::guarded("ConfigActualDetails.SelectCommand", [&] {
        // Gate: only inside a real config menu (ConfigController / title OptionController drive
        // the flag); other screens share this base class.
        if (!is_active()) return;
        if (kDetailsKI.is(self)) field::cache::register_object(kDetailsKI, self);
        announce_selected_config_command(self);
    });
}

// One-frame-delayed initial focus for the TITLE Options menu, whose OptionController does not
// fire SelectCommand on open. The pending flag collapses a same-frame SetActive + ShowConfig pair.
void initial_focus()
{
    g_initial_focus_pending = false;
    void* inst = g_initial_focus_option;
    g_initial_focus_option = nullptr;
    if (!inst || !unity::alive(inst) || !is_active()) return;
    // The active page's configActualDetailsController has its SelectedCommand at init; a blind
    // FindObjectOfType could return the always-present language section, so it is the fallback.
    void* ctrl = ref::rd<void*>(inst, fOptionDetails);
    if (!ctrl) ctrl = field::cache::get(kDetailsKI);
    if (ctrl && kDetailsKI.is(ctrl)) field::cache::register_object(kDetailsKI, ctrl);
    std::wstring announced = announce_selected_config_command(ctrl);
    if (!announced.empty())
        LOGF("[Config Menu] Initial focus announced via %s: %ls", g_initial_focus_source, announced.c_str());
}

void announce_initial_focus_delayed(void* inst, const char* source)
{
    if (g_initial_focus_pending) return;
    g_initial_focus_pending = true;
    g_initial_focus_source = source;
    g_initial_focus_option = inst;
    frame::next_frame([] { hooks::guarded("Config.InitialFocus", [] { initial_focus(); }); }, "Config.InitialFocus");
}

// --- arrow toggles and sliders --------------------------------------------------------------

// ConfigActualDetails_SwitchArrowSelectType_Patch: left/right changed a toggle option.
using SwitchArrow_t = void (*)(void* self, void* controller, int32_t key, const void* mi);
SwitchArrow_t o_SwitchArrowSelectType;
void d_SwitchArrowSelectType(void* self, void* controller, int32_t key, const void* mi)
{
    o_SwitchArrowSelectType(self, controller, key, mi);
    hooks::guarded("ConfigActualDetails.SwitchArrowSelectTypeProcess", [&] {
        void* view = controller ? ref::rd<void*>(controller, fCmdView) : nullptr;
        if (!view) return;
        void* root = ref::rd<void*>(view, fViewArrowRoot);
        if (!root || !unity::active_self(root)) return;
        std::wstring value = arrow_text_under(root);
        if (!value.empty()) ffpr::say(value, true);
    });
}

// ConfigCommandController_SetSliderValue_Patch: the game calls SetSliderValue every frame on the
// focused slider; only a changed percentage (what would be said) is announced.
using SetSliderValue_t = void (*)(void* self, float value, const void* mi);
SetSliderValue_t o_SetSliderValue;
void d_SetSliderValue(void* self, float value, const void* mi)
{
    o_SetSliderValue(self, value, mi);
    hooks::guarded("ConfigCommandController.SetSliderValue", [&] {
        if (!self || !is_active()) return;
        // Skip per-slider initialization fires for non-focused sliders.
        void* details = field::cache::get(kDetailsKI);
        if (!details) return;
        void* selected = ref::rd<void*>(details, fDetailsSelected);
        if (!selected || selected != self) return;

        void* view = ref::rd<void*>(self, fCmdView);
        if (!view) return;
        void* slider = ref::rd<void*>(view, fViewSlider);
        if (!unity::alive(slider)) return;
        void* name_text = ref::rd<void*>(view, fViewNameText);
        std::wstring setting_name = name_text ? unity::ui_text(name_text) : std::wstring();
        std::wstring percentage = slider_percentage(slider, name_text ? &setting_name : nullptr);
        if (percentage.empty()) return;

        // Newly focused slider: SelectCommand already said "Name: Value". Record and skip.
        if (dedup::object(kCtxSliderRow, self, dedup::Scope::Session)) {
            dedup::reset(kCtxSlider);
            dedup::text(kCtxSlider, percentage, dedup::Scope::Session);
            return;
        }
        // Same value: the per-frame UI sync, not an event.
        if (!dedup::text(kCtxSlider, percentage, dedup::Scope::Session)) return;
        ffpr::say(percentage, true);
    });
}

// ConfigActualDetailsTouch_SwitchArrowType_Patch
using TouchSwitchArrow_t = void (*)(void* self, void* controller, int32_t value, const void* mi);
TouchSwitchArrow_t o_TouchSwitchArrow;
void d_TouchSwitchArrow(void* self, void* controller, int32_t value, const void* mi)
{
    o_TouchSwitchArrow(self, controller, value, mi);
    hooks::guarded("ConfigActualDetailsTouch.SwitchArrowTypeProcess", [&] {
        if (kDetailsTouch.is(self)) field::cache::register_object(kDetailsTouch, self);
        void* view = controller ? ref::rd<void*>(controller, fCmdTouchView) : nullptr;
        if (!view) return;
        void* root = ref::rd<void*>(view, fTouchArrowRoot);
        if (!root || !unity::active_self(root)) return;
        std::wstring text_value = arrow_text_under(root);
        if (!text_value.empty()) ffpr::say(text_value, true);
    });
}

// ConfigActualDetailsTouch_SwitchSliderType_Patch: announce only a change on the SAME option.
using TouchSwitchSlider_t = void (*)(void* self, void* controller, float value, const void* mi);
TouchSwitchSlider_t o_TouchSwitchSlider;
void d_TouchSwitchSlider(void* self, void* controller, float value, const void* mi)
{
    o_TouchSwitchSlider(self, controller, value, mi);
    hooks::guarded("ConfigActualDetailsTouch.SwitchSliderTypeProcess", [&] {
        if (kDetailsTouch.is(self)) field::cache::register_object(kDetailsTouch, self);
        void* view = controller ? ref::rd<void*>(controller, fCmdTouchView) : nullptr;
        if (!view) return;
        void* root = ref::rd<void*>(view, fTouchSliderRoot);
        if (!root) return;
        void* slider = unity::get_component_in_children(root, kSlider, false);
        if (!slider) return;
        std::wstring percentage = slider_percentage(slider);
        if (percentage.empty()) return;
        // Moved to a different option: SetFocus says the full "Name: Value".
        if (dedup::object(kCtxTouchSliderRow, controller, dedup::Scope::Session)) return;
        ffpr::say(percentage, true);
    });
}

// --- ConfigController_SetActive_Patch -------------------------------------------------------

// TryAnnounceFocusedRow: true once it spoke (self-gates on the row being active).
bool try_announce_focused_row(void* controller)
{
    if (!controller) return false;
    void* details = ref::rd<void*>(controller, fConfigDetails);
    if (details && kDetailsKI.is(details)) field::cache::register_object(kDetailsKI, details);
    if (!details) return false;   // not ready yet
    return !announce_selected_config_command(details).empty();
}

// Re-read the focused row, retrying at most kReannounceMaxFrames frames until it is active.
void start_reannounce()
{
    int gen = ++g_reannounce_gen;
    auto tries = std::make_shared<int>(0);
    frame::every_frame([gen, tries]() -> bool {
        if (gen != g_reannounce_gen) return false;   // superseded, or the config menu closed
        // One lookup per try: the config menu has a single KeyInput ConfigController.
        bool done = hooks::guarded_r("Config.Reannounce", false, [] {
            return try_announce_focused_row(field::cache::get(kConfigController));
        });
        if (done) return false;
        return ++*tries < kReannounceMaxFrames;
    }, "Config.Reannounce", kReannounceMaxFrames);
}

using SetActive_t = void (*)(void* self, bool is_active, const void* mi);
SetActive_t o_ConfigSetActive;
void d_ConfigSetActive(void* self, bool active, const void* mi)
{
    o_ConfigSetActive(self, active, mi);
    hooks::guarded("ConfigController.SetActive", [&] {
        if (active && kConfigController.is(self)) {
            field::cache::register_object(kConfigController, self);
            if (void* details = ref::rd<void*>(self, fConfigDetails)) field::cache::register_object(kDetailsKI, details);
        }
        if (active) {
            // The config menu's lifecycle drives IsActive, so SelectCommand announces only inside
            // it. No initial-focus read: ConfigController fires SelectCommand for the first row.
            ffpr::system::clear_other_menu_states("Config");
            set_active(true);
        } else {
            reset_state();
            g_pending_config_reannounce = false;   // never let a stale arm leak into the next open
            ++g_reannounce_gen;                    // and drop a read in progress
            reset_slider_tracking();
        }
    });
}

// OptionController_SetActive_Patch: the title-screen Options menu (same pipeline).
SetActive_t o_OptionSetActive;
void d_OptionSetActive(void* self, bool active, const void* mi)
{
    o_OptionSetActive(self, active, mi);
    hooks::guarded("OptionController.SetActive", [&] {
        if (active) {
            // No initial-focus read here: the title Options LIST is the title menu's; the
            // Config-settings sub-screen is read by ShowConfig.
            ffpr::system::clear_other_menu_states("Config");
            set_active(true);
        } else {
            reset_state();
            reset_slider_tracking();
        }
    });
}

using Void_t = void (*)(void* self, const void* mi);

// OptionController_ShowConfig_Patch
Void_t o_OptionShowConfig;
void d_OptionShowConfig(void* self, const void* mi)
{
    o_OptionShowConfig(self, mi);
    hooks::guarded("OptionController.ShowConfig", [&] {
        if (kOption.is(self))
            if (void* details = ref::rd<void*>(self, fOptionDetails)) field::cache::register_object(kDetailsKI, details);
        announce_initial_focus_delayed(self, "OptionController.ShowConfig");
    });
}

// OptionController_InitSelectLanguage_Patch: the language option highlighted (closed dropdown).
Void_t o_OptionInitSelectLanguage;
void d_OptionInitSelectLanguage(void* self, const void* mi)
{
    o_OptionInitSelectLanguage(self, mi);
    hooks::guarded("OptionController.InitSelectLanguage", [&] {
        announce_initial_focus_delayed(self, "InitSelectLanguage");
    });
}

// --- title-screen Language dropdown ---------------------------------------------------------

// GetFocusedLanguageLabel: the tracked dropdown item, else the dropdown value, else (the current
// language's item has an empty label) the current language.
std::wstring focused_language_label(void* inst)
{
    void* item = ref::rd<void*>(inst, fOptionSelectedItem);
    if (!item) return {};
    if (void* view = ref::rd<void*>(item, fDropdownItemView)) {
        if (void* label = ref::rd<void*>(view, fDropdownItemLabel)) {
            std::wstring t = unity::ui_text(label);
            if (!text::is_blank(t)) return t;
        }
    }
    std::wstring opt = dropdown_selected_text(ref::rd<void*>(inst, fOptionSelectedDropDown));
    if (!text::is_blank(opt)) return opt;
    return current_language_display_name();
}

// SetDropDownItemFocus_Postfix: the focused dropdown item changed. Gated on the config menu being
// open: the title builds the language OptionController and fires this before the menu opens.
Void_t o_SetDropDownItemFocus;
void d_SetDropDownItemFocus(void* self, const void* mi)
{
    o_SetDropDownItemFocus(self, mi);
    hooks::guarded("OptionController.SetDropDownItemFocus", [&] {
        if (!self) return;
        std::wstring label = focused_language_label(self);
        if (!is_active()) {
            dedup::reset(kCtxLanguageDropdown);
            return;
        }
        if (text::is_blank(label)) return;
        std::wstring trimmed = text::trim(label);
        // The game fires this twice when the dropdown opens: drop the identical repeat.
        if (!dedup::text(kCtxLanguageDropdown, trimmed, dedup::Scope::Session)) return;

        // Position: the dropdown's value is the COMMITTED selection, so use the focused item's
        // place in the item list.
        int index = -1, count = -1;
        void* list = ref::rd<void*>(self, fOptionDropdownItems);
        void* item = ref::rd<void*>(self, fOptionSelectedItem);
        if (list && item) {
            count = unity::list_count(list);
            for (int i = 0; i < count; ++i)
                if (unity::list_at(list, i) == item) { index = i; break; }
        }
        ffpr::say(ui::menu_position(trimmed, index, count), true);
    });
}

// --- controls remap screen (ConfigKeysSettingController) ------------------------------------

bool has_ci(const std::wstring& s, const wchar_t* token)
{
    auto up = [](wchar_t c) { return (c >= L'a' && c <= L'z') ? static_cast<wchar_t>(c - 32) : c; };
    size_t n = 0;
    while (token[n]) ++n;
    for (size_t i = 0; i + n <= s.size(); ++i) {
        size_t j = 0;
        while (j < n && up(s[i + j]) == up(token[j])) ++j;
        if (j == n) return true;
    }
    return n == 0;
}

bool contains_part(const std::vector<std::wstring>& parts, const std::wstring& s)
{
    for (const std::wstring& p : parts)
        if (p == s) return true;
    return false;
}

// IsRealName: not blank and not an editor placeholder.
bool is_real_name(const std::wstring& s)
{
    if (text::is_blank(s)) return false;
    std::wstring t = text::trim(s);
    return t != L"New Text" && t != L"NewText" && t != L"Text" && t != L"Name" && t != L"Label";
}

// TryGetMessage: a message id through the game's localization, icon markup stripped.
std::wstring try_get_message(const std::wstring& message_id)
{
    if (text::is_blank(message_id)) return {};
    std::wstring t = ffpr::status::game_message(message_id, false);
    if (text::is_blank(t)) return {};
    return text::trim(text::strip_markup(t));
}

// AppendCommandName: remap rows carry their name in view.nameTexts; HELP rows leave nameTexts
// as a "New Text" placeholder and render the name into messageTexts (MessageId as fallback).
void append_command_name(std::vector<std::wstring>& parts, void* command, bool is_help_list)
{
    if (is_help_list) {
        void* msg_texts = ref::rd<void*>(command, fCtrlMessageTexts);
        int n = unity::list_count(msg_texts);
        for (int i = 0; i < n; ++i) {
            void* t = unity::list_at(msg_texts, i);
            if (!t) continue;
            std::wstring s = unity::ui_text(t);
            if (!is_real_name(s)) continue;
            s = text::trim(s);
            if (!contains_part(parts, s)) parts.push_back(s);
        }
        // The rendered text was not ready: resolve the message id directly.
        if (parts.empty()) {
            std::wstring loc = try_get_message(ref::rd_str(command, fCtrlMessageId));
            if (is_real_name(loc)) parts.push_back(text::trim(loc));
        }
        return;
    }

    void* view = ref::rd<void*>(command, fCtrlView);
    void* names = view ? ref::rd<void*>(view, fCtrlNameTexts) : nullptr;
    int n = unity::list_count(names);
    for (int i = 0; i < n; ++i) {
        void* t = unity::list_at(names, i);
        if (!t) continue;
        std::wstring s = unity::ui_text(t);
        if (text::is_blank(s)) continue;
        s = text::trim(s);
        if (!text::starts_with(s, L"MENU_") && !contains_part(parts, s)) parts.push_back(s);
    }
}

// AppendIconTexts: an icon controller's binding labels (iconTextList).
void append_icon_texts(std::vector<std::wstring>& parts, void* icon)
{
    void* view = icon ? ref::rd<void*>(icon, fIconView) : nullptr;
    void* list = view ? ref::rd<void*>(view, fIconTextList) : nullptr;
    int n = unity::list_count(list);
    for (int i = 0; i < n; ++i) {
        void* t = unity::list_at(list, i);
        if (!t) continue;
        std::wstring s = unity::ui_text(t);
        if (text::is_blank(s)) continue;
        s = text::trim(s);
        // Gamepad-help rows leave the keyboard icon text as a "New Text" placeholder.
        if (is_real_name(s) && !contains_part(parts, s)) parts.push_back(s);
    }
}

// JoystickKeyCodeToSdlButton: Unity KeyCode.JoystickButtonN (330+) to an SDL gamepad button.
// FFPR keeps Confirm on JoystickButton1 and Cancel on JoystickButton0 (Japanese arrangement);
// the American build confirms with the BOTTOM button, so JB1 -> SOUTH and JB0 -> EAST.
int joystick_keycode_to_sdl_button(int key_code)
{
    switch (key_code) {
    case 330: return SDL_GAMEPAD_BUTTON_EAST;            // JoystickButton0: Cancel (B / Circle)
    case 331: return SDL_GAMEPAD_BUTTON_SOUTH;           // JoystickButton1: Confirm (A / Cross)
    case 332: return SDL_GAMEPAD_BUTTON_WEST;            // JoystickButton2: X / Square
    case 333: return SDL_GAMEPAD_BUTTON_NORTH;           // JoystickButton3: Y / Triangle
    case 334: return SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;   // JoystickButton4: LB
    case 335: return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;  // JoystickButton5: RB
    case 336: return SDL_GAMEPAD_BUTTON_BACK;            // JoystickButton6: Back / View
    case 337: return SDL_GAMEPAD_BUTTON_START;           // JoystickButton7: Start / Menu
    case 338: return SDL_GAMEPAD_BUTTON_LEFT_STICK;      // JoystickButton8: LS
    case 339: return SDL_GAMEPAD_BUTTON_RIGHT_STICK;     // JoystickButton9: RS
    default: return -1;
    }
}

// ResolveGamepadButtonText: a remap row's LIVE (remappable) gamepad button: KeyConfigData's
// GameKey -> KeyCode, to an SDL button, to the connected controller's label.
std::wstring resolve_gamepad_button_text(void* owner, void* command)
{
    if (!owner || !command) return {};
    void* keydata = ref::rd<void*>(owner, fKeysKeydata);
    if (!keydata) return {};
    void* dict = ref::call<void*>(mGetGamePadDict, keydata);
    if (!dict) return {};
    int key_code = 0;
    if (!ffpr::status::int_dict_get(dict, ref::rd<int32_t>(command, fCtrlKey), key_code)) return {};
    int sdl = joystick_keycode_to_sdl_button(key_code);
    if (sdl < 0) return {};
    return input::button_label(sdl);
}

// GamepadGlyphSpriteToLabel: the FIXED buttons only ("UI_Common_<Button>button01"); the
// remappable face buttons and unknowns return empty so the live read owns them.
std::wstring gamepad_glyph_sprite_to_label(const std::wstring& sprite_name)
{
    if (sprite_name.empty()) return {};
    if (has_ci(sprite_name, L"LBbutton")) return input::button_label(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    if (has_ci(sprite_name, L"RBbutton")) return input::button_label(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    if (has_ci(sprite_name, L"LTbutton")) return input::left_trigger_label();
    if (has_ci(sprite_name, L"RTbutton")) return input::right_trigger_label();
    // Stick clicks, phrased as a click (not "LS"/"RS", which reads like moving the stick).
    if (has_ci(sprite_name, L"L3button")) return input::left_stick_click_label();
    if (has_ci(sprite_name, L"R3button")) return input::right_stick_click_label();
    // The Menu (Start) button is the mod menu's and cannot be remapped.
    if (has_ci(sprite_name, L"Menubutton")) return T(L"used for mod menu");
    if (has_ci(sprite_name, L"Backbutton") || has_ci(sprite_name, L"Selectbutton") || has_ci(sprite_name, L"Viewbutton"))
        return input::button_label(SDL_GAMEPAD_BUTTON_BACK);
    // The movement glyph: the mod took the D-pad and right stick, so only the left stick moves.
    if (has_ci(sprite_name, L"Tenkeybutton") || has_ci(sprite_name, L"Dpadbutton") ||
        has_ci(sprite_name, L"Crossbutton") || has_ci(sprite_name, L"Directionbutton"))
        return T(L"Left Stick");
    return {};
}

// The first active Image's sprite name under `root` that `map` turns into a label.
std::wstring sprite_label_under(void* root, std::wstring (*map)(const std::wstring&))
{
    for (void* img : unity::components_in_children(root, kImage, true)) {
        if (!unity::active_in_hierarchy(img)) continue;
        void* sprite = ref::call<void*>(mImageSprite, img);
        if (!unity::alive(sprite)) continue;
        std::wstring label = map(unity::name(sprite));
        if (!label.empty()) return label;
    }
    return {};
}

// GetGamepadGlyphLabel: the rendered glyph under view.gamePadIconsRoot (fixed buttons).
std::wstring gamepad_glyph_label(void* command)
{
    void* view = command ? ref::rd<void*>(command, fCtrlView) : nullptr;
    void* root = view ? ref::rd<void*>(view, fCtrlGamePadIconsRoot) : nullptr;
    if (!root || !unity::active_self(root)) return {};
    return sprite_label_under(root, &gamepad_glyph_sprite_to_label);
}

// MouseSpriteToLabel: "UI_Common_mouse_l/_r/_rad". The wheel ("mouse_rad") contains "mouse_r",
// so it is checked first.
std::wstring mouse_sprite_to_label(const std::wstring& s)
{
    if (s.empty() || !has_ci(s, L"mouse")) return {};
    if (has_ci(s, L"mouse_rad") || has_ci(s, L"wheel") || has_ci(s, L"scroll")) return T(L"Mouse Wheel");
    if (has_ci(s, L"mouse_l")) return T(L"Left Mouse Button");
    if (has_ci(s, L"mouse_r")) return T(L"Right Mouse Button");
    if (has_ci(s, L"mouse_c") || has_ci(s, L"mouse_m")) return T(L"Middle Mouse Button");
    return T(L"Mouse Button");   // a rebound extra button (gaming mouse)
}

// ResolveMouseButtonText: the mouse row's rendered glyph (it tracks the live binding).
std::wstring resolve_mouse_button_text(void* command)
{
    if (!command) return {};
    void* view = ref::rd<void*>(command, fCtrlView);
    void* root = view ? ref::rd<void*>(view, fCtrlKeyboardIconsRoot) : nullptr;
    return sprite_label_under(unity::alive(root) ? root : command, &mouse_sprite_to_label);
}

// BuildCommandAnnouncement: action name + keyboard binding + gamepad binding.
std::wstring build_command_announcement(void* owner, void* command, bool is_help_list = false)
{
    if (!command) return {};
    std::vector<std::wstring> parts;
    append_command_name(parts, command, is_help_list);

    // Mouse rows render their button as a glyph: translate it. Keyboard rows carry readable key
    // names in the keyboard icon controller's iconTextList.
    if (ref::rd<bool>(command, fCtrlIsMouseKey)) {
        std::wstring mouse = resolve_mouse_button_text(command);
        if (!mouse.empty()) parts.push_back(L"(" + mouse + L")");
    } else {
        append_icon_texts(parts, ref::rd<void*>(command, fCtrlKeyboardIcon));
    }

    // Gamepad binding, ONLY when the row shows a gamepad icon (not "Reset to Defaults" etc.).
    void* view = ref::rd<void*>(command, fCtrlView);
    void* gp_root = view ? ref::rd<void*>(view, fCtrlGamePadIconsRoot) : nullptr;
    if (gp_root && unity::active_self(gp_root)) {
        std::wstring btn = resolve_gamepad_button_text(owner, command);
        // Fixed buttons are not in the remap dictionary: read the rendered glyph.
        if (btn.empty()) btn = gamepad_glyph_label(command);
        if (!btn.empty()) parts.push_back(L"(" + btn + L")");
    }

    std::wstring out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out += L" ";
        out += parts[i];
    }
    return out;
}

// SelectContent_Postfix: navigating the controls list.
using KeysSelectContent_t = void (*)(void* self, int32_t index, void* scroll_view, void* cursor, void* content_list,
                                     int32_t type, const void* mi);
KeysSelectContent_t o_KeysSelectContent;
void d_KeysSelectContent(void* self, int32_t index, void* scroll_view, void* cursor, void* content_list, int32_t type,
                         const void* mi)
{
    o_KeysSelectContent(self, index, scroll_view, cursor, content_list, type, mi);
    hooks::guarded("ConfigKeysSettingController.SelectContent", [&] {
        if (kKeys.is(self)) field::cache::register_object(kKeys, self);
        if (!self || !content_list) return;
        // contentList.TryCast<List<T>>() + SelectContentHelper.TryGetItem.
        void* command = is_list(content_list) ? ui::list_item(content_list, index) : nullptr;
        std::wstring announcement = build_command_announcement(self, command);
        if (text::is_blank(announcement)) return;
        ffpr::say(announcement, true);
    });
}

// KeyboardSettingInit / GamePadSettingInit: entering assign mode.
void announce_assign_prompt(void* inst, bool gamepad)
{
    if (!inst) return;
    ffpr::say(gamepad ? T(L"Press a button.") : T(L"Press a key."), true);
}

Void_t o_KeyboardSettingInit;
void d_KeyboardSettingInit(void* self, const void* mi)
{
    o_KeyboardSettingInit(self, mi);
    hooks::guarded("ConfigKeysSettingController.KeyboardSettingInit", [&] { announce_assign_prompt(self, false); });
}

Void_t o_GamePadSettingInit;
void d_GamePadSettingInit(void* self, const void* mi)
{
    o_GamePadSettingInit(self, mi);
    hooks::guarded("ConfigKeysSettingController.GamePadSettingInit", [&] { announce_assign_prompt(self, true); });
}

// The Gamepad/Keyboard Controls pop-up (the Help state): render every row once and hand the
// lines to the key-help reader (system area), which steps them with the arrows.
std::vector<std::wstring> build_help_entries(void* owner, void* list)
{
    std::vector<std::wstring> entries;
    int n = unity::list_count(list);
    for (int i = 0; i < n; ++i) {
        void* cmd = unity::list_at(list, i);
        if (!cmd) continue;
        std::wstring ann = build_command_announcement(owner, cmd, true);
        if (!text::is_blank(ann)) entries.push_back(ann);
    }
    return entries;
}

// One frame later, so each row's binding text is populated. The state machine also cycles the
// Help Init callbacks during scene construction: only an on-screen controller opens the list.
void open_controls_help(void* inst, bool gamepad)
{
    if (!inst) return;
    if (kKeys.is(inst)) field::cache::register_object(kKeys, inst);
    frame::next_frame([inst, gamepad] {
        std::vector<std::wstring> entries;
        hooks::guarded("Config.OpenControlsHelp", [&] {
            if (!unity::alive(inst) || !unity::active_in_hierarchy(inst)) {
                ffpr::system::close_controls_help();
                return;
            }
            void* list = ref::rd<void*>(inst, gamepad ? fKeysHelpList : fKeysKeyboardHelpList);
            entries = build_help_entries(inst, list);
        });
        if (!entries.empty()) ffpr::system::open_controls_help(inst, entries);
    }, "Config.ControlsHelp");
}

Void_t o_GamePadHelpInit;
void d_GamePadHelpInit(void* self, const void* mi)
{
    o_GamePadHelpInit(self, mi);
    hooks::guarded("ConfigKeysSettingController.GamePadHelpInit", [&] { open_controls_help(self, true); });
}

Void_t o_KeyboardHelpInit;
void d_KeyboardHelpInit(void* self, const void* mi)
{
    o_KeyboardHelpInit(self, mi);
    hooks::guarded("ConfigKeysSettingController.KeyboardHelpInit", [&] { open_controls_help(self, false); });
}

// Back to the controls list (Select state) or the screen closed: tear the list down.
Void_t o_GamePadSelectInit;
void d_GamePadSelectInit(void* self, const void* mi)
{
    o_GamePadSelectInit(self, mi);
    hooks::guarded("ConfigKeysSettingController.GamePadSelectInit", [self] {
        if (kKeys.is(self)) field::cache::register_object(kKeys, self);
        ffpr::system::close_controls_help();
    });
}

Void_t o_KeyboardSelectInit;
void d_KeyboardSelectInit(void* self, const void* mi)
{
    o_KeyboardSelectInit(self, mi);
    hooks::guarded("ConfigKeysSettingController.KeyboardSelectInit", [self] {
        if (kKeys.is(self)) field::cache::register_object(kKeys, self);
        ffpr::system::close_controls_help();
    });
}

Void_t o_KeysClose;
void d_KeysClose(void* self, const void* mi)
{
    o_KeysClose(self, mi);
    hooks::guarded("ConfigKeysSettingController.Close", [self] {
        if (field::cache::get(kKeys) == self) field::cache::clear(kKeys);
        ffpr::system::close_controls_help();
    });
}

// ChangeKeySetting (both overloads): a binding was applied; read the edited command again.
void announce_changed_key(void* self)
{
    if (!self) return;
    std::wstring announcement = build_command_announcement(self, ref::rd<void*>(self, fKeysSelectedCommand));
    if (text::is_blank(announcement)) return;
    ffpr::say(announcement, true);
}

using ChangeKey3_t = void (*)(void* self, int32_t type, int32_t key, int32_t key_code, const void* mi);
ChangeKey3_t o_ChangeKeySetting3;
void d_ChangeKeySetting3(void* self, int32_t type, int32_t key, int32_t key_code, const void* mi)
{
    o_ChangeKeySetting3(self, type, key, key_code, mi);
    hooks::guarded("ConfigKeysSettingController.ChangeKeySetting", [&] { announce_changed_key(self); });
}

using ChangeKey2_t = void (*)(void* self, int32_t key, int32_t action_type, const void* mi);
ChangeKey2_t o_ChangeKeySetting2;
void d_ChangeKeySetting2(void* self, int32_t key, int32_t action_type, const void* mi)
{
    o_ChangeKeySetting2(self, key, action_type, mi);
    hooks::guarded("ConfigKeysSettingController.ChangeKeySetting", [&] { announce_changed_key(self); });
}

// --- install --------------------------------------------------------------------------------

int install()
{
    int n = 0;
    auto add = [&n](bool ok) { if (ok) ++n; };
    add(hooks::attach(mSelectCommand, &d_SelectCommand, &o_SelectCommand));
    add(hooks::attach(mSwitchArrowSelectType, &d_SwitchArrowSelectType, &o_SwitchArrowSelectType));
    add(hooks::attach(mSetSliderValue, &d_SetSliderValue, &o_SetSliderValue));
    add(hooks::attach(mTouchSwitchArrow, &d_TouchSwitchArrow, &o_TouchSwitchArrow));
    add(hooks::attach(mTouchSwitchSlider, &d_TouchSwitchSlider, &o_TouchSwitchSlider));
    add(hooks::attach(mConfigSetActive, &d_ConfigSetActive, &o_ConfigSetActive));
    add(hooks::attach(mOptionSetActive, &d_OptionSetActive, &o_OptionSetActive));
    add(hooks::attach(mOptionShowConfig, &d_OptionShowConfig, &o_OptionShowConfig));
    add(hooks::attach(mOptionInitSelectLanguage, &d_OptionInitSelectLanguage, &o_OptionInitSelectLanguage));
    add(hooks::attach(mOptionSetDropDownItemFocus, &d_SetDropDownItemFocus, &o_SetDropDownItemFocus));
    add(hooks::attach(mKeysSelectContent, &d_KeysSelectContent, &o_KeysSelectContent));
    add(hooks::attach(mKeyboardSettingInit, &d_KeyboardSettingInit, &o_KeyboardSettingInit));
    add(hooks::attach(mGamePadSettingInit, &d_GamePadSettingInit, &o_GamePadSettingInit));
    add(hooks::attach(mGamePadHelpInit, &d_GamePadHelpInit, &o_GamePadHelpInit));
    add(hooks::attach(mKeyboardHelpInit, &d_KeyboardHelpInit, &o_KeyboardHelpInit));
    add(hooks::attach(mGamePadSelectInit, &d_GamePadSelectInit, &o_GamePadSelectInit));
    add(hooks::attach(mKeyboardSelectInit, &d_KeyboardSelectInit, &o_KeyboardSelectInit));
    add(hooks::attach(mKeysClose, &d_KeysClose, &o_KeysClose));
    add(hooks::attach(mChangeKeySetting3, &d_ChangeKeySetting3, &o_ChangeKeySetting3));
    add(hooks::attach(mChangeKeySetting2, &d_ChangeKeySetting2, &o_ChangeKeySetting2));
    LOGF("[Config Menu] %d patches applied", n);

    queries::define("ConfigMenuState.IsActive", [] { return is_active(); });
    queries::define("ConfigMenuState.ShouldSuppress", [] { return should_suppress(); });
    actions::define("ConfigMenuState.ResetState", [] { reset_state(); });
    actions::define("ConfigController_SetActive_Patch.ReannounceFocusedConfigOption",
                    [] { reannounce_focused_config_option(); });
    actions::define("ConfigController_SetActive_Patch.ArmReannounceAfterLibrary",
                    [] { arm_reannounce_after_library(); });
    actions::define("ConfigController_SetActive_Patch.OnMenuResumedAfterLibrary",
                    [] { on_menu_resumed_after_library(); });

    // FF1 OnSceneLoaded: config state must never survive a scene change (a stale flag leaked
    // into title construction and spoke "Language" over the title screen).
    events::on(events::Kind::SceneLoaded, [] { reset_state(); });
    return n;
}

core::Install s_install{"ConfigMenu", &install, 125};

} // namespace

// --- ConfigMenuState ------------------------------------------------------------------------

bool is_active() { return menu_state::is_active(menu_state::CONFIG_MENU); }

void set_active(bool active)
{
    // C# MenuStateRegistry.SetActive(key, false) runs the key's reset handler: menu_state::reset.
    if (active) menu_state::set_active(menu_state::CONFIG_MENU, true);
    else menu_state::reset(menu_state::CONFIG_MENU);
}

void reset_state() { set_active(false); }

bool should_suppress()
{
    try {
        // The keyboard/gamepad remap sub-screen is its own controller, announced by the
        // SelectContent / SettingInit / ChangeKeySetting postfixes: suppress the generic reader
        // whenever it is up, even with a stale IsActive.
        void* keys = field::cache::get(kKeys);
        if (keys && unity::active_in_hierarchy(keys)) return true;

        if (!is_active()) return false;

        // Validate the base config controller is still active (auto-resets a stuck flag).
        void* controller = field::cache::get(kDetailsKI);
        if (!controller || !unity::active_in_hierarchy(controller)) {
            reset_state();
            return false;
        }
        return true;
    } catch (...) {
        reset_state();
        return false;
    }
}

// --- ConfigController_SetActive_Patch -------------------------------------------------------

void reannounce_focused_config_option() { start_reannounce(); }
void arm_reannounce_after_library() { g_pending_config_reannounce = true; }

void on_menu_resumed_after_library()
{
    if (!g_pending_config_reannounce) return;
    g_pending_config_reannounce = false;
    start_reannounce();
}

} // namespace ffpr::config
