// Ports FF4 Patches/BattleTargetPatches.cs: "All allies" / "All enemies" when the battle target
// cursor switches to the whole side (BattleTargetSelectController.PlayerAllInit / EnemyAllInit),
// once per switch (lastWasAllPlayers / lastWasAllEnemies, reset when the target window shows or
// hides). Single targets are the core's (src/core/battle/battle_command.cpp), which has no
// whole-side read; it keeps no target guard, so nothing needs resetting here.
//
// Folded body: BattleTargetSelectController.ShowWindow shares its RVA with 9 other methods in
// FF4 (as in every game): the detour class-checks self. The core chains its own ShowWindow
// subscriber on the same address.
#include "ff4.h"
#include "../../core/core.h"
#include "../../core/hooks.h"
#include "../../core/modtext.h"
#include "../../core/say.h"

namespace ffpr::ff4 {
namespace {

ref::Class  kTargetSelect{"Last.UI.KeyInput", "BattleTargetSelectController", game::kFF4};
ref::Method mShowWindow{kTargetSelect, "ShowWindow", 1};
ref::Method mPlayerAllInit{kTargetSelect, "PlayerAllInit", 0};
ref::Method mEnemyAllInit{kTargetSelect, "EnemyAllInit", 0};

// Scattered trackers 8-9 (DEDUP_INVENTORY.md FF4): flags, as in the C#.
bool g_last_was_all_players = false;
bool g_last_was_all_enemies = false;
bool g_target_selection_active = false;   // BattleTargetPatches.IsTargetSelectionActive

using ShowWindow_t = void (*)(void* self, bool is_show, const void* mi);
ShowWindow_t o_ShowWindow;
void d_ShowWindow(void* self, bool is_show, const void* mi)
{
    o_ShowWindow(self, is_show, mi);
    hooks::guarded("FF4 BattleTargetSelectController.ShowWindow", [&] {
        if (!kTargetSelect.is(self)) return;   // folded body
        g_target_selection_active = is_show;
        g_last_was_all_players = false;
        g_last_was_all_enemies = false;
    });
}

using Void_t = void (*)(void* self, const void* mi);
Void_t o_PlayerAllInit;
void d_PlayerAllInit(void* self, const void* mi)
{
    o_PlayerAllInit(self, mi);
    hooks::guarded("FF4 BattleTargetSelectController.PlayerAllInit", [&] {
        if (g_last_was_all_players) return;
        g_last_was_all_players = true;
        g_last_was_all_enemies = false;
        ffpr::say(T(L"All allies"), true);
    });
}

Void_t o_EnemyAllInit;
void d_EnemyAllInit(void* self, const void* mi)
{
    o_EnemyAllInit(self, mi);
    hooks::guarded("FF4 BattleTargetSelectController.EnemyAllInit", [&] {
        if (g_last_was_all_enemies) return;
        g_last_was_all_enemies = true;
        g_last_was_all_players = false;
        ffpr::say(T(L"All enemies"), true);
    });
}

int install()
{
    int n = 0;
    if (hooks::attach(mShowWindow, &d_ShowWindow, &o_ShowWindow)) ++n;
    if (hooks::attach(mPlayerAllInit, &d_PlayerAllInit, &o_PlayerAllInit)) ++n;
    if (hooks::attach(mEnemyAllInit, &d_EnemyAllInit, &o_EnemyAllInit)) ++n;
    queries::define("BattleTargetPatches.IsTargetSelectionActive", [] { return g_target_selection_active; });
    return n;
}

core::Install s_install{"FF4.BattleTargets", &install, 521, game::kFF4};

} // namespace
} // namespace ffpr::ff4
