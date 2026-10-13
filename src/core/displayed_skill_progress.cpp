#include "displayed_ui.h"
#include "skill_progress.h"
#include "core.h"
#include "hooks.h"
#include "refs.h"
#include "modtext.h"
#include "text.h"
#include "unity.h"
#include "battle/battle_util.h"
#include "ui/preferences.h"
#include <cmath>
#include <map>

namespace displayed {
namespace {
ref::Class kProvider{"Last.UI", "SkillLevelProvider", game::kFF2};
ref::Method mSetting{kProvider, "SettingSkillLevel", 3};
ref::Class kSkill{"Serial.FF2.UI", "SkillLevelContentController", game::kFF2};
ref::Method mColor{kSkill, "SetSkillLevelUpColor", 2};
ref::Field fType{kSkill, "weaponType"};
using Value = skill_progress::Value;
struct Change { Value before, after; };
std::map<void*, Value> g_gauges;
std::map<void*, Change> g_changes;
Value native_progress(int points)
{
    return skill_progress::from_points(points);
}
using Setting = void (*)(int, void*, void*, const void*);
Setting o_setting;
void d_setting(int points, void* level, void* gauge, const void* mi)
{
    o_setting(points, level, gauge, mi);
    hooks::guarded("Displayed.SkillGauge", [=] {
        // Values contain no managed references. Session/scene reset clears keys;
        // reads validate the current content, level and rendered gauge.
        const auto value = native_progress(points);
        if (gauge && value.valid()) g_gauges[gauge] = value;
        else g_gauges.erase(gauge);
    });
}
using Color = void (*)(void*, void*, void*, const void*);
Color o_color;
void d_color(void* self, void* before, void* after, const void* mi)
{
    o_color(self, before, after, mi);
    hooks::guarded("Displayed.SkillChange", [=] {
        g_changes.erase(self);
        const int type = ref::rd<int>(self, fType);
        if (type < 0 || type >= 8) return;
        int before_points = 0, after_points = 0;
        if (!ffpr::battle::dict_int_int(field(before, "<SkillLevelTargets>k__BackingField"), type, before_points) ||
            !ffpr::battle::dict_int_int(field(after, "<SkillLevelTargets>k__BackingField"), type, after_points)) return;
        // User-authorized weapon progress comparison (2026-10-12). Only a matching
        // currently rendered weapon/level/gauge can later disclose this snapshot.
        const auto first = native_progress(before_points), last = native_progress(after_points);
        if (first.valid() && last.valid()) g_changes[self] = {first, last};
    });
}
bool matches(void* content, Value value)
{
    if (!game::is(game::Id::FF2) || !value.valid() || !container_visible(content)) return false;
    void* view = field(content, "view");
    const auto fill = bar_fraction(field(view, "gauge"));
    return text_field(view, "levelText") == text::num(value.level) && fill &&
        std::fabs(*fill - value.fill()) <= 0.00001f;
}
int install()
{
    events::on(events::Kind::SceneLoaded, [] { clear_skill_changes(); });
    return (hooks::attach(mSetting, &d_setting, &o_setting) ? 1 : 0) +
           (hooks::attach(mColor, &d_color, &o_color) ? 1 : 0);
}
core::Install s_install{"Displayed.SkillProgress", &install, 105, game::kFF2};
}
std::optional<int> skill_percentage(void* content)
{
    if (!game::is(game::Id::FF2)) return {};
    void* gauge = field(field(content, "view"), "gauge");
    const auto it = g_gauges.find(gauge);
    return it != g_gauges.end() && matches(content, it->second) ?
        std::optional<int>(it->second.percent()) : std::nullopt;
}
std::wstring skill_change(void* content)
{
    const auto it = g_changes.find(content);
    if (it == g_changes.end() || !matches(content, it->second.after)) return {};
    const auto gain = skill_progress::gain(it->second.before, it->second.after);
    const auto name = skill_name(content);
    if (!gain || *gain <= 0 || name.empty()) return {};
    if (ui::prefs::stat_delta_mode() == 1)
        return TF(L"{0}: gained {1}", {name, text::num(*gain) + L"%"});
    return TF(L"{0}: level {1} at {2}% to level {3} at {4}%, gained {5}%",
        {name, text::num(it->second.before.level), text::num(it->second.before.percent()),
         text::num(it->second.after.level), text::num(it->second.after.percent()), text::num(*gain)});
}
void clear_skill_changes() { g_changes.clear(); g_gauges.clear(); }
}
