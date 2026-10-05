// Ports FF1 Patches/MagicMenuPatches.cs, Patches/MagicTargetPatches.cs and
// MagicMenuState.ShouldSuppress (Patches/MagicMenuState.cs).
//
// FF1 ONLY (game::kFF1). The field magic menu is a per-game class (Serial.FF1.UI.KeyInput,
// Serial.FF2.UI.KeyInput ... Serial.FF5.UI.KeyInput AbilityWindowController,
// AbilityContentListController, AbilityUseContentListController), and FF1's spells are bought per
// level with charges (MagicMenuState.GetChargesForLevel). The other games need their own spell
// list hooks on their Serial.FFn classes: FF2 spell skill levels, FF3 per-level charges like
// FF1, FF4 and FF5 MP. They can reuse menus.h (spell_name, spell_description, condition_name,
// set_last_announced_ability) and the shared MAGIC_MENU state.
//
// Folded body: Serial.FF1.UI.KeyInput.AbilityWindowController.SetNextState shares its RVA with 8
// other methods in FF1 (field setters such as OwnedCharacterData.set_NumberOfButtles); the
// detour class-checks `self`. Harmony's postfix had no such check, so in FF1 a SetNextState(0)
// of any of those 9 methods reset the magic state.
#include <memory>
#include "menus.h"
#include "menus_internal.h"
#include "../core.h"
#include "../dedup.h"
#include "../frame.h"
#include "../hooks.h"
#include "../modtext.h"
#include "../say.h"
#include "../text.h"
#include "../ui_util.h"
#include "../unity.h"

namespace ffpr::menus {
namespace {

ref::Class  kFF1AbilityWindowController{"Serial.FF1.UI.KeyInput", "AbilityWindowController", game::kFF1};
ref::Field  fFF1AbilityWindowStateMachine{kFF1AbilityWindowController, "stateMachine"};
ref::Field  fFF1AbilityWindowStatusController{kFF1AbilityWindowController, "statusController"};
ref::Field  fFF1AbilityWindowListController{kFF1AbilityWindowController, "listController"};
ref::Method mFF1AbilityWindowSetNextState{kFF1AbilityWindowController, "SetNextState", 1};
ref::Method mFF1AbilityWindowUseListInit{kFF1AbilityWindowController, "UseListInit", 0};
ref::Method mFF1AbilityWindowForgetInit{kFF1AbilityWindowController, "ForgetInit", 0};
ref::Method mFF1AbilityWindowSetActive{kFF1AbilityWindowController, "SetActive", 1};

ref::Class  kFF1AbilityCharaStatusController{"Serial.FF1.UI.KeyInput", "AbilityCharaStatusController", game::kFF1};
ref::Field  fFF1AbilityCharaStatusTargetData{kFF1AbilityCharaStatusController, "targetData"};

ref::Class  kFF1AbilityContentListController{"Serial.FF1.UI.KeyInput", "AbilityContentListController", game::kFF1};
ref::Field  fFF1AbilityListSelectCursor{kFF1AbilityContentListController, "selectCursor"};
ref::Field  fFF1AbilityListContentList{kFF1AbilityContentListController, "contentList"};
ref::Method mFF1AbilityListSetCursor{kFF1AbilityContentListController, "SetCursor", 2};

ref::Class  kFF1AbilityUseContentListController{"Serial.FF1.UI.KeyInput", "AbilityUseContentListController", game::kFF1};
ref::Field  fFF1AbilityUseContentList{kFF1AbilityUseContentListController, "contentList"};
ref::Method mFF1AbilityUseSetCursor{kFF1AbilityUseContentListController, "SetCursor", 1};
ref::Method mFF1AbilityUseShow{kFF1AbilityUseContentListController, "Show", 3};
ref::Method mFF1AbilityUseClose{kFF1AbilityUseContentListController, "Close", 0};

ref::Class  kFF1BattleAbilityInfoContent{"Last.UI.KeyInput", "BattleAbilityInfomationContentController", game::kFF1};
ref::Field  fFF1BattleAbilityInfoData{kFF1BattleAbilityInfoContent, "<Data>k__BackingField"};

ref::Class  kFF1MagicTargetContent{"Last.UI.KeyInput", "ItemTargetSelectContentController", game::kFF1};
ref::Field  fFF1MagicTargetCurrentData{kFF1MagicTargetContent, "<CurrentData>k__BackingField"};

// AbilityWindowController.State: None=0, UseList=1, UseTarget=2, Forget=3, Command=4, Popup=5.
constexpr int STATE_NONE = 0;
constexpr int STATE_COMMAND = 4;

// Spell-list (re)entry read: a newer entry supersedes a pending read; state 0 (None) cancels it.
int g_initial_spell_read_gen = 0;
// Same cap the old per-frame consume used.
constexpr int INITIAL_SPELL_MAX_FRAMES = 30;

// The last spell line spoken (MagicMenuPatches._lastSpellAnnouncement, full line including the
// position). On the Forget-popup back-out both the cursor-settle SetCursor and the re-announce
// reach announce_spell for the same slot; the settle fires first and equals the prior navigation
// line, so it is the one suppressed. The (re)entry read resets it so a genuine (re)entry always
// speaks. Empty slots are not recorded (as in FF1).
constexpr const char* kCtxSpell = "Magic.List";

/// MagicMenuState.ShouldSuppress (FF1's AbilityWindowController state machine).
bool should_suppress()
{
    // Target selection always suppresses: it uses AbilityUseContentListController, not the
    // window's state machine.
    if (detail::magic_target_selection_active()) return true;
    if (!detail::magic_spell_list_active()) return false;

    if (void* window = unity::find_object(kFF1AbilityWindowController)) {
        int state = ui::state_tag(window, fFF1AbilityWindowStateMachine);
        if (state == STATE_COMMAND || state == STATE_NONE) {
            detail::magic_reset_state();
            return false;
        }
    }
    return true;
}

/// MagicMenuPatches.AnnounceSpell: name, level, charges, AutoDetail description, "(X of Y)".
bool announce_spell(void* ability, int index, int count)
{
    int spell_lv = spell_level(ability);

    // Cache for the I key.
    detail::magic_set_last_announced_ability(ability);

    std::wstring name = spell_name(ability);
    if (name.empty()) return false;

    std::wstring base = name;
    if (spell_lv > 0) {
        base += L" " + TF(L"LV{0}", {text::num(spell_lv)});
        if (void* character = detail::magic_current_character()) {
            int current = 0, max = 0;
            charges_for_level(character, spell_lv, current, max);
            if (max > 0) base += L": " + TF(L"MP: {0}/{1}", {text::num(current), text::num(max)});
        }
    }

    // AutoDetail: append the description on focus.
    std::wstring announcement = base;
    if (detail::auto_detail()) {
        std::wstring description = spell_description(ability);
        if (!detail::blank(description)) announcement = base + L": " + description;
    }

    // Position last, after any AutoDetail description.
    announcement = ui::menu_position(announcement, index, count);

    // Identical to the last line (a cursor-settle re-fire): skip.
    if (!dedup::text(kCtxSpell, announcement)) return false;

    ffpr::say(announcement, true);
    return true;
}

void announce_empty(int index, int count)
{
    ffpr::say(ui::menu_position(T(L"Empty"), index, count), true);
}

/// MagicMenuPatches.AnnounceSpellAtIndex.
void announce_spell_at_index(void* controller, int index)
{
    void* content_list = ref::rd<void*>(controller, fFF1AbilityListContentList);
    if (!content_list) return;
    int count = unity::list_count(content_list);
    if (index < 0 || index >= count) return;

    void* content = unity::list_at(content_list, index);
    if (!content) {
        announce_empty(index, count);
        return;
    }
    void* ability = ref::rd<void*>(content, fFF1BattleAbilityInfoData);
    if (!ability) {
        announce_empty(index, count);
        return;
    }
    announce_spell(ability, index, count);
}

/// MagicMenuPatches.TryAnnounceInitialSpell: speaks the focused spell if the list is genuinely
/// ready (menu open, contentList populated, index in range, slot and ability set). True only
/// when it spoke, so the caller keeps retrying until the list finishes building.
bool try_announce_initial_spell(void* controller)
{
    // Never read during a map/asset load.
    if (!detail::menu_manager_open()) return false;

    void* content_list = ref::rd<void*>(controller, fFF1AbilityListContentList);
    int count = unity::list_count(content_list);
    if (count <= 0) return false;

    int index = ui::cursor_index(ref::rd<void*>(controller, fFF1AbilityListSelectCursor));
    if (index < 0 || index >= count) return false;

    void* content = unity::list_at(content_list, index);
    if (!content) return false;
    void* ability = ref::rd<void*>(content, fFF1BattleAbilityInfoData);
    if (!ability) return false;

    // Force the (re)entry line even if it equals the last one (a cursor-settle SetCursor may
    // have fired first on a popup back-out): this read is the authoritative re-entry source.
    dedup::reset(kCtxSpell);
    return announce_spell(ability, index, count);
}

/// MagicMenuPatches.BeginSpellListEntry / SpellListEntryRoutine. Starting the frame after the
/// Init, each frame while the list controller is active: mark the spell list focused (so
/// SetCursor navigation speaks), resolve the character (for charges) from the window's status
/// controller, and try the focused-spell line until the list is built - at most
/// INITIAL_SPELL_MAX_FRAMES tries. No event exists for "list populated".
void begin_spell_list_entry(void* window)
{
    if (!kFF1AbilityWindowController.is(window)) return;
    int gen = ++g_initial_spell_read_gen;
    struct Progress { int frame = 0; int tries = 0; };
    auto p = std::make_shared<Progress>();
    frame::every_frame([=]() -> bool {
        if (gen != g_initial_spell_read_gen) return false;
        if (!unity::alive(window)) return false;
        ++p->frame;

        bool done = false;
        void* list = ref::rd<void*>(window, fFF1AbilityWindowListController);
        // Not shown yet: doesn't count as a try.
        if (list && unity::alive(list) && unity::active_in_hierarchy(list)) {
            ++p->tries;
            if (!detail::magic_spell_list_active()) detail::magic_on_spell_list_focused();

            if (!detail::magic_current_character()) {
                void* status = ref::rd<void*>(window, fFF1AbilityWindowStatusController);
                if (void* character = ref::rd<void*>(status, fFF1AbilityCharaStatusTargetData))
                    detail::magic_set_current_character(character);
            }
            done = try_announce_initial_spell(list);
        }
        if (done) return false;
        return p->frame < INITIAL_SPELL_MAX_FRAMES * 2 && p->tries <= INITIAL_SPELL_MAX_FRAMES;
    }, "MagicMenu.SpellListRead", INITIAL_SPELL_MAX_FRAMES * 2 + 1);
}

// --- AbilityContentListController.SetCursor(int, Cursor): spell list navigation -------------

using ListSetCursor_t = void (*)(void* self, int32_t index, void* cursor, const void* mi);
ListSetCursor_t o_ListSetCursor;

void d_ListSetCursor(void* self, int32_t index, void* cursor, const void* mi)
{
    o_ListSetCursor(self, index, cursor, mi);
    hooks::guarded("AbilityContentListController.SetCursor", [&] {
        if (!detail::magic_spell_list_active()) return;
        if (!self || !kFF1AbilityContentListController.is(self)) return;
        if (!unity::active_in_hierarchy(self)) return;
        announce_spell_at_index(self, index);
    });
}

// --- AbilityWindowController: SetNextState, UseListInit / ForgetInit, SetActive -------------

using SetNextState_t = void (*)(void* self, int32_t state, const void* mi);
SetNextState_t o_SetNextState;

void d_SetNextState(void* self, int32_t state, const void* mi)
{
    o_SetNextState(self, state, mi);
    hooks::guarded("AbilityWindowController.SetNextState", [&] {
        if (!kFF1AbilityWindowController.is(self)) return;   // folded body: other classes
        // Arming is done by UseListInit/ForgetInit; None resets and cancels a pending read.
        if (state == STATE_NONE) {
            detail::magic_reset_state();
            ++g_initial_spell_read_gen;
        }
    });
}

using Init_t = void (*)(void* self, const void* mi);
Init_t o_UseListInit, o_ForgetInit;

void d_UseListInit(void* self, const void* mi)
{
    o_UseListInit(self, mi);
    hooks::guarded("AbilityWindowController.UseListInit", [&] { begin_spell_list_entry(self); });
}

void d_ForgetInit(void* self, const void* mi)
{
    o_ForgetInit(self, mi);
    hooks::guarded("AbilityWindowController.ForgetInit", [&] { begin_spell_list_entry(self); });
}

using SetActive_t = void (*)(void* self, bool is_active, const void* mi);
SetActive_t o_WindowSetActive;

void d_WindowSetActive(void* self, bool is_active, const void* mi)
{
    o_WindowSetActive(self, is_active, mi);
    hooks::guarded("AbilityWindowController.SetActive", [&] {
        if (!is_active) detail::magic_reset_state();
    });
}

// --- MagicTargetPatches: AbilityUseContentListController -----------------------------------

/// MagicTargetPatches.AnnounceTargetCharacter: "Name: HP: a / b status, status (X of Y)".
void announce_target_character(void* character, int index, int count)
{
    std::wstring name = detail::character_name(character);
    if (detail::blank(name)) return;

    std::wstring announcement = name;
    detail::Vitals v = detail::character_vitals(character);
    if (v.has_parameter) {
        announcement += L": " + TF(L"HP: {0} / {1}", {text::num(v.hp), text::num(v.max_hp)});
        if (!v.conditions.empty()) announcement += L" " + detail::join_all(v.conditions, L", ");
    }
    ffpr::say(ui::menu_position(announcement, index, count), true);
}

using UseSetCursor_t = void (*)(void* self, void* cursor, const void* mi);
UseSetCursor_t o_UseSetCursor;

void d_UseSetCursor(void* self, void* cursor, const void* mi)
{
    o_UseSetCursor(self, cursor, mi);
    hooks::guarded("AbilityUseContentListController.SetCursor", [&] {
        if (!self || !cursor) return;
        if (!kFF1AbilityUseContentListController.is(self) || !unity::active_in_hierarchy(self)) return;

        if (!detail::magic_target_selection_active()) detail::magic_on_target_selection_active();

        int index = ui::cursor_index(cursor);
        void* content_list = ref::rd<void*>(self, fFF1AbilityUseContentList);
        if (!content_list) return;
        int count = unity::list_count(content_list);
        if (index < 0 || index >= count) return;

        void* content = unity::list_at(content_list, index);
        if (!content) return;
        void* character = ref::rd<void*>(content, fFF1MagicTargetCurrentData);
        if (!character) return;
        announce_target_character(character, index, count);
    });
}

using UseShow_t = void (*)(void* self, void* character, void* ability, void* targets, const void* mi);
UseShow_t o_UseShow;

void d_UseShow(void* self, void* character, void* ability, void* targets, const void* mi)
{
    o_UseShow(self, character, ability, targets, mi);
    hooks::guarded("AbilityUseContentListController.Show", [&] {
        if (!kFF1AbilityUseContentListController.is(self)) return;
        detail::magic_on_target_selection_active();
    });
}

using UseClose_t = void (*)(void* self, const void* mi);
UseClose_t o_UseClose;

void d_UseClose(void* self, const void* mi)
{
    o_UseClose(self, mi);
    hooks::guarded("AbilityUseContentListController.Close", [&] {
        if (!kFF1AbilityUseContentListController.is(self)) return;
        if (!detail::magic_target_selection_active()) return;
        detail::magic_on_target_selection_closed();
    });
}

int install()
{
    queries::define("MagicMenuState.ShouldSuppress", [] { return should_suppress(); });

    int n = 0;
    // MagicMenuPatches.ApplyPatches
    n += hooks::attach(mFF1AbilityListSetCursor, &d_ListSetCursor, &o_ListSetCursor) ? 1 : 0;
    n += hooks::attach(mFF1AbilityWindowSetNextState, &d_SetNextState, &o_SetNextState) ? 1 : 0;
    n += hooks::attach(mFF1AbilityWindowUseListInit, &d_UseListInit, &o_UseListInit) ? 1 : 0;
    n += hooks::attach(mFF1AbilityWindowForgetInit, &d_ForgetInit, &o_ForgetInit) ? 1 : 0;
    // FF1 patched SetActive twice (attribute + manual) with the same reset; once is enough.
    n += hooks::attach(mFF1AbilityWindowSetActive, &d_WindowSetActive, &o_WindowSetActive) ? 1 : 0;
    // MagicTargetPatches.ApplyPatches
    n += hooks::attach(mFF1AbilityUseSetCursor, &d_UseSetCursor, &o_UseSetCursor) ? 1 : 0;
    n += hooks::attach(mFF1AbilityUseShow, &d_UseShow, &o_UseShow) ? 1 : 0;
    n += hooks::attach(mFF1AbilityUseClose, &d_UseClose, &o_UseClose) ? 1 : 0;
    return n;
}

core::Install s_install{"Menus.MagicMenuFF1", &install, 120, game::kFF1};

} // namespace
} // namespace ffpr::menus
