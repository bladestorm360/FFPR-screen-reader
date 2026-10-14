#include "../../core/displayed_ui.h"
// Ports FF5 Patches/JobAbilityPatches.cs (JobMenuTracker, AbilityMenuTracker, AbilitySlotMenuTracker,
// AbilityEquipMenuTracker, JobAbilityTrackerHelper, the five navigation patches, the four details
// announcers and FieldJobAbilityReannouncePatches), Utils/JobEquipData.cs, and the FF5 job/ability
// branches of Core/InputManager.cs HandleItemInfoKey. Phase 1's FF5 proof hook
// (AbilityChangeController) lives here.
//
// Screens: Jobs (JobChangeWindowController), the Abilities/Magic window (AbilityWindowController:
// command bar, spell list, target list) and ability equipping (AbilityChangeController: command
// slots and the ability list). FF5's field spell list is the ability list here, so this file is
// also FF5's magic menu (the core magic list is FF1-only).
//
// The details key: the core's I key and right-stick-up call the core cascade directly, so FF5's
// four announcers are published through the cascade's job-selection entry
// ("JobSelectionPatches.IsActive" / "JobSelectionPatches.GetCurrentDescription", FF1-only in the
// core). The text query returns T("No description available") itself when empty, FF5's wording.
//
// Folded body: AbilityUseContentListController.SelectContent shares its RVA with SetCursor of the
// same class (as in the C#, where the Harmony patch on the shared body fired for both); the detour
// class-checks self. Never hooked (folded empty stubs): JobChangeWindowController.NoneInit /
// ResetController, AbilityChangeController.NoneInit, AbilityUseContentListController.NonInit.
//
// The "last index" guards are the C#'s own index checks, moved to dedup:: as they are (Session
// scope: they are cleared only where the C# cleared them).
#include <cstdint>
#include <string>
#include <vector>
#include "ff5_internal.h"
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/battle/battle_util.h"
#include "../../core/system/cursor_event.h"
#include "../../core/system/system.h"

namespace ffpr::ff5 {
namespace {

constexpr uint8_t k5 = game::kFF5;

// --- game names ------------------------------------------------------------------------------
ref::Class  kJobWin{"Serial.FF5.UI.KeyInput", "JobChangeWindowController", k5};
ref::Field  fJobView{kJobWin, "view"};
ref::Method mJobSelectContent{kJobWin, "SelectContent", 2};
ref::Method mJobSetActive{kJobWin, "SetActive", 1};
ref::Method mJobConfirmPopupInit{kJobWin, "ConfirmPopupInit", 0};
ref::Method mJobFixJobChangeInit{kJobWin, "FixJobChangeInit", 0};
ref::Method mJobGetTargetCharacterData{kJobWin, "GetTargetCharacterData", 0};
ref::Method mJobGetReleaseJobs{kJobWin, "GetReleaseJobs", 0};

ref::Class  kJobView{"Serial.FF5.UI.KeyInput", "JobChangeWindowView", k5};
ref::Method mViewInfoSkillLevelValueText{kJobView, "get_InfoSkillLevelValueText", 0};
ref::Method mViewInfoJobLevelMasterText{kJobView, "get_InfoJobLevelMasterText", 0};
ref::Method mViewInfoJobDescriptText{kJobView, "get_InfoJobDescriptText", 0};
ref::Method mViewInfoFixedEquipText{kJobView, "get_InfoFixedEquipText", 0};
ref::Method mViewInfoAllEquipText{kJobView, "get_InfoAllEquipText", 0};
ref::Method mViewInfoNothingEquipAllText{kJobView, "get_InfoNothingEquipAllText", 0};
ref::Field  fViewInfoJobLevelDetailsValue{kJobView, "infoJobLevelDetailsValue"};
ref::Field  fViewInfoJobLevelDetailsMaxValue{kJobView, "infoJobLevelDetailsMaxValue"};
ref::Field  fViewInfoContentBase{kJobView, "infoContentBase"};
ref::Field  fViewInfoDescriptionBase{kJobView, "infoDescriptionBase"};

ref::Class  kJob{"Last.Data.Master", "Job", k5};
ref::Method mJobId{kJob, "get_Id", 0};
ref::Method mJobMesIdName{kJob, "get_MesIdName", 0};

ref::Class  kCommand{"Last.Data.Master", "Command", k5};
ref::Method mCommandMesIdName{kCommand, "get_MesIdName", 0};

ref::Class  kAbility{"Last.Data.Master", "Ability", k5};
ref::Method mAbilityUseValue{kAbility, "get_UseValue", 0};

ref::Class  kOwnedAbility{"Last.Data.User", "OwnedAbility", k5};
ref::Method mOwnedAbilityMesIdName{kOwnedAbility, "get_MesIdName", 0};
ref::Method mOwnedAbilityMesIdDescription{kOwnedAbility, "get_MesIdDescription", 0};
ref::Method mOwnedAbilityAbility{kOwnedAbility, "get_Ability", 0};

ref::Class  kAbilityUtility{"Last.Systems", "AbilityUtility", k5};
ref::Method mCanUseMenuAbility{kAbilityUtility, "CanUseMenuAbility", 2};

ref::Class  kAbilityCmd{"Serial.FF5.UI.KeyInput", "AbilityCommandController", k5};
ref::Field  fCmdContentList{kAbilityCmd, "contentList"};
ref::Method mCmdSelectContent{kAbilityCmd, "SelectContent", 1};
ref::Class  kAbilityCmdView{"Serial.FF5.UI.KeyInput", "AbilityCommandContentView", k5};
ref::Field  fCmdViewCommand{kAbilityCmdView, "<Command>k__BackingField"};

ref::Class  kAbilityChange{"Serial.FF5.UI.KeyInput", "AbilityChangeController", k5};
ref::Field  fChangeView{kAbilityChange, "view"};
ref::Method mChangeSelectContent{kAbilityChange, "SelectContent", 2};
ref::Method mChangeSelectCommand{kAbilityChange, "SelectCommand", 1};
ref::Method mChangeSetActive{kAbilityChange, "SetActive", 1};
ref::Method mChangeSelectCommandInit{kAbilityChange, "SelectCommandInit", 0};
ref::Method mChangeSelectListInit{kAbilityChange, "SelectListInit", 0};
ref::Method mChangeChangedInit{kAbilityChange, "ChangedInit", 0};
ref::Method mChangeConfirmPopupInit{kAbilityChange, "ConfirmPopupInit", 0};
ref::Class  kAbilityChangeView{"Serial.FF5.UI.KeyInput", "AbilityChangeView", k5};
ref::Field  fChangeViewScrollView{kAbilityChangeView, "scrollView"};
ref::Field  fChangeViewCurrentList{kAbilityChangeView, "currentList"};
ref::Class  kAbilityChangeContent{"Serial.FF5.UI.KeyInput", "AbilityChangeContentController", k5};
ref::Class  kAbilityChangeContentBase{"Serial.Template.UI", "AbilityChangeContentBaseController", k5};
ref::Field  fContentTargetData{kAbilityChangeContentBase, "targetData"};
ref::Class  kAbilityChangeEquiped{"Serial.FF5.UI.KeyInput", "AbilityChangeEquipedController", k5};
ref::Field  fEquipedView{kAbilityChangeEquiped, "view"};
ref::Class  kAbilityChangeEquipedView{"Serial.FF5.UI.KeyInput", "AbilityChangeEquipedView", k5};
ref::Field  fEquipedViewContent{kAbilityChangeEquipedView, "content"};

ref::Class  kEquipData{"Serial.Template.UI", "AbilityEquipData", k5};
ref::Field  fEquipIndex{kEquipData, "<Index>k__BackingField"};
ref::Field  fEquipAbility{kEquipData, "<Ability>k__BackingField"};
ref::Field  fEquipNameMessageId{kEquipData, "<NameMessageId>k__BackingField"};
ref::Field  fEquipDescriptionMessageId{kEquipData, "<DescriptionMessageId>k__BackingField"};
ref::Field  fEquipIsFocus{kEquipData, "<IsFocus>k__BackingField"};
ref::Field  fEquipIsEquiped{kEquipData, "<IsEquiped>k__BackingField"};

ref::Class  kAbilityUse{"Serial.FF5.UI.KeyInput", "AbilityUseContentListController", k5};
ref::Field  fUseContentList{kAbilityUse, "contentList"};
ref::Field  fUseSelectCursor{kAbilityUse, "selectCursor"};
ref::Method mUseSelectContent{kAbilityUse, "SelectContent", 2};
ref::Method mUseSingleInit{kAbilityUse, "SingleInit", 0};
ref::Method mUseAllInit{kAbilityUse, "AllInit", 0};
ref::Class  kItemTargetContent{"Last.UI.KeyInput", "ItemTargetSelectContentController", k5};
ref::Field  fTargetCurrentData{kItemTargetContent, "<CurrentData>k__BackingField"};

ref::Class  kAbilityList{"Serial.FF5.UI.KeyInput", "AbilityContentListController", k5};
ref::Field  fListSelectCursor{kAbilityList, "selectCursor"};
ref::Field  fListContentList{kAbilityList, "contentList"};
ref::Method mListSetCursor{kAbilityList, "SetCursor", 4};
ref::Method mListGetTargetData{kAbilityList, "get_TargetData", 0};
ref::Class  kAbilityInfoContent{"Last.UI.KeyInput", "BattleAbilityInfomationContentController", k5};
ref::Field  fAbilityInfoData{kAbilityInfoContent, "<Data>k__BackingField"};

ref::Class  kAbilityWin{"Serial.FF5.UI.KeyInput", "AbilityWindowController", k5};
ref::Field  fWinListController{kAbilityWin, "listController"};
ref::Field  fWinUseController{kAbilityWin, "useController"};
ref::Method mWinSetActive{kAbilityWin, "SetActive", 1};
ref::Method mWinCommandInit{kAbilityWin, "CommandInit", 0};
ref::Method mWinUseListInit{kAbilityWin, "UseListInit", 0};
ref::Method mWinUseTargetInit{kAbilityWin, "UseTargetInit", 0};
ref::Method mWinNonInit{kAbilityWin, "NonInit", 0};

ref::Class  kMainMenu{"Last.UI.KeyInput", "MainMenuController", k5};
ref::Method mMainMenuShow{kMainMenu, "Show", 1};

// --- dedup contexts (the C#'s _lastIndex guards) -----------------------------------------------
constexpr const char* kCtxJob = "FF5.JobChangeWindowController.SelectContent";
constexpr const char* kCtxCommand = "FF5.AbilityCommandController.SelectContent";
constexpr const char* kCtxEquipList = "FF5.AbilityChangeController.SelectContent";
constexpr const char* kCtxEquipCommand = "FF5.AbilityChangeController.SelectCommand";
constexpr const char* kCtxUseTarget = "FF5.AbilityUseContentListController.SelectContent";
// Bumped each time the target SelectContent hook speaks. AbilityWindowController.UseTargetInit
// calls AbilityUseContentListController.SelectContent itself (FF5 0x9BD8C0 +0x1FD), so the entry
// target was spoken by that hook and, after the Init reset the guard, again by the focus read.
// UseTargetInit compares this around its original and requests the read only when the game's own
// call spoke nothing (one event, one line; identity, not text).
int g_use_target_spoken_seq = 0;
constexpr const char* kCtxSpell = "FF5.AbilityContentListController.SetCursor";

bool is_new(const char* ctx, int index) { return dedup::index(ctx, index, dedup::Scope::Session); }

// --- trackers ----------------------------------------------------------------------------------
struct JobTracker { void* controller = nullptr; int index = -1; } g_job;
struct AbilityTracker { void* controller = nullptr; std::wstring description; } g_ability;
struct SlotTracker { void* controller = nullptr; std::wstring description; } g_slot;
struct EquipTracker { void* controller = nullptr; std::wstring description; } g_equip;

// AbilityChangeController has no cursor field: the index only arrives as a parameter.
int g_cached_list_index = 0;
int g_cached_command_index = 0;

// The windows this file reads (for the generic cursor reader's suppression), from their hooks.
void* g_job_window = nullptr;
void* g_ability_window = nullptr;
void* g_change_window = nullptr;

bool controller_active(void* c) { return c && unity::alive(c) && unity::active_in_hierarchy(c); }

void clear_ability_tracker()
{
    menu_state::set_active(ABILITY_MENU, false);
    g_ability = {};
}
void clear_slot_tracker()
{
    menu_state::set_active(ABILITY_SLOT_MENU, false);
    g_slot = {};
}
void clear_equip_tracker()
{
    menu_state::set_active(ABILITY_EQUIP_MENU, false);
    g_equip = {};
}
void clear_job_tracker()
{
    menu_state::set_active(JOB_MENU, false);
    g_job = {};
}

/// ItemMenuTracker.ClearState (the core item menu's reset).
void clear_item_tracker()
{
    if (actions::defined("ItemMenuState.ResetState")) actions::run("ItemMenuState.ResetState");
}

/// JobAbilityTrackerHelper.ClearAllTrackers.
void clear_all_trackers()
{
    clear_job_tracker();
    clear_ability_tracker();
    clear_slot_tracker();
    clear_equip_tracker();
}

bool job_validate()
{
    if (menu_state::is_active(JOB_MENU) && !controller_active(g_job.controller)) {
        clear_job_tracker();
        return false;
    }
    return menu_state::is_active(JOB_MENU);
}
bool ability_validate()
{
    if (menu_state::is_active(ABILITY_MENU) && !controller_active(g_ability.controller)) {
        clear_ability_tracker();
        return false;
    }
    return menu_state::is_active(ABILITY_MENU);
}
bool slot_validate()
{
    if (menu_state::is_active(ABILITY_SLOT_MENU) && !controller_active(g_slot.controller)) {
        clear_slot_tracker();
        return false;
    }
    return menu_state::is_active(ABILITY_SLOT_MENU);
}
bool equip_validate()
{
    if (menu_state::is_active(ABILITY_EQUIP_MENU) && !controller_active(g_equip.controller)) {
        clear_equip_tracker();
        return false;
    }
    return menu_state::is_active(ABILITY_EQUIP_MENU);
}

// --- small readers ---------------------------------------------------------------------------
std::wstring mes(ref::Method& getter, void* obj) { return obj ? message(ref::call_str(getter, obj)) : std::wstring(); }

int use_value(void* ability) { return ability ? ref::call<int32_t>(mAbilityUseValue, ability) : 0; }

/// Regex.Replace(name, "<[^>]+>", "").Trim()
std::wstring strip_tags(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'<') {
            size_t close = s.find(L'>', i + 1);
            if (close != std::wstring::npos && close > i + 1) {
                i = close;
                continue;
            }
        }
        out += s[i];
    }
    return text::trim(out);
}

std::wstring view_text(ref::Method& getter, void* view) { return view ? text_of(ref::call<void*>(getter, view)) : std::wstring(); }

// --- JobEquipData ----------------------------------------------------------------------------
struct IconName { int id; const wchar_t* key; };
constexpr IconName kIconNames[] = {
    {1, L"Sword"},  {3, L"Knight Sword"}, {4, L"Katana"}, {5, L"Knife"},     {6, L"Spear"}, {8, L"Axe"},
    {9, L"Hammer"}, {10, L"Staff"},       {11, L"Rod"},   {12, L"Bow"},      {14, L"Boomerang"},
    {16, L"Harp"},  {17, L"Bell"},        {19, L"Whip"},  {20, L"Flail"},    {29, L"Shield"},
};
struct JobIcons { int job; int icons[6]; int count; };
constexpr JobIcons kJobIcons[] = {
    {1, {}, 0},                        // Freelancer (Any)
    {2, {5, 14}, 2},                   // Thief
    {3, {}, 0},                        // Monk (none)
    {4, {5, 1, 11, 10, 20}, 5},        // Red Mage
    {5, {10, 20}, 2},                  // White Mage
    {6, {5, 11}, 2},                   // Black Mage
    {7, {5, 1, 3, 29}, 4},             // Knight
    {8, {5, 14}, 2},                   // Ninja
    {9, {5, 12}, 2},                   // Ranger
    {10, {5, 17}, 2},                  // Geomancer
    {11, {5, 6, 29}, 3},               // Dragoon
    {12, {5, 16}, 2},                  // Bard
    {13, {5, 11}, 2},                  // Summoner
    {14, {5, 8, 9, 29}, 4},            // Berserker
    {15, {5, 4, 29}, 3},               // Samurai
    {16, {5, 11, 10, 20}, 4},          // Time Mage
    {17, {5, 10, 20}, 3},              // Chemist
    {18, {5}, 1},                      // Dancer
    {19, {5, 1, 11, 29}, 4},           // Blue Mage
    {20, {5, 1, 29}, 3},               // Mystic Knight
    {21, {5, 19}, 2},                  // Beastmaster
    {22, {5, 11, 10, 20, 14, 29}, 6},  // Mime
};

/// JobEquipData.GetEquippableText: localized, comma-joined categories in render order, or empty.
std::wstring equippable_text(int job_id)
{
    for (const JobIcons& j : kJobIcons) {
        if (j.job != job_id) continue;
        std::vector<std::wstring> parts;
        for (int i = 0; i < j.count; ++i)
            for (const IconName& n : kIconNames)
                if (n.id == j.icons[i]) parts.push_back(T(n.key));
        return text::join(parts);
    }
    return {};
}

// --- JobChangeWindowController.SelectContent ---------------------------------------------------
void* job_at(void* controller, int index, int& count)
{
    void* jobs = ref::call<void*>(mJobGetReleaseJobs, controller);
    count = jobs ? unity::list_count(jobs) : 0;
    return ui::list_item(jobs, index);
}

void announce_job_details(bool interrupt, bool announce_if_empty);

bool announce_job(void* self, int index)
{
    if (!self) return false;
    clear_item_tracker();
    menu_state::set_active(JOB_MENU, true);
    g_job.controller = self;
    g_job.index = index;
    g_job_window = self;

    if (!ref::call<void*>(mJobGetTargetCharacterData, self)) return false;
    int count = 0;
    void* job = job_at(self, index, count);
    if (!job) return false;
    std::wstring job_name = mes(mJobMesIdName, job);
    if (blank(job_name)) return false;

    void* view = ref::rd<void*>(self, fJobView);
    if (!view) return false;
    std::wstring level = view_text(mViewInfoSkillLevelValueText, view);
    void* master_text = ref::call<void*>(mViewInfoJobLevelMasterText, view);
    bool mastered = master_text && unity::active_in_hierarchy(master_text);

    std::wstring announcement = job_name;
    if (!blank(level)) {
        announcement += L" Lv. " + level + L":";
        if (mastered) {
            announcement += L" Mastered!";
        } else {
            std::wstring cur = text_of(ref::rd<void*>(view, fViewInfoJobLevelDetailsValue));
            std::wstring max = text_of(ref::rd<void*>(view, fViewInfoJobLevelDetailsMaxValue));
            if (!blank(cur) && !blank(max)) announcement += L" ABP: " + cur + L"/" + max;
        }
    }
    announcement = ui::menu_position(announcement, index, count);

    if (!is_new(kCtxJob, index)) return false;
    ffpr::say(announcement);
    if (auto_detail()) announce_job_details(false, false);
    return true;
}

// --- JobDetailsAnnouncer -----------------------------------------------------------------------
bool panel_active(void* go) { return go && unity::alive(go) && unity::active_in_hierarchy(go); }

/// BuildEquippableAnnouncement: the "Equippable" row (icons named from JobEquipData; Freelancer and
/// Monk read the game's own text).
std::wstring build_equippable(void* controller, void* view)
{
    std::wstring label = text_if_shown(ref::call<void*>(mViewInfoFixedEquipText, view));
    std::wstring special = text_if_shown(ref::call<void*>(mViewInfoAllEquipText, view));
    if (special.empty()) special = text_if_shown(ref::call<void*>(mViewInfoNothingEquipAllText, view));
    if (!blank(special)) return blank(label) ? special : label + L": " + special;

    int count = 0;
    void* job = job_at(controller, g_job.index, count);
    if (!job) return {};
    std::wstring equippable = equippable_text(ref::call<int32_t>(mJobId, job));
    if (blank(equippable)) return {};
    return blank(label) ? equippable : label + L": " + equippable;
}

/// The text the details key speaks for the job screen ("" = say nothing).
std::wstring job_details_text(bool announce_if_empty)
{
    if (!job_validate()) return {};
    void* controller = g_job.controller;
    if (!controller_active(controller)) {
        clear_job_tracker();
        return {};
    }
    void* view = ref::rd<void*>(controller, fJobView);
    if (!view) {
        LOGF("[Job Details] Could not access view");
        return {};
    }
    if (panel_active(ref::rd<void*>(view, fViewInfoContentBase)) &&
        !panel_active(ref::rd<void*>(view, fViewInfoDescriptionBase))) {
        std::wstring equippable = build_equippable(controller, view);
        if (!blank(equippable)) return equippable;
        if (!announce_if_empty) return {};
    }
    std::wstring description = view_text(mViewInfoJobDescriptText, view);
    if (blank(description)) return announce_if_empty ? T(L"No description available") : std::wstring();
    return description;
}

void announce_job_details(bool interrupt, bool announce_if_empty)
{
    std::wstring s = hooks::guarded_r("FF5.JobDetails", std::wstring(), [&] { return job_details_text(announce_if_empty); });
    if (!s.empty()) ffpr::say(s, interrupt);
}

/// AbilityDetailsAnnouncer / AbilitySlotDetailsAnnouncer / AbilityEquipDetailsAnnouncer.
std::wstring stored_details(bool valid, const std::wstring& description, bool announce_if_empty)
{
    if (!valid) return {};
    if (blank(description)) return announce_if_empty ? T(L"No description available") : std::wstring();
    return description;
}

void speak_details(const std::wstring& s, bool interrupt)
{
    if (!s.empty()) ffpr::say(s, interrupt);
}

// --- AbilityCommandController.SelectContent ----------------------------------------------------
bool announce_command(void* self, int index)
{
    if (!self) return false;
    // The UI pre-load initialises the window on the field (core game_state.cpp, IsFieldState).
    if (ffpr::system::is_field_state()) return false;
    void* list = ref::rd<void*>(self, fCmdContentList);
    void* content_view = ui::list_item(list, index);
    if (!content_view) return false;
    int slot_count = list ? unity::list_count(list) : 0;

    void* command = ref::rd<void*>(content_view, fCmdViewCommand);
    if (!command) {
        if (!is_new(kCtxCommand, index)) return false;
        ffpr::say(ui::menu_position(L"Empty slot", index, slot_count));   // C#: hardcoded English
        return true;
    }
    std::wstring name = mes(mCommandMesIdName, command);
    if (blank(name)) return false;
    if (!is_new(kCtxCommand, index)) return false;
    ffpr::say(ui::menu_position(name, index, slot_count));
    return true;
}

// --- AbilityChangeController.SelectContent (ability list) --------------------------------------
bool announce_equip_list(void* self, int index)
{
    if (!self) return false;
    g_change_window = self;
    void* view = ref::rd<void*>(self, fChangeView);
    if (!view) return false;
    void* scroll = ref::rd<void*>(view, fChangeViewScrollView);
    if (!scroll) return false;

    std::vector<void*> contents = unity::components_in_children(scroll, kAbilityChangeContent);
    void* data = nullptr;
    for (void* c : contents) {
        void* d = c ? ref::rd<void*>(c, fContentTargetData) : nullptr;
        if (d && ref::rd<bool>(d, fEquipIsFocus)) {
            data = d;
            break;
        }
    }
    if (!data) {
        for (void* c : contents) {
            void* d = c ? ref::rd<void*>(c, fContentTargetData) : nullptr;
            if (d && ref::rd<int32_t>(d, fEquipIndex) == index) {
                data = d;
                break;
            }
        }
    }
    if (!data) {
        if (!is_new(kCtxEquipList, index)) return false;
        ffpr::say(T(L"Empty"));
        return true;
    }

    clear_slot_tracker();
    clear_ability_tracker();
    clear_item_tracker();
    menu_state::set_active(ABILITY_EQUIP_MENU, true);
    g_equip.controller = self;
    g_equip.description = message(ref::rd_str(data, fEquipDescriptionMessageId));

    std::wstring name_id = ref::rd_str(data, fEquipNameMessageId);
    std::wstring name = name_id.empty() ? std::wstring() : message(name_id);
    if (blank(name)) {
        if (!is_new(kCtxEquipList, index)) return false;
        ffpr::say(T(L"Empty"));
        return true;
    }
    std::wstring announcement = name;
    if (ref::rd<bool>(data, fEquipIsEquiped)) announcement += L", equipped";   // C#: hardcoded English
    int mp = use_value(ref::rd<void*>(data, fEquipAbility));
    if (mp > 0) announcement += L", MP " + text::num(mp);

    if (!is_new(kCtxEquipList, index)) return false;
    ffpr::say(announcement);
    if (auto_detail()) speak_details(stored_details(equip_validate(), g_equip.description, false), false);
    return true;
}

// --- AbilityChangeController.SelectCommand (command slots) -------------------------------------
bool announce_equip_command(void* self, int index)
{
    if (!self) return false;
    g_change_window = self;
    void* view = ref::rd<void*>(self, fChangeView);
    if (!view) return false;

    clear_equip_tracker();
    clear_ability_tracker();
    clear_item_tracker();

    std::wstring slot_content = L"empty";   // C#: hardcoded English
    std::wstring slot_description;
    hooks::guarded("FF5.AbilityChange.SlotRead", [&] {
        void* list = ref::rd<void*>(view, fChangeViewCurrentList);
        void* equiped = ui::list_item(list, index);
        void* eview = equiped ? ref::rd<void*>(equiped, fEquipedView) : nullptr;
        void* content = eview ? ref::rd<void*>(eview, fEquipedViewContent) : nullptr;
        void* data = content ? ref::rd<void*>(content, fContentTargetData) : nullptr;
        if (!data) return;
        std::wstring name = message(ref::rd_str(data, fEquipNameMessageId));
        if (!blank(name)) slot_content = name;
        slot_description = message(ref::rd_str(data, fEquipDescriptionMessageId));
    });

    menu_state::set_active(ABILITY_SLOT_MENU, true);
    g_slot.controller = self;
    g_slot.description = slot_description;

    std::wstring announcement = L"Slot " + text::num(index + 1) + L": " + slot_content;   // C#: hardcoded
    if (!is_new(kCtxEquipCommand, index)) return false;
    ffpr::say(announcement);
    if (auto_detail()) speak_details(stored_details(slot_validate(), g_slot.description, false), false);
    return true;
}

// --- AbilityUseContentListController.SelectContent (targets) -----------------------------------
bool announce_use_target(void* self, int index)
{
    if (!self) return false;
    void* list = ref::rd<void*>(self, fUseContentList);
    void* cursor = ref::rd<void*>(self, fUseSelectCursor);
    void* content = displayed::cursor_row(cursor, list);
    if (!content) content = unity::get_component_in_parent(cursor, kItemTargetContent);
    void* data = content ? ref::rd<void*>(content, fTargetCurrentData) : nullptr;
    if (!data) return false;
    std::wstring announcement = displayed::target(content);
    if (announcement.empty()) return false;
    const int count = displayed::target_count(list);
    if (index < 0 || index >= count) return false;
    announcement = ui::menu_position(announcement, index, count);
    ffpr::say(announcement);
    return true;
}

// --- AbilityContentListController.SetCursor (spell list) ---------------------------------------
bool announce_spell(void* self, int index)
{
    if (!self) return false;
    void* list = ref::rd<void*>(self, fListContentList);
    int spell_count = list ? unity::list_count(list) : 0;
    void* controller = ui::list_item(list, index);
    void* ability = controller ? ref::rd<void*>(controller, fAbilityInfoData) : nullptr;

    if (!ability) {
        if (!is_new(kCtxSpell, index)) return false;
        ffpr::say(ui::menu_position(T(L"Empty"), index, spell_count));
        return true;
    }
    std::wstring name = mes(mOwnedAbilityMesIdName, ability);
    if (blank(name)) return false;
    name = strip_tags(name);

    std::wstring announcement = name;
    int mp = use_value(ref::call<void*>(mOwnedAbilityAbility, ability));
    if (mp > 0) announcement += L", MP " + text::num(mp);
    void* target = ref::call<void*>(mListGetTargetData, self);
    if (target && mCanUseMenuAbility && !ref::call_static<bool>(mCanUseMenuAbility, target, ability))
        announcement += L", Not learned";   // C#: hardcoded English
    announcement = ui::menu_position(announcement, index, spell_count);

    if (!is_new(kCtxSpell, index)) return false;

    clear_slot_tracker();
    clear_equip_tracker();
    clear_item_tracker();
    menu_state::set_active(ABILITY_MENU, true);
    g_ability.controller = self;
    g_ability.description = mes(mOwnedAbilityMesIdDescription, ability);

    ffpr::say(announcement);
    if (auto_detail()) speak_details(stored_details(ability_validate(), g_ability.description, false), false);
    return true;
}

// --- FieldJobAbilityReannouncePatches ----------------------------------------------------------
void clear_ability_window_guards()
{
    dedup::reset(kCtxCommand);
    dedup::reset(kCtxSpell);
    dedup::reset(kCtxUseTarget);
}
void clear_ability_equip_guards()
{
    dedup::reset(kCtxEquipCommand);
    dedup::reset(kCtxEquipList);
}

bool usable(void* controller) { return focus::alive(controller) && focus::menu_open(); }

bool announce_use_target_now(void* use_controller)
{
    void* cursor = ref::rd<void*>(use_controller, fUseSelectCursor);
    if (!cursor) return false;
    return announce_use_target(use_controller, cursor_index(cursor));
}

/// Re-find a window from a hook pointer when the deferred read runs.
bool still(void* obj, ref::Class& k) { return obj && unity::alive(obj) && k.is(obj); }

// --- the I key (via the core cascade's job-selection entry) -----------------------------------
bool details_active() { return job_validate() || slot_validate() || equip_validate() || ability_validate(); }

std::wstring details_text()
{
    // HandleItemInfoKey's FF5 order: job, ability slot, ability equip, ability.
    if (job_validate()) return job_details_text(true);
    if (slot_validate()) return stored_details(true, g_slot.description, true);
    if (equip_validate()) return stored_details(true, g_equip.description, true);
    if (ability_validate()) return stored_details(true, g_ability.description, true);
    return {};
}

/// The generic cursor reader stays silent while one of these screens owns the cursor (FF5's
/// CursorExclusionHelper "job", "ability_command", "ability_change" and the ability window's lists).
bool owns_cursor()
{
    return controller_active(g_job_window) || controller_active(g_ability_window) ||
           controller_active(g_change_window);
}

// --- detours -----------------------------------------------------------------------------------
using V_t = void (*)(void* self, const void* mi);
using VB_t = void (*)(void* self, bool b, const void* mi);
using VI_t = void (*)(void* self, int32_t i, const void* mi);
using VII_t = void (*)(void* self, int32_t i, int32_t j, const void* mi);
using VPP_t = void (*)(void* self, void* a, void* b, const void* mi);
using SetCursor_t = void (*)(void* self, void* cursor, bool is_scroll, int32_t type, bool page_skip, const void* mi);

VII_t o_JobSelectContent;
void d_JobSelectContent(void* self, int32_t index, int32_t scroll_type, const void* mi)
{
    o_JobSelectContent(self, index, scroll_type, mi);
    hooks::guarded("FF5.JobChange.SelectContent", [&] { announce_job(self, index); });
}

VB_t o_JobSetActive;
void d_JobSetActive(void* self, bool active, const void* mi)
{
    o_JobSetActive(self, active, mi);
    hooks::guarded("FF5.JobChange.SetActive", [&] {
        if (active) g_job_window = self;
        else dedup::reset(kCtxJob);
    });
}

V_t o_JobConfirmPopupInit;
void d_JobConfirmPopupInit(void* self, const void* mi)
{
    o_JobConfirmPopupInit(self, mi);
    dedup::reset(kCtxJob);
}

V_t o_JobFixJobChangeInit;
void d_JobFixJobChangeInit(void* self, const void* mi)
{
    o_JobFixJobChangeInit(self, mi);
    dedup::reset(kCtxJob);
}

VI_t o_CmdSelectContent;
void d_CmdSelectContent(void* self, int32_t index, const void* mi)
{
    o_CmdSelectContent(self, index, mi);
    hooks::guarded("FF5.AbilityCommand.SelectContent", [&] { announce_command(self, index); });
}

VII_t o_ChangeSelectContent;
void d_ChangeSelectContent(void* self, int32_t index, int32_t scroll_type, const void* mi)
{
    o_ChangeSelectContent(self, index, scroll_type, mi);
    hooks::guarded("FF5.AbilityChange.SelectContent", [&] {
        g_cached_list_index = index;
        announce_equip_list(self, index);
    });
}

VI_t o_ChangeSelectCommand;
void d_ChangeSelectCommand(void* self, int32_t index, const void* mi)
{
    o_ChangeSelectCommand(self, index, mi);
    hooks::guarded("FF5.AbilityChange.SelectCommand", [&] {
        g_cached_command_index = index;
        announce_equip_command(self, index);
    });
}

VB_t o_ChangeSetActive;
void d_ChangeSetActive(void* self, bool active, const void* mi)
{
    o_ChangeSetActive(self, active, mi);
    hooks::guarded("FF5.AbilityChange.SetActive", [&] {
        if (active) g_change_window = self;
        else clear_ability_equip_guards();
    });
}

V_t o_ChangeSelectCommandInit;
void d_ChangeSelectCommandInit(void* self, const void* mi)
{
    o_ChangeSelectCommandInit(self, mi);
    hooks::guarded("FF5.AbilityChange.SelectCommandInit", [&] {
        if (!kAbilityChange.is(self)) return;
        g_change_window = self;
        dedup::reset(kCtxEquipList);   // sibling only
        focus::request("AbilityEquipCommand", [self] {
            if (!still(self, kAbilityChange) || !usable(self)) return false;
            return announce_equip_command(self, g_cached_command_index);
        });
    });
}

V_t o_ChangeSelectListInit;
void d_ChangeSelectListInit(void* self, const void* mi)
{
    o_ChangeSelectListInit(self, mi);
    hooks::guarded("FF5.AbilityChange.SelectListInit", [&] {
        if (!kAbilityChange.is(self)) return;
        g_change_window = self;
        dedup::reset(kCtxEquipCommand);   // sibling only
        focus::request("AbilityEquipList", [self] {
            if (!still(self, kAbilityChange) || !usable(self)) return false;
            return announce_equip_list(self, g_cached_list_index);
        });
    });
}

V_t o_ChangeChangedInit;
void d_ChangeChangedInit(void* self, const void* mi)
{
    o_ChangeChangedInit(self, mi);
    clear_ability_equip_guards();
}

V_t o_ChangeConfirmPopupInit;
void d_ChangeConfirmPopupInit(void* self, const void* mi)
{
    o_ChangeConfirmPopupInit(self, mi);
    clear_ability_equip_guards();
}

VPP_t o_UseSelectContent;
void d_UseSelectContent(void* self, void* contents, void* cursor, const void* mi)
{
    o_UseSelectContent(self, contents, cursor, mi);
    hooks::guarded("FF5.AbilityUse.SelectContent", [&] {
        if (!cursor || !kAbilityUse.is(self)) return;   // shared body (SelectContent / SetCursor)
        if (announce_use_target(self, cursor_index(cursor))) ++g_use_target_spoken_seq;
    });
}

void use_list_init(void* self)
{
    if (!kAbilityUse.is(self)) return;
    dedup::reset(kCtxUseTarget);
    focus::request("AbilityTarget", [self] {
        if (!still(self, kAbilityUse) || !focus::alive(self)) return false;
        if (queries::is("BattleStateHelper.IsInBattle")) return false;   // battle has its own reader
        if (!focus::menu_open()) return false;
        return announce_use_target_now(self);
    });
}

V_t o_UseSingleInit;
void d_UseSingleInit(void* self, const void* mi)
{
    o_UseSingleInit(self, mi);
    hooks::guarded("FF5.AbilityUse.SingleInit", [&] { use_list_init(self); });
}

V_t o_UseAllInit;
void d_UseAllInit(void* self, const void* mi)
{
    o_UseAllInit(self, mi);
    hooks::guarded("FF5.AbilityUse.AllInit", [&] { use_list_init(self); });
}

SetCursor_t o_ListSetCursor;
void d_ListSetCursor(void* self, void* cursor, bool is_scroll, int32_t type, bool page_skip, const void* mi)
{
    o_ListSetCursor(self, cursor, is_scroll, type, page_skip, mi);
    hooks::guarded("FF5.AbilityList.SetCursor", [&] {
        if (!cursor) return;
        announce_spell(self, cursor_index(cursor));
    });
}

VB_t o_WinSetActive;
void d_WinSetActive(void* self, bool active, const void* mi)
{
    o_WinSetActive(self, active, mi);
    hooks::guarded("FF5.AbilityWindow.SetActive", [&] {
        if (active) g_ability_window = self;
        else clear_ability_window_guards();
    });
}

V_t o_WinNonInit;
void d_WinNonInit(void* self, const void* mi)
{
    o_WinNonInit(self, mi);
    hooks::guarded("FF5.AbilityWindow.NonInit", [&] {
        focus::cancel();
        clear_ability_window_guards();
        clear_ability_equip_guards();
    });
}

V_t o_WinCommandInit;
void d_WinCommandInit(void* self, const void* mi)
{
    o_WinCommandInit(self, mi);
    hooks::guarded("FF5.AbilityWindow.CommandInit", [&] {
        if (!kAbilityWin.is(self)) return;
        g_ability_window = self;
        // Siblings only: the command bar announces its own entry through SelectContent.
        dedup::reset(kCtxSpell);
        dedup::reset(kCtxUseTarget);
    });
}

V_t o_WinUseListInit;
void d_WinUseListInit(void* self, const void* mi)
{
    o_WinUseListInit(self, mi);
    hooks::guarded("FF5.AbilityWindow.UseListInit", [&] {
        if (!kAbilityWin.is(self)) return;
        g_ability_window = self;
        dedup::reset(kCtxCommand);
        dedup::reset(kCtxUseTarget);
        focus::request("SpellList", [self] {
            if (!still(self, kAbilityWin) || !usable(self)) return false;
            void* list = ref::rd<void*>(self, fWinListController);
            void* cursor = list ? ref::rd<void*>(list, fListSelectCursor) : nullptr;
            if (!cursor) return false;
            return announce_spell(list, cursor_index(cursor));
        });
    });
}

V_t o_WinUseTargetInit;
void d_WinUseTargetInit(void* self, const void* mi)
{
    const int spoken_before = g_use_target_spoken_seq;
    o_WinUseTargetInit(self, mi);
    hooks::guarded("FF5.AbilityWindow.UseTargetInit", [&] {
        if (!kAbilityWin.is(self)) return;
        g_ability_window = self;
        dedup::reset(kCtxCommand);
        dedup::reset(kCtxSpell);
        if (g_use_target_spoken_seq != spoken_before) return;   // the game's own SelectContent spoke it
        dedup::reset(kCtxUseTarget);   // self-clear kept: Single/All share this guard
        focus::request("AbilityTarget", [self] {
            if (!still(self, kAbilityWin) || !usable(self)) return false;
            void* use = ref::rd<void*>(self, fWinUseController);
            return use && announce_use_target_now(use);
        });
    });
}

using Show_t = void (*)(void* self, bool is_save_point, const void* mi);
Show_t o_MainMenuShow;
void d_MainMenuShow(void* self, bool is_save_point, const void* mi)
{
    o_MainMenuShow(self, is_save_point, mi);
    // MainMenuController_Show_Patch: JobAbilityTrackerHelper.ClearAllTrackers (the core's own
    // Show hook clears the item, equipment, save/load and config states).
    hooks::guarded("FF5.MainMenu.Show", [] { clear_all_trackers(); });
}

int install()
{
    int n = 0;
    n += hooks::attach(mJobSelectContent, &d_JobSelectContent, &o_JobSelectContent) ? 1 : 0;
    n += hooks::attach(mJobSetActive, &d_JobSetActive, &o_JobSetActive) ? 1 : 0;
    n += hooks::attach(mJobConfirmPopupInit, &d_JobConfirmPopupInit, &o_JobConfirmPopupInit) ? 1 : 0;
    n += hooks::attach(mJobFixJobChangeInit, &d_JobFixJobChangeInit, &o_JobFixJobChangeInit) ? 1 : 0;
    n += hooks::attach(mCmdSelectContent, &d_CmdSelectContent, &o_CmdSelectContent) ? 1 : 0;
    n += hooks::attach(mChangeSelectContent, &d_ChangeSelectContent, &o_ChangeSelectContent) ? 1 : 0;
    n += hooks::attach(mChangeSelectCommand, &d_ChangeSelectCommand, &o_ChangeSelectCommand) ? 1 : 0;
    n += hooks::attach(mChangeSetActive, &d_ChangeSetActive, &o_ChangeSetActive) ? 1 : 0;
    n += hooks::attach(mChangeSelectCommandInit, &d_ChangeSelectCommandInit, &o_ChangeSelectCommandInit) ? 1 : 0;
    n += hooks::attach(mChangeSelectListInit, &d_ChangeSelectListInit, &o_ChangeSelectListInit) ? 1 : 0;
    n += hooks::attach(mChangeChangedInit, &d_ChangeChangedInit, &o_ChangeChangedInit) ? 1 : 0;
    n += hooks::attach(mChangeConfirmPopupInit, &d_ChangeConfirmPopupInit, &o_ChangeConfirmPopupInit) ? 1 : 0;
    n += hooks::attach(mUseSelectContent, &d_UseSelectContent, &o_UseSelectContent) ? 1 : 0;
    n += hooks::attach(mUseSingleInit, &d_UseSingleInit, &o_UseSingleInit) ? 1 : 0;
    n += hooks::attach(mUseAllInit, &d_UseAllInit, &o_UseAllInit) ? 1 : 0;
    n += hooks::attach(mListSetCursor, &d_ListSetCursor, &o_ListSetCursor) ? 1 : 0;
    n += hooks::attach(mWinSetActive, &d_WinSetActive, &o_WinSetActive) ? 1 : 0;
    n += hooks::attach(mWinNonInit, &d_WinNonInit, &o_WinNonInit) ? 1 : 0;
    n += hooks::attach(mWinCommandInit, &d_WinCommandInit, &o_WinCommandInit) ? 1 : 0;
    n += hooks::attach(mWinUseListInit, &d_WinUseListInit, &o_WinUseListInit) ? 1 : 0;
    n += hooks::attach(mWinUseTargetInit, &d_WinUseTargetInit, &o_WinUseTargetInit) ? 1 : 0;
    n += hooks::attach(mMainMenuShow, &d_MainMenuShow, &o_MainMenuShow) ? 1 : 0;

    // The core details key (I / right stick up): FF5's job and ability announcers.
    queries::define("JobSelectionPatches.IsActive", [] { return details_active(); });
    queries::define_text("JobSelectionPatches.GetCurrentDescription", [] {
        return hooks::guarded_r("FF5.DetailsKey", std::wstring(), [] { return details_text(); });
    });

    queries::define("JobMenuTracker.ValidateState", [] { return job_validate(); });
    queries::define("AbilityMenuTracker.ValidateState", [] { return ability_validate(); });
    queries::define("AbilitySlotMenuTracker.ValidateState", [] { return slot_validate(); });
    queries::define("AbilityEquipMenuTracker.ValidateState", [] { return equip_validate(); });
    queries::define("FF5JobAbility.OwnsCursor", [] { return owns_cursor(); });
    cursor::add_suppressor("FF5JobAbility.OwnsCursor");

    actions::define("FF5JobAbility.ClearAllTrackers", [] { clear_all_trackers(); });
    actions::define("JobDetailsAnnouncer.AnnounceCurrentJobDetails", [] { announce_job_details(true, true); });
    return n;
}

core::Install s_install{"FF5.JobAbility", &install, 500, game::kFF5};

} // namespace
} // namespace ffpr::ff5
