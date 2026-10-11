// Field item/spell controllers refresh the target BEFORE starting its use animation.
// Arm at their coroutine factory, then read the native refreshed, displayed panel.
#include "../core.h"
#include "../displayed_ui.h"
#include "../hooks.h"
#include "../frame.h"
#include "../say.h"
#include "../modtext.h"
#include "../unity.h"
#include "../battle/battle.h"
#include <algorithm>
#include <map>

namespace {
ref::Class kTarget{"Last.UI.KeyInput", "ItemTargetSelectContentController"};
ref::Class kItem{"Last.UI.KeyInput", "ItemUseController"};
ref::Method mItemPlay{kItem, "PlayUseAnimation", 3}, mUpdate{kTarget, "UpdateView", 1};
struct Before { unsigned generation; std::vector<std::wstring> icons; };
std::map<void*, Before> g_pending;
unsigned g_generation = 0;
void arm(void* content)
{
    hooks::guarded("UseFeedback.Begin", [=] {
        if (ffpr::battle::in_battle() || !displayed::visible(content)) return;
        const unsigned generation = ++g_generation;
        g_pending[content] = {generation, displayed::condition_lines(content)};
        frame::after_frames(180, [content, generation] {
            auto it = g_pending.find(content);
            if (it != g_pending.end() && it->second.generation == generation) g_pending.erase(it);
        }, "UseFeedback.Expiry");
    });
}
using ItemPlay = void* (*)(void*, void*, bool, void*, const void*);
ItemPlay o_item;
void* d_item(void* self, void* content, bool revive, void* end, const void* mi)
{
    arm(content);
    return o_item(self, content, revive, end, mi);
}
using AbilityPlay = void* (*)(void*, void*, bool, int, const void*);
#define ABILITY_USE(N, NS, MASK) \
ref::Class kAbility##N{NS, "AbilityUseContentListController", MASK}; \
ref::Method mAbility##N{kAbility##N, "PlayUseAnimation", 3}; \
AbilityPlay o_ability##N; \
void* d_ability##N(void* self, void* content, bool revive, int id, const void* mi) \
{ arm(content); return o_ability##N(self, content, revive, id, mi); }
ABILITY_USE(1, "Serial.FF1.UI.KeyInput", game::kFF1)
ABILITY_USE(2, "Serial.FF2.UI.KeyInput", game::kFF2)
ABILITY_USE(3, "Serial.FF3.UI.KeyInput", game::kFF3)
ABILITY_USE(4, "Serial.FF4.UI.KeyInput", game::kFF4)
ABILITY_USE(5, "Serial.FF5.UI.KeyInput", game::kFF5)
ABILITY_USE(6, "Serial.FF6.UI.KeyInput", game::kFF6)
#undef ABILITY_USE
using Update = void (*)(void*, void*, const void*);
Update o_update;
void d_update(void* self, void* data, const void* mi)
{
    o_update(self, data, mi);
    hooks::guarded("UseFeedback.Display", [=] {
        auto it = g_pending.find(self);
        if (it == g_pending.end()) return;
        auto previous = std::move(it->second.icons);
        g_pending.erase(it);
        frame::every_frame([self, previous] {
            if (ffpr::battle::in_battle() || !unity::alive(self)) return false;
            auto line = displayed::target(self);
            if (line.empty()) return true;
            const auto current = displayed::condition_lines(self);
            // These are changes to actual drawn icons, not hidden condition parameters
            // or invented numeric healing deltas. The refreshed native values follow.
            for (const auto& name : previous)
                if (std::find(current.begin(), current.end(), name) == current.end())
                    line += L", " + TF(L"{0} removed", {name});
            ffpr::say(line, false);
            return false;
        }, "UseFeedback.Display", 30);
    });
}
int install()
{
    events::on(events::Kind::BattleStart, [] {
        g_pending.clear(); frame::cancel_label("UseFeedback.");
    });
    events::on(events::Kind::SceneLoaded, [] {
        g_pending.clear(); frame::cancel_label("UseFeedback.");
    });
    int n = hooks::attach(mItemPlay, &d_item, &o_item) ? 1 : 0;
#define ATTACH_ABILITY(N) if (mAbility##N.cls.applies()) n += hooks::attach(mAbility##N, &d_ability##N, &o_ability##N) ? 1 : 0;
    ATTACH_ABILITY(1) ATTACH_ABILITY(2) ATTACH_ABILITY(3)
    ATTACH_ABILITY(4) ATTACH_ABILITY(5) ATTACH_ABILITY(6)
#undef ATTACH_ABILITY
    return n + (hooks::attach(mUpdate, &d_update, &o_update) ? 1 : 0);
}
core::Install s_install{"Menus.UseFeedback", &install, 125};
}
