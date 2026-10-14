// Ports FF1 Patches/StatusMenuPatches.cs (StatusMenuState, CharacterSelectTransitionPatches,
// StatusDetailsState, StatusMenuHelpers, StatusMenuPatches, StatusDetailsPatches,
// StatusDetailsHelpers, StatusWindowSetActivePatch) and Menus/StatusDetailsReader.cs.
//
// All games: the Status menu's character list (Last.UI.KeyInput.StatusWindowController
// .SelectContent; FF6 has its own variant with the row, src/games/ff6/status_list.cpp) and the
// clears on its transitions to the item / magic / equipment windows (each game's own
// Serial.FFn AbilityWindowController).
// FF1 only: the status details screen (Serial.FF1.UI.KeyInput.StatusDetailsController). FF2-FF5
// have their own Serial.FFn StatusDetailsController and stat sets (status_navigation.cpp).
//
// Not ported: StatusMenuHelpers.GetCharacterRow (FF1 never calls it - "FF1 doesn't use front/back
// row") and the unreachable GetMemberData(0) fallback is kept only for a faulting read.
#include "status.h"
#include "../displayed_ui.h"
#include "../core.h"
#include "../dedup.h"
#include "../frame.h"
#include "../hooks.h"
#include "../log.h"
#include "../modtext.h"
#include "../refs.h"
#include "../say.h"
#include "../text.h"
#include "../unity.h"
#include "../system/system.h"

namespace ffpr::status {
namespace {

// --- game names -----------------------------------------------------------------------------

ref::Class  kMenuManager{"Last.UI", "MenuManager"};
ref::Method mMenuIsOpen{kMenuManager, "get_IsOpen", 0};

ref::Class  kStatusWindow{"Last.UI.KeyInput", "StatusWindowController"};
ref::Method mStatusWindowSetActive{kStatusWindow, "SetActive", 1};
// FF6 reads this list in src/games/ff6/status_list.cpp (this reader plus the character's row,
// which FF6's C# spoke on every main-menu character list): the SelectContent hook is FF1-FF5.
constexpr uint8_t kFF12345 = game::kFF1 | game::kFF2 | game::kFF3 | game::kFF4 | game::kFF5;
ref::Class  kStatusWindowList{"Last.UI.KeyInput", "StatusWindowController", kFF12345};
ref::Method mStatusWindowSelectContent{kStatusWindowList, "SelectContent", 3};
ref::Class  kStatusContent{"Serial.Template.UI", "StatusWindowContentControllerBase"};
ref::Field  fContentCharacterData{kStatusContent, "<CharacterData>k__BackingField"};

// CharacterSelectTransitionPatches: the windows the character list leads to.
ref::Class  kItemWindow{"Last.UI.KeyInput", "ItemWindowController"};
ref::Method mItemWindowSetActive{kItemWindow, "SetActive", 1};
ref::Class  kEquipWindow{"Last.UI.KeyInput", "EquipmentWindowController"};
ref::Method mEquipWindowSetActive{kEquipWindow, "SetActive", 1};
ref::Class  kAbilityWindowFF1{"Serial.FF1.UI.KeyInput", "AbilityWindowController", game::kFF1};
ref::Class  kAbilityWindowFF2{"Serial.FF2.UI.KeyInput", "AbilityWindowController", game::kFF2};
ref::Class  kAbilityWindowFF3{"Serial.FF3.UI.KeyInput", "AbilityWindowController", game::kFF3};
ref::Class  kAbilityWindowFF4{"Serial.FF4.UI.KeyInput", "AbilityWindowController", game::kFF4};
ref::Class  kAbilityWindowFF5{"Serial.FF5.UI.KeyInput", "AbilityWindowController", game::kFF5};
ref::Class  kAbilityWindowFF6{"Serial.FF6.UI.KeyInput", "AbilityWindowController", game::kFF6};
ref::Method mAbilityWindowSetActiveFF1{kAbilityWindowFF1, "SetActive", 1};
ref::Method mAbilityWindowSetActiveFF2{kAbilityWindowFF2, "SetActive", 1};
ref::Method mAbilityWindowSetActiveFF3{kAbilityWindowFF3, "SetActive", 1};
ref::Method mAbilityWindowSetActiveFF4{kAbilityWindowFF4, "SetActive", 1};
ref::Method mAbilityWindowSetActiveFF5{kAbilityWindowFF5, "SetActive", 1};
ref::Method mAbilityWindowSetActiveFF6{kAbilityWindowFF6, "SetActive", 1};

ref::Class  kUserDataManager{"Last.Management", "UserDataManager"};
ref::Method mUserDataInstance{kUserDataManager, "Instance", 0};
ref::Method mGetMemberData{kUserDataManager, "GetMemberData", 1};

// Characters and their parameters.
ref::Class  kOwnedCharacter{"Last.Data.User", "OwnedCharacterData"};
ref::Method mCharName{kOwnedCharacter, "get_Name", 0};
ref::Method mCharJobId{kOwnedCharacter, "get_JobId", 0};
ref::Method mCharCurrentExp{kOwnedCharacter, "get_CurrentExp", 0};
ref::Method mCharNextExp{kOwnedCharacter, "GetNextExp", 0};
ref::Method mCharParameter{kOwnedCharacter, "get_Parameter", 0};

ref::Class  kParamBase{"Last.Data", "CharacterParameterBase"};
ref::Field  fParamCurrentHp{kParamBase, "currentHP"};
ref::Field  fParamBaseLevel{kParamBase, "<BaseLevel>k__BackingField"};
ref::Class  kPlayerParam{"Last.Data", "PlayerCharacterParameter"};
ref::Field  fParamMpCountList{kPlayerParam, "<CurrentMpCountList>k__BackingField"};
ref::Method mConfirmedLevel{kPlayerParam, "ConfirmedLevel", 0};
ref::Method mConfirmedMaxHp{kPlayerParam, "ConfirmedMaxHp", 0};
ref::Field  fParamCurrentMp{kParamBase, "currentMP"};
ref::Method mConfirmedMaxMp{kPlayerParam, "ConfirmedMaxMp", 0};
ref::Method mConfirmedPower{kPlayerParam, "ConfirmedPower", 0};
ref::Method mConfirmedAgility{kPlayerParam, "ConfirmedAgility", 0};
ref::Method mConfirmedVitality{kPlayerParam, "ConfirmedVitality", 0};
ref::Method mConfirmedIntelligence{kPlayerParam, "ConfirmedIntelligence", 0};
ref::Method mConfirmedLuck{kPlayerParam, "ConfirmedLuck", 0};
ref::Method mConfirmedAttack{kPlayerParam, "ConfirmedAttack", 0};
ref::Method mConfirmedAccuracyRate{kPlayerParam, "ConfirmedAccuracyRate", 1};
ref::Method mConfirmedDefense{kPlayerParam, "ConfirmedDefense", 0};
ref::Method mConfirmedEvasionRate{kPlayerParam, "ConfirmedEvasionRate", 1};

ref::Class  kJob{"Last.Data.Master", "Job"};
ref::Method mJobMesIdName{kJob, "get_MesIdName", 0};

// Status.Character: the existing FF1 check that skips a player switch to the character already
// shown (StatusNavigationTracker.CurrentCharacterData.Name). Text, as today (DEDUP_INVENTORY.md
// row 32); recorded on open, cleared by StatusNavigationTracker.Reset.
constexpr const char* kCtxCharacter = "Status.Character";

// StatusDetailsReader.currentCharacterData
void* g_reader_character = nullptr;

Tracker g_tracker;

// --- StatusDetailsHelpers -------------------------------------------------------------------

// --- StatusMenuPatches.SelectContent_Postfix ------------------------------------------------

void on_select_content(void* contents, int index, void* cursor, void* window = nullptr)
{
    // Only announce when the menu is actually open. IsActive is set only after validation, so
    // SelectContent firing during game load (menu not open) suppresses nothing.
    void* menu_manager = ref::instance(kMenuManager);
    if (!menu_manager || !ref::call<bool>(mMenuIsOpen, menu_manager)) return;

    std::vector<void*> slots;
    if (!unity::enumerate_bounded(contents, slots, 9)) return;
    const int count = static_cast<int>(slots.size());
    if (count == 0) return;
    if (index < 0 || index >= count) return;
    void* content = displayed::cursor_row(cursor, contents);
    if (!content && cursor) content = unity::get_component_in_parent(cursor, kStatusContent);
    if (!content) content = slots[static_cast<size_t>(index)];
    if (!content) return;
    if (!displayed::container_visible(displayed::field(content, "view"))) return;
    void* data = ref::rd<void*>(content, fContentCharacterData);

    // Menu confirmed open with valid data: clear the other menu states, then set this one.
    ffpr::system::clear_other_menu_states("Status");
    menu_state::set_active(menu_state::STATUS_MENU, true);
    if (!data) { ffpr::say(T(L"Empty"), true); return; }

    void* view = displayed::field(content, "view");
    std::wstring announcement = displayed::character(view);
    auto job = displayed::text_field(view, "jobNameText");
    auto level = displayed::pair(view, "fixedLevelText", "currentLevelText");
    auto next = displayed::pair(view, "fixedNextText", "nextExpText");
    auto row = displayed::formation_row(window, content);
    if (!job.empty()) announcement += L", " + job;
    if (!level.empty()) announcement += L", " + level;
    if (!next.empty()) announcement += L", " + next;
    if (!row.empty()) announcement += L", " + row;
    if (announcement.empty()) return;

    ffpr::say(announcement, true);
}

using SelectContent_t = void (*)(void* self, void* contents, int32_t index, void* cursor, const void* mi);
SelectContent_t o_StatusSelectContent;
void d_StatusSelectContent(void* self, void* contents, int32_t index, void* cursor, const void* mi)
{
    o_StatusSelectContent(self, contents, index, cursor, mi);
    hooks::guarded("StatusWindowController.SelectContent", [&] { on_select_content(contents, index, cursor, self); });
}

ref::Class kFormationWindow{"Last.UI.KeyInput", "StatusWindowController", game::kFF2 | game::kFF3 | game::kFF4 | game::kFF5 | game::kFF6};
ref::Method mSwitchCorps{kFormationWindow, "SwitchCorps", 0};
using Switch = void (*)(void*, const void*);
Switch o_switch;
void d_switch(void* self, const void* mi) {
    o_switch(self, mi);
    hooks::guarded("StatusWindow.Formation", [self] {
        if (!kStatusWindow.is(self)) return;
        void* cursor = displayed::field(self, "selectCursor");
        void* content = displayed::cursor_row(cursor, displayed::field(self, "contentList"));
        if (!content) content = unity::get_component_in_parent(cursor, kStatusContent);
        auto row = displayed::formation_row(self, content);
        if (!row.empty()) ffpr::say(row, true);
    });
}

// --- SetActive postfixes --------------------------------------------------------------------

using SetActive_t = void (*)(void* self, bool is_active, const void* mi);

// CharacterSelectTransitionPatches.SetActive_Postfix: a menu window opening clears StatusMenuState
// (character selection -> the item / magic / equipment command bar).
void transition(bool is_active)
{
    if (is_active && status_menu_active()) reset_status_menu_state();
}

SetActive_t o_ItemSetActive;
void d_ItemSetActive(void* self, bool is_active, const void* mi)
{
    o_ItemSetActive(self, is_active, mi);
    hooks::guarded("ItemWindowController.SetActive", [&] { transition(is_active); });
}

SetActive_t o_EquipSetActive;
void d_EquipSetActive(void* self, bool is_active, const void* mi)
{
    o_EquipSetActive(self, is_active, mi);
    hooks::guarded("EquipmentWindowController.SetActive", [&] { transition(is_active); });
}

SetActive_t o_AbilitySetActive;
void d_AbilitySetActive(void* self, bool is_active, const void* mi)
{
    o_AbilitySetActive(self, is_active, mi);
    hooks::guarded("AbilityWindowController.SetActive", [&] { transition(is_active); });
}

// StatusWindowSetActivePatch: the character selection closing resets StatusMenuState.
SetActive_t o_StatusWindowSetActive;
void d_StatusWindowSetActive(void* self, bool is_active, const void* mi)
{
    o_StatusWindowSetActive(self, is_active, mi);
    hooks::guarded("StatusWindowController.SetActive", [&] {
        if (!is_active) reset_status_menu_state();
    });
}

// --- installs -------------------------------------------------------------------------------

// StatusMenuPatches + CharacterSelectTransitionPatches + StatusWindowSetActivePatch (all games).
int install_status_menu()
{
    int n = 0;
    if (hooks::attach(mSwitchCorps, &d_switch, &o_switch)) ++n;
    if (hooks::attach(mStatusWindowSelectContent, &d_StatusSelectContent, &o_StatusSelectContent)) ++n;
    else if (kStatusWindowList.applies()) LOGF("[Status Menu] Could not find SelectContent method");
    if (hooks::attach(mStatusWindowSetActive, &d_StatusWindowSetActive, &o_StatusWindowSetActive)) ++n;
    if (hooks::attach(mItemWindowSetActive, &d_ItemSetActive, &o_ItemSetActive)) ++n;
    if (hooks::attach(mEquipWindowSetActive, &d_EquipSetActive, &o_EquipSetActive)) ++n;
    // One AbilityWindowController per game; only the running game's ref applies.
    ref::Method* ability[] = {&mAbilityWindowSetActiveFF1, &mAbilityWindowSetActiveFF2, &mAbilityWindowSetActiveFF3,
                              &mAbilityWindowSetActiveFF4, &mAbilityWindowSetActiveFF5, &mAbilityWindowSetActiveFF6};
    for (ref::Method* m : ability)
        if (m->cls.applies() && hooks::attach(*m, &d_AbilitySetActive, &o_AbilitySetActive)) ++n;

    queries::define("StatusMenuState.IsActive", [] { return status_menu_active(); });
    queries::define("StatusMenuState.ShouldSuppress", [] { return status_menu_active(); });
    queries::define("StatusDetailsState.IsActive", [] { return status_details_active(); });
    queries::define("StatusDetailsState.ShouldSuppress", [] { return status_details_active(); });
    actions::define("StatusMenuState.ResetState", [] { reset_status_menu_state(); });
    return n;
}

core::Install s_status_menu{"StatusMenu", &install_status_menu, 120};

} // namespace

// --- StatusNavigationTracker ----------------------------------------------------------------

Tracker& tracker() { return g_tracker; }

void Tracker::reset()
{
    is_navigation_active = false;
    current_stat_index = 0;
    current_character_data = nullptr;
    active_controller = nullptr;
    dedup::reset(kCtxCharacter);
}

bool Tracker::validate_state() const
{
    return is_navigation_active && current_character_data && active_controller &&
           unity::alive(active_controller) && unity::active_in_hierarchy(active_controller);
}

// --- StatusMenuState / StatusDetailsState ---------------------------------------------------

bool status_menu_active() { return menu_state::is_active(menu_state::STATUS_MENU); }
void reset_status_menu_state() { menu_state::reset(menu_state::STATUS_MENU); }
bool status_details_active() { return menu_state::is_active(menu_state::STATUS_DETAILS); }

// --- StatusDetailsReader --------------------------------------------------------------------

void set_current_character_data(void* data) { g_reader_character = data; }
void clear_current_character_data() { g_reader_character = nullptr; }
void* get_current_character_data() { return g_reader_character; }

std::wstring read_status_details(void* controller)
{
    if (!controller) return {};
    void* view = displayed::field(displayed::field(controller, "statusController"), "view");
    auto result = displayed::character(view);
    auto level = displayed::pair(view, "fixedLevelText", "currentLevelText");
    return result + (level.empty() ? L"" : L", " + level);
}

// --- characters -----------------------------------------------------------------------------

std::wstring character_name(void* data) { return data ? ref::call_str(mCharName, data) : std::wstring(); }
int character_job_id(void* data) { return data ? ref::call<int32_t>(mCharJobId, data) : 0; }
int character_current_exp(void* data) { return data ? ref::call<int32_t>(mCharCurrentExp, data) : 0; }
int character_next_exp(void* data) { return data ? ref::call<int32_t>(mCharNextExp, data) : 0; }
void* character_parameter(void* data) { return data ? ref::call<void*>(mCharParameter, data) : nullptr; }

int param_confirmed_level(void* p) { return p ? ref::call<int32_t>(mConfirmedLevel, p) : 0; }

int param_level_or_base(void* p)
{
    if (!p) return 0;
    // C#: try ConfirmedLevel(), on an exception BaseLevel. A failed native call answers 0.
    int level = param_confirmed_level(p);
    if (level <= 0) level = ref::rd<int32_t>(p, fParamBaseLevel);
    return level;
}

int param_current_hp(void* p) { return ref::rd<int32_t>(p, fParamCurrentHp); }
int param_max_hp(void* p) { return p ? ref::call<int32_t>(mConfirmedMaxHp, p) : 0; }

int param_mp_count(void* p, int level)
{
    int value = 0;
    if (p && int_dict_get(ref::rd<void*>(p, fParamMpCountList), level, value)) return value;
    return 0;
}

int param_power(void* p) { return p ? ref::call<int32_t>(mConfirmedPower, p) : 0; }
int param_agility(void* p) { return p ? ref::call<int32_t>(mConfirmedAgility, p) : 0; }
int param_vitality(void* p) { return p ? ref::call<int32_t>(mConfirmedVitality, p) : 0; }
int param_intelligence(void* p) { return p ? ref::call<int32_t>(mConfirmedIntelligence, p) : 0; }
int param_luck(void* p) { return p ? ref::call<int32_t>(mConfirmedLuck, p) : 0; }
int param_attack(void* p) { return p ? ref::call<int32_t>(mConfirmedAttack, p) : 0; }
int param_accuracy_rate(void* p) { return p ? ref::call<int32_t>(mConfirmedAccuracyRate, p, false) : 0; }
int param_defense(void* p) { return p ? ref::call<int32_t>(mConfirmedDefense, p) : 0; }
int param_evasion_rate(void* p) { return p ? ref::call<int32_t>(mConfirmedEvasionRate, p, false) : 0; }

// StatusMenuHelpers.GetCurrentJobName
std::wstring current_job_name(void* data)
{
    if (!data) return {};
    int job_id = character_job_id(data);
    if (job_id <= 0) return {};
    void* job = master_entry(kJob, job_id);
    if (!job) return {};
    std::wstring mes_id = ref::call_str(mJobMesIdName, job);
    if (mes_id.empty()) return {};
    std::wstring name = game_message(mes_id, false);
    return text::is_blank(name) ? std::wstring() : name;
}

} // namespace ffpr::status
