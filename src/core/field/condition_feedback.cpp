// Field poison/venom presentation is a character flash, not a numeric damage popup.
#include "../core.h"
#include "../displayed_ui.h"
#include "../hooks.h"
#include "../frame.h"
#include "../modtext.h"
#include "../say.h"
#include "../battle/battle.h"
#include "../ui/preferences.h"

namespace {
ref::Class kDamage{"Last.Map", "ConditionDamage"};
ref::Method mExecute{kDamage, "Last.Map.IFootMonitor.Excute", 4};
ref::Field fEffect{kDamage, "currentEffect"}, fAnimation{kDamage, "animationController"};
using Execute = bool (*)(void*, void*, void*, int32_t, int32_t, const void*);
Execute o_execute;
bool d_execute(void* self, void* accessor, void* entity, int32_t x, int32_t y, const void* mi)
{
    bool result = o_execute(self, accessor, entity, x, y, mi);
    hooks::guarded("Field.ConditionFlash", [=] {
        // Native Excute runs once for the footstep; ExcuteEffect can run repeatedly
        // while its animation finishes. Describe the producer, not that update loop.
        // EffectData is one int, followed by Nullable.HasValue in every matching dump.
        if (!result || fEffect.get() < 0 || !il2cpp::read_field<bool>(self, fEffect.get() + 4) ||
            !ref::rd<void*>(self, fAnimation) || !displayed::world_visible(entity)) return;
        frame::next_frame([entity] {
            if (ui::prefs::status_damage_enabled() && !ffpr::battle::in_battle() &&
                queries::is("GameStatePatches.IsFieldPlayerState") && displayed::world_visible(entity))
                ffpr::say(T(L"Status damage"), false);
        }, "Field.ConditionFlash");
    });
    return result;
}
int install()
{
    events::on(events::Kind::BattleStart, [] { frame::cancel_label("Field.ConditionFlash"); });
    events::on(events::Kind::SceneLoaded, [] { frame::cancel_label("Field.ConditionFlash"); });
    return hooks::attach(mExecute, &d_execute, &o_execute) ? 1 : 0;
}
core::Install s_install{"Field.ConditionFlash", &install, 115};
}
