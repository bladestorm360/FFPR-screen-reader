// Ports FF2 Patches/BattleMagicPatches.cs: the battle spell list with FF2's spell skill levels
// ("Fire lv3, 45 percent: description (2 of 8)") and the I-key description.
//
// The core's battle spell list is FF1's (Serial.FF1 BattleFrequencyAbilityInfomationController,
// charges); FF2's list is Serial.FF2.UI.KeyInput.BattleQuantityAbilityInfomationController.
// SelectContent(Cursor, WithinRangeType) is private, one RVA in FF2's dump.cs.
//
// Not ported: the C#'s Touch variant. Serial.FF2.UI.Touch.BattleQuantityAbilityInfomationController
// has no dataList / contentList fields (the C# read the KeyInput offsets on it), so it never read
// a spell.
//
// State: BATTLE_MAGIC (the core publishes "BattleMagicMenuState.IsActive" from it; its
// ShouldSuppress falls back to that flag, as FF1's did). The C# also re-armed the battle command
// back-out re-announce (BattleCommandPatches.NotifyCommandSubmenuActive); the core keeps that
// internal, so it runs here only once the core publishes it as an action (reported).
#include <cstdint>
#include <string>
#include "ff2.h"
#include "../../core/core.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/menus/menus.h"
#include "../../core/ui/preferences.h"

namespace ffpr::ff2 {
namespace {

ref::Class kQuantityCtl{"Serial.FF2.UI.KeyInput", "BattleQuantityAbilityInfomationController", game::kFF2};
ref::Field fDataList{kQuantityCtl, "dataList"};
ref::Field fContentList{kQuantityCtl, "contentList"};
ref::Method mSelectContent{kQuantityCtl, "SelectContent", 2};
ref::Class kCtlBase{"Serial.Template.UI.KeyInput", "BattleAbilityInfomationControllerBase", game::kFF2};
ref::Field fSelectedPlayer{kCtlBase, "selectedBattlePlayerData"};

ref::Class kBattlePlayerData{"", "BattlePlayerData", game::kFF2};
ref::Field fOwnedCharacterData{kBattlePlayerData, "<ownedCharacterData>k__BackingField"};

ref::Class kOwnedCharacter{"Last.Data.User", "OwnedCharacterData", game::kFF2};
ref::Method mCharAbilityList{kOwnedCharacter, "get_OwnedAbilityList", 0};
ref::Class kOwnedAbility{"Last.Data.User", "OwnedAbility", game::kFF2};
ref::Method mOwnedAbilityAbility{kOwnedAbility, "get_Ability", 0};
ref::Method mOwnedAbilitySkillLevel{kOwnedAbility, "get_SkillLevel", 0};
ref::Class kAbility{"Last.Data.Master", "Ability", game::kFF2};
ref::Method mAbilityId{kAbility, "get_Id", 0};
ref::Class kExpUtility{"Last.Systems", "ExpUtility", game::kFF2};
ref::Method mGetExpLevel{kExpUtility, "GetExpLevel", 3};

ref::Class kAbilityInfoContent{"Last.UI.KeyInput", "BattleAbilityInfomationContentController", game::kFF2};
ref::Field fAbilityInfoGauge{kAbilityInfoContent, "commonGauge"};
ref::Class kCommonGauge{"Last.UI", "CommonGauge", game::kFF2};
ref::Field fGaugeImage{kCommonGauge, "gaugeImage"};
ref::Class kImage{"UnityEngine.UI", "Image", game::kFF2};
ref::Method mImageFillAmount{kImage, "get_fillAmount", 0};

// MenuDetailCache for the battle list: the focused spell's description.
std::wstring g_last_description;

int ability_id(void* owned_ability)
{
    void* ability = owned_ability ? ref::call<void*>(mOwnedAbilityAbility, owned_ability) : nullptr;
    return ability ? ref::call<int32_t>(mAbilityId, ability) : -1;
}

float gauge_fill(void* controller, int index)
{
    void* list = ref::rd<void*>(controller, fContentList);
    if (!list || index < 0 || index >= unity::list_count(list)) return -1.0f;
    void* content = unity::list_at(list, index);
    void* gauge = content ? ref::rd<void*>(content, fAbilityInfoGauge) : nullptr;
    void* image = gauge ? ref::rd<void*>(gauge, fGaugeImage) : nullptr;
    if (!image || !mImageFillAmount) return -1.0f;
    return ref::call<float>(mImageFillAmount, image);
}

/// The spell level from the character's own OwnedAbility exp (the list entry's as a fallback).
int spell_level(void* ability, void* character)
{
    return hooks::guarded_r("FF2.BattleMagic.Level", 1, [&] {
        int raw = 0;
        const int id = ability_id(ability);
        if (character && id >= 0) {
            void* owned = ref::call<void*>(mCharAbilityList, character);
            for (int i = 0, n = unity::list_count(owned); i < n; ++i) {
                void* a = unity::list_at(owned, i);
                if (a && ability_id(a) == id) {
                    raw = ref::call<int32_t>(mOwnedAbilitySkillLevel, a);
                    break;
                }
            }
        }
        if (raw <= 0) raw = ref::call<int32_t>(mOwnedAbilitySkillLevel, ability);
        int level = ref::call_static<int32_t>(mGetExpLevel, static_cast<int32_t>(1), static_cast<int32_t>(raw),
                                              static_cast<int32_t>(1));
        return level < 1 ? 1 : (level > 16 ? 16 : level);
    });
}

/// FormatAbilityAnnouncement.
std::wstring format_ability(void* ability, void* character, float gauge)
{
    std::wstring name = menus::spell_name(ability);
    if (name.empty()) return {};
    std::wstring announcement = TF(L"{0} lv{1}", {name, text::num(spell_level(ability, character))});
    if (gauge >= 0.0f) {
        int pct = static_cast<int>(gauge * 100);
        pct = pct < 0 ? 0 : (pct > 99 ? 99 : pct);
        announcement = TF(L"{0}, {1} percent", {announcement, text::num(pct)});
    }
    const std::wstring description = menus::spell_description(ability);
    g_last_description = description;
    if (ui::prefs::auto_detail_enabled() && !text::is_blank(description)) announcement += L": " + description;
    return announcement;
}

/// TryGetAbilityAnnouncement.
std::wstring ability_announcement(void* controller, int index, int& count)
{
    count = -1;
    void* list = ref::rd<void*>(controller, fDataList);
    if (!list || index < 0 || index >= unity::list_count(list)) return {};
    count = unity::list_count(list);
    void* ability = unity::list_at(list, index);
    if (!ability) return T(L"Empty");
    void* player = ref::rd<void*>(controller, fSelectedPlayer);
    void* character = player ? ref::rd<void*>(player, fOwnedCharacterData) : nullptr;
    const float gauge = hooks::guarded_r("FF2.BattleMagic.Gauge", -1.0f, [&] { return gauge_fill(controller, index); });
    return format_ability(ability, character, gauge);
}

using SelectContent_t = void (*)(void* self, void* cursor, int32_t type, const void* mi);
SelectContent_t o_SelectContent;

void d_SelectContent(void* self, void* cursor, int32_t type, const void* mi)
{
    o_SelectContent(self, cursor, type, mi);
    hooks::guarded("FF2.BattleMagic.SelectContent", [&] {
        if (!self || !cursor) return;
        const int index = ui::cursor_index(cursor);
        int count = -1;
        std::wstring announcement = ability_announcement(self, index, count);
        if (announcement.empty()) return;
        set_active_exclusive(menu_state::BATTLE_MAGIC);
        if (actions::defined("BattleCommandPatches.NotifyCommandSubmenuActive"))
            actions::run("BattleCommandPatches.NotifyCommandSubmenuActive");
        ffpr::say(ui::menu_position(announcement, index, count), true);
    });
}

int install()
{
    // The I key in the battle spell list (core cascade: BattleMagicMenuState.IsActive).
    actions::define("BattleMagicPatches.AnnounceCurrentDescription", [] {
        ffpr::say(text::is_blank(g_last_description) ? T(L"No description") : text::trim(g_last_description), true);
    });
    return hooks::attach(mSelectContent, &d_SelectContent, &o_SelectContent) ? 1 : 0;
}

core::Install s_install{"FF2.BattleMagic", &install, 530, game::kFF2};

} // namespace
} // namespace ffpr::ff2
