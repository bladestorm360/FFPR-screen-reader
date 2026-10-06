// Ports the part of FF5 Patches/BattleTargetPatches.cs the core battle area does not cover:
// "All allies" / "All enemies" when the target cursor covers every ally (PlayerAllInit) or every
// enemy (EnemyAllInit), with the C#'s (mode, index) guard for the two all-target modes.
//
// The single-target readers (SelectContent per overload, PlayerInit's first ally, ShowWindow) are
// the core's (src/core/battle/battle_command.cpp, FF1's behaviour). FF5's own versions of them are
// NOT ported: the core installs its SelectContent hooks with hooks::attach_addr, which
// hooks::disable cannot switch off, so a second reader would speak every target twice.
// Not ported for that reason: enemy letters (setting "EnemyLetters"), FF5's player line
// ("Name: HP a/b. MP c/d" + the FF5 status whitelist) and enemy line (HP per EnemyHPDisplay +
// statuses + letter suffix).
//
// This file also chains on both SelectContent overloads and ShowWindow, only to move the guard
// (no speech): a single-target move between two all-target selections re-arms the "all" line.
// ShowWindow's body is shared by 10 methods in FF5: that detour class-checks self.
#include <cstdint>
#include "ff5_internal.h"
#include "../../core/core.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/battle/battle_util.h"

namespace ffpr::ff5 {
namespace {

ref::Class  kTargetSelect{"Last.UI.KeyInput", "BattleTargetSelectController", game::kFF5};
ref::Method mPlayerAllInit{kTargetSelect, "PlayerAllInit", 0};
ref::Method mEnemyAllInit{kTargetSelect, "EnemyAllInit", 0};
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

using V_t = void (*)(void* self, const void* mi);
using VB_t = void (*)(void* self, bool b, const void* mi);
using Sel_t = void (*)(void* self, void* list, int32_t index, const void* mi);

V_t o_PlayerAllInit;
void d_PlayerAllInit(void* self, const void* mi)
{
    o_PlayerAllInit(self, mi);
    hooks::guarded("FF5.BattleTarget.PlayerAllInit", [] {
        if (target_changed(Mode::AllPlayers)) ffpr::say(T(L"All allies"));
    });
}

V_t o_EnemyAllInit;
void d_EnemyAllInit(void* self, const void* mi)
{
    o_EnemyAllInit(self, mi);
    hooks::guarded("FF5.BattleTarget.EnemyAllInit", [] {
        if (target_changed(Mode::AllEnemies)) ffpr::say(T(L"All enemies"));
    });
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
    n += hooks::attach(mShowWindow, &d_ShowWindow, &o_ShowWindow) ? 1 : 0;
    il2cpp::Class* k = kTargetSelect.get();
    if (il2cpp::Method* m = battle::find_method_param_type(k, "SelectContent", 2, 0, "BattlePlayerData"))
        n += hooks::attach_addr(il2cpp::method_pointer(m), "FF5 BattleTargetSelectController.SelectContent(Player)",
                                reinterpret_cast<void*>(&d_SelectPlayer), reinterpret_cast<void**>(&o_SelectPlayer)) ? 1 : 0;
    if (il2cpp::Method* m = battle::find_method_param_type(k, "SelectContent", 2, 0, "BattleEnemyData"))
        n += hooks::attach_addr(il2cpp::method_pointer(m), "FF5 BattleTargetSelectController.SelectContent(Enemy)",
                                reinterpret_cast<void*>(&d_SelectEnemy), reinterpret_cast<void**>(&o_SelectEnemy)) ? 1 : 0;
    // (The C# also reset on the BattleTarget menu-state reset; registering one here would replace
    // the core's handler for that key, so ShowWindow's open/close reset carries it alone.)
    return n;
}

core::Install s_install{"FF5.BattleTarget", &install, 540, game::kFF5};

} // namespace
} // namespace ffpr::ff5
