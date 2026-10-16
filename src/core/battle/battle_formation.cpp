// The native enemy-panel producer groups the enemies actually in this encounter.
// Capture its assigned, active rows, including names/counts during the opening fade
// (explicit user-approved formation disclosure). No preset's future/hidden spawns,
// master statistics, scene search, polling, native enumeration or worker pointers.
#include "battle_internal.h"
#include "battle.h"
#include "../core.h"
#include "../displayed_ui.h"
#include "../hooks.h"
#include "../modtext.h"
#include "../say.h"
#include "../unity.h"
#include <utility>
#include <vector>

namespace ffpr::battle {
namespace {
ref::Class kEnemies{"Last.UI.KeyInput", "BattleEnemyInfomationController"};
ref::Field fContents{kEnemies, "contentList"};
ref::Method mUpdate{kEnemies, "UpdateView", 2};
bool g_ready = false, g_announced = false, g_first_turn = false, g_captured = false;
std::vector<std::wstring> g_rows;

void publish()
{
    if (!in_battle() || !g_ready || g_announced || !g_captured || g_rows.empty()) return;
    g_announced = true;
    g_first_turn = true;
    // Cleaning, punctuation joining, chunking and backend work stay on speech's
    // existing worker. Opening condition messages already queue on this chain.
    auto parts = std::move(g_rows);
    parts.insert(parts.begin(), T(L"Enemy formation"));
    ffpr::say_parts(std::move(parts), false);
    timing::mark("Battle formation submitted");
}

using Update = void (*)(void*, void*, void*, const void*);
Update o_update;
void d_update(void* self, void* enemies, void* initial_enemies, const void* mi)
{
    o_update(self, enemies, initial_enemies, mi);
    if (!in_battle() || g_announced) return;
    hooks::guarded("Battle opening enemy rows", [self] {
        if (!kEnemies.is(self)) return;
        g_captured = false;
        void* list = ref::rd<void*>(self, fContents);
        const int count = unity::list_count(list);
        // Pools can reserve more entries than the preset's nine enemy slots.
        // Bound pool inspection separately from the active formation size.
        if (count <= 0 || count > 64) return;
        std::vector<std::wstring> rows;
        rows.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
            void* row = unity::list_at(list, i);
            if (!unity::alive(row) || !unity::active_self(row)) continue;
            if (rows.size() == 9) return; // never publish a truncated roster
            void* view = displayed::field(row, "view");
            // The native producer activates each used row; its parent canvas can
            // still be fading. Read only the text it just assigned, not old pools.
            auto name = unity::ui_text(displayed::field(view, "nameText"));
            if (name.empty()) return; // incomplete producer; a later native update retries
            auto amount = unity::ui_text(displayed::field(view, "countText"));
            if (name.size() > 256 || amount.size() > 16) return;
            if (amount.empty()) return;
            rows.push_back(amount + L" " + name);
        }
        if (rows.empty()) return;
        g_rows = std::move(rows);
        g_captured = true;
        publish();
    });
}

int install()
{
    events::on(events::Kind::BattleEntering, [] {
        // Constant-time only. Retire old owned strings after the native entrance.
        g_ready = g_announced = g_first_turn = g_captured = false;
    });
    events::on(events::Kind::BattleStart, [] {
        g_ready = true;
        publish();
    });
    events::on(events::Kind::BattleEnd, [] {
        g_ready = g_announced = g_first_turn = g_captured = false;
        g_rows.clear();
    });
    return hooks::attach(mUpdate, &d_update, &o_update) ? 1 : 0;
}
core::Install s_install{"Battle.Formation", &install, 110};
}

bool formation_first_turn()
{
    return std::exchange(g_first_turn, false);
}
}
