// Ports FF1 Patches/BattleStartPatches.cs.
//
// Battle-start lifecycle hook only. The start condition ("Preemptive strike!", "Back attack!") is
// the game's own message and is spoken by the BattleCommandMessageController.SetMessage postfix
// (battle_message.cpp); nothing is synthesized here (that produced a duplicate in the C# mod).
// BattleController.StartPreeMptiveMes exists in all five games, unique RVA in each.
#include "battle_internal.h"
#include "../core.h"
#include "../hooks.h"

namespace ffpr::battle {
namespace {

ref::Class kBattleController{"Last.Battle", "BattleController"};
ref::Method mStartPreeMptiveMes{kBattleController, "StartPreeMptiveMes", 0};

using Void_t = void (*)(void* self, const void* mi);
Void_t o_StartPreeMptiveMes;

void d_StartPreeMptiveMes(void* self, const void* mi)
{
    o_StartPreeMptiveMes(self, mi);
    hooks::guarded("BattleController.StartPreeMptiveMes", [&] {
        // Mark that we're in battle (enables state clearing on battle end); fires BattleStart.
        on_battle_start();
        // Reset the battle-result state for this new battle.
        result_reset_state();
    });
}

int install()
{
    return hooks::attach(mStartPreeMptiveMes, &d_StartPreeMptiveMes, &o_StartPreeMptiveMes) ? 1 : 0;
}
core::Install s_install{"Battle.Start", &install, 110};

} // namespace
} // namespace ffpr::battle
