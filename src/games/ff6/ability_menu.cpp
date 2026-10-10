// Ports FF6 Patches/AbilityMenuPatches.cs, the field Ability menu (all but the esper patches, which
// are in ability_espers.cpp): AbilityCommandController_SelectContent_Patch,
// AbilityContentListController_SelectContent_Patch, SpecialAbilityContentListController_SelectContent
// _AbilityMenu_Patch (with TryAnnounceBlitzSequence), AbilityChangeController_SelectContent_Patch,
// AbilityChangeController_SelectCommand_Patch and AbilityUseContentListController_SelectContent_Patch;
// the Blitz and esper branches of Core/InputManager.cs HandleItemInfoKey; and the ability-menu
// clause of Patches/CursorNavigationPatches.cs (the generic cursor reader stays silent here).
// Ported from FF6ScreenReader (BlindGuyNW/FF6ScreenReader, by ZKline), with its author's permission.
//
// Screens: the command list (Magic, Espers, Blitz, Lore, Rage, Dance, Bushido ...), the ability
// list (spells and Lores, with MP and learning progress), the special list (Blitz, Tools, Rage,
// Dance ...), the target list, and Gogo's command change (status screen). The core's magic list is
// FF1-only (src/core/menus/magic_menu_ff1.cpp), so these are FF6's own readers.
//
// Mod-side behaviour is FF1's, as FF4's ability_menu.cpp and FF5's job_ability.cpp do it (lead
// decision, user rule 2026-09-25): descriptions only with AutoDetail, "(n of N)" last
// (ui::menu_position), and the I key reads the focused entry's description. FF6's own additions on
// the I key: a focused Blitz speaks its input sequence; an open esper details panel is re-read.
// These go through the core cascade's game-module entry ("GameModule.HasDetails" /
// "GameModule.Details"), valid only while the focused list is alive and active (the core's own
// I-key screens keep their details).
//
// List sizes: AbilityContentListController / SpecialAbilityContentListController keep a pool of
// 50 content controllers (FF6 CreateContentList 0x822E60), so the count is abilityList's; the
// command bar's is GetActiveObjectCount(); the target list counts its active slots.
//
// Repeat checks: each C# lastAnnouncement is a dedup:: text context (Menu scope), cleared when its
// list is (re)entered - AbilityWindowController CommandInit / UseListInit / UseTargetInit /
// SelectMagicStoneInit, AbilityChangeController Show / SelectCommandInit / SelectListInit - and all
// of them when the window closes (SetActive(false)), as FF4/FF5 scope theirs. The command bar's
// focused command is read again one frame after CommandInit (FF4's back-out read).
//
// SpecialAbilityContentListController is also used in battle, where the battle area reads it
// (its SetCursor hook): this file's SelectContent hook does nothing while BattleStateHelper.IsInBattle.
//
// Folded body: AbilityUseContentListController.SelectContent shares its RVA (FF6 0x8289A0) with the
// same class's SetCursor (identical signature), as it did under Harmony; the detour class-checks
// self.
#include <cstdint>
#include <string>
#include <vector>
#include "ability.h"
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/frame.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/settings.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/battle/battle_util.h"
#include "../../core/system/cursor_event.h"
#include "../../core/system/system.h"

namespace ffpr::ff6 {
namespace {

constexpr uint8_t k6 = game::kFF6;

// --- game names ------------------------------------------------------------------------------
ref::Class  kAbilityWindow{"Serial.FF6.UI.KeyInput", "AbilityWindowController", k6};
ref::Field  fWindowCommandController{kAbilityWindow, "commandController"};
ref::Method mWindowSetActive{kAbilityWindow, "SetActive", 1};
ref::Method mWindowCommandInit{kAbilityWindow, "CommandInit", 0};
ref::Method mWindowUseListInit{kAbilityWindow, "UseListInit", 0};
ref::Method mWindowUseTargetInit{kAbilityWindow, "UseTargetInit", 0};
ref::Method mWindowSelectMagicStoneInit{kAbilityWindow, "SelectMagicStoneInit", 0};

ref::Class  kAbilityCommand{"Serial.FF6.UI.KeyInput", "AbilityCommandController", k6};
ref::Field  fCommandContentList{kAbilityCommand, "contentList"};
ref::Field  fCommandSelectCursor{kAbilityCommand, "selectCursor"};
ref::Method mCommandSelectContent{kAbilityCommand, "SelectContent", 1};
ref::Method mCommandActiveCount{kAbilityCommand, "GetActiveObjectCount", 0};
ref::Class  kAbilityCommandView{"Serial.FF6.UI.KeyInput", "AbilityCommandContentView", k6};
ref::Field  fCommandViewText{kAbilityCommandView, "text"};

ref::Class  kAbilityList{"Serial.FF6.UI.KeyInput", "AbilityContentListController", k6};
ref::Field  fListView{kAbilityList, "view"};
ref::Field  fListContentList{kAbilityList, "contentList"};
ref::Field  fListAbilityList{kAbilityList, "abilityList"};
ref::Field  fListSelectCursor{kAbilityList, "selectCursor"};
ref::Field  fListTargetSelectCursor{kAbilityList, "targetSelectCursor"};
ref::Method mListSelectContent{kAbilityList, "SelectContent", 3};
ref::Class  kAbilityListView{"Serial.FF6.UI.KeyInput", "AbilityContentListView", k6};
ref::Field  fListViewMpValue{kAbilityListView, "mpValueText"};
ref::Class  kAbilityInfoContent{"Last.UI.KeyInput", "BattleAbilityInfomationContentController", k6};
ref::Field  fAbilityInfoData{kAbilityInfoContent, "<Data>k__BackingField"};

ref::Class  kSpecialList{"Serial.FF6.UI.KeyInput", "SpecialAbilityContentListController", k6};
ref::Field  fSpecialContentList{kSpecialList, "contentList"};
ref::Field  fSpecialAbilityList{kSpecialList, "abilityList"};
ref::Field  fSpecialSelectCursor{kSpecialList, "selectCursor"};
ref::Field  fSpecialTargetSelectCursor{kSpecialList, "targetSelectCursor"};
ref::Method mSpecialSelectContent{kSpecialList, "SelectContent", 3};
ref::Class  kSpecialInfoContent{"Last.UI.KeyInput", "BattleSpecialInfomationContentController", k6};
ref::Field  fSpecialInfoData{kSpecialInfoContent, "<Data>k__BackingField"};

ref::Class  kOwnedAbility{"Last.Data.User", "OwnedAbility", k6};
ref::Method mOwnedAbilityMesIdName{kOwnedAbility, "get_MesIdName", 0};
ref::Method mOwnedAbilityMesIdDescription{kOwnedAbility, "get_MesIdDescription", 0};
ref::Method mOwnedAbilitySkillLevel{kOwnedAbility, "get_SkillLevel", 0};

ref::Class  kAbilityUse{"Serial.FF6.UI.KeyInput", "AbilityUseContentListController", k6};
ref::Field  fUseContentList{kAbilityUse, "contentList"};
ref::Field  fUseSelectCursor{kAbilityUse, "selectCursor"};
ref::Method mUseSelectContent{kAbilityUse, "SelectContent", 2};
ref::Class  kTargetContent{"Last.UI.KeyInput", "ItemTargetSelectContentController", k6};
ref::Field  fTargetCurrentData{kTargetContent, "<CurrentData>k__BackingField"};

ref::Class  kAbilityChange{"Serial.FF6.UI.KeyInput", "AbilityChangeController", k6};
ref::Field  fChangeView{kAbilityChange, "view"};
ref::Method mChangeSelectContent{kAbilityChange, "SelectContent", 2};
ref::Method mChangeSelectCommand{kAbilityChange, "SelectCommand", 1};
ref::Method mChangeShow{kAbilityChange, "Show", 1};
ref::Method mChangeSelectCommandInit{kAbilityChange, "SelectCommandInit", 0};
ref::Method mChangeSelectListInit{kAbilityChange, "SelectListInit", 0};
ref::Class  kChangeBase{"Serial.Template.UI", "StatusDetailsCommandChangeBaseController", k6};
ref::Field  fChangeAllEquips{kChangeBase, "allEquips"};
ref::Class  kChangeKeyBase{"Serial.Template.UI.KeyInput", "StatusDetailsCommandChangeBaseController", k6};
ref::Field  fChangeCommandsCursor{kChangeKeyBase, "commandsCursor"};
ref::Field  fChangeContentsCursor{kChangeKeyBase, "contentsCursor"};
ref::Class  kAbilityChangeView{"Serial.FF6.UI.KeyInput", "AbilityChangeView", k6};
ref::Field  fChangeViewCurrentList{kAbilityChangeView, "currentList"};
ref::Class  kChangeContentBase{"Serial.Template.UI", "AbilityChangeContentBaseController", k6};
ref::Field  fChangeContentTargetData{kChangeContentBase, "targetData"};
ref::Class  kEquipData{"Serial.Template.UI", "AbilityEquipData", k6};
ref::Field  fEquipNameMessageId{kEquipData, "<NameMessageId>k__BackingField"};
ref::Field  fEquipDescriptionMessageId{kEquipData, "<DescriptionMessageId>k__BackingField"};
ref::Field  fEquipIsFixed{kEquipData, "<IsFixed>k__BackingField"};
ref::Field  fEquipIsEquiped{kEquipData, "<IsEquiped>k__BackingField"};

// --- dedup contexts (each C# patch's lastAnnouncement) -----------------------------------------
constexpr const char* kCtxCommand = "FF6.AbilityCommandController.SelectContent";
constexpr const char* kCtxList = "FF6.AbilityContentListController.SelectContent";
constexpr const char* kCtxSpecial = "FF6.SpecialAbilityContentListController.SelectContent";
constexpr const char* kCtxChangeList = "FF6.AbilityChangeController.SelectContent";
constexpr const char* kCtxChangeCommand = "FF6.AbilityChangeController.SelectCommand";
constexpr const char* kCtxUseTarget = "FF6.AbilityUseContentListController.SelectContent";
constexpr dedup::Scope kScope = dedup::Scope::Menu;

// --- state -------------------------------------------------------------------------------------
// The controllers these hooks last read: identities for the cursor suppressor, re-checked with
// unity::alive before any read.
void* g_command = nullptr;
void* g_list = nullptr;
void* g_special = nullptr;
void* g_use = nullptr;
void* g_change = nullptr;

// The I key's focused entry (FF4 LastDescription / FF5's trackers).
struct Focus {
    void* controller = nullptr;
    std::wstring description;
    std::wstring blitz_mes_id;   // SpecialAbilityContentListController ... CurrentMesIdName
} g_focus;

std::wstring strip(const std::wstring& s) { return battle::strip_icon(s); }
bool blank(const std::wstring& s) { return text::is_blank(s); }
std::wstring message(const std::wstring& mes_id) { return battle::message(mes_id); }
bool in_battle() { return queries::is("BattleStateHelper.IsInBattle"); }
bool alive_active(void* c) { return c && unity::alive(c) && unity::active_in_hierarchy(c); }

/// The localized, stripped description for a message id ("" when none).
std::wstring description_of(const std::wstring& mes_id)
{
    return blank(mes_id) ? std::wstring() : strip(message(mes_id));
}

/// abilityList's size (the shown entries; contentList is a pool of 50).
int ability_count(void* controller, ref::Field& ability_list, ref::Field& content_list)
{
    void* abilities = ref::rd<void*>(controller, ability_list);
    int count = abilities ? unity::list_count(abilities) : 0;
    if (count <= 0) {
        void* contents = ref::rd<void*>(controller, content_list);
        count = contents ? unity::list_count(contents) : 0;
    }
    return count;
}

void reset_window_guards()
{
    dedup::reset(kCtxCommand);
    dedup::reset(kCtxList);
    dedup::reset(kCtxSpecial);
    dedup::reset(kCtxUseTarget);
    reset_esper_list_guard();
}

// --- AbilityCommandController.SelectContent ---------------------------------------------------
void announce_command(void* self, int index)
{
    if (!self || !kAbilityCommand.is(self)) return;
    // The UI pre-load initialises the window on the field (core game_state.cpp, IsFieldState).
    if (ffpr::system::is_field_state()) return;
    g_command = self;
    void* list = ref::rd<void*>(self, fCommandContentList);
    void* content_view = ui::list_item(list, index);
    if (!content_view) return;
    void* text_component = ref::rd<void*>(content_view, fCommandViewText);
    if (!text_component) return;

    std::wstring name = unity::ui_text(text_component);
    if (blank(name)) return;
    name = strip(name);
    if (blank(name)) return;

    // The command bar has no entry focused for the details key.
    set_ability_focus(self, std::wstring());
    if (!dedup::text(kCtxCommand, name, kScope)) return;

    int count = ref::call<int32_t>(mCommandActiveCount, self);
    if (count <= 0) count = unity::list_count(list);
    ffpr::say(ui::menu_position(name, index, count));
}

// --- AbilityContentListController.SelectContent(Cursor, ...) ----------------------------------
void announce_ability(void* self, void* cursor)
{
    if (!self || !cursor || !kAbilityList.is(self)) return;
    g_list = self;
    void* list = ref::rd<void*>(self, fListContentList);
    const int index = ui::cursor_index(cursor);
    void* content = ui::list_item(list, index);
    if (!content) return;
    void* ability = ref::rd<void*>(content, fAbilityInfoData);
    if (!ability) return;

    const std::wstring mes_id_name = ref::call_str(mOwnedAbilityMesIdName, ability);
    if (blank(mes_id_name)) return;
    std::wstring name = message(mes_id_name);
    if (blank(name)) return;
    name = strip(name);
    if (blank(name)) return;
    std::wstring announcement = name;

    // MP cost from the list's view (SelectContent has just set it: FF6 0x823AE0 -> SetUseValue).
    if (void* view = ref::rd<void*>(self, fListView)) {
        if (void* mp_text = ref::rd<void*>(view, fListViewMpValue)) {
            const std::wstring raw = unity::ui_text(mp_text);
            if (!blank(raw)) {
                const std::wstring mp = text::trim(raw);
                if (mp != L"0" && mp != L"-") announcement += TF(L", MP {0}", {mp});
            }
        }
    }

    // Learning progress (SkillLevel 100 = learned, 0 = not started).
    const int skill_level = ref::call<int32_t>(mOwnedAbilitySkillLevel, ability);
    if (skill_level < 100 && skill_level > 0)
        announcement += TF(L", learning, {0} percent", {text::num(skill_level)});

    const std::wstring description = description_of(ref::call_str(mOwnedAbilityMesIdDescription, ability));
    set_ability_focus(self, description);
    if (auto_detail() && !blank(description)) announcement += L". " + description;

    if (!dedup::text(kCtxList, announcement, kScope)) return;
    ffpr::say(ui::menu_position(announcement, index, ability_count(self, fListAbilityList, fListContentList)));
}

// --- SpecialAbilityContentListController.SelectContent(Cursor, ...) (field menu only) ---------
void announce_special(void* self, void* cursor)
{
    if (!self || !cursor || !kSpecialList.is(self)) return;
    if (in_battle()) return;   // battle reads this list itself
    g_special = self;
    void* list = ref::rd<void*>(self, fSpecialContentList);
    const int index = ui::cursor_index(cursor);
    void* content = ui::list_item(list, index);
    if (!content) return;
    void* ability = ref::rd<void*>(content, fSpecialInfoData);
    if (!ability) return;

    const std::wstring mes_id_name = ref::call_str(mOwnedAbilityMesIdName, ability);
    if (blank(mes_id_name)) return;
    std::wstring name = message(mes_id_name);
    if (blank(name)) return;
    name = strip(name);
    if (blank(name)) return;
    std::wstring announcement = name;

    std::wstring description = description_of(ref::call_str(mOwnedAbilityMesIdDescription, ability));
    // The game's descriptions start with "Name: ...": drop the repeated name.
    const std::wstring prefix = name + L":";
    if (!blank(description) && text::starts_with(description, prefix))
        description = text::trim(description.substr(prefix.size()));

    // The focused special ability; a Blitz's MesIdName gives the I key its input sequence.
    set_ability_focus(self, description, mes_id_name);
    if (auto_detail() && !blank(description)) announcement += L". " + description;

    if (!dedup::text(kCtxSpecial, announcement, kScope)) return;
    ffpr::say(ui::menu_position(announcement, index, ability_count(self, fSpecialAbilityList, fSpecialContentList)));
}

// --- AbilityChangeController.SelectContent (Gogo: the full command list) --------------------------
void announce_change_list(void* self, int index)
{
    if (!self || !kAbilityChange.is(self)) return;
    g_change = self;
    // allEquips backs the scroll view (view.CurrentList holds only the 4 slots).
    void* all = ref::rd<void*>(self, fChangeAllEquips);
    const int count = all ? unity::array_length(all) : 0;
    if (index < 0 || index >= count) return;
    void* data = unity::array_at(all, index);
    if (!data) return;

    const std::wstring name_id = ref::rd_str(data, fEquipNameMessageId);
    if (blank(name_id)) return;
    const std::wstring name = strip(message(name_id));
    if (blank(name)) return;

    std::wstring announcement = name;
    if (ref::rd<bool>(data, fEquipIsEquiped)) announcement += T(L", equipped");

    const std::wstring description = description_of(ref::rd_str(data, fEquipDescriptionMessageId));
    set_ability_focus(self, description);
    if (auto_detail() && !blank(description)) announcement += L". " + description;

    if (!dedup::text(kCtxChangeList, announcement, kScope)) return;
    ffpr::say(ui::menu_position(announcement, index, count));
}

// --- AbilityChangeController.SelectCommand (Gogo: the 4 command slots) ----------------------------
void announce_change_command(void* self, int index)
{
    if (!self || !kAbilityChange.is(self)) return;
    g_change = self;
    void* view = ref::rd<void*>(self, fChangeView);
    if (!view) return;
    void* list = ref::rd<void*>(view, fChangeViewCurrentList);
    const int count = list ? unity::list_count(list) : 0;
    if (count <= 0 || index < 0 || index >= count) return;

    const std::wstring slot = text::num(index + 1);
    void* content = unity::list_at(list, index);
    void* data = content ? ref::rd<void*>(content, fChangeContentTargetData) : nullptr;
    const std::wstring name_id = data ? ref::rd_str(data, fEquipNameMessageId) : std::wstring();

    std::wstring announcement;
    std::wstring description;
    const std::wstring name = (!data || blank(name_id)) ? std::wstring() : strip(message(name_id));
    if (blank(name)) {
        announcement = TF(L"Slot {0}: Empty", {slot});
    } else {
        announcement = TF(L"Slot {0}: {1}", {slot, name});
        if (ref::rd<bool>(data, fEquipIsFixed)) announcement += T(L", fixed");
        description = description_of(ref::rd_str(data, fEquipDescriptionMessageId));
        if (auto_detail() && !blank(description)) announcement += L". " + description;
    }
    set_ability_focus(self, description);

    if (!dedup::text(kCtxChangeCommand, announcement, kScope)) return;
    ffpr::say(ui::menu_position(announcement, index, count));
}

// --- AbilityUseContentListController.SelectContent (targets) ------------------------------------
void announce_use_target(void* self, void* cursor)
{
    if (!self || !cursor || !kAbilityUse.is(self)) return;   // shared body: SelectContent / SetCursor
    g_use = self;
    void* list = ref::rd<void*>(self, fUseContentList);
    const int slots = list ? unity::list_count(list) : 0;
    const int index = ui::cursor_index(cursor);
    if (slots <= 0 || index < 0 || index >= slots) return;
    void* content = unity::list_at(list, index);
    void* character = content ? ref::rd<void*>(content, fTargetCurrentData) : nullptr;
    if (!character) return;

    const std::wstring name = battle::character_name(character);
    if (name.empty()) return;
    std::wstring announcement = name;

    // HP / MP and status conditions; on a failed read the name alone (the C# catch).
    if (void* param = battle::character_parameter(character)) {
        int max_hp = 0, max_mp = 0;
        if (battle::param_confirmed(param, battle::Stat::MaxHp, max_hp) &&
            battle::param_confirmed(param, battle::Stat::MaxMp, max_mp)) {
            announcement += TF(L", HP {0}/{1}, MP {2}/{3}",
                               {text::num(battle::param_current_hp(param)), text::num(max_hp),
                                text::num(battle::param_current_mp(param)), text::num(max_mp)});

            std::wstring statuses;
            for (void* condition : battle::confirmed_conditions(param)) {
                if (!condition) continue;
                const std::wstring mes_id = battle::condition_mes_id(condition);
                // No message id: an internal / hidden status.
                if (mes_id.empty() || mes_id == L"None") continue;
                const std::wstring localized = message(mes_id);
                if (localized.empty()) continue;
                if (!statuses.empty()) statuses += L", ";
                statuses += localized;
            }
            if (!statuses.empty()) announcement += L", " + statuses;
        }
    }

    // The I key keeps reading the chosen ability's description (FF4), from this list now.
    g_focus.controller = self;
    g_focus.blitz_mes_id.clear();

    if (!dedup::text(kCtxUseTarget, announcement, kScope)) return;

    // The slots shown (empty party slots are inactive).
    int count = 0;
    for (int i = 0; i < slots; ++i)
        if (void* c = unity::list_at(list, i); c && unity::active_in_hierarchy(c)) ++count;
    if (count <= index) count = slots;
    ffpr::say(ui::menu_position(announcement, index, count));
}

// --- the I key ------------------------------------------------------------------------------------
bool focus_valid() { return alive_active(g_focus.controller); }

/// TryAnnounceBlitzSequence, as text: the focused special ability is a Blitz.
std::wstring blitz_text()
{
    if (g_focus.blitz_mes_id.empty() || !focus_valid()) return {};
    std::wstring sequence;
    if (!try_get_blitz_sequence(g_focus.blitz_mes_id, sequence)) return {};
    return sequence;
}

bool has_details()
{
    return hooks::guarded_r("FF6.AbilityMenu.HasDetails", false, [] {
        if (in_battle()) return false;
        return focus_valid() || !esper_details_reannounce_text().empty();
    });
}

/// Blitz input (a Blitz focused), then the esper details re-read, then the focused description
/// ("" = the core says "No description").
std::wstring details_text()
{
    return hooks::guarded_r("FF6.AbilityMenu.Details", std::wstring(), []() -> std::wstring {
        if (in_battle()) return {};
        std::wstring s = blitz_text();
        if (!s.empty()) {
            LOGW(L"[Blitz Input] ", s.c_str());
            return s;
        }
        s = esper_details_reannounce_text();
        if (!s.empty()) return s;
        return focus_valid() ? g_focus.description : std::wstring();
    });
}

// --- the generic cursor reader's suppressor ----------------------------------------------------
/// CursorNavigationPatches' ability-menu clause: a parent of the cursor named like the ability or
/// esper screens ("ability", "magicstone", "magic_stone", "esper", any case).
bool ability_menu_path(void* cursor_transform)
{
    if (!cursor_transform) return false;
    void* p = unity::parent(cursor_transform);
    for (int depth = 0; p && depth < 64; ++depth, p = unity::parent(p)) {
        const std::wstring name = text::lower(unity::name(p));
        if (text::contains(name, L"ability") || text::contains(name, L"magicstone") ||
            text::contains(name, L"magic_stone") || text::contains(name, L"esper"))
            return true;
    }
    return false;
}

bool cursor_of(void* controller, ref::Field& field, void* cursor)
{
    return controller && unity::alive(controller) && ref::rd<void*>(controller, field) == cursor;
}

/// One of the cursors these screens read (identity), whatever their object names.
bool tracked_cursor(void* cursor)
{
    return cursor_of(g_command, fCommandSelectCursor, cursor) ||
           cursor_of(g_list, fListSelectCursor, cursor) || cursor_of(g_list, fListTargetSelectCursor, cursor) ||
           cursor_of(g_special, fSpecialSelectCursor, cursor) ||
           cursor_of(g_special, fSpecialTargetSelectCursor, cursor) ||
           cursor_of(g_use, fUseSelectCursor, cursor) ||
           cursor_of(g_change, fChangeCommandsCursor, cursor) || cursor_of(g_change, fChangeContentsCursor, cursor) ||
           is_esper_cursor(cursor);
}

bool owns_cursor()
{
    return hooks::guarded_r("FF6.AbilityMenu.OwnsCursor", false, [] {
        const cursor::CursorEvent& e = cursor::current();
        if (!e.cursor) return false;
        return ability_menu_path(e.transform) || tracked_cursor(e.cursor);
    });
}

// --- detours ---------------------------------------------------------------------------------------
using Void_t = void (*)(void* self, const void* mi);
using Bool_t = void (*)(void* self, bool value, const void* mi);
using IndexSelect_t = void (*)(void* self, int32_t index, const void* mi);
using CursorSelect_t = void (*)(void* self, void* cursor, int32_t type, bool page_skip, const void* mi);
using UseSelect_t = void (*)(void* self, void* target_contents, void* cursor, const void* mi);
using ChangeSelect_t = void (*)(void* self, int32_t index, int32_t scroll_type, const void* mi);

IndexSelect_t o_CommandSelectContent;
void d_CommandSelectContent(void* self, int32_t index, const void* mi)
{
    o_CommandSelectContent(self, index, mi);
    hooks::guarded("FF6.AbilityCommand.SelectContent", [&] { announce_command(self, index); });
}

CursorSelect_t o_ListSelectContent;
void d_ListSelectContent(void* self, void* cursor, int32_t type, bool page_skip, const void* mi)
{
    o_ListSelectContent(self, cursor, type, page_skip, mi);
    hooks::guarded("FF6.AbilityList.SelectContent", [&] { announce_ability(self, cursor); });
}

CursorSelect_t o_SpecialSelectContent;
void d_SpecialSelectContent(void* self, void* cursor, int32_t type, bool page_skip, const void* mi)
{
    o_SpecialSelectContent(self, cursor, type, page_skip, mi);
    hooks::guarded("FF6.SpecialAbilityList.SelectContent", [&] { announce_special(self, cursor); });
}

UseSelect_t o_UseSelectContent;
void d_UseSelectContent(void* self, void* target_contents, void* cursor, const void* mi)
{
    o_UseSelectContent(self, target_contents, cursor, mi);
    hooks::guarded("FF6.AbilityUse.SelectContent", [&] { announce_use_target(self, cursor); });
}

ChangeSelect_t o_ChangeSelectContent;
void d_ChangeSelectContent(void* self, int32_t index, int32_t scroll_type, const void* mi)
{
    o_ChangeSelectContent(self, index, scroll_type, mi);
    hooks::guarded("FF6.AbilityChange.SelectContent", [&] { announce_change_list(self, index); });
}

IndexSelect_t o_ChangeSelectCommand;
void d_ChangeSelectCommand(void* self, int32_t index, const void* mi)
{
    o_ChangeSelectCommand(self, index, mi);
    hooks::guarded("FF6.AbilityChange.SelectCommand", [&] { announce_change_command(self, index); });
}

// AbilityWindowController state entries: each list's guard clears as it (re)gains focus, before
// the game's own selection runs; on command (re)entry the focused command is read one frame later
// (the cursor does not move on a back-out). FF4 AbilityWindowController_StateInit_Patch.
Void_t o_WindowCommandInit;
void d_WindowCommandInit(void* self, const void* mi)
{
    const bool mine = kAbilityWindow.is(self);
    if (mine) dedup::reset(kCtxCommand);
    o_WindowCommandInit(self, mi);
    if (!mine) return;
    hooks::guarded("FF6.AbilityWindow.CommandInit", [&] {
        frame::next_frame([] {
            hooks::guarded("FF6.AbilityWindow.CommandFocus", [] {
                void* window = unity::find_object(kAbilityWindow);
                if (!window || !unity::active_in_hierarchy(window)) return;
                void* controller = ref::rd<void*>(window, fWindowCommandController);
                void* cursor = controller ? ref::rd<void*>(controller, fCommandSelectCursor) : nullptr;
                if (cursor) announce_command(controller, ui::cursor_index(cursor));
            });
        }, "FF6.AbilityMenu.CommandFocus");
    });
}

Void_t o_WindowUseListInit;
void d_WindowUseListInit(void* self, const void* mi)
{
    if (kAbilityWindow.is(self)) {
        dedup::reset(kCtxList);
        dedup::reset(kCtxSpecial);
    }
    o_WindowUseListInit(self, mi);
}

Void_t o_WindowUseTargetInit;
void d_WindowUseTargetInit(void* self, const void* mi)
{
    if (kAbilityWindow.is(self)) dedup::reset(kCtxUseTarget);
    o_WindowUseTargetInit(self, mi);
}

Void_t o_WindowSelectMagicStoneInit;
void d_WindowSelectMagicStoneInit(void* self, const void* mi)
{
    if (kAbilityWindow.is(self)) reset_esper_list_guard();
    o_WindowSelectMagicStoneInit(self, mi);
}

// The window closing: every guard clears and the I key no longer has a focused entry.
Bool_t o_WindowSetActive;
void d_WindowSetActive(void* self, bool active, const void* mi)
{
    o_WindowSetActive(self, active, mi);
    hooks::guarded("FF6.AbilityWindow.SetActive", [&] {
        if (active || !kAbilityWindow.is(self)) return;
        reset_window_guards();
        g_focus = Focus{};
    });
}

// Gogo's command change: Show opens it (both guards clear), each state entry clears its own.
using ChangeShow_t = void (*)(void* self, int32_t target_index, const void* mi);
ChangeShow_t o_ChangeShow;
void d_ChangeShow(void* self, int32_t target_index, const void* mi)
{
    if (kAbilityChange.is(self)) {
        dedup::reset(kCtxChangeCommand);
        dedup::reset(kCtxChangeList);
    }
    o_ChangeShow(self, target_index, mi);
}

Void_t o_ChangeSelectCommandInit;
void d_ChangeSelectCommandInit(void* self, const void* mi)
{
    if (kAbilityChange.is(self)) dedup::reset(kCtxChangeCommand);
    o_ChangeSelectCommandInit(self, mi);
}

Void_t o_ChangeSelectListInit;
void d_ChangeSelectListInit(void* self, const void* mi)
{
    if (kAbilityChange.is(self)) dedup::reset(kCtxChangeList);
    o_ChangeSelectListInit(self, mi);
}

int install()
{
    int n = 0;
    n += hooks::attach(mCommandSelectContent, &d_CommandSelectContent, &o_CommandSelectContent) ? 1 : 0;
    n += hooks::attach(mListSelectContent, &d_ListSelectContent, &o_ListSelectContent) ? 1 : 0;
    n += hooks::attach(mSpecialSelectContent, &d_SpecialSelectContent, &o_SpecialSelectContent) ? 1 : 0;
    n += hooks::attach(mUseSelectContent, &d_UseSelectContent, &o_UseSelectContent) ? 1 : 0;
    n += hooks::attach(mChangeSelectContent, &d_ChangeSelectContent, &o_ChangeSelectContent) ? 1 : 0;
    n += hooks::attach(mChangeSelectCommand, &d_ChangeSelectCommand, &o_ChangeSelectCommand) ? 1 : 0;
    n += hooks::attach(mWindowCommandInit, &d_WindowCommandInit, &o_WindowCommandInit) ? 1 : 0;
    n += hooks::attach(mWindowUseListInit, &d_WindowUseListInit, &o_WindowUseListInit) ? 1 : 0;
    n += hooks::attach(mWindowUseTargetInit, &d_WindowUseTargetInit, &o_WindowUseTargetInit) ? 1 : 0;
    n += hooks::attach(mWindowSelectMagicStoneInit, &d_WindowSelectMagicStoneInit, &o_WindowSelectMagicStoneInit) ? 1 : 0;
    n += hooks::attach(mWindowSetActive, &d_WindowSetActive, &o_WindowSetActive) ? 1 : 0;
    n += hooks::attach(mChangeShow, &d_ChangeShow, &o_ChangeShow) ? 1 : 0;
    n += hooks::attach(mChangeSelectCommandInit, &d_ChangeSelectCommandInit, &o_ChangeSelectCommandInit) ? 1 : 0;
    n += hooks::attach(mChangeSelectListInit, &d_ChangeSelectListInit, &o_ChangeSelectListInit) ? 1 : 0;

    // The core details key (I / right stick up).
    queries::define("GameModule.HasDetails", [] { return has_details(); });
    queries::define_text("GameModule.Details", [] { return details_text(); });
    // The same entry under this area's own names, should the module need to combine several.
    queries::define("AbilityMenuPatches.HasDetails", [] { return has_details(); });
    queries::define_text("AbilityMenuPatches.Details", [] { return details_text(); });

    // The generic cursor reader stays silent on these screens.
    queries::define("FF6AbilityMenu.OwnsCursor", [] { return owns_cursor(); });
    cursor::add_suppressor("FF6AbilityMenu.OwnsCursor");
    return n;
}

core::Install s_install{"FF6.AbilityMenu", &install, 520, game::kFF6};

} // namespace

bool auto_detail() { return settings::get_bool("AutoDetail", true); }

void set_ability_focus(void* controller, const std::wstring& description, const std::wstring& blitz_mes_id)
{
    g_focus.controller = controller;
    g_focus.description = description;
    g_focus.blitz_mes_id = blitz_mes_id;
}

} // namespace ffpr::ff6
