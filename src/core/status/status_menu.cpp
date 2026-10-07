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

// FF1's status details screen. ExitDisplay's body is FOLDED in FF1 (RVA 0x29B6F0 is shared by
// 11 methods, among them Touch StatusDetailsController.ExitDisplay, NewGameWindowController
// .ExitNameSelect and DetectionScriptInputManager.RemoveInput): its detour class-checks self.
ref::Class  kStatusDetailsFF1{"Serial.FF1.UI.KeyInput", "StatusDetailsController", game::kFF1};
ref::Field  fDetailsStatusController{kStatusDetailsFF1, "statusController"};
ref::Method mDetailsInitDisplay{kStatusDetailsFF1, "InitDisplay", 0};
ref::Method mDetailsExitDisplay{kStatusDetailsFF1, "ExitDisplay", 0};
ref::Class  kCharaStatusFF1{"Serial.FF1.UI.KeyInput", "AbilityCharaStatusController", game::kFF1};
ref::Field  fCharaStatusTargetData{kCharaStatusFF1, "targetData"};
ref::Class  kDetailsBase{"Serial.Template.UI", "StatusDetailsControllerBase", game::kFF1};
ref::Method mSetNextPlayer{kDetailsBase, "SetNextPlayer", 0};
ref::Method mSetPrevPlayer{kDetailsBase, "SetPrevPlayer", 0};

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

// GetCharacterDataFromController: statusController (AbilityCharaStatusController) -> targetData.
void* character_data_from_controller(void* controller)
{
    if (!controller) return nullptr;
    try {
        void* status_controller = ref::rd<void*>(controller, fDetailsStatusController);
        if (!status_controller) {
            LOGF("[Status Details] statusController is null");
            return nullptr;
        }
        void* target = ref::rd<void*>(status_controller, fCharaStatusTargetData);
        if (!target) {
            LOGF("[Status Details] targetData is null");
            return nullptr;
        }
        return target;
    } catch (...) {
        LOGF("[Status Details] Error getting character data from the controller");
    }

    // Fallback: the first party member (only after a faulting read).
    void* udm = ref::call_static<void*>(mUserDataInstance);
    return udm ? ref::call<void*>(mGetMemberData, udm, static_cast<int32_t>(0)) : nullptr;
}

// --- StatusMenuPatches.SelectContent_Postfix ------------------------------------------------

void on_select_content(void* contents, int index)
{
    // Only announce when the menu is actually open. IsActive is set only after validation, so
    // SelectContent firing during game load (menu not open) suppresses nothing.
    void* menu_manager = ref::instance(kMenuManager);
    if (!menu_manager || !ref::call<bool>(mMenuIsOpen, menu_manager)) return;

    int count = unity::list_count(contents);
    if (!contents || count == 0) return;
    if (index < 0 || index >= count) return;
    void* content = unity::list_at(contents, index);
    if (!content) return;
    void* data = ref::rd<void*>(content, fContentCharacterData);
    if (!data) return;

    // Menu confirmed open with valid data: clear the other menu states, then set this one.
    ffpr::system::clear_other_menu_states("Status");
    menu_state::set_active(menu_state::STATUS_MENU, true);

    std::wstring name = character_name(data);
    if (text::is_blank(name)) return;
    std::wstring announcement = name;

    std::wstring job = current_job_name(data);
    if (!job.empty()) announcement += L", " + job;

    // Row information removed: FF1 has no front/back row system.

    if (void* param = character_parameter(data)) {
        int level = param_level_or_base(param);
        if (level > 0) announcement += L", " + TF(L"Level {0}", {text::num(level)});
        announcement += L", " + TF(L"HP {0}/{1}", {text::num(param_current_hp(param)), text::num(param_max_hp(param))});
        if (game::flags().has_mp)
            announcement += L", " + TF(L"MP {0}/{1}", {text::num(ref::rd<int32_t>(param, fParamCurrentMp)),
                                                     text::num(ref::call<int32_t>(mConfirmedMaxMp, param))});
    }

    ffpr::say(announcement, true);
}

using SelectContent_t = void (*)(void* self, void* contents, int32_t index, void* cursor, const void* mi);
SelectContent_t o_StatusSelectContent;
void d_StatusSelectContent(void* self, void* contents, int32_t index, void* cursor, const void* mi)
{
    o_StatusSelectContent(self, contents, index, cursor, mi);
    hooks::guarded("StatusWindowController.SelectContent", [&] { on_select_content(contents, index); });
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

// --- StatusDetailsPatches (FF1) -------------------------------------------------------------

// DelayedStatusInit: one frame after InitDisplay, when the UI has updated.
void delayed_status_init(void* controller)
{
    if (!unity::alive(controller)) return;
    // Only initialize when the status screen is actually visible.
    if (!unity::active_in_hierarchy(controller)) return;

    void* data = character_data_from_controller(controller);
    if (!data) {
        LOGF("[Status Details] Could not get character data");
        return;
    }

    Tracker& t = tracker();
    t.is_navigation_active = true;
    t.current_stat_index = 0;
    t.active_controller = controller;
    t.current_character_data = data;
    dedup::reset(kCtxCharacter);
    dedup::text(kCtxCharacter, character_name(data), dedup::Scope::Session);

    set_current_character_data(data);
    initialize_stat_list();

    std::wstring status = read_status_details(controller);
    if (!text::is_blank(status)) ffpr::say(status);
}

using Void_t = void (*)(void* self, const void* mi);

Void_t o_InitDisplay;
void d_InitDisplay(void* self, const void* mi)
{
    o_InitDisplay(self, mi);
    hooks::guarded("StatusDetailsController.InitDisplay", [&] {
        if (!kStatusDetailsFF1.is(self)) return;
        // Status details active immediately: clear the other states and set this one.
        ffpr::system::clear_other_menu_states("StatusDetails");
        menu_state::set_active(menu_state::STATUS_DETAILS, true);
        void* controller = self;
        frame::next_frame([controller] {
            hooks::guarded("StatusDetails.DelayedStatusInit", [&] { delayed_status_init(controller); });
        }, "StatusDetails.Init");
    });
}

Void_t o_ExitDisplay;
void d_ExitDisplay(void* self, const void* mi)
{
    o_ExitDisplay(self, mi);
    hooks::guarded("StatusDetailsController.ExitDisplay", [&] {
        if (!kStatusDetailsFF1.is(self)) return;   // folded body: other classes share it
        clear_current_character_data();
        tracker().reset();
        menu_state::reset(menu_state::STATUS_DETAILS);
    });
}

// DelayedPlayerSwitchInit: one frame after SetNextPlayer / SetPrevPlayer (RB / LB).
void delayed_player_switch_init(void* controller)
{
    if (!unity::alive(controller) || !status_details_active()) return;
    if (!unity::active_in_hierarchy(controller)) return;

    void* data = character_data_from_controller(controller);
    if (!data) {
        LOGF("[Status Details] Could not get character data after switch");
        return;
    }

    // Skip if it is the character already shown (redundant calls).
    Tracker& t = tracker();
    if (t.current_character_data) {
        if (!dedup::text(kCtxCharacter, character_name(data), dedup::Scope::Session)) return;
    } else {
        dedup::reset(kCtxCharacter);
        dedup::text(kCtxCharacter, character_name(data), dedup::Scope::Session);
    }

    t.current_character_data = data;
    t.current_stat_index = 0;
    t.active_controller = controller;
    set_current_character_data(data);

    std::wstring status = read_status_details(controller);
    if (!text::is_blank(status)) ffpr::say(status, true);
}

void handle_player_switch(void* self)
{
    if (!status_details_active()) return;
    if (!tracker().is_navigation_active) return;
    if (!kStatusDetailsFF1.is(self)) return;
    void* controller = self;
    frame::next_frame([controller] {
        hooks::guarded("StatusDetails.DelayedPlayerSwitch", [&] { delayed_player_switch_init(controller); });
    }, "StatusDetails.Switch");
}

Void_t o_SetNextPlayer;
void d_SetNextPlayer(void* self, const void* mi)
{
    o_SetNextPlayer(self, mi);
    hooks::guarded("StatusDetailsControllerBase.SetNextPlayer", [&] { handle_player_switch(self); });
}

Void_t o_SetPrevPlayer;
void d_SetPrevPlayer(void* self, const void* mi)
{
    o_SetPrevPlayer(self, mi);
    hooks::guarded("StatusDetailsControllerBase.SetPrevPlayer", [&] { handle_player_switch(self); });
}

// --- installs -------------------------------------------------------------------------------

// StatusMenuPatches + CharacterSelectTransitionPatches + StatusWindowSetActivePatch (all games).
int install_status_menu()
{
    int n = 0;
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

// StatusDetailsPatches (FF1's status details controller).
int install_status_details()
{
    int n = 0;
    if (hooks::attach(mDetailsInitDisplay, &d_InitDisplay, &o_InitDisplay)) ++n;
    else LOGF("[Status Details] Could not find InitDisplay method");
    if (hooks::attach(mDetailsExitDisplay, &d_ExitDisplay, &o_ExitDisplay)) ++n;
    else LOGF("[Status Details] Could not find ExitDisplay method");
    if (hooks::attach(mSetNextPlayer, &d_SetNextPlayer, &o_SetNextPlayer)) ++n;
    else LOGF("[Status Details] Could not find SetNextPlayer method");
    if (hooks::attach(mSetPrevPlayer, &d_SetPrevPlayer, &o_SetPrevPlayer)) ++n;
    else LOGF("[Status Details] Could not find SetPrevPlayer method");
    return n;
}

core::Install s_status_details{"StatusDetails", &install_status_details, 120, game::kFF1};

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
    std::vector<std::wstring> parts;
    if (void* data = g_reader_character) {
        std::wstring name = character_name(data);
        if (!text::is_blank(name)) parts.push_back(name);
        if (void* param = character_parameter(data)) {
            parts.push_back(TF(L"Level {0}", {text::num(param_confirmed_level(param))}));
            parts.push_back(TF(L"HP: {0} / {1}", {text::num(param_current_hp(param)), text::num(param_max_hp(param))}));
        }
    }
    std::wstring out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out += L". ";
        out += parts[i];
    }
    return out;
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
