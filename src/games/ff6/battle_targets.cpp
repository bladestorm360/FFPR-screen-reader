// Ports FF6 Patches/BattleMessagePatches.cs's target selection (BattleTargetSelectController
// SelectContent player/enemy and SetActiveCursor) by the core: single targets are spoken by
// src/core/battle/battle_command.cpp (FF1's behaviour), and the C#'s index guards have no core
// counterpart. FF6's C# spoke no whole-side line; this file adds FF4/FF5's for FF6, which is ATB
// like them (lead's brief, 2026-10-07), so behaviour matches the other games: "All allies" /
// "All enemies" when the target cursor covers every ally or every enemy, modelled on FF5's
// Patches/BattleTargetPatches.cs port (src/games/ff5/battle_target.cpp): a (mode, index) guard,
// reset when the target window shows or hides, and moved (silently) by the single-target
// SelectContent overloads so that single -> all -> single -> all speaks each switch.
//
// FF6's whole-side states (GameAssembly.dll): PlayerAllInit and EnemyAllInit; the side-attack
// variants SideAttackEnemyAllInit and EnemySideAttackPlayerAllInit are a tail jump into those two,
// so their hooks cover them; SideAttackPlayerAllInit and EnemySideAttackEnemyAllInit have their
// own bodies and are hooked here. AllInit (every unit) is not announced, as in FF4/FF5.
//
// Folded body: BattleTargetSelectController.ShowWindow shares its RVA with 9 other methods in
// FF6 (x10, as in every game): the detour class-checks self. The core chains its own ShowWindow
// and SelectContent subscribers on the same addresses. Every *AllInit RVA is unique.
#include <cstdint>
#include "../../core/core.h"
#include "../../core/hooks.h"
#include "../../core/il2cpp.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/battle/battle_util.h"

namespace ffpr::ff6 {
namespace {

ref::Class  kTargetSelect{"Last.UI.KeyInput", "BattleTargetSelectController", game::kFF6};
ref::Method mPlayerAllInit{kTargetSelect, "PlayerAllInit", 0};
ref::Method mEnemyAllInit{kTargetSelect, "EnemyAllInit", 0};
ref::Method mSideAttackPlayerAllInit{kTargetSelect, "SideAttackPlayerAllInit", 0};
ref::Method mEnemySideAttackEnemyAllInit{kTargetSelect, "EnemySideAttackEnemyAllInit", 0};
ref::Method mShowWindow{kTargetSelect, "ShowWindow", 1};

enum class Mode { None, SinglePlayer, SingleEnemy, AllPlayers, AllEnemies };
Mode g_mode = Mode::None;
int g_index = -1;

/// TargetChanged: true when (mode, index) differs from the last, recording it.
bool target_changed(Mode mode, int index = -1)
{
    if (g_mode == mode && g_index == index) return false;
    g_mode = mode;
    g_index = index;
    return true;
}

void reset_state()
{
    g_mode = Mode::None;
    g_index = -1;
}

void all_players()
{
    hooks::guarded("FF6.BattleTarget.AllPlayers", [] {
        if (target_changed(Mode::AllPlayers)) ffpr::say(T(L"All allies"), true);
    });
}

void all_enemies()
{
    hooks::guarded("FF6.BattleTarget.AllEnemies", [] {
        if (target_changed(Mode::AllEnemies)) ffpr::say(T(L"All enemies"), true);
    });
}

using V_t = void (*)(void* self, const void* mi);
using VB_t = void (*)(void* self, bool b, const void* mi);
using Sel_t = void (*)(void* self, void* list, int32_t index, const void* mi);

V_t o_PlayerAllInit;
void d_PlayerAllInit(void* self, const void* mi)
{
    o_PlayerAllInit(self, mi);
    all_players();
}

V_t o_SideAttackPlayerAllInit;
void d_SideAttackPlayerAllInit(void* self, const void* mi)
{
    o_SideAttackPlayerAllInit(self, mi);
    all_players();
}

V_t o_EnemyAllInit;
void d_EnemyAllInit(void* self, const void* mi)
{
    o_EnemyAllInit(self, mi);
    all_enemies();
}

V_t o_EnemySideAttackEnemyAllInit;
void d_EnemySideAttackEnemyAllInit(void* self, const void* mi)
{
    o_EnemySideAttackEnemyAllInit(self, mi);
    all_enemies();
}

VB_t o_ShowWindow;
void d_ShowWindow(void* self, bool show, const void* mi)
{
    o_ShowWindow(self, show, mi);
    if (self && kTargetSelect.is(self)) reset_state();   // folded body: 10 methods
}

Sel_t o_SelectPlayer;
void d_SelectPlayer(void* self, void* list, int32_t index, const void* mi)
{
    o_SelectPlayer(self, list, index, mi);
    target_changed(Mode::SinglePlayer, index);   // the core speaks it
}

Sel_t o_SelectEnemy;
void d_SelectEnemy(void* self, void* list, int32_t index, const void* mi)
{
    o_SelectEnemy(self, list, index, mi);
    target_changed(Mode::SingleEnemy, index);   // the core speaks it
}

int install()
{
    int n = 0;
    n += hooks::attach(mPlayerAllInit, &d_PlayerAllInit, &o_PlayerAllInit) ? 1 : 0;
    n += hooks::attach(mEnemyAllInit, &d_EnemyAllInit, &o_EnemyAllInit) ? 1 : 0;
    n += hooks::attach(mSideAttackPlayerAllInit, &d_SideAttackPlayerAllInit, &o_SideAttackPlayerAllInit) ? 1 : 0;
    n += hooks::attach(mEnemySideAttackEnemyAllInit, &d_EnemySideAttackEnemyAllInit, &o_EnemySideAttackEnemyAllInit)
        ? 1 : 0;
    n += hooks::attach(mShowWindow, &d_ShowWindow, &o_ShowWindow) ? 1 : 0;
    il2cpp::Class* k = kTargetSelect.get();
    if (il2cpp::Method* m = battle::find_method_param_type(k, "SelectContent", 2, 0, "BattlePlayerData"))
        n += hooks::attach_addr(il2cpp::method_pointer(m), "FF6 BattleTargetSelectController.SelectContent(Player)",
                                reinterpret_cast<void*>(&d_SelectPlayer), reinterpret_cast<void**>(&o_SelectPlayer)) ? 1 : 0;
    if (il2cpp::Method* m = battle::find_method_param_type(k, "SelectContent", 2, 0, "BattleEnemyData"))
        n += hooks::attach_addr(il2cpp::method_pointer(m), "FF6 BattleTargetSelectController.SelectContent(Enemy)",
                                reinterpret_cast<void*>(&d_SelectEnemy), reinterpret_cast<void**>(&o_SelectEnemy)) ? 1 : 0;
    return n;
}

core::Install s_install{"FF6.BattleTargets", &install, 511, game::kFF6};

} // namespace
} // namespace ffpr::ff6
