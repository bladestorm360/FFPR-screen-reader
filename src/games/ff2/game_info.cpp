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

void announce_equip_restrictions()
{
    if (queries::is("ShopMenuTracker.ValidateState") || queries::is("EquipMenuState.IsActive"))
        ffpr::say(T(L"Any character can equip this"), true);
}

int install()
{
    actions::define("UsableByAnnouncer.AnnounceForCurrentContext", [] { announce_equip_restrictions(); });
    return 0;
}

// After the core's input (100) and menus (120) areas, so these definitions replace FF1's.
core::Install s_install{"FF2.GameInfo", &install, 540, game::kFF2};

} // namespace
} // namespace ffpr::ff2
