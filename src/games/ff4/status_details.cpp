// Ports FF4 Patches/StatusDetailsPatches.cs (StatusMenuStatePatches, StatusMenuTracker,
// StatusDetailsController InitDisplay/ExitDisplay, StatusDetailsHelpers) and
// Menus/StatusDetailsReader.cs (ReadStatusDetails and FF4's 17-stat StatusNavigationReader).
//
// The status character list (StatusWindowController.SelectContent) is read by the core
// (src/core/status/status_menu.cpp, all games). This file hooks the same method only to record
// that the player opened the list (StatusMenuTracker.MarkUserOpened); it speaks nothing there.
//
// The core's status details screen and stat list are FF1's. FF4's controller is
// Serial.FF4.UI.KeyInput.StatusDetailsController; this file feeds the core's StatusNavigationTracker
// (status::tracker(), which drives the Status key context) and replaces the
// "StatusNavigationReader.*" actions with FF4's stat list for FF4.
//
// Folded body: StatusDetailsController.ExitDisplay (RVA 0x2B7440) is shared by 8 methods in FF4:
// the detour class-checks self.
#include <string>
#include <vector>
#include "ff4.h"
#include "../../core/core.h"
#include "../../core/frame.h"
#include "../../core/hooks.h"
#include "../../core/il2cpp.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/status/status.h"

namespace ffpr::ff4 {
namespace {

ref::Class  kStatusDetails{"Serial.FF4.UI.KeyInput", "StatusDetailsController", game::kFF4};
ref::Field  fDetailsStatusController{kStatusDetails, "statusController"};
ref::Method mInitDisplay{kStatusDetails, "InitDisplay", 0};
ref::Method mExitDisplay{kStatusDetails, "ExitDisplay", 0};
ref::Class  kCharaStatus{"Serial.FF4.UI.KeyInput", "AbilityCharaStatusController", game::kFF4};
ref::Field  fCharaTargetData{kCharaStatus, "targetData"};
ref::Class  kCharaStatusView{"Serial.FF4.UI.KeyInput", "AbilityCharaStatusView", game::kFF4};
ref::Field  fDominantArmText{kCharaStatusView, "dominantArmText"};

ref::Class  kStatusWindow{"Last.UI.KeyInput", "StatusWindowController", game::kFF4};
ref::Method mStatusSelectContent{kStatusWindow, "SelectContent", 3};
ref::Method mStatusSetActive{kStatusWindow, "SetActive", 1};

ref::Class  kCanvas{"UnityEngine", "Canvas", game::kFF4};
ref::Class  kBehaviour{"UnityEngine", "Behaviour", game::kFF4};
ref::Method mBehaviourEnabled{kBehaviour, "get_enabled", 0};

ref::Class  kParamContent{"Last.UI.KeyInput", "ParameterContentController", game::kFF4};
ref::Field  fParamType{kParamContent, "type"};
ref::Field  fParamView{kParamContent, "view"};
ref::Class  kParamView{"Last.UI.KeyInput", "ParameterContentView", game::kFF4};
ref::Field  fMultipliedValueText{kParamView, "multipliedValueText"};

ref::Class  kOwnedChar{"Last.Data.User", "OwnedCharacterData", game::kFF4};
ref::Method mCharCurrentExp{kOwnedChar, "get_CurrentExp", 0};
ref::Method mCharNextExp{kOwnedChar, "GetNextExp", 0};
ref::Method mCharDominantType{kOwnedChar, "GetEquipDominantArmType", 0};

// Abstract on CharacterParameterBase: called through the object's override.
ref::Class  kParamBase{"Last.Data", "CharacterParameterBase", game::kFF4};
ref::Method mGetCurrentHP{kParamBase, "get_CurrentHP", 0};
ref::Method mGetCurrentMP{kParamBase, "get_CurrentMP", 0};
ref::Method mConfirmedLevel{kParamBase, "ConfirmedLevel", 0};
ref::Method mConfirmedMaxHp{kParamBase, "ConfirmedMaxHp", 0};
ref::Method mConfirmedMaxMp{kParamBase, "ConfirmedMaxMp", 0};
ref::Method mConfirmedPower{kParamBase, "ConfirmedPower", 0};
ref::Method mConfirmedAgility{kParamBase, "ConfirmedAgility", 0};
ref::Method mConfirmedVitality{kParamBase, "ConfirmedVitality", 0};
ref::Method mConfirmedIntelligence{kParamBase, "ConfirmedIntelligence", 0};
ref::Method mConfirmedSpirit{kParamBase, "ConfirmedSpirit", 0};
ref::Method mConfirmedAttack{kParamBase, "ConfirmedAttack", 0};
ref::Method mConfirmedDefense{kParamBase, "ConfirmedDefense", 0};
ref::Method mConfirmedAbilityDefense{kParamBase, "ConfirmedAbilityDefense", 0};
ref::Method mConfirmedAccuracyRate{kParamBase, "ConfirmedAccuracyRate", 1};
ref::Method mConfirmedEvasionRate{kParamBase, "ConfirmedEvasionRate", 1};
ref::Method mConfirmedAbilityEvasionRate{kParamBase, "ConfirmedAbilityEvasionRate", 1};

// ParameterType values read from the UI (multiplied "count x" values).
constexpr int PARAMETER_TYPE_ATTACK = 10;
constexpr int PARAMETER_TYPE_DEFENSE = 11;
constexpr int PARAMETER_TYPE_ABILITY_DEFENSE = 12;

// StatusMenuTracker.IsUserOpened
bool g_user_opened = false;
// StatusDetailsReader.currentCharacterData (party data: lives as long as the party).
void* g_reader_character = nullptr;

/// obj.M(bool) dispatched to the override (ConfirmedAccuracyRate(false) and the like).
int virtual_int_bool(ref::Method& m, void* obj, bool arg)
{
    il2cpp::Method* base = m.get();
    if (!base || !obj) return 0;
    il2cpp::Method* vm = il2cpp::virtual_method(reinterpret_cast<il2cpp::Object*>(obj), base);
    if (!vm) vm = base;
    void* fp = il2cpp::method_pointer(vm);
    if (!fp) return 0;
    try {
        using F = int32_t (*)(void*, bool, const void*);
        return reinterpret_cast<F>(fp)(obj, arg, vm);
    } catch (...) {
        ref::call_failed_note(m.name);
        return 0;
    }
}

std::wstring kv(const wchar_t* key, const std::wstring& value) { return TF(L"{0}: {1}", {T(key), value}); }
std::wstring kv(const wchar_t* key, int value) { return kv(key, text::num(value)); }
std::wstring na(const wchar_t* key) { return kv(key, T(L"N/A")); }

void* param_of(void* data) { return data ? character_parameter(data) : nullptr; }

// --- StatusNavigationReader private helpers -----------------------------------------------------

/// TryReadCountFromUI: the "count" shown before an Attack / Defense / Magic Defense value, or -1.
int count_from_ui(int parameter_type)
{
    for (void* controller : unity::find_objects(kParamContent)) {
        if (!controller) continue;
        if (ref::rd<int32_t>(controller, fParamType) != parameter_type) continue;
        void* view = ref::rd<void*>(controller, fParamView);
        if (!view) return -1;
        void* text_component = ref::rd<void*>(view, fMultipliedValueText);
        if (!text_component) return -1;
        std::wstring value = text::trim(unity::ui_text(text_component));
        if (value.empty()) return -1;
        try {
            size_t used = 0;
            int n = std::stoi(value, &used);
            return used == value.size() ? n : -1;
        } catch (...) {
            return -1;
        }
    }
    return -1;
}

/// TryReadHandednessFromUI: the localized handedness line of the active status view.
std::wstring handedness_from_ui()
{
    for (void* view : unity::find_objects(kCharaStatusView)) {
        if (!view || !unity::active_in_hierarchy(view)) continue;
        std::wstring t = text::trim(unity::ui_text(ref::rd<void*>(view, fDominantArmText)));
        if (!t.empty()) return t;
    }
    return {};
}

// --- the 17 stats, in the screen's order ---------------------------------------------------------

std::wstring read_level(void* d)
{
    void* p = param_of(d);
    return p ? kv(L"Level", call_virtual_int(mConfirmedLevel, p)) : na(L"Level");
}
std::wstring read_handed(void* d)
{
    std::wstring ui = handedness_from_ui();
    if (!ui.empty()) return ui;
    if (!d) return T(L"N/A");
    switch (ref::call<int32_t>(mCharDominantType, d)) {
    case 1: return T(L"Right-handed");
    case 2: return T(L"Left-handed");
    case 3: return T(L"Ambidextrous");
    default: return T(L"N/A");
    }
}
std::wstring read_experience(void* d)
{
    return d ? kv(L"Experience", ref::call<int32_t>(mCharCurrentExp, d)) : na(L"Experience");
}
std::wstring read_next_level(void* d)
{
    return d ? kv(L"Next Level", ref::call<int32_t>(mCharNextExp, d)) : na(L"Next Level");
}
std::wstring read_vital(void* d, const wchar_t* key, ref::Method& current, ref::Method& max)
{
    void* p = param_of(d);
    if (!p) return na(key);
    return TF(L"{0}: {1} / {2}", {T(key), text::num(ref::call<int32_t>(current, p)), text::num(call_virtual_int(max, p))});
}
std::wstring read_hp(void* d) { return read_vital(d, L"HP", mGetCurrentHP, mConfirmedMaxHp); }
std::wstring read_mp(void* d) { return read_vital(d, L"MP", mGetCurrentMP, mConfirmedMaxMp); }
std::wstring read_plain(void* d, const wchar_t* key, ref::Method& m)
{
    void* p = param_of(d);
    return p ? kv(key, call_virtual_int(m, p)) : na(key);
}
std::wstring read_strength(void* d) { return read_plain(d, L"Strength", mConfirmedPower); }
std::wstring read_agility(void* d) { return read_plain(d, L"Agility", mConfirmedAgility); }
std::wstring read_stamina(void* d) { return read_plain(d, L"Stamina", mConfirmedVitality); }
std::wstring read_intellect(void* d) { return read_plain(d, L"Intellect", mConfirmedIntelligence); }
std::wstring read_spirit(void* d) { return read_plain(d, L"Spirit", mConfirmedSpirit); }
std::wstring read_counted(void* d, const wchar_t* key, ref::Method& m, int parameter_type)
{
    void* p = param_of(d);
    if (!p) return na(key);
    int power = call_virtual_int(m, p);
    int count = count_from_ui(parameter_type);
    if (count > 0) return TF(L"{0}: {1}x {2}", {T(key), text::num(count), text::num(power)});
    return kv(key, power);
}
std::wstring read_rate(void* d, const wchar_t* key, ref::Method& m)
{
    void* p = param_of(d);
    if (!p) return na(key);
    return TF(L"{0}: {1}%", {T(key), text::num(virtual_int_bool(m, p, false))});
}
std::wstring read_attack(void* d) { return read_counted(d, L"Attack", mConfirmedAttack, PARAMETER_TYPE_ATTACK); }
std::wstring read_accuracy(void* d) { return read_rate(d, L"Accuracy", mConfirmedAccuracyRate); }
std::wstring read_defense(void* d) { return read_counted(d, L"Defense", mConfirmedDefense, PARAMETER_TYPE_DEFENSE); }
std::wstring read_evasion(void* d) { return read_rate(d, L"Evasion", mConfirmedEvasionRate); }
std::wstring read_magic_defense(void* d) { return read_counted(d, L"Magic Defense", mConfirmedAbilityDefense, PARAMETER_TYPE_ABILITY_DEFENSE); }
std::wstring read_magic_evasion(void* d) { return read_rate(d, L"Magic Evasion", mConfirmedAbilityEvasionRate); }

using Reader = std::wstring (*)(void*);
const Reader kStats[] = {
    read_level, read_handed, read_experience, read_next_level,                       // CharacterInfo
    read_hp, read_mp,                                                                // Vitals
    read_strength, read_agility, read_stamina, read_intellect, read_spirit,         // Attributes
    read_attack, read_accuracy, read_defense, read_evasion, read_magic_defense, read_magic_evasion,  // CombatStats
};
constexpr int kStatCount = static_cast<int>(sizeof(kStats) / sizeof(kStats[0]));
constexpr int kGroupStarts[] = {0, 4, 6, 11};
constexpr int kGroupCount = static_cast<int>(sizeof(kGroupStarts) / sizeof(kGroupStarts[0]));

// --- StatusNavigationReader ----------------------------------------------------------------------

void read_stat_at(int index)
{
    status::Tracker& tr = status::tracker();
    if (index < 0 || index >= kStatCount) {
        LOGF("[FF4 Status] Invalid stat index: %d", index);
        return;
    }
    if (!tr.current_character_data) {
        ffpr::say(T(L"No character data"), true);
        return;
    }
    std::wstring value = hooks::guarded_r<std::wstring>("FF4 Status.ReadStat", std::wstring(), [&] {
        return kStats[index](tr.current_character_data);
    });
    if (value.empty()) {
        ffpr::say(T(L"Error reading stat"), true);
        return;
    }
    ffpr::say(ui::menu_position(value, index, kStatCount), true);
}

void read_current_stat()
{
    status::Tracker& tr = status::tracker();
    if (!tr.validate_state()) {
        ffpr::say(T(L"Navigation not available"), true);
        return;
    }
    read_stat_at(tr.current_stat_index);
}

void navigate_next()
{
    status::Tracker& tr = status::tracker();
    if (!tr.is_navigation_active) return;
    tr.current_stat_index = (tr.current_stat_index + 1) % kStatCount;
    read_current_stat();
}

void navigate_previous()
{
    status::Tracker& tr = status::tracker();
    if (!tr.is_navigation_active) return;
    if (--tr.current_stat_index < 0) tr.current_stat_index = kStatCount - 1;
    read_current_stat();
}

void jump_to_next_group()
{
    status::Tracker& tr = status::tracker();
    if (!tr.is_navigation_active) return;
    int next = -1;
    for (int start : kGroupStarts)
        if (start > tr.current_stat_index) { next = start; break; }
    tr.current_stat_index = next < 0 ? kGroupStarts[0] : next;
    read_current_stat();
}

void jump_to_previous_group()
{
    status::Tracker& tr = status::tracker();
    if (!tr.is_navigation_active) return;
    int prev = -1;
    for (int i = kGroupCount - 1; i >= 0; --i)
        if (kGroupStarts[i] < tr.current_stat_index) { prev = kGroupStarts[i]; break; }
    tr.current_stat_index = prev < 0 ? kGroupStarts[kGroupCount - 1] : prev;
    read_current_stat();
}

void jump_to_top()
{
    status::Tracker& tr = status::tracker();
    if (!tr.is_navigation_active) return;
    tr.current_stat_index = 0;
    read_current_stat();
}

void jump_to_bottom()
{
    status::Tracker& tr = status::tracker();
    if (!tr.is_navigation_active) return;
    tr.current_stat_index = kStatCount - 1;
    read_current_stat();
}

// --- StatusDetailsReader.ReadStatusDetails --------------------------------------------------------

std::wstring read_status_details()
{
    std::vector<std::wstring> parts;
    if (void* data = g_reader_character) {
        std::wstring name = character_name(data);
        if (!name.empty()) parts.push_back(name);
        if (void* p = character_parameter(data)) {
            parts.push_back(kv(L"Level", call_virtual_int(mConfirmedLevel, p)));
            parts.push_back(T(L"HP") + L" " + text::num(ref::call<int32_t>(mGetCurrentHP, p)) + L"/" +
                            text::num(call_virtual_int(mConfirmedMaxHp, p)));
            parts.push_back(T(L"MP") + L" " + text::num(ref::call<int32_t>(mGetCurrentMP, p)) + L"/" +
                            text::num(call_virtual_int(mConfirmedMaxMp, p)));
        }
    }
    if (parts.empty()) return T(L"Status Details");
    std::wstring out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out += L", ";
        out += parts[i];
    }
    return out;
}

// --- hooks ---------------------------------------------------------------------------------------

/// StatusDetailsHelpers.GetCharacterDataFromController: statusController -> targetData.
void* character_from_controller(void* controller)
{
    void* status_controller = ref::rd<void*>(controller, fDetailsStatusController);
    return status_controller ? ref::rd<void*>(status_controller, fCharaTargetData) : nullptr;
}

/// InitializeNavigation: arrow-key stat browsing starts at the top for the shown character.
void initialize_navigation(void* controller)
{
    void* data = character_from_controller(controller);
    if (!data) return;
    status::Tracker& tr = status::tracker();
    tr.is_navigation_active = true;
    tr.current_stat_index = 0;
    tr.active_controller = controller;
    tr.current_character_data = data;
    g_reader_character = data;
}

using Void_t = void (*)(void* self, const void* mi);
Void_t o_InitDisplay;
void d_InitDisplay(void* self, const void* mi)
{
    o_InitDisplay(self, mi);
    hooks::guarded("FF4 StatusDetailsController.InitDisplay", [&] {
        if (!self || !unity::active_in_hierarchy(self)) return;
        // A visible canvas parent: not a background init during map load.
        void* canvas = unity::get_component_in_parent(self, kCanvas);
        if (!canvas || !ref::call<bool>(mBehaviourEnabled, canvas)) return;

        initialize_navigation(self);
        if (!g_user_opened) return;   // StatusMenuTracker.ShouldAnnounce

        frame::next_frame([] {
            void* controller = status::tracker().active_controller;
            if (!controller || !unity::alive(controller) || !unity::active_in_hierarchy(controller)) return;
            std::wstring status_text = read_status_details();
            if (!text::is_blank(status_text)) ffpr::say(status_text, true);
        }, "FF4.StatusDetails.Announce");
    });
}

Void_t o_ExitDisplay;
void d_ExitDisplay(void* self, const void* mi)
{
    o_ExitDisplay(self, mi);
    hooks::guarded("FF4 StatusDetailsController.ExitDisplay", [&] {
        if (!kStatusDetails.is(self)) return;   // folded body
        g_reader_character = nullptr;
        status::tracker().reset();
        g_user_opened = false;
    });
}

using SelectContent_t = void (*)(void* self, void* contents, int32_t index, void* cursor, const void* mi);
SelectContent_t o_SelectContent;
void d_SelectContent(void* self, void* contents, int32_t index, void* cursor, const void* mi)
{
    o_SelectContent(self, contents, index, cursor, mi);
    hooks::guarded("FF4 StatusWindowController.SelectContent", [&] {
        // StatusMenuTracker.MarkUserOpened, under the C# filters (window and cursor visible).
        if (!self || !contents || index < 0 || index >= unity::list_count(contents)) return;
        if (!unity::active_in_hierarchy(self)) return;
        if (!cursor || !unity::active_in_hierarchy(cursor)) return;
        g_user_opened = true;
    });
}

using SetActive_t = void (*)(void* self, bool is_active, const void* mi);
SetActive_t o_SetActive;
void d_SetActive(void* self, bool is_active, const void* mi)
{
    o_SetActive(self, is_active, mi);
    // StatusMenuStatePatches: the character list closing resets StatusMenuTracker (the core
    // resets StatusMenuState).
    if (!is_active) g_user_opened = false;   // a plain store: nothing to guard
}

int install()
{
    int n = 0;
    if (hooks::attach(mInitDisplay, &d_InitDisplay, &o_InitDisplay)) ++n;
    if (hooks::attach(mExitDisplay, &d_ExitDisplay, &o_ExitDisplay)) ++n;
    if (hooks::attach(mStatusSelectContent, &d_SelectContent, &o_SelectContent)) ++n;
    if (hooks::attach(mStatusSetActive, &d_SetActive, &o_SetActive)) ++n;

    // FF4's stat list replaces the core's (FF1) for this game; the Status key context and its
    // bindings stay the core's.
    actions::define("StatusNavigationReader.NavigateNext", [] { navigate_next(); });
    actions::define("StatusNavigationReader.NavigatePrevious", [] { navigate_previous(); });
    actions::define("StatusNavigationReader.JumpToNextGroup", [] { jump_to_next_group(); });
    actions::define("StatusNavigationReader.JumpToPreviousGroup", [] { jump_to_previous_group(); });
    actions::define("StatusNavigationReader.JumpToTop", [] { jump_to_top(); });
    actions::define("StatusNavigationReader.JumpToBottom", [] { jump_to_bottom(); });
    actions::define("StatusNavigationReader.ReadCurrentStat", [] { read_current_stat(); });
    return n;
}

core::Install s_install{"FF4.StatusDetails", &install, 505, game::kFF4};

} // namespace
} // namespace ffpr::ff4
