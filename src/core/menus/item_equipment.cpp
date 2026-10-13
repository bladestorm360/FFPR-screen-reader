// Native Items -> equipment details, present in the KeyInput UI of all six games.
#include "menus_internal.h"
#include "../core.h"
#include "../displayed_ui.h"
#include "../frame.h"
#include "../hooks.h"
#include "../modtext.h"
#include "../say.h"
#include "../ui/review_buffer.h"
#include "../field/object_cache.h"
#include "../field/capture_budget.h"
#include "../unity.h"
#include "../ui_util.h"
#include <climits>
#include <memory>
#include <unordered_map>

namespace ffpr::menus {
namespace {
ref::Class kWindow{"Last.UI.KeyInput", "ItemWindowController"};
ref::Method mEnter{kWindow, "EquipmentInit", 0};
ref::Field fState{kWindow, "stateMachine"};
ref::Class kEquipment{"Last.UI.KeyInput", "ItemEquipmentController"};
ref::Method mPage{kEquipment, "SetPage", 1}, mUpdate{kEquipment, "UpdateView", 2};
ref::Method mPanelDescription{kEquipment, "SetDescriptionText", 1};
ref::Class kAttribute{"Last.UI", "ItemEquipmentAttributeImage"};
ref::Field fAttribute{kAttribute, "attributeType"};
ref::Class kAttributeController{"Last.UI", "ItemEquipmentAttributeController"};
ref::Method mAllAttributes{kAttributeController, "SetAllContentEnable", 1},
            mOneAttribute{kAttributeController, "SetEnableAttribute", 1};
std::unordered_map<void*, unsigned> g_icons;
unsigned g_generation = 0;

using Enable = void (*)(void*, bool, const void*);
Enable o_all_attributes;
void d_all_attributes(void* self, bool enabled, const void* mi) {
    o_all_attributes(self, enabled, mi);
    hooks::guarded("ItemEquipment.AttributeReset", [=] {
        if (kAttributeController.is(self) && (g_icons.size() < 128 || g_icons.count(self)))
            g_icons[self] = enabled ? 0x7FEu : 0u;
    });
}
using Attribute = void (*)(void*, int32_t, const void*);
Attribute o_one_attribute;
void d_one_attribute(void* self, int32_t type, const void* mi) {
    o_one_attribute(self, type, mi);
    hooks::guarded("ItemEquipment.AttributeHighlight", [=] {
        if (type > 0 && type <= 10 && kAttributeController.is(self) && (g_icons.size() < 128 || g_icons.count(self)))
            g_icons[self] |= 1u << type;
    });
}
std::wstring attribute_icon(void* controller, void* icon)
{
    void* sprite = displayed::image_sprite(displayed::field(icon, "image"));
    auto observed = g_icons.find(controller);
    const int type = ref::rd<int>(icon, fAttribute);
    if (!sprite || type <= 0 || type > 10 || observed == g_icons.end() || !(observed->second & (1u << type))) return {};
    // Serialized AttributeType identifies the rendered icon. The native producer
    // owns its highlighted state (color in FF1-FF5, sprite in FF6).
    static const wchar_t* names[] = {L"", L"Fire", L"Ice", L"Lightning", L"Earth", L"Ice",
        L"Poison", L"Earth", L"Holy", L"Water", L"Wind"};
    return T(names[type]);
}
void capture(void* equipment, bool announce)
{
    void* window = field::cache::get(kWindow);
    if (!window || ui::state_tag(window, fState) != 7 || displayed::field(window, "itemEquipController") != equipment) return;
    void* detail = displayed::field(equipment, "detailController");
    if (!displayed::container_visible(detail)) return;
    const auto generation = ++g_generation;
    frame::cancel_label("ItemEquipment.Capture");
    auto selected = displayed::field(displayed::field(equipment, "view"), "selectItemView");
    auto name = displayed::text_field(displayed::field(selected, "IconTextView"), "nameText");
    ui::review::Options options;
    options.domain = ui::review::Domain::Information;
    options.include_headline = true;
    options.details_action = "ItemDescriptionAnnouncer.AnnounceCurrentDescription";
    options.announce = announce;
    options.restore_index = !announce && ui::review::owns(detail) ? ui::review::index() : 0;
    ui::review::begin(detail, name, options);
    if (announce && !name.empty()) ffpr::say(name, true);
    struct Capture { int table = 0, index = 0, restore = 0; std::vector<std::wstring> rows; };
    auto state = std::make_shared<Capture>();
    state->restore = options.restore_index;
    frame::every_frame([window, detail, generation, state] {
        if (generation != g_generation || !displayed::container_visible(detail)) return false;
        static const char* tables[] = {"statusContentList", "abilityContentList", "magicContentList", "attributeContentList"};
        field::CaptureBudget budget;
        void* view = displayed::field(detail, "view");
        while (state->table < 4 && budget.available()) {
            void* list = displayed::field(view, tables[state->table]);
            const int count = unity::list_count(list);
            if (count < 0 || count > 64) { ui::review::clear(); return false; }
            if (state->index >= count) { ++state->table; state->index = 0; continue; }
            void* controller = unity::list_at(list, state->index++);
            void* row = displayed::field(controller, "view");
            if (!displayed::container_visible(row)) continue;
            auto line = displayed::text_field(row, "nameText");
            if (line.empty()) continue;
            if (state->table == 3) {
                void* icons = displayed::field(row, "iconsList");
                const int icon_count = unity::list_count(icons);
                if (icon_count < 0 || icon_count > 16) { ui::review::clear(); return false; }
                for (int i = 0; i < icon_count; ++i) {
                    auto value = attribute_icon(controller, unity::list_at(icons, i));
                    if (!value.empty()) line += L" " + value;
                }
            } else {
                line += displayed::text_field(row, "separationText") + L" " + displayed::text_field(row, "plusText") +
                    displayed::text_field(row, "valueText");
            }
            state->rows.push_back(std::move(line));
        }
        if (state->table < 4) return true;
        auto panel = displayed::text_field(displayed::field(window, "view"), "descriptionText");
        if (!panel.empty()) state->rows.push_back(std::move(panel));
        ui::review::replace_lines(std::move(state->rows), state->restore);
        return false;
    }, "ItemEquipment.Capture", INT_MAX);
}
using Enter = void (*)(void*, const void*);
Enter o_enter;
void d_enter(void* self, const void* mi) {
    o_enter(self, mi);
    hooks::guarded("ItemEquipment.Entry", [self] {
        if (!kWindow.is(self)) return;
        field::cache::register_object(kWindow, self);
        capture(displayed::field(self, "itemEquipController"), true);
    });
}
using Page = void (*)(void*, int32_t, const void*);
Page o_page;
void d_page(void* self, int32_t page, const void* mi) {
    o_page(self, page, mi);
    hooks::guarded("ItemEquipment.Page", [self] { if (kEquipment.is(self)) capture(self, false); });
}
using Update = void (*)(void*, void*, bool, const void*);
Update o_update;
void d_update(void* self, void* item, bool parameter, const void* mi) {
    o_update(self, item, parameter, mi);
    hooks::guarded("ItemEquipment.Item", [self] { if (kEquipment.is(self)) capture(self, false); });
}
using PanelDescription = void (*)(void*, bool, const void*);
PanelDescription o_panel_description;
void d_panel_description(void* self, bool parameter, const void* mi) {
    o_panel_description(self, parameter, mi);
    hooks::guarded("ItemEquipment.InformationPage", [self] { if (kEquipment.is(self)) capture(self, false); });
}
int install() {
    events::on(events::Kind::SceneLoaded, [] { g_icons.clear(); });
    return (hooks::attach(mEnter, &d_enter, &o_enter) ? 1 : 0) + (hooks::attach(mPage, &d_page, &o_page) ? 1 : 0) +
        (hooks::attach(mUpdate, &d_update, &o_update) ? 1 : 0) +
        (hooks::attach(mPanelDescription, &d_panel_description, &o_panel_description) ? 1 : 0) +
        (hooks::attach(mAllAttributes, &d_all_attributes, &o_all_attributes) ? 1 : 0) +
        (hooks::attach(mOneAttribute, &d_one_attribute, &o_one_attribute) ? 1 : 0);
}
core::Install s_install{"Menus.ItemEquipment", &install, 120};
}
}
