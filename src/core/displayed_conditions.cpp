#include "displayed_ui.h"
#include "core.h"
#include "hooks.h"
#include "refs.h"
#include "unity.h"
#include "text.h"
#include "modtext.h"
#include "menus/menus.h"
#include <map>

namespace displayed {
namespace {
ref::Class kIcons{"Last.UI", "ConditionIconController"};
ref::Method mUpdate{kIcons, "UpdateView", 1}, mIcon{kIcons, "GetIcon", 1};
ref::Class kSkill{"Serial.FF2.UI", "SkillLevelContentController", game::kFF2};
ref::Method mWeapon{kSkill, "SetWeaponIcon", 0}, mAbility{kSkill, "SetAbilityIcon", 1};
ref::Class kImage{"UnityEngine.UI", "Image"};
ref::Method mSprite{kImage, "get_overrideSprite", 0};
struct Icon { void* sprite; std::wstring description; };
std::map<void*, std::vector<Icon>> g_icons;
std::map<void*, Icon> g_weapons;
void* g_updating = nullptr;
using GetIcon = void* (*)(void*, void*, const void*);
GetIcon o_icon;
void* d_icon(void* self, void* condition, const void* mi)
{
    void* sprite = o_icon(self, condition, mi);
    hooks::guarded("Displayed.ConditionIcon", [=] {
        if (self != g_updating || !unity::alive(sprite)) return;
        auto name = ffpr::menus::condition_name(condition);
        if (!name.empty()) g_icons[self].push_back({sprite, std::move(name)});
    });
    return sprite;
}
using Update = void (*)(void*, void*, const void*);
Update o_update;
void d_update(void* self, void* values, const void* mi)
{
    hooks::guarded("Displayed.ConditionIcons.Begin", [=] {
        for (auto it = g_icons.begin(); it != g_icons.end();)
            if (!unity::alive(it->first)) it = g_icons.erase(it); else ++it;
        g_icons[self].clear();
    });
    struct Scope {
        void* previous = g_updating;
        ~Scope() { g_updating = previous; }
    } scope;
    g_updating = self;
    // Capture exactly the icons selected by the native producer, without another
    // traversal or asset lookup. Readers still require a visible matching Image.
    o_update(self, values, mi);
}
using Weapon = void (*)(void*, const void*);
Weapon o_weapon;
void d_weapon(void* self, const void* mi)
{
    o_weapon(self, mi);
    hooks::guarded("Displayed.WeaponIcon", [=] {
        for (auto it = g_weapons.begin(); it != g_weapons.end();)
            if (!unity::alive(it->first)) it = g_weapons.erase(it); else ++it;
        g_weapons.erase(self);
        const auto type = il2cpp::read_field<int32_t>(self, il2cpp::field_offset(kSkill.get(), "weaponType"));
        static const wchar_t* labels[] = {L"Sword", L"Knife", L"Spear", L"Axe", L"Staff", L"Bow", L"Shield", L"Unarmed"};
        if (type < 0 || type >= 8) return;
        void* image = field(field(field(self, "view"), "iconText"), "iconImage");
        void* sprite = ref::call<void*>(mSprite, image);
        if (unity::alive(sprite)) g_weapons[self] = {sprite, T(labels[type])};
    });
}
using Ability = void (*)(void*, void*, const void*);
Ability o_ability;
void d_ability(void* self, void* data, const void* mi)
{
    o_ability(self, data, mi);
    hooks::guarded("Displayed.AbilityIcon", [=] { g_weapons.erase(self); });
}
int install() {
    int n = hooks::attach(mIcon, &d_icon, &o_icon) ? 1 : 0;
    if (game::is(game::Id::FF2)) {
        n += hooks::attach(mWeapon, &d_weapon, &o_weapon) ? 1 : 0;
        n += hooks::attach(mAbility, &d_ability, &o_ability) ? 1 : 0;
    }
    return n + (hooks::attach(mUpdate, &d_update, &o_update) ? 1 : 0);
}
core::Install s_install{"Displayed.ConditionIcons", &install, 105};
}
std::vector<std::wstring> condition_lines(void* content)
{
    if (!visible(content)) return {};
    void* controller = kIcons.is(content) ? content : field(content, "conditionIconController");
    if (!controller) controller = field(content, "iconController");
    if (!controller) return {};
    auto it = g_icons.find(controller);
    if (it == g_icons.end() || !visible(controller)) return {};
    void* images = field(field(controller, "view"), "iconImageList");
    std::vector<std::wstring> lines;
    for (int i = 0; i < unity::list_count(images); ++i) {
        void* sprite = image_sprite(unity::list_at(images, i));
        if (!sprite) continue;
        for (const auto& icon : it->second)
            if (icon.sprite == sprite) { lines.push_back(icon.description); break; }
    }
    return lines;
}
std::wstring conditions(void* content) { return ::text::join(condition_lines(content), L", "); }
std::wstring skill_row(void* content)
{
    auto value = ::text::join(texts(content), L" ");
    void* controller = kSkill.is(content) ? content : unity::get_component_in_children(content, kSkill, false);
    auto it = g_weapons.find(controller);
    if (it != g_weapons.end()) {
        void* image = field(field(field(controller, "view"), "iconText"), "iconImage");
        if (image_sprite(image) == it->second.sprite) value = it->second.description + L" " + value;
    }
    auto gauge = bar(field(field(controller, "view"), "gauge"));
    return value + (gauge.empty() ? L"" : L", " + gauge);
}
}
