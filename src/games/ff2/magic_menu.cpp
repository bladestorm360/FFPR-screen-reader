#include "../../core/displayed_ui.h"
// Ports FF2 Patches/MagicMenuPatches.cs (MagicMenuState and MagicMenuPatches), the
// AbilityWindowController.SetActive(false) postfix of MenuTransitionPatches
// (Core/FFII_ScreenReaderMod.cs) and FF2Constants.MagicMenuStates (Utils/FF2Constants.cs).
//
// FF2's field magic menu (Serial.FF2.UI.KeyInput): the Use / Forget command bar, the spell list
// ("Cure lv3, 45 percent", spell skill level 1-16 and its progress gauge; the level is the MP
// cost) and the healing targets ("Name, HP a/b, MP c/d, status").
//
// State: the registry key is the shared MAGIC_MENU, so the core's I-key cascade
// ("MagicMenuState.IsSpellListActive" -> MagicDetailsAnnouncer) and its suppression fallback
// work unchanged. The I key reads the core's last announced ability, which this file feeds
// (FF2's MenuDetailCache held the same description). The core owns MAGIC_MENU's reset handler, so
// FF2's own reset fields (list / target / command flags, the character and the three guards) are
// cleared lazily: any read finds MAGIC_MENU inactive and clears them first.
//
// Parity notes:
//  * The target guard (lastTargetAnnouncement) is reset right after every spoken target by
//    OnTargetSelectionActive, so it never suppresses anything (DEDUP_INVENTORY FF2 #26). Kept.
//  * Empty slots record -1, so a second Empty in a row is silent (DEDUP_INVENTORY finding 7).
//
// No hook here is folded: one RVA each in FF2's dump.cs. FF2's SetNextState body is folded with
// eight other methods and has no callers; the C# no longer hooked it and neither does this port.
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "ff2.h"
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/frame.h"
#include "../../core/hooks.h"
#include "../../core/il2cpp.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/menus/menus.h"
#include "../../core/system/system.h"
#include "../../core/ui/preferences.h"
#include "../../core/field/object_cache.h"

namespace ffpr::ff2 {
namespace {

// --- game names ------------------------------------------------------------------------------
ref::Class kWindow{"Serial.FF2.UI.KeyInput", "AbilityWindowController", game::kFF2};
ref::Field fWindowStateMachine{kWindow, "stateMachine"};
ref::Field fWindowCommandController{kWindow, "commandController"};
ref::Field fWindowListController{kWindow, "listController"};
ref::Method mCommandInit{kWindow, "CommandInit", 0};
ref::Method mUseListInit{kWindow, "UseListInit", 0};
ref::Method mForgetInit{kWindow, "ForgetInit", 0};
ref::Method mInitializeSelfOrderly{kWindow, "InitializeSelfOrderly", 0};
ref::Method mInitializeSelfOrderlyTarget{kWindow, "InitializeSelfOrderlyTarget", 0};
ref::Method mWindowSetActive{kWindow, "SetActive", 1};

ref::Class kCommand{"Serial.FF2.UI.KeyInput", "AbilityCommandController", game::kFF2};
ref::Field fCommandContentList{kCommand, "contentList"};
ref::Field fCommandSelectCursor{kCommand, "selectCursor"};
ref::Method mSetCommandSelectCursor{kCommand, "SetCommandSelectCursor", 0};
ref::Class kCommandContentView{"Serial.FF2.UI.KeyInput", "AbilityCommandContentView", game::kFF2};
ref::Field fCommandContentData{kCommandContentView, "<Data>k__BackingField"};
ref::Class kCommandContentData{"Last.UI", "AbilityCommandContentData", game::kFF2};
ref::Field fCommandDataName{kCommandContentData, "<Name>k__BackingField"};

ref::Class kList{"Serial.FF2.UI.KeyInput", "AbilityContentListController", game::kFF2};
ref::Field fListSelectCursor{kList, "selectCursor"};
ref::Field fListContentList{kList, "contentList"};
ref::Method mListSetCursor{kList, "SetCursor", 3};

ref::Class kUseList{"Serial.FF2.UI.KeyInput", "AbilityUseContentListController", game::kFF2};
ref::Field fUseContentList{kUseList, "contentList"};
ref::Method mUseSetCursor{kUseList, "SetCursor", 1};

ref::Class kAbilityInfoContent{"Last.UI.KeyInput", "BattleAbilityInfomationContentController", game::kFF2};
ref::Field fAbilityInfoData{kAbilityInfoContent, "<Data>k__BackingField"};
ref::Field fAbilityInfoGauge{kAbilityInfoContent, "commonGauge"};
ref::Class kCommonGauge{"Last.UI", "CommonGauge", game::kFF2};
ref::Field fGaugeImage{kCommonGauge, "gaugeImage"};
ref::Class kImage{"UnityEngine.UI", "Image", game::kFF2};
ref::Method mImageFillAmount{kImage, "get_fillAmount", 0};

ref::Class kTargetContent{"Last.UI.KeyInput", "ItemTargetSelectContentController", game::kFF2};
ref::Field fTargetCurrentData{kTargetContent, "<CurrentData>k__BackingField"};

ref::Class kOwnedAbility{"Last.Data.User", "OwnedAbility", game::kFF2};
ref::Method mOwnedAbilityAbility{kOwnedAbility, "get_Ability", 0};
ref::Method mOwnedAbilitySkillLevel{kOwnedAbility, "get_SkillLevel", 0};
ref::Class kAbility{"Last.Data.Master", "Ability", game::kFF2};
ref::Method mAbilityId{kAbility, "get_Id", 0};
ref::Class kExpUtility{"Last.Systems", "ExpUtility", game::kFF2};
ref::Method mGetExpLevel{kExpUtility, "GetExpLevel", 3};

ref::Class kOwnedCharacter{"Last.Data.User", "OwnedCharacterData", game::kFF2};
ref::Method mCharName{kOwnedCharacter, "get_Name", 0};
ref::Method mCharParameter{kOwnedCharacter, "get_Parameter", 0};
ref::Class kParamBase{"Last.Data", "CharacterParameterBase", game::kFF2};
ref::Field fParamCurrentHP{kParamBase, "currentHP"};
ref::Field fParamCurrentMP{kParamBase, "currentMP"};
ref::Method mParamConditionList{kParamBase, "get_CurrentConditionList", 0};
ref::Class kPlayerParam{"Last.Data", "PlayerCharacterParameter", game::kFF2};
ref::Method mConfirmedMaxHp{kPlayerParam, "ConfirmedMaxHp", 0};
ref::Method mConfirmedMaxMp{kPlayerParam, "ConfirmedMaxMp", 0};

// --- FF2Constants.MagicMenuStates (AbilityWindowController.State) ----------------------------
constexpr int STATE_NONE = 0;
constexpr int STATE_USE_LIST = 1;
constexpr int STATE_USE_TARGET = 2;
constexpr int STATE_FORGET = 3;
constexpr int STATE_COMMAND = 4;
constexpr int STATE_SELF_ORDERLY_TARGET = 8;
constexpr int kExpTableLevelExp = 1;   // ExpTableType.LevelExp
constexpr int MAX_RETRY_FRAMES = 30;

// --- MagicMenuState --------------------------------------------------------------------------
// The C# guards, moved to dedup:: as they were; reset where the C# reset them.
constexpr const char* kCtxSpell = "FF2.Magic.Spell";       // lastSpellId (-1 = Empty)
constexpr const char* kCtxTarget = "FF2.Magic.Target";     // lastTargetAnnouncement (text)
constexpr const char* kCtxCommand = "FF2.Magic.Command";   // lastCommandAnnouncement (text)

bool g_spell_list_focused = false;
bool g_target_selection_active = false;
bool g_command_menu_active = false;
bool g_fields_live = false;   // set while MAGIC_MENU is active through this file

bool magic_active() { return menu_state::is_active(menu_state::MAGIC_MENU); }

/// MagicMenuState's reset handler, run lazily (see the file comment).
void sync()
{
    if (magic_active() || !g_fields_live) return;
    g_fields_live = false;
    g_spell_list_focused = false;
    g_target_selection_active = false;
    g_command_menu_active = false;
    dedup::reset(kCtxSpell);
    dedup::reset(kCtxTarget);
    dedup::reset(kCtxCommand);
}

void set_exclusive()
{
    sync();
    set_active_exclusive(menu_state::MAGIC_MENU);
    g_fields_live = true;
}

void reset_state()
{
    menu_state::reset(menu_state::MAGIC_MENU);
    sync();
}

void update_registry_state()
{
    if (!g_spell_list_focused && !g_target_selection_active && !g_command_menu_active) reset_state();
}

void on_spell_list_focused()
{
    set_exclusive();
    g_spell_list_focused = true;
    dedup::reset(kCtxSpell);
}

void on_spell_list_unfocused()
{
    sync();
    g_spell_list_focused = false;
    dedup::reset(kCtxSpell);
    update_registry_state();
}

void on_target_selection_active()
{
    set_exclusive();
    g_target_selection_active = true;
    dedup::reset(kCtxTarget);
}

void on_command_menu_active()
{
    set_exclusive();
    g_command_menu_active = true;
    g_spell_list_focused = false;
    dedup::reset(kCtxCommand);
}

int window_state(void* window) { return ui::state_tag(window, fWindowStateMachine); }

/// MagicMenuState.ShouldSuppress: validated against the window's state machine.
bool should_suppress()
{
    sync();
    if (!magic_active()) return false;
    void* window = field::cache::get(kWindow);
    if (!window || !unity::active_in_hierarchy(window)) {
        reset_state();
        return false;
    }
    if (window_state(window) == STATE_NONE) {
        reset_state();
        return false;
    }
    return true;
}

/// GetSpellProficiency: ExpUtility.GetExpLevel(1, SkillLevel, LevelExp), clamped to 1..16.
int spell_proficiency(void* ability)
{
    if (!ability) return 0;
    return hooks::guarded_r("FF2.Magic.Proficiency", 1, [&] {
        const int raw = ref::call<int32_t>(mOwnedAbilitySkillLevel, ability);
        int level = ref::call_static<int32_t>(mGetExpLevel, static_cast<int32_t>(1), static_cast<int32_t>(raw),
                                              static_cast<int32_t>(kExpTableLevelExp));
        if (level < 1) level = 1;
        if (level > 16) level = 16;
        return level;
    });
}

/// The spell's progress gauge: Gauge.gaugeImage.fillAmount as 0-99 percent, -1 when unreadable.
int gauge_percent(void* content)
{
    void* gauge = ref::rd<void*>(content, fAbilityInfoGauge);
    void* image = gauge ? ref::rd<void*>(gauge, fGaugeImage) : nullptr;
    if (!image || !mImageFillAmount) return -1;
    const float fill = ref::call<float>(mImageFillAmount, image);
    int pct = static_cast<int>(fill * 100);
    if (pct < 0) pct = 0;
    if (pct > 99) pct = 99;
    return pct;
}

void announce_empty(int index, int count)
{
    if (dedup::index(kCtxSpell, -1)) ffpr::say(ui::menu_position(T(L"Empty"), index, count), true);
}

/// AnnounceSpell: "Name lvN, P percent[: description] (X of Y)".
void announce_spell(void* ability, void* content, int index, int count)
{
    int spell_id = 0;
    if (void* data = ref::call<void*>(mOwnedAbilityAbility, ability)) spell_id = ref::call<int32_t>(mAbilityId, data);
    if (!dedup::index(kCtxSpell, spell_id)) return;

    std::wstring name = menus::spell_name(ability);
    if (name.empty()) return;

    std::wstring announcement = name;
    const int proficiency = spell_proficiency(ability);
    if (proficiency > 0) announcement = TF(L"{0} lv{1}", {announcement, text::num(proficiency)});

    const int pct = content ? hooks::guarded_r("FF2.Magic.Gauge", -1, [&] { return gauge_percent(content); }) : -1;
    if (pct >= 0) announcement = TF(L"{0}, {1} percent", {announcement, text::num(pct)});

    // The level already conveys the MP cost. Description with Auto Detail; the I key reads it.
    const std::wstring description = menus::spell_description(ability);
    menus::set_last_announced_ability(ability);
    if (ui::prefs::auto_detail_enabled() && !description.empty()) announcement += L": " + description;

    ffpr::say(ui::menu_position(announcement, index, count), true);
}

/// AnnounceSpellAtIndex.
void announce_spell_at_index(void* list_ctrl, int index)
{
    void* content_list = ref::rd<void*>(list_ctrl, fListContentList);
    if (!content_list) return;
    const int count = unity::list_count(content_list);
    if (index < 0 || index >= count) return;
    void* content = unity::list_at(content_list, index);
    if (!content) {
        announce_empty(index, count);
        return;
    }
    void* ability = ref::rd<void*>(content, fAbilityInfoData);
    if (!ability) {
        announce_empty(index, count);
        return;
    }
    announce_spell(ability, content, index, count);
}

/// TryAnnounceFocusedCommand: false only while the focus is not readable yet.
bool try_announce_focused_command(void* controller)
{
    void* cursor = ref::rd<void*>(controller, fCommandSelectCursor);
    if (!cursor) return false;
    const int index = ui::cursor_index(cursor);
    void* list = ref::rd<void*>(controller, fCommandContentList);
    if (!list) return false;
    const int count = unity::list_count(list);
    if (index < 0 || index >= count) return false;
    void* view = unity::list_at(list, index);
    void* data = view ? ref::rd<void*>(view, fCommandContentData) : nullptr;
    std::wstring name = data ? ref::rd_str(data, fCommandDataName) : std::wstring();
    if (name.empty()) return false;
    if (dedup::text(kCtxCommand, name)) ffpr::say(ui::menu_position(name, index, count), true);
    return true;
}

enum class ReadResult { Spoken, NotReady, Abandon };

ReadResult read_command_entry(void* window)
{
    // The UI pre-load runs CommandInit on the field (core game_state.cpp, IsFieldState).
    if (ffpr::system::is_field_state()) return ReadResult::Abandon;
    if (!unity::alive(window) || !unity::active_in_hierarchy(window)) return ReadResult::NotReady;
    const int state = window_state(window);
    if (state != STATE_COMMAND) return state < 0 ? ReadResult::NotReady : ReadResult::Abandon;
    void* cmd = ref::rd<void*>(window, fWindowCommandController);
    if (!cmd) return ReadResult::NotReady;
    return try_announce_focused_command(cmd) ? ReadResult::Spoken : ReadResult::NotReady;
}

ReadResult read_spell_entry(void* window)
{
    sync();
    if (!unity::alive(window) || !unity::active_in_hierarchy(window)) return ReadResult::NotReady;
    const int state = window_state(window);
    if (state != STATE_USE_LIST && state != STATE_FORGET) return state < 0 ? ReadResult::NotReady : ReadResult::Abandon;
    if (!g_spell_list_focused) return ReadResult::Abandon;   // reset (popup) since the Init

    void* list = ref::rd<void*>(window, fWindowListController);
    if (!list) return ReadResult::NotReady;
    void* cursor = ref::rd<void*>(list, fListSelectCursor);
    void* content_list = ref::rd<void*>(list, fListContentList);
    if (!cursor || !content_list) return ReadResult::NotReady;
    const int index = ui::cursor_index(cursor);
    if (index < 0 || index >= unity::list_count(content_list)) return ReadResult::NotReady;

    // (The C# also cached targetCharacterData as MagicMenuState.CurrentCharacter; nothing read it.)
    announce_spell_at_index(list, index);   // guarded by spell id
    return ReadResult::Spoken;
}

// Bumped per Init; an older deferred read exits when superseded.
int g_command_read_gen = 0;
int g_list_read_gen = 0;
// True while an Init body runs: its cursor moves stay quiet, the deferred read speaks.
bool g_command_init_in_progress = false;
bool g_list_init_in_progress = false;

void deferred_read(void* window, int* gen_counter, ReadResult (*read)(void*), const char* label)
{
    const int gen = ++*gen_counter;
    auto frames = std::make_shared<int>(0);
    frame::every_frame([=]() -> bool {
        if (gen != *gen_counter) return false;
        if (++*frames > MAX_RETRY_FRAMES) return false;
        const ReadResult r = hooks::guarded_r(label, ReadResult::NotReady, [&] { return read(window); });
        return r == ReadResult::NotReady;
    }, label, MAX_RETRY_FRAMES + 1);
}

// --- detours ---------------------------------------------------------------------------------
using Void_t = void (*)(void* self, const void* mi);
using VoidBool_t = void (*)(void* self, bool a, const void* mi);
using ListSetCursor_t = void (*)(void* self, void* cursor, bool is_scroll, int32_t type, const void* mi);
using UseSetCursor_t = void (*)(void* self, void* cursor, const void* mi);

Void_t o_CommandInit, o_UseListInit, o_ForgetInit, o_SelfOrderly, o_SelfOrderlyTarget, o_SetCommandSelectCursor;
VoidBool_t o_WindowSetActive;
ListSetCursor_t o_ListSetCursor;
UseSetCursor_t o_UseSetCursor;

void d_CommandInit(void* self, const void* mi)
{
    if (kWindow.is(self)) field::cache::register_object(kWindow, self);
    hooks::guarded("FF2.Magic.CommandInit.pre", [&] { on_command_menu_active(); });
    g_command_init_in_progress = true;
    o_CommandInit(self, mi);
    g_command_init_in_progress = false;
    hooks::guarded("FF2.Magic.CommandInit", [&] {
        if (self) deferred_read(self, &g_command_read_gen, &read_command_entry, "FF2.Magic.CommandRead");
    });
}

void list_init(Void_t original, void* self, const void* mi)
{
    if (kWindow.is(self)) field::cache::register_object(kWindow, self);
    hooks::guarded("FF2.Magic.ListInit.pre", [&] { on_spell_list_focused(); });
    g_list_init_in_progress = true;
    original(self, mi);
    g_list_init_in_progress = false;
    hooks::guarded("FF2.Magic.ListInit", [&] {
        if (self) deferred_read(self, &g_list_read_gen, &read_spell_entry, "FF2.Magic.SpellRead");
    });
}

void d_UseListInit(void* self, const void* mi) { list_init(o_UseListInit, self, mi); }
void d_ForgetInit(void* self, const void* mi) { list_init(o_ForgetInit, self, mi); }

// Manual sort states reuse the spell list outside Use / Forget: the list stops speaking.
void self_orderly()
{
    sync();
    if (g_spell_list_focused) on_spell_list_unfocused();
}

void d_SelfOrderly(void* self, const void* mi)
{
    o_SelfOrderly(self, mi);
    hooks::guarded("FF2.Magic.InitializeSelfOrderly", [&] { self_orderly(); });
}

void d_SelfOrderlyTarget(void* self, const void* mi)
{
    o_SelfOrderlyTarget(self, mi);
    hooks::guarded("FF2.Magic.InitializeSelfOrderlyTarget", [&] { self_orderly(); });
}

void d_WindowSetActive(void* self, bool is_active, const void* mi)
{
    o_WindowSetActive(self, is_active, mi);
    hooks::guarded("FF2.Magic.SetActive", [&] {
        if (!kWindow.is(self)) return;
        if (is_active) field::cache::register_object(kWindow, self);
        else if (field::cache::get(kWindow) == self) field::cache::clear(kWindow);
        if (!is_active) reset_state();
    });
}

void d_SetCommandSelectCursor(void* self, const void* mi)
{
    o_SetCommandSelectCursor(self, mi);
    hooks::guarded("FF2.Magic.SetCommandSelectCursor", [&] {
        if (g_command_init_in_progress || !self || !unity::active_in_hierarchy(self)) return;
        if (ffpr::system::is_field_state()) return;   // UI pre-load on the field
        void* window = field::cache::get(kWindow);
        if (!window || window_state(window) != STATE_COMMAND) return;
        sync();
        // Cursor MOVES only. The bar's first command is the entry read's (CommandInit): the game
        // sets the cursor once more while opening the window (SetActive -> ResetController), with
        // the state machine still on Command from the last visit, BEFORE CommandInit runs. The
        // C# (and the first port) took that as the entry, spoke "Use", then CommandInit forgot
        // the guard and the entry read spoke "Use" again (FF2 test, 2026-10-10).
        if (!g_command_menu_active) {
            LOGF("[FF2 Magic] command cursor set before CommandInit: left to the entry read");
            return;
        }
        try_announce_focused_command(self);
    });
}

void d_ListSetCursor(void* self, void* cursor, bool is_scroll, int32_t type, const void* mi)
{
    o_ListSetCursor(self, cursor, is_scroll, type, mi);
    hooks::guarded("FF2.Magic.ListSetCursor", [&] {
        if (!self || !cursor) return;
        if (g_list_init_in_progress) return;   // the entry read speaks the initial focus
        if (!unity::active_in_hierarchy(self)) return;
        // The state machine first: never read spells on the command bar.
        if (void* window = field::cache::get(kWindow)) {
            const int state = window_state(window);
            if (state != STATE_USE_LIST && state != STATE_FORGET) return;
        }
        sync();
        if (!g_spell_list_focused) return;
        announce_spell_at_index(self, ui::cursor_index(cursor));
    });
}

int virtual_int(ref::Method& m, void* obj)
{
    il2cpp::Method* base = m.get();
    if (!base || !obj) return 0;
    return il2cpp::invoke_int(il2cpp::virtual_method(obj, base), obj, nullptr, 0);
}

void d_UseSetCursor(void* self, void* cursor, const void* mi)
{
    o_UseSetCursor(self, cursor, mi);
    hooks::guarded("FF2.Magic.TargetSetCursor", [&] {
        if (!self || !cursor || !unity::active_in_hierarchy(self)) return;
        if (void* window = field::cache::get(kWindow)) {
            const int state = window_state(window);
            if (state != STATE_USE_TARGET && state != STATE_SELF_ORDERLY_TARGET) return;
        }
        const int index = ui::cursor_index(cursor);
        void* list = ref::rd<void*>(self, fUseContentList);
        const int slots = unity::list_count(list);
        if (!list || slots <= 0 || slots > 9 || index < 0 || index >= slots) return;
        const int count = displayed::target_count(list);
        if (count == 0) return;
        // The native SetCursor uses contentList[cursor.Index], regardless of parent ancestry.
        void* content = unity::list_at(list, index);
        void* character = content ? ref::rd<void*>(content, fTargetCurrentData) : nullptr;
        if (!character) return;
        std::wstring announcement = displayed::target(content);
        if (announcement.empty()) return;

        sync();
        on_target_selection_active();
        ffpr::say(ui::menu_position(announcement, index, count), true);
    });
}

int install()
{
    queries::define("MagicMenuState.ShouldSuppress", [] { return should_suppress(); });
    queries::define("MagicMenuState.IsTargetSelectionActive", [] { sync(); return g_target_selection_active; });
    queries::define("MagicMenuState.IsCommandMenuActive", [] { sync(); return g_command_menu_active; });

    int n = 0;
    n += hooks::attach(mListSetCursor, &d_ListSetCursor, &o_ListSetCursor) ? 1 : 0;
    n += hooks::attach(mCommandInit, &d_CommandInit, &o_CommandInit) ? 1 : 0;
    n += hooks::attach(mUseListInit, &d_UseListInit, &o_UseListInit) ? 1 : 0;
    n += hooks::attach(mForgetInit, &d_ForgetInit, &o_ForgetInit) ? 1 : 0;
    n += hooks::attach(mInitializeSelfOrderly, &d_SelfOrderly, &o_SelfOrderly) ? 1 : 0;
    n += hooks::attach(mInitializeSelfOrderlyTarget, &d_SelfOrderlyTarget, &o_SelfOrderlyTarget) ? 1 : 0;
    n += hooks::attach(mSetCommandSelectCursor, &d_SetCommandSelectCursor, &o_SetCommandSelectCursor) ? 1 : 0;
    n += hooks::attach(mUseSetCursor, &d_UseSetCursor, &o_UseSetCursor) ? 1 : 0;
    n += hooks::attach(mWindowSetActive, &d_WindowSetActive, &o_WindowSetActive) ? 1 : 0;
    LOGF("[FF2 Magic] %d of 9 hooks queued", n);
    return n;
}

core::Install s_install{"FF2.MagicMenu", &install, 520, game::kFF2};

} // namespace
} // namespace ffpr::ff2
