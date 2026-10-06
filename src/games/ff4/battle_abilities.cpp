// Ports FF4 Patches/BattleCommandPatches.cs: BattleQuantityAbilityInfomationController (the battle
// spell / ability list: name, MP cost, description with Auto Detail) and its ShowUseSelect guard
// reset, plus BattleListDetails.LastDescription for the I key.
//
// The core's battle magic list is FF1's (Serial.FF1 classes). FF4's list uses the core's
// BATTLE_MAGIC state, so the core's I-key cascade (BattleMagicMenuState.IsActive ->
// "BattleMagicPatches.AnnounceCurrentDescription") reaches the description defined here; the core
// defines that action only for FF1. The rest of FF4's battle (turns, commands, targets, items,
// actions, damage, statuses, results, pause) is the core's.
#include <string>
#include "ff4.h"
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/hooks.h"
#include "../../core/modtext.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/system/system.h"

namespace ffpr::ff4 {
namespace {

ref::Class  kAbilityInfo{"Serial.FF0.UI.KeyInput", "BattleQuantityAbilityInfomationController", game::kFF4};
ref::Field  fAbilityContentList{kAbilityInfo, "contentList"};
ref::Method mAbilitySelectContent{kAbilityInfo, "SelectContent", 2};
ref::Method mAbilityShowUseSelect{kAbilityInfo, "ShowUseSelect", 4};
ref::Class  kAbilityInfoContent{"Last.UI.KeyInput", "BattleAbilityInfomationContentController", game::kFF4};
ref::Field  fInfoData{kAbilityInfoContent, "<Data>k__BackingField"};

ref::Class  kOwnedAbility{"Last.Data.User", "OwnedAbility", game::kFF4};
ref::Field  fOwnedAbilityAbility{kOwnedAbility, "ability"};
ref::Method mOwnedAbilityMesIdName{kOwnedAbility, "get_MesIdName", 0};
ref::Method mOwnedAbilityMesIdDescription{kOwnedAbility, "get_MesIdDescription", 0};
ref::Class  kAbility{"Last.Data.Master", "Ability", game::kFF4};
ref::Method mAbilityUseValue{kAbility, "get_UseValue", 0};

// AnnouncementContexts.BATTLE_ABILITY_SELECT (DEDUP_INVENTORY.md FF4 row 7): text, as in the C#.
constexpr const char* kCtxAbilitySelect = "BattleAbility.Select";

// BattleListDetails.LastDescription for the ability list (empty: none focused).
std::wstring g_last_description;
bool g_has_description = false;

using Select_t = void (*)(void* self, void* cursor, int32_t type, const void* mi);
Select_t o_SelectContent;
void d_SelectContent(void* self, void* cursor, int32_t type, const void* mi)
{
    o_SelectContent(self, cursor, type, mi);
    hooks::guarded("FF4 BattleQuantityAbilityInfomationController.SelectContent", [&] {
        int index = ui::cursor_index(cursor);
        if (index < 0) return;
        void* list = ref::rd<void*>(self, fAbilityContentList);
        void* content = ui::list_item(list, index);
        if (!content) return;
        void* owned = ref::rd<void*>(content, fInfoData);
        if (!owned) return;

        std::wstring mes_name = ref::call_str(mOwnedAbilityMesIdName, owned);
        std::wstring mes_description = ref::call_str(mOwnedAbilityMesIdDescription, owned);
        if (text::is_blank(mes_name)) return;
        std::wstring name = strip_icon(message(mes_name));
        if (text::is_blank(name)) return;

        std::wstring announcement = name;
        void* ability = ref::rd<void*>(owned, fOwnedAbilityAbility);
        int mp_cost = ability ? ref::call<int32_t>(mAbilityUseValue, ability) : 0;
        if (mp_cost > 0) announcement += L", " + T(L"MP") + L" " + text::num(mp_cost);

        std::wstring description = text::is_blank(mes_description) ? std::wstring() : strip_icon(message(mes_description));
        g_last_description = description;
        g_has_description = !text::is_blank(mes_description);
        if (auto_detail() && !text::is_blank(description)) announcement += L", " + description;

        if (!dedup::text(kCtxAbilitySelect, announcement, dedup::Scope::Battle)) return;

        // The battle magic state, so the I key reads this list's description.
        ffpr::system::clear_other_menu_states("BattleMagic");
        menu_state::set_active(menu_state::BATTLE_MAGIC, true);
        ffpr::say(ui::menu_position(announcement, index, unity::list_count(list)), true);
    });
}

using ShowUseSelect_t = void (*)(void* self, void* player, void* command, void* selected, void* cancel, const void* mi);
ShowUseSelect_t o_ShowUseSelect;
void d_ShowUseSelect(void* self, void* player, void* command, void* selected, void* cancel, const void* mi)
{
    dedup::reset(kCtxAbilitySelect);   // prefix: the list re-reads its first entry
    o_ShowUseSelect(self, player, command, selected, cancel, mi);
}

/// HandleItemDetailsKey's battle branch for the ability list.
void announce_current_description()
{
    if (!g_has_description || text::is_blank(g_last_description)) {
        ffpr::say(T(L"No description"), true);
        return;
    }
    ffpr::say(text::trim(g_last_description), true);
}

int install()
{
    int n = 0;
    if (hooks::attach(mAbilitySelectContent, &d_SelectContent, &o_SelectContent)) ++n;
    if (hooks::attach(mAbilityShowUseSelect, &d_ShowUseSelect, &o_ShowUseSelect)) ++n;
    menu_state::register_reset(menu_state::BATTLE_MAGIC, [] {
        g_last_description.clear();
        g_has_description = false;
    });
    actions::define("BattleMagicPatches.AnnounceCurrentDescription", [] { announce_current_description(); });
    return n;
}

core::Install s_install{"FF4.BattleAbilities", &install, 520, game::kFF4};

} // namespace
} // namespace ffpr::ff4
