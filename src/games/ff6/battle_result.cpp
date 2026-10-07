// Ports FF6 Patches/BattleResultPatches.cs (ResultMenuController.Show / ShowPointsInit): the
// victory-screen information FF6 has and FF1 does not - the espers' Magic AP ("{0} Magic AP",
// BattleResultData.GetMp: FF6 keeps Magic AP there, not in GetAbp) and the spells learned from
// them ("{0} learned {1}", each id of the character's LearningList found in its OwnedAbilityList
// and named by MessageManager).
//
// The victory screen itself stays the core's (src/core/battle/battle_result.cpp; FF6's
// ResultMenuController is FF1's, member for member): gil, EXP per character, the EXP counter tone
// and its monitor, item drops at ShowGetItemsInit, and level-ups with stat gains at
// ShowStatusUpInit, in FF1's wording. This file registers two battle::ResultExtras callbacks, so
// the core speaks Magic AP right after the gil line and each character's learned spells after
// that character's EXP line, in the C#'s order and inside the core's once-per-result guard. No
// hook of its own.
//
// Not ported (the core's wording and phases instead): the C#'s single combined line, its "{0} gil",
// "{0} x{1}" and " and leveled up to level {0}" pieces, its EXP line for a character with 0 EXP,
// the duplicate Show announcement and its same-battle text check (the core speaks once per result
// screen).
#include <cstdint>
#include <string>
#include <vector>
#include "../../core/core.h"
#include "../../core/game.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/text.h"
#include "../../core/unity.h"
#include "../../core/battle/battle.h"
#include "../../core/battle/battle_util.h"

namespace ffpr::ff6 {
namespace {

constexpr uint8_t k6 = game::kFF6;

ref::Class  kResultData{"Last.Data", "BattleResultData", k6};
ref::Field  fGetMp{kResultData, "<GetMp>k__BackingField"};
ref::Class  kResultChar{"Last.Data", "BattleResultData.BattleResultCharacterData", k6};
ref::Field  fAfterData{kResultChar, "<AfterData>k__BackingField"};
ref::Field  fLearningList{kResultChar, "<LearningList>k__BackingField"};   // List<int> ability ids
ref::Class  kOwnedChar{"Last.Data.User", "OwnedCharacterData", k6};
ref::Field  fOwnedAbilityList{kOwnedChar, "<OwnedAbilityList>k__BackingField"};
ref::Class  kOwnedAbility{"Last.Data.User", "OwnedAbility", k6};
ref::Field  fAbility{kOwnedAbility, "ability"};                           // OwnedAbility.Ability
ref::Method mOwnedAbilityMesIdName{kOwnedAbility, "get_MesIdName", 0};
ref::Class  kAbility{"Last.Data.Master", "Ability", k6};
ref::Method mAbilityId{kAbility, "get_Id", 0};

/// "{0} Magic AP" when the battle gave any.
std::vector<std::wstring> magic_ap(void* result_data)
{
    if (!result_data) return {};
    const int magic_ap = ref::rd<int32_t>(result_data, fGetMp);
    if (magic_ap <= 0) return {};
    return {TF(L"{0} Magic AP", {text::num(magic_ap)})};
}

/// The character's OwnedAbility whose Ability.Id is `id`, or null.
void* owned_ability(void* owned_list, int id)
{
    for (void* owned : battle::list_items(owned_list)) {
        void* ability = owned ? ref::rd<void*>(owned, fAbility) : nullptr;
        if (ability && ref::call<int32_t>(mAbilityId, ability) == id) return owned;
    }
    return nullptr;
}

/// "{0} learned {1}" for each ability in the character's LearningList.
std::vector<std::wstring> learned(void* char_result)
{
    std::vector<std::wstring> out;
    if (!char_result) return out;
    void* learning = ref::rd<void*>(char_result, fLearningList);
    const int count = learning ? unity::list_count(learning) : 0;
    if (count <= 0) return out;
    void* after = ref::rd<void*>(char_result, fAfterData);
    void* owned_list = after ? ref::rd<void*>(after, fOwnedAbilityList) : nullptr;
    if (!owned_list) return out;
    const std::wstring name = battle::character_name(after);
    for (int i = 0; i < count; ++i) {
        void* owned = owned_ability(owned_list, unity::list_value<int32_t>(learning, i));
        if (!owned) continue;
        const std::wstring ability_name = battle::message(ref::call_str(mOwnedAbilityMesIdName, owned));
        if (!battle::blank(ability_name)) out.push_back(TF(L"{0} learned {1}", {name, ability_name}));
    }
    return out;
}

int install()
{
    battle::ResultExtras extras;
    extras.after_gil = &magic_ap;
    extras.after_character = &learned;
    battle::set_result_extras(extras);
    return 0;
}

core::Install s_install{"FF6.BattleResult", &install, 500, game::kFF6};

} // namespace
} // namespace ffpr::ff6
