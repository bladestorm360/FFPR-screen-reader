// Ports FF1 Patches/BattleStartPatches.cs.
//
// Battle-start lifecycle hook only. The start condition ("Preemptive strike!", "Back attack!") is
// the game's own message and is spoken by the BattleCommandMessageController.SetMessage postfix
// (battle_message.cpp); nothing is synthesized here (that produced a duplicate in the C# mod).
// BattleController.StartPreeMptiveMes exists in all six games, unique RVA in each.
#include "battle_internal.h"
#include "battle_message_line.h"
#include "../core.h"
#include "../hooks.h"

namespace ffpr::battle {
namespace {

ref::Class kBattleController{"Last.Battle", "BattleController"};
ref::Method mStartPreeMptiveMes{kBattleController, "StartPreeMptiveMes", 0};
ref::Method mStartInit{kBattleController, "StartInit", 0};
ref::Method mCreateObjects{kBattleController, "StartCreateObj", 0};
ref::Method mStartInBattle{kBattleController, "StartInBattle", 0};
ref::Class kEncounter{"Last.InGame.Effects", "EncountEffectManager"};
ref::Method mEncounter{kEncounter, "SetAnimation", 6};

using Void_t = void (*)(void* self, const void* mi);
Void_t o_StartPreeMptiveMes;
Void_t o_stage[3];
constexpr const char* kStages[] = {"Battle initialize", "Battle create objects", "Battle entrance and BGM"};

template<int N>
void d_stage(void* self, const void* mi)
{
    timing::mark(kStages[N], 0);
    { timing::Scope native(kStages[N], timing::Kind::Original); o_stage[N](self, mi); }
    timing::mark(kStages[N], 1);
}

using Encounter = void (*)(void*, void*, void*, int32_t, bool, bool, void*, const void*);
Encounter o_encounter;
void d_encounter(void* self, void* entity, void* texture, int32_t effect,
                 bool visible, bool map_bgm, void* callback, const void* mi)
{
    // Rare, audited native entry only. No probe of every engine/runtime call.
    timing::mark("Encounter animation begin", effect);
    { timing::Scope native("Encounter animation setup", timing::Kind::Original);
      o_encounter(self, entity, texture, effect, visible, map_bgm, callback, mi); }
    timing::mark("Encounter animation ready", effect);
}

void d_StartPreeMptiveMes(void* self, const void* mi)
{
    timing::mark("Battle opening begin");
    { message_line::OpeningPresentation opening;
      timing::Scope native("Battle opening presentation", timing::Kind::Original);
      o_StartPreeMptiveMes(self, mi); }
    hooks::guarded("BattleController.StartPreeMptiveMes", [&] {
        // Mark that we're in battle (enables state clearing on battle end); fires BattleStart.
        on_battle_start();
    });
    timing::mark("Battle opening ready");
}

int install()
{
    int n = hooks::attach(mStartPreeMptiveMes, &d_StartPreeMptiveMes, &o_StartPreeMptiveMes) ? 1 : 0;
    n += hooks::attach(mStartInit, &d_stage<0>, &o_stage[0]) ? 1 : 0;
    n += hooks::attach(mCreateObjects, &d_stage<1>, &o_stage[1]) ? 1 : 0;
    n += hooks::attach(mStartInBattle, &d_stage<2>, &o_stage[2]) ? 1 : 0;
    n += hooks::attach(mEncounter, &d_encounter, &o_encounter) ? 1 : 0;
    return n;
}
core::Install s_install{"Battle.Start", &install, 110};

} // namespace
} // namespace ffpr::battle
