// Ports FF1 Menus/ConfigMenuReader.cs: reading a config row's value (arrow toggle, slider,
// dropdown, the Language row) for the KeyInput and Touch config screens. All five games.
#include <cmath>
#include "config.h"
#include "../refs.h"
#include "../text.h"
#include "../unity.h"

namespace ffpr::config {
namespace {

ref::Class  kCmdKI{"Last.UI.KeyInput", "ConfigCommandController"};
ref::Field  fCmdKIView{kCmdKI, "view"};
ref::Field  fCmdKIData{kCmdKI, "commandsData"};
ref::Class  kViewKI{"Last.UI.KeyInput", "ConfigCommandView"};
ref::Field  fViewNameText{kViewKI, "nameText"};
ref::Field  fViewArrowRoot{kViewKI, "arrowSelectTypeRoot"};
ref::Field  fViewSliderRoot{kViewKI, "sliderTypeRoot"};
ref::Field  fViewSlider{kViewKI, "slider"};
ref::Field  fViewDropDownRoot{kViewKI, "dropDownTypeRoot"};
ref::Field  fViewDropDown{kViewKI, "dropDown"};
ref::Class  kCommandsData{"Last.UI", "ConfigCommandsData"};
ref::Field  fCommandType{kCommandsData, "configCommandType"};
// Last.UI.ConfigCommandType.Language = 23 in all five dump.cs.
constexpr int32_t kCommandTypeLanguage = 23;

ref::Class  kCmdTouch{"Last.UI.Touch", "ConfigCommandController"};
ref::Field  fCmdTouchView{kCmdTouch, "view"};
ref::Class  kViewTouch{"Last.UI.Touch", "ConfigCommandView"};
ref::Field  fTouchArrowRoot{kViewTouch, "arrowButtonTypeRoot"};
ref::Field  fTouchSliderRoot{kViewTouch, "sliderTypeRoot"};

ref::Class  kSlider{"UnityEngine.UI", "Slider"};
ref::Method mSliderMin{kSlider, "get_minValue", 0};
ref::Method mSliderMax{kSlider, "get_maxValue", 0};
ref::Method mSliderValue{kSlider, "get_value", 0};

ref::Class  kDropdown{"UnityEngine.UI", "Dropdown"};
ref::Method mDropdownOptions{kDropdown, "get_options", 0};
ref::Method mDropdownValue{kDropdown, "get_value", 0};
ref::Class  kOptionData{"UnityEngine.UI", "Dropdown.OptionData"};
ref::Field  fOptionText{kOptionData, "m_Text"};

ref::Class  kMessageManager{"Last.Management", "MessageManager"};
ref::Field  fCurrentLanguage{kMessageManager, "currentLanguage"};

// Language (enum int) -> display name. The game's own GetLanguageMessage returns EMPTY for the
// current language (it blanks the current item's label), so the names are mapped here, in the
// English-name style of the language dropdown.
struct LanguageName { int id; const wchar_t* name; };
constexpr LanguageName kLanguageNames[] = {
    {1, L"Japanese"}, {2, L"English"}, {3, L"French"}, {4, L"Italian"},
    {5, L"German"}, {6, L"Spanish"}, {7, L"Korean"}, {8, L"Chinese (Traditional)"},
    {9, L"Chinese (Simplified)"}, {10, L"Russian"}, {11, L"Thai"}, {12, L"Brazilian Portuguese"},
};

// C# IndexOf(token, StringComparison.OrdinalIgnoreCase) >= 0 for an ASCII token.
bool contains_ascii_ci(const std::wstring& s, const wchar_t* token)
{
    auto up = [](wchar_t c) { return (c >= L'a' && c <= L'z') ? static_cast<wchar_t>(c - 32) : c; };
    size_t n = 0;
    while (token[n]) ++n;
    if (n == 0) return true;
    for (size_t i = 0; i + n <= s.size(); ++i) {
        size_t j = 0;
        while (j < n && up(s[i + j]) == up(token[j])) ++j;
        if (j == n) return true;
    }
    return false;
}

// The text of the dropdown's selected option (empty when out of range or missing).
std::wstring dropdown_option_text(void* dropdown)
{
    void* options = ref::call<void*>(mDropdownOptions, dropdown);
    int value = ref::call<int32_t>(mDropdownValue, dropdown);
    if (!options || value < 0 || value >= unity::list_count(options)) return {};
    void* opt = unity::list_at(options, value);
    return opt ? ref::rd_str(opt, fOptionText) : std::wstring();
}

// GetValueFromKeyInputCommand
std::wstring value_from_key_input_command(void* command)
{
    void* view = command ? ref::rd<void*>(command, fCmdKIView) : nullptr;
    if (!view) return {};

    // Language row: its dropdown text is empty while the popup is closed, so read the current
    // language from the game. Identified by the command's config type, not a localized string.
    void* data = ref::rd<void*>(command, fCmdKIData);
    if (data && ref::rd<int32_t>(data, fCommandType) == kCommandTypeLanguage) {
        std::wstring lang = current_language_display_name();
        if (!lang.empty()) return lang;
    }

    // Arrow change text (toggle / selection options such as BGM Type).
    void* arrow_root = ref::rd<void*>(view, fViewArrowRoot);
    if (arrow_root && unity::active_self(arrow_root)) {
        std::wstring arrow = arrow_text_under(arrow_root);
        if (!arrow.empty()) return arrow;
    }

    // Slider value (volume sliders).
    void* slider_root = ref::rd<void*>(view, fViewSliderRoot);
    if (slider_root && unity::active_self(slider_root)) {
        void* slider = ref::rd<void*>(view, fViewSlider);
        if (unity::alive(slider)) {
            void* name_text = ref::rd<void*>(view, fViewNameText);
            std::wstring setting_name = name_text ? unity::ui_text(name_text) : std::wstring();
            std::wstring value = slider_percentage(slider, name_text ? &setting_name : nullptr);
            if (!value.empty()) return value;
        }
    }

    // Dropdown: ONLY while the popup is actually open. Reading closed dropdowns made the rows that
    // open sub-screens (Gamepad/Keyboard Settings) announce Unity's placeholder "Option A".
    void* dd_root = ref::rd<void*>(view, fViewDropDownRoot);
    void* dropdown = ref::rd<void*>(view, fViewDropDown);
    if (dd_root && unity::active_self(dd_root) && unity::alive(dropdown)) {
        std::wstring text = dropdown_option_text(dropdown);
        if (!text.empty()) return text;
    }
    return {};
}

} // namespace

std::wstring find_config_value_from_controller(void* controller)
{
    if (!controller) return {};
    return value_from_key_input_command(controller);
}

std::wstring current_language_display_name()
{
    void* mgr = ref::instance(kMessageManager);
    if (!mgr) return {};
    int id = ref::rd<int32_t>(mgr, fCurrentLanguage);
    for (const LanguageName& l : kLanguageNames)
        if (l.id == id) return l.name;
    return {};
}

std::wstring slider_percentage(void* slider, const std::wstring* setting_name)
{
    if (!slider) return {};
    float min = ref::call<float>(mSliderMin, slider);
    float max = ref::call<float>(mSliderMax, slider);
    float current = ref::call<float>(mSliderValue, slider);

    // BGM / SFX sliders are 1-10: the raw value. Math.Round rounds half to even, as nearbyint does.
    if (setting_name && !setting_name->empty() &&
        (contains_ascii_ci(*setting_name, L"BGM") || contains_ascii_ci(*setting_name, L"SFX")))
        return text::num(static_cast<long long>(std::nearbyint(static_cast<double>(current))));

    float range = max - min;
    if (range <= 0) return L"0%";
    float percentage = ((current - min) / range) * 100.0f;
    return text::num(static_cast<long long>(std::nearbyint(static_cast<double>(percentage)))) + L"%";
}

bool is_valid_config_value(const std::wstring& value)
{
    if (text::is_blank(value)) return false;
    // Arrow characters.
    static const wchar_t* const kArrows[] = {L"<", L">", L"◀", L"▶", L"←", L"→"};
    for (const wchar_t* a : kArrows)
        if (value == a) return false;
    // Template / placeholder values.
    static const wchar_t* const kPlaceholders[] = {L"NewText", L"ReEquip", L"Text", L"Label", L"Value", L"Name"};
    for (const wchar_t* p : kPlaceholders)
        if (value == p) return false;
    return true;
}

std::wstring arrow_text_under(void* root)
{
    if (!root) return {};
    for (void* t : unity::texts(root, false)) {
        // Skip inactive text components.
        if (!t || !unity::active_in_hierarchy(t)) continue;
        std::wstring raw = unity::ui_text(t);
        if (text::is_blank(raw)) continue;
        std::wstring value = text::trim(raw);
        if (is_valid_config_value(value)) return value;
    }
    return {};
}

std::wstring dropdown_selected_text(void* dropdown)
{
    return unity::alive(dropdown) ? dropdown_option_text(dropdown) : std::wstring();
}

std::wstring value_from_touch_command(void* command)
{
    void* view = command ? ref::rd<void*>(command, fCmdTouchView) : nullptr;
    if (!view) return {};

    // Arrow button type (toggle / selection options).
    void* arrow_root = ref::rd<void*>(view, fTouchArrowRoot);
    if (arrow_root && unity::active_self(arrow_root)) {
        std::wstring arrow = arrow_text_under(arrow_root);
        if (!arrow.empty()) return arrow;
    }

    // Slider (Touch mode exposes no setting name: always a percentage).
    void* slider_root = ref::rd<void*>(view, fTouchSliderRoot);
    if (slider_root && unity::active_self(slider_root)) {
        void* slider = unity::get_component_in_children(slider_root, kSlider, false);
        if (slider) {
            std::wstring value = slider_percentage(slider);
            if (!value.empty()) return value;
        }
    }
    return {};
}

} // namespace ffpr::config
