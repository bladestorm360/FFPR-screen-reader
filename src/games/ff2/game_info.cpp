// Ports FF2 Core/GameInfoAnnouncer.cs.
//
// AnnounceGilAmount, AnnounceCurrentMap and AnnounceVehicleState are the core's (FF1's) actions
// with the same wording. AnnounceCharacterStatus differs for a game reason: FF2 characters have
// MP, so the H key says "Name: HP a/b, MP c/d, Poison" where FF1 says "Name: a/b HP Poison". This
// file replaces the core's "Mod.AnnounceCharacterStatus" for FF2 (same actor: the core battle
// area's BattleCommandState.CurrentActor).
//
// Also ports FF2 Core/InputManager.AnnounceEquipRestrictions (U key, pad right stick left): FF2
// has no equipment restrictions, so in a shop or the equipment menu it says "Any character can
// equip this" and is silent elsewhere. FF1's UsableByAnnouncer reads job restrictions, which FF2's
// data does not have; "UsableByAnnouncer.AnnounceForCurrentContext" is replaced for FF2.
#include <string>
#include <vector>
#include "../../core/core.h"
#include "../../core/hooks.h"
#include "../../core/il2cpp.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/unity.h"
#include "../../core/battle/battle.h"
#include "../../core/menus/menus.h"

namespace ffpr::ff2 {
namespace {

ref::Class kOwnedCharacter{"Last.Data.User", "OwnedCharacterData", game::kFF2};
ref::Method mCharName{kOwnedCharacter, "get_Name", 0};
ref::Method mCharParameter{kOwnedCharacter, "get_Parameter", 0};
ref::Class kParamBase{"Last.Data", "CharacterParameterBase", game::kFF2};
ref::Method mGetCurrentHP{kParamBase, "get_CurrentHP", 0};
ref::Method mGetCurrentMP{kParamBase, "get_CurrentMP", 0};
ref::Method mConditionList{kParamBase, "get_CurrentConditionList", 0};
ref::Method mConfirmedMaxHp{kParamBase, "ConfirmedMaxHp", 0};
ref::Method mConfirmedMaxMp{kParamBase, "ConfirmedMaxMp", 0};

int virtual_int(ref::Method& m, void* obj)
{
    il2cpp::Method* base = m.get();
    if (!base || !obj) return 0;
    return il2cpp::invoke_int(il2cpp::virtual_method(obj, base), obj, nullptr, 0);
}

/// BattleCommandPatches.BuildStatusSuffix: ", Poison, Blind" (distinct names), or empty.
std::wstring status_suffix(void* param)
{
    std::vector<std::wstring> names;
    void* list = ref::call<void*>(mConditionList, param);
    for (int i = 0, n = unity::list_count(list); i < n; ++i) {
        std::wstring c = menus::condition_name(unity::list_at(list, i));
        if (text::is_blank(c)) continue;
        bool seen = false;
        for (const std::wstring& s : names) seen = seen || s == c;
        if (!seen) names.push_back(c);
    }
    return names.empty() ? std::wstring() : L", " + text::join(names, L", ");
}

void announce_character_status()
{
    if (!queries::is("BattleStateHelper.IsInBattle")) {
        ffpr::say(T(L"Party status only available in battle"), true);
        return;
    }
    void* actor = battle::current_actor();
    if (!actor) {
        ffpr::say(T(L"No active character"), true);
        return;
    }
    void* param = ref::call<void*>(mCharParameter, actor);
    if (!param) {
        ffpr::say(T(L"Character status not available"), true);
        return;
    }
    std::wstring line = TF(L"{0}: HP {1}/{2}, MP {3}/{4}",
                           {ref::call_str(mCharName, actor), text::num(ref::call<int32_t>(mGetCurrentHP, param)),
                            text::num(virtual_int(mConfirmedMaxHp, param)), text::num(ref::call<int32_t>(mGetCurrentMP, param)),
                            text::num(virtual_int(mConfirmedMaxMp, param))});
    line += hooks::guarded_r("FF2.CharacterStatus.Conditions", std::wstring(), [&] { return status_suffix(param); });
    ffpr::say(line, true);
}

void announce_equip_restrictions()
{
    if (queries::is("ShopMenuTracker.ValidateState") || queries::is("EquipMenuState.IsActive"))
        ffpr::say(T(L"Any character can equip this"), true);
}

int install()
{
    actions::define("Mod.AnnounceCharacterStatus", [] { announce_character_status(); });
    actions::define("UsableByAnnouncer.AnnounceForCurrentContext", [] { announce_equip_restrictions(); });
    return 0;
}

// After the core's input (100) and menus (120) areas, so these definitions replace FF1's.
core::Install s_install{"FF2.GameInfo", &install, 540, game::kFF2};

} // namespace
} // namespace ffpr::ff2
