// Native Items -> equipment details, present in the KeyInput UI of all six games.
// Replaces FF6 ItemDetailPatches/ItemDetailNavigator (ZKline/BlindGuyNW origin)
// with the shared displayed-panel buffer; preserve original-mod attribution.
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
ref::Method mWindowUpdate{kWindow, "UpdateController", 0};
ref::Field fState{kWindow, "stateMachine"};
ref::Class kEquipment{"Last.UI.KeyInput", "ItemEquipmentController"};
ref::Method mPage{kEquipment, "SetPage", 1}, mUpdate{kEquipment, "UpdateView", 2};
ref::Method mPanelDescription{kEquipment, "SetDescriptionText", 1};
ref::Method mParameterFlag{kEquipment, "SetParameterFlag", 1};
ref::Class kAttribute{"Last.UI", "ItemEquipmentAttributeImage"};
ref::Field fAttribute{kAttribute, "attributeType"};
ref::Class kAttributeController{"Last.UI", "ItemEquipmentAttributeController"};
ref::Method mAllAttributes{kAttributeController, "SetAllContentEnable", 1},
            mOneAttribute{kAttributeController, "SetEnableAttribute", 1};
std::unordered_map<void*, unsigned> g_icons;
ref::Class kEquipableContent{"Last.UI.KeyInput", "ItemEquipableContentController"};
ref::Method mEquipableUpdate{kEquipableContent, "UpdateView", 1};
ref::Class kEquipableData{"Last.UI", "ItemEquipableContetnData"};
ref::Field fCanEquip{kEquipableData, "<IsActive>k__BackingField"};
std::unordered_map<void*, bool> g_can_equip;
unsigned g_generation = 0;
void* g_pending_entry = nullptr;
int g_pending_frames = 0;
void* g_owner = nullptr;
using EquipableUpdate = void (*)(void*, void*, const void*);
EquipableUpdate o_equipable_update;
void d_equipable_update(void* self, void* data, const void* mi) {
    o_equipable_update(self, data, mi);
    hooks::guarded("ItemEquipment.EquipableColor", [=] {
        if (kEquipableContent.is(self) && data && (g_can_equip.size() < 128 || g_can_equip.count(self)))
            g_can_equip[self] = ref::rd<bool>(data, fCanEquip);
    });
}

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
bool capture(void* equipment, bool announce)
{
    void* window = field::cache::get(kWindow);
    if (!window || ui::state_tag(window, fState) != 7 || displayed::field(window, "itemEquipController") != equipment) return false;
    void* detail = displayed::field(equipment, "detailController");
    if (!displayed::container_visible(equipment)) return false;
    auto selected = displayed::field(displayed::field(equipment, "view"), "selectItemView");
    auto name = displayed::text_field(displayed::field(selected, "IconTextView"), "nameText");
    if (name.empty()) return false;
    const auto quantity = displayed::text_field(selected, "countText");
    if (!quantity.empty()) name += L", " + TF(L"quantity {0}", {quantity});
    g_owner = equipment;
    const auto generation = ++g_generation;
    frame::cancel_label("ItemEquipment.Capture");
    ui::review::Options options;
    options.domain = ui::review::Domain::Information;
    options.include_headline = true;
    options.announce = announce;
    options.restore_index = !announce && ui::review::owns(equipment) ? ui::review::index() : 0;
    ui::review::begin(equipment, name, options);
    if (announce) ffpr::say(name, true); // one rendered name/quantity entry; begin is silent
    struct Capture { int table = 0, index = 0, page = 0, restore = 0; std::vector<std::wstring> rows; };
    auto state = std::make_shared<Capture>();
    state->restore = options.restore_index;
    frame::every_frame([window, equipment, detail, generation, state] {
        if (generation != g_generation || ui::state_tag(window, fState) != 7 || !displayed::container_visible(equipment)) return false;
        static const char* tables[] = {"statusContentList", "abilityContentList", "magicContentList", "attributeContentList"};
        field::CaptureBudget budget;
        void* view = displayed::field(detail, "view");
        while (state->table < 4 && budget.available()) {
            if (!displayed::container_visible(detail)) { state->table = 4; break; }
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
        void* pages = displayed::field(equipment, "equipableList");
        const int page_count = unity::list_count(pages);
        if (page_count < 0 || page_count > 8) { ui::review::clear(); return false; }
        while (state->page < page_count && budget.available()) {
            void* page = unity::list_at(pages, state->page);
            if (!displayed::container_visible(page)) { ++state->page; state->index = 0; continue; }
            void* list = displayed::field(page, "contentList");
            const int count = unity::list_count(list);
            if (count < 0 || count > 32) { ui::review::clear(); return false; }
            if (state->index >= count) { ++state->page; state->index = 0; continue; }
            void* row = unity::list_at(list, state->index++);
            const auto name = displayed::text_field(displayed::field(row, "view"), "nameText");
            if (name.empty()) continue;
            const auto state_it = g_can_equip.find(row);
            state->rows.push_back(name + (state_it == g_can_equip.end() ? L"" : L", " +
                T(state_it->second ? L"Can equip" : L"Cannot equip")));
        }
        if (state->page < page_count) return true;
        void* pager = displayed::field(displayed::field(equipment, "pager"), "view");
        const auto current = displayed::text_field(pager, "currentPageText");
        const auto maximum = displayed::text_field(pager, "maxPageText");
        if (!current.empty() && !maximum.empty()) state->rows.push_back(T(L"Page") + L" " + current +
            displayed::text_field(pager, "separatorText") + maximum);
        auto panel = displayed::text_field(displayed::field(window, "view"), "descriptionText");
        if (!panel.empty()) state->rows.push_back(std::move(panel));
        ui::review::replace_lines(std::move(state->rows), state->restore);
        return false;
    }, "ItemEquipment.Capture", INT_MAX);
    return true;
}
using Enter = void (*)(void*, const void*);
Enter o_enter;
void d_enter(void* self, const void* mi) {
    o_enter(self, mi);
    hooks::guarded("ItemEquipment.Entry", [self] {
        if (!kWindow.is(self)) return;
        field::cache::register_object(kWindow, self);
        g_pending_entry = self;
        g_pending_frames = 0;
    });
}
Enter o_window_update;
void d_window_update(void* self, const void* mi) {
    o_window_update(self, mi);
    hooks::guarded("ItemEquipment.Ready", [self] {
        if (!kWindow.is(self)) return;
        if (!displayed::container_visible(self)) {
            g_pending_entry = nullptr;
            if (g_owner && ui::review::owns(g_owner)) { ++g_generation; ui::review::clear(); }
            g_owner = nullptr;
            return;
        }
        if (ui::state_tag(self, fState) != 7) {
            if (g_owner && ui::review::owns(g_owner)) { ++g_generation; ui::review::clear(); }
            return;
        }
        if (g_pending_entry == self) {
            if (++g_pending_frames > 10 || capture(displayed::field(self, "itemEquipController"), true))
                g_pending_entry = nullptr;
        }
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
PanelDescription o_parameter_flag;
void d_panel_description(void* self, bool parameter, const void* mi) {
    o_panel_description(self, parameter, mi);
    hooks::guarded("ItemEquipment.InformationPage", [self] { if (kEquipment.is(self)) capture(self, false); });
}
void d_parameter_flag(void* self, bool parameter, const void* mi) {
    o_parameter_flag(self, parameter, mi);
    hooks::guarded("ItemEquipment.ParameterFlag", [self] { if (kEquipment.is(self)) capture(self, false); });
}
int install() {
    events::on(events::Kind::SceneLoaded, [] { g_icons.clear(); g_can_equip.clear(); g_pending_entry = g_owner = nullptr; ++g_generation; });
    return (hooks::attach(mEnter, &d_enter, &o_enter) ? 1 : 0) + (hooks::attach(mPage, &d_page, &o_page) ? 1 : 0) +
        (hooks::attach(mWindowUpdate, &d_window_update, &o_window_update) ? 1 : 0) +
        (hooks::attach(mEquipableUpdate, &d_equipable_update, &o_equipable_update) ? 1 : 0) +
        (hooks::attach(mUpdate, &d_update, &o_update) ? 1 : 0) +
        (hooks::attach(mPanelDescription, &d_panel_description, &o_panel_description) ? 1 : 0) +
        (hooks::attach(mParameterFlag, &d_parameter_flag, &o_parameter_flag) ? 1 : 0) +
        (hooks::attach(mAllAttributes, &d_all_attributes, &o_all_attributes) ? 1 : 0) +
        (hooks::attach(mOneAttribute, &d_one_attribute, &o_one_attribute) ? 1 : 0);
}
core::Install s_install{"Menus.ItemEquipment", &install, 120};
}
}
