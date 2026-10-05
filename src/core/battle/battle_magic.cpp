// Ports FF1 Patches/BattleMagicPatches.cs: the battle spell list with FF1's spell levels and
// charges ("Cure Lv1: MP: 3/5. description"), and the on-demand description.
//
// FF1 ONLY (game::kFF1): the hooked class is Serial.FF1.UI.KeyInput.
// BattleFrequencyAbilityInfomationController, FF1's charge-based spell list. FF3's class of the same
// name has a different SetCursor layout and FF2/FF4/FF5 have no such class; their battle ability
// lists are not ported here (they were each mod's own patches).
// SetCursor(Cursor, bool, WithinRangeType) is the FF1 override, unique RVA.
#include <string>
#include <utility>
#include "battle_internal.h"
#include "battle_util.h"
#include "../core.h"
#include "../dedup.h"
#include "../frame.h"
#include "../hooks.h"
#include "../modtext.h"
#include "../say.h"
#include "../settings.h"
#include "../text.h"
#include "../ui_util.h"
#include "../unity.h"

namespace ffpr::battle {
namespace {

ref::Class kFreqCtl{"Serial.FF1.UI.KeyInput", "BattleFrequencyAbilityInfomationController", game::kFF1};
ref::Method mSetCursor{kFreqCtl, "SetCursor", 3};
ref::Field fDataList{kFreqCtl, "dataList"};
ref::Field fSelectedPlayer{kFreqCtl, "selectedBattlePlayerData"};

ref::Class kOwnedAbility{"Last.Data.User", "OwnedAbility", game::kFF1};
ref::Field fAbilityData{kOwnedAbility, "ability"};
ref::Method mAbilityMesIdName{kOwnedAbility, "get_MesIdName", 0};
ref::Method mAbilityMesIdDescription{kOwnedAbility, "get_MesIdDescription", 0};

ref::Class kAbility{"Last.Data.Master", "Ability", game::kFF1};
ref::Method mAbilityLv{kAbility, "get_AbilityLv", 0};

ref::Class kPlayerParam{"Last.Data", "PlayerCharacterParameter", game::kFF1};
ref::Field fCurrentMpCountList{kPlayerParam, "<CurrentMpCountList>k__BackingField"};
ref::Field fBaseMaxMpCountList{kPlayerParam, "<BaseMaxMpCountList>k__BackingField"};
ref::Method mConfirmedMaxMpCount{kPlayerParam, "ConfirmedMaxMpCount", 1};

// _lastSpellAnnouncement: backup against a same-string settle re-fire (the gen latch handles the
// stale/fresh double).
constexpr const char* kSpellCtx = "BattleMagic.Spell";

// CurrentPlayer (BattlePlayerData): refreshed from the controller before each read.
void* g_current_player = nullptr;
// LastFocusedDescription.
std::wstring g_last_focused_description;
// _magicReadGen: bumped on every SetCursor so a newer cursor event supersedes a pending read.
int g_magic_read_gen = 0;

/// InMagicContext: the spell list is the genuine active focus (shown, and no other battle sub-menu
/// owns the context). Checked at schedule time and after the one-frame defer.
bool in_magic_context(void* c)
{
    return c && unity::alive(c) && unity::active_in_hierarchy(c)
        && !state_active(menu_state::BATTLE_ITEM)
        && !state_active(menu_state::BATTLE_TARGET)
        && !(state_active(menu_state::BATTLE_COMMAND) && last_selected_command_index() != 1);
}

/// GetChargesForLevel: (current, max) charges for a spell level, from the battle parameter.
std::pair<int, int> charges_for_level(int level)
{
    void* player = g_current_player;
    if (!player) {
        if (void* controller = unity::find_object(kFreqCtl)) {
            if (void* p = ref::rd<void*>(controller, fSelectedPlayer)) {
                player = p;
                g_current_player = p;
            }
        }
    }
    if (!player) return {0, 0};

    // ownedCharacterData.Parameter first; else BattleUnitDataInfo.Parameter (works in battle).
    void* param = nullptr;
    if (void* owned = player_owned_character(player)) {
        void* p = character_parameter(owned);
        if (p && kPlayerParam.is(p)) param = p;
    }
    if (!param) {
        void* p = unit_parameter(player);
        if (p && kPlayerParam.is(p)) param = p;
    }
    if (!param) return {0, 0};

    int current = 0;
    dict_int_int(ref::rd<void*>(param, fCurrentMpCountList), level, current);

    int32_t max = 0;
    if (!vcall(mConfirmedMaxMpCount, param, max, static_cast<int32_t>(level))) {
        int base = 0;   // ConfirmedMaxMpCount failed: use the base list
        if (dict_int_int(ref::rd<void*>(param, fBaseMaxMpCountList), level, base)) max = base;
    }
    return {current, max};
}

/// TryGetAbilityDescription: stripped description, or empty.
std::wstring ability_description(void* ability)
{
    std::wstring mes_id = ref::call_str(mAbilityMesIdDescription, ability);
    if (mes_id.empty()) return std::wstring();
    std::wstring description = message(mes_id, false);
    if (blank(description)) return std::wstring();
    description = strip_icon(description);
    return blank(description) ? std::wstring() : description;
}

/// FormatAbilityAnnouncement: "Spell LvX: MP: x/y. Description".
std::wstring format_ability(void* ability)
{
    std::wstring name;
    std::wstring mes_id = ref::call_str(mAbilityMesIdName, ability);
    if (!mes_id.empty()) name = message(mes_id, false);
    if (name.empty()) return std::wstring();
    name = strip_icon(name);
    if (name.empty()) return std::wstring();

    std::wstring announcement = name;
    int spell_level = 0;
    if (void* data = ref::rd<void*>(ability, fAbilityData)) {
        spell_level = ref::call<int32_t>(mAbilityLv, data);
        if (spell_level > 0 && spell_level <= 8) announcement += L" " + TF(L"Lv{0}", {text::num(spell_level)});
    }

    // Charges, in the out-of-battle wording "MP: current/max".
    if (spell_level > 0 && spell_level <= 8) {
        std::pair<int, int> charges = charges_for_level(spell_level);
        if (charges.second > 0)
            announcement += L": " + TF(L"MP: {0}/{1}", {text::num(charges.first), text::num(charges.second)});
    }

    std::wstring description = ability_description(ability);
    g_last_focused_description = description;
    if (settings::get_bool("AutoDetail", true) && !blank(description)) announcement += L". " + description;
    return announcement;
}

/// TryGetAbilityAnnouncement: the focused spell from dataList. An empty or missing dataList never
/// falls through to contentList (template entries gave false "Empty" lines). The C# "method 3"
/// (a content controller's IsFocus property) never matched: BattleAbilityInfomationContentController
/// has no IsFocus in FF1, so it is not ported.
std::wstring ability_announcement(void* controller, int index, int& count)
{
    count = -1;
    void* list = ref::rd<void*>(controller, fDataList);
    int n = list ? unity::list_count(list) : 0;
    if (list && n > 0) {
        if (index >= 0 && index < n) {
            if (void* ability = unity::list_at(list, index)) {
                std::wstring result = format_ability(ability);
                if (!result.empty()) {
                    count = n;
                    return result;
                }
            }
            // Slot exists but is empty: only announce when already in the magic menu.
            return state_active(menu_state::BATTLE_MAGIC) ? T(L"Empty") : std::wstring();
        }
        if (index >= n) return state_active(menu_state::BATTLE_MAGIC) ? T(L"Empty") : std::wstring();
    }
    return std::wstring();
}

/// AnnounceMagicFocus: the original SetCursor body, run one frame later.
void announce_magic_focus(void* controller, int index)
{
    // Re-validate after the defer (the player may have left for item/target), BEFORE
    // ClearOtherMenuStates below would mask the guard.
    if (!in_magic_context(controller)) return;

    // Refresh the selected player (avoids stale charges when another character's turn begins).
    g_current_player = ref::rd<void*>(controller, fSelectedPlayer);

    // True = the magic list was already the focus (a settle re-fire); a fresh entry speaks.
    bool was_magic_active = state_active(menu_state::BATTLE_MAGIC);

    int count = -1;
    std::wstring announcement = ability_announcement(controller, index, count);
    if (announcement.empty()) return;

    clear_other_menu_states(menu_state::BATTLE_MAGIC);
    state_set(menu_state::BATTLE_MAGIC, true);
    set_target_selection_active(false);

    // "(X of Y)" after the full announcement, description included.
    announcement = ui::menu_position(announcement, index, count);

    if (was_magic_active) {
        if (!dedup::text(kSpellCtx, announcement, dedup::Scope::Session)) return;
    } else {
        dedup::reset(kSpellCtx);
        dedup::text(kSpellCtx, announcement, dedup::Scope::Session);
    }

    notify_command_submenu_active();   // restore the command window + arm the back-out re-announce
    ffpr::say(announcement, true);
}

void announce_current_description()
{
    const std::wstring& desc = g_last_focused_description;
    ffpr::say(blank(desc) ? T(L"No description") : text::trim(desc), true);
}

// SetCursor(Cursor targetCursor, bool isScroll, WithinRangeType type): postfix that schedules a
// one-frame-deferred read (the dataList is briefly the previous character's on a handoff).
using SetCursor_t = void (*)(void* self, void* cursor, bool is_scroll, int32_t type, const void* mi);
SetCursor_t o_SetCursor;

void d_SetCursor(void* self, void* cursor, bool is_scroll, int32_t type, const void* mi)
{
    o_SetCursor(self, cursor, is_scroll, type, mi);
    hooks::guarded("BattleFrequencyAbilityInfomationController.SetCursor", [&] {
        if (!cursor || !self) return;
        // Drop stale cross-menu callbacks while the context is still correct.
        if (!in_magic_context(self)) return;
        int gen = ++g_magic_read_gen;
        int index = ui::cursor_index(cursor);
        void* controller = self;
        frame::next_frame(
            [controller, index, gen] {
                if (gen != g_magic_read_gen) return;   // a newer SetCursor superseded this read
                announce_magic_focus(controller, index);
            },
            "BattleMagic.Read");
    });
}

int install()
{
    actions::define("BattleMagicPatches.AnnounceCurrentDescription", [] { announce_current_description(); });
    return hooks::attach(mSetCursor, &d_SetCursor, &o_SetCursor) ? 1 : 0;
}
core::Install s_install{"Battle.Magic", &install, 110, game::kFF1};

} // namespace

void magic_reset_state()
{
    g_current_player = nullptr;
    g_last_focused_description.clear();
    dedup::reset(kSpellCtx);
}

} // namespace ffpr::battle
