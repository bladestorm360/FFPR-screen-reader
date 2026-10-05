// Ports FF1 Patches/BattlePausePatches.cs (BattlePauseState, BattlePausePatches).
//
// The pause menu state is read straight from game memory on demand: BattleUIManager.Instance,
// Initialized, pauseController.isActivePauseMenu. No hook (the C# ApplyPatches is empty; popup
// buttons are read by the popup area's CommonPopup hooks). BattleUIManager.pauseController exists in
// all five games; the controller's class differs (Touch/KeyInput), so isActivePauseMenu is looked
// up on the live object's own class by name.
#include "battle_internal.h"
#include "../core.h"
#include "../il2cpp.h"
#include "../refs.h"

namespace ffpr::battle {
namespace {

ref::Class kBattleUIManager{"Last.UI", "BattleUIManager"};
ref::Field fPauseController{kBattleUIManager, "pauseController"};
ref::Method mInitialized{kBattleUIManager, "get_Initialized", 0};

/// BattlePauseState.IsActive.
bool pause_active()
{
    void* ui = ref::instance(kBattleUIManager);
    if (!ui) return false;
    // Must be initialized (actually in battle): garbage values outside battle gave false positives.
    if (!ref::call<bool>(mInitialized, ui)) return false;
    void* pause = ref::rd<void*>(ui, fPauseController);
    if (!pause) return false;
    int32_t off = il2cpp::field_offset(il2cpp::object_class(pause), "isActivePauseMenu");
    return il2cpp::read_field<bool>(pause, off);
}

int install()
{
    queries::define("BattlePauseState.IsActive", [] { return pause_active(); });
    actions::define("BattlePausePatches.Reset", [] { pause_reset(); });
    return 0;
}
core::Install s_install{"Battle.Pause", &install, 110};

} // namespace

void pause_reset()
{
    // No-op: the state is read directly from game memory (BattlePauseState.Reset).
}

} // namespace ffpr::battle
