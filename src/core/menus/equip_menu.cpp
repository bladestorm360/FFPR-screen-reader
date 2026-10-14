// Ports FF1 Patches/EquipMenuPatches.cs: EquipMenuState, EquipmentInfoWindowController_SelectContent_Patch,
// EquipmentWindowController_SetActive_Patch, EquipmentSelectWindowController_SelectContent_Patch,
// EquipMenuPatches (command bar SetFocus) and EquipDetailsAnnouncer. All five games (every name
// below exists in each dump.cs).
#include "menus_internal.h"
#include "../core.h"
#include "../displayed_ui.h"
#include "../frame.h"
#include "../hooks.h"
#include "../log.h"
#include "../modtext.h"
#include "../say.h"
#include "../text.h"
#include "../ui_util.h"
#include "../unity.h"
#include "../field/object_cache.h"
#include "../ui/review_buffer.h"
#include "../ui/preferences.h"
#include "../field/capture_budget.h"
#include <climits>
#include <memory>

namespace ffpr::menus {
namespace {

ref::Class  kEquipmentWindowController{"Last.UI.KeyInput", "EquipmentWindowController"};
ref::Field  fEquipWindowStateMachine{kEquipmentWindowController, "stateMachine"};
ref::Method mEquipWindowSetActive{kEquipmentWindowController, "SetActive", 1};

ref::Class  kEquipmentInfoWindowController{"Last.UI.KeyInput", "EquipmentInfoWindowController"};
ref::Field  fEquipInfoContentList{kEquipmentInfoWindowController, "contentList"};
ref::Method mEquipInfoSelectContent{kEquipmentInfoWindowController, "SelectContent", 1};

ref::Class  kEquipmentSelectWindowController{"Last.UI.KeyInput", "EquipmentSelectWindowController"};
ref::Field  fEquipSelectContentDataList{kEquipmentSelectWindowController, "contentDataList"};
ref::Method mEquipSelectSelectContent{kEquipmentSelectWindowController, "SelectContent", 2};
ref::Class kEquipmentItemRow{"Last.UI.KeyInput", "ItemListContentController"};

ref::Class  kEquipmentCommandController{"Last.UI.KeyInput", "EquipmentCommandController"};
ref::Method mEquipCommandSetFocus{kEquipmentCommandController, "SetFocus", 2};

ref::Class kEquipmentStatus{"Last.UI.KeyInput", "EquipmentStatusController"};
ref::Method mDifference{kEquipmentStatus, "SetDifferenceData", 3};
using Difference = void (*)(void*, void*, void*, int32_t, const void*);
Difference o_difference;
unsigned g_comparison_generation = 0;
enum class Focus { None, Slot, Candidate };
Focus g_focus = Focus::None;
void* g_comparison_owner = nullptr;
std::wstring g_candidate_name;
int g_selecting = 0;
struct SelectionScope {
    SelectionScope() { ++g_selecting; }
    ~SelectionScope() { --g_selecting; }
};

void clear_comparison()
{
    ++g_comparison_generation;
    frame::cancel_label("Equipment.Comparison");
    if (ui::review::owns(g_comparison_owner)) ui::review::clear();
    g_comparison_owner = nullptr;
}

void capture_comparison(void* self, bool announce = true)
{
    if (g_focus != Focus::Candidate || !kEquipmentStatus.is(self) || !displayed::container_visible(self)) return;
    void* window = field::cache::get(kEquipmentWindowController);
    if (displayed::field(window, "statusController") != self ||
        !displayed::container_visible(displayed::field(displayed::field(window, "selectWindowController"), "view"))) return;
    const int restore = !announce && ui::review::owns(self) ? ui::review::index() : 0;
    clear_comparison();
    g_comparison_owner = self;
    const auto generation = g_comparison_generation;
    ui::review::Options options;
    options.announce = announce;
    options.restore_index = restore;
    options.domain = ui::review::Domain::Information;
    options.include_headline = true;
    options.details_action = "EquipDetailsAnnouncer.AnnounceCurrentItemDetails";
    options.brief_details = ui::prefs::stat_delta_mode() == 1;
    ui::review::begin(self, g_candidate_name, std::move(options));
    struct Capture { int index = 0; std::vector<std::wstring> rows, gains; };
    auto capture = std::make_shared<Capture>();
    frame::every_frame([self, generation, capture, restore] {
        if (generation != g_comparison_generation || g_focus != Focus::Candidate ||
            !detail::equip_menu_active() || !displayed::container_visible(self)) return false;
        void* list = displayed::field(self, "contentList");
        const int count = unity::list_count(list);
        if (count <= 0 || count > 64) { clear_comparison(); return false; }
        field::CaptureBudget budget;
        while (capture->index < count && budget.available()) {
            auto row = displayed::comparison_reading(displayed::field(unity::list_at(list, capture->index++), "view"));
            if (!row.line.empty()) {
                if (row.changed) capture->gains.push_back(row.line);
                capture->rows.push_back(std::move(row.line));
            }
        }
        if (capture->index < count) return true;
        auto panel = detail::equip_description_from_ui();
        if (!panel.empty()) { capture->gains.push_back(panel); capture->rows.push_back(std::move(panel)); }
        ui::review::replace_lines(std::move(capture->rows), restore, std::move(capture->gains));
        return false;
    }, "Equipment.Comparison", INT_MAX);
}

void d_difference(void* self, void* data, void* item, int32_t slot, const void* mi)
{
    o_difference(self, data, item, slot, mi);
    hooks::guarded("Equipment.DisplayedComparison", [self] {
        if (g_focus == Focus::Candidate) capture_comparison(self);
    });
}

ref::Class  kEquipmentDescriptionWindowController{"Last.UI.KeyInput", "EquipmentDescriptionWindowController"};
ref::Field  fEquipDescWindowView{kEquipmentDescriptionWindowController, "view"};
ref::Class  kEquipmentDescriptionWindowView{"Last.UI.KeyInput", "EquipmentDescriptionWindowView"};
ref::Field  fEquipDescViewDescriptionText{kEquipmentDescriptionWindowView, "descriptionText"};
ref::Method mDescription{kEquipmentDescriptionWindowController, "SetDescriptionText", 2};
using Description = void (*)(void*, void*, bool, const void*);
Description o_description;
void d_description(void* self, void* text, bool parameter, const void* mi) {
    o_description(self, text, parameter, mi);
    hooks::guarded("Equipment.InformationPage", [self] {
        if (g_selecting || g_focus != Focus::Candidate ||
            displayed::field(field::cache::get(kEquipmentWindowController), "descriptionController") != self) return;
        capture_comparison(g_comparison_owner, false);
    });
}

ref::Class  kEquipItemListContentData{"Last.UI", "ItemListContentData"};
ref::Field  fEquipItemDataName{kEquipItemListContentData, "<Name>k__BackingField"};

// EquipmentWindowController.State (identical in all five games).
constexpr int STATE_NONE = 0;
constexpr int STATE_COMMAND = 1;   // Command bar (Equip/Optimal/Remove All)

// Restore the native shop when returning from its equipment submenu.
bool g_entered_from_shop = false;

void set_equip_active(bool active)
{
    if (active) menu_state::set_active(menu_state::EQUIP_MENU, true);
    else menu_state::reset(menu_state::EQUIP_MENU);   // FF1 SetActive(false) runs the reset handler
}

/// EquipMenuState.ClearState / ResetState.
void clear_state() { set_equip_active(false); }

/// EquipMenuState.ShouldSuppress: state machine first (command bar = don't suppress), then
/// whether one of the list controllers is really shown.
bool should_suppress()
{
    if (!detail::equip_menu_active()) return false;

    if (void* equipment = field::cache::get(kEquipmentWindowController)) {
        int state = ui::state_tag(equipment, fEquipWindowStateMachine);
        // Command bar: let the generic reader handle it. None: the menu is closing.
        if (state == STATE_COMMAND || state == STATE_NONE) {
            clear_state();
            return false;
        }
    }

    void* window = field::cache::get(kEquipmentWindowController);
    void* info = displayed::field(window, "infoWindowController");
    void* select = displayed::field(window, "selectWindowController");
    bool info_active = info && unity::active_in_hierarchy(info);
    bool select_active = select && unity::active_in_hierarchy(select);
    if (!info_active && !select_active) {
        // Neither list controller is active - we've left the equipment submenu.
        clear_state();
        return false;
    }
    return true;
}

/// Capture the shop context BEFORE other states are cleared, then claim the menu.
void activate_equip_menu()
{
    if (!detail::equip_menu_active()) g_entered_from_shop = detail::shop_menu_active();
    detail::clear_other_menu_states("Equip");
    set_equip_active(true);
}

// --- EquipmentInfoWindowController.SelectContent: the equipment slots -----------------------

using InfoSelectContent_t = void (*)(void* self, void* cursor, const void* mi);
InfoSelectContent_t o_EquipInfoSelectContent;

void d_EquipInfoSelectContent(void* self, void* cursor, const void* mi)
{
    if (kEquipmentInfoWindowController.is(self) && displayed::container_visible(displayed::field(self, "view"))) {
        g_focus = Focus::Slot;
        clear_comparison(); // before nested native stat updates; slot view has no deltas
    }
    { SelectionScope scope; o_EquipInfoSelectContent(self, cursor, mi); }
    hooks::guarded("EquipmentInfoWindowController.SelectContent", [&] {
        if (!kEquipmentInfoWindowController.is(self) || !cursor) return;
        void* list = ref::rd<void*>(self, fEquipInfoContentList);
        const int index = ui::cursor_index(cursor);
        if (index < 0 || index >= unity::list_count(list)) return;
        void* view = unity::list_at(list, index);
        auto name = displayed::text_field(displayed::field(view, "equipTextView"), "nameText");
        if (name.empty()) return;
        // Native localized labels and qualifications are part of the slot readout
        // even with Autodetail off. Hidden qualifications must not be inferred.
        std::vector<std::wstring> labels{displayed::text_field(view, "partText"),
                                         displayed::text_field(view, "optionText")};
        if (game::is(game::Id::FF5) || game::is(game::Id::FF6))
            labels.push_back(displayed::text_field(view, "bothHandsText"));
        auto label = text::join(labels, L" ");
        if (!label.empty()) name = std::move(label) + L": " + name;
        g_focus = Focus::Slot;
        clear_comparison();
        activate_equip_menu();
        std::vector<std::wstring> parts{std::move(name)};
        if (detail::auto_detail()) {
            auto panel = detail::equip_description_from_ui();
            if (!panel.empty()) parts.push_back(std::move(panel));
        }
        ffpr::say_sequence(std::move(parts), true);
    });
}

// --- EquipmentWindowController.SetActive: clear the state, restore the shop -----------------

using SetActive_t = void (*)(void* self, bool is_active, const void* mi);
SetActive_t o_EquipWindowSetActive;

void d_EquipWindowSetActive(void* self, bool is_active, const void* mi)
{
    // Native SetActive can announce its initial slot before returning.
    if (is_active && kEquipmentWindowController.is(self)) field::cache::register_object(kEquipmentWindowController, self);
    o_EquipWindowSetActive(self, is_active, mi);
    hooks::guarded("EquipmentWindowController.SetActive", [&] {
        if (!kEquipmentWindowController.is(self)) return;
        if (is_active) field::cache::register_object(kEquipmentWindowController, self);
        else if (field::cache::get(kEquipmentWindowController) == self) field::cache::clear(kEquipmentWindowController);
        if (is_active) return;
        g_focus = Focus::None;
        clear_comparison();
        bool restore_shop = g_entered_from_shop;   // captured before the reset clears it
        clear_state();
        if (restore_shop) detail::shop_set_menu_active();
    });
}

// --- EquipmentSelectWindowController.SelectContent: the candidate items ---------------------

using SelectSelectContent_t = void (*)(void* self, void* cursor, int32_t type, const void* mi);
SelectSelectContent_t o_EquipSelectSelectContent;

void d_EquipSelectSelectContent(void* self, void* cursor, int32_t type, const void* mi)
{
    if (kEquipmentSelectWindowController.is(self) && displayed::container_visible(displayed::field(self, "view"))) {
        g_focus = Focus::Candidate;
        clear_comparison();
    }
    { SelectionScope scope; o_EquipSelectSelectContent(self, cursor, type, mi); }
    hooks::guarded("EquipmentSelectWindowController.SelectContent", [&] {
        if (!kEquipmentSelectWindowController.is(self) || !cursor ||
            !displayed::container_visible(displayed::field(self, "view"))) return;
        int index = ui::cursor_index(cursor);

        void* list = ref::rd<void*>(self, fEquipSelectContentDataList);
        int count = unity::list_count(list);
        if (count <= 0) return;   // an empty list is normal for empty slots
        if (index < 0 || index >= count) return;
        void* item = unity::list_at(list, index);
        if (!item) return;

        // A nameless entry is the "Remove" row.
        std::wstring name = ref::rd_str(item, fEquipItemDataName);
        if (detail::blank(name)) name = T(L"Remove");
        name = detail::strip_icon(name);

        // The cursor is parented beneath the rendered candidate row. Its count
        // can differ from total ownership (equipped copies); read the visible label.
        void* row = unity::get_component_in_parent(cursor, kEquipmentItemRow);
        const auto quantity = displayed::text_field(displayed::field(row, "view"), "countText");
        if (!quantity.empty()) name += L", " + quantity;

        g_focus = Focus::Candidate; // outer focus wins after nested native preview updates
        activate_equip_menu();
        g_candidate_name = name;
        ffpr::say(name, true);
        capture_comparison(displayed::field(self, "statusController"));
    });
}

// --- EquipmentCommandController.SetFocus: the command bar takes over from the shop ----------

using SetFocus_t = void (*)(void* self, int32_t id, bool is_focus, const void* mi);
SetFocus_t o_EquipCommandSetFocus;

void d_EquipCommandSetFocus(void* self, int32_t id, bool is_focus, const void* mi)
{
    o_EquipCommandSetFocus(self, id, is_focus, mi);
    hooks::guarded("EquipmentCommandController.SetFocus", [&] {
        // Clears the shop state so the generic cursor reads the command bar.
        if (is_focus && detail::shop_menu_active()) detail::shop_clear_for_equipment_submenu();
    });
}

/// EquipDetailsAnnouncer.AnnounceCurrentItemDetails (I key / right stick up).
void announce_current_item_details()
{
    if (!detail::equip_menu_active()) return;
    std::wstring announcement = detail::equip_description_from_ui();
    if (announcement.empty()) announcement = T(L"No description");
    ffpr::say(announcement, true);
}

int install()
{
    // EquipMenuState's static constructor.
    menu_state::register_reset(menu_state::EQUIP_MENU, [] {
        g_focus = Focus::None;
        clear_comparison();
        g_entered_from_shop = false;
        g_candidate_name.clear();
    });

    queries::define("EquipMenuState.IsActive", [] { return detail::equip_menu_active(); });
    queries::define("EquipMenuState.EnteredFromShop", [] { return g_entered_from_shop; });
    queries::define("EquipMenuState.ShouldSuppress", [] { return should_suppress(); });
    actions::define("EquipMenuState.ClearState", [] { clear_state(); });
    actions::define("EquipMenuState.ResetState", [] { clear_state(); });
    actions::define("EquipDetailsAnnouncer.AnnounceCurrentItemDetails", [] { announce_current_item_details(); });
    actions::define("Settings.Changed.StatDeltaMode", [] {
        if (g_focus == Focus::Candidate) capture_comparison(g_comparison_owner, false);
        actions::run("BattleResult.RebuildStatDeltas");
    });

    int n = 0;
    n += hooks::attach(mEquipInfoSelectContent, &d_EquipInfoSelectContent, &o_EquipInfoSelectContent) ? 1 : 0;
    n += hooks::attach(mEquipWindowSetActive, &d_EquipWindowSetActive, &o_EquipWindowSetActive) ? 1 : 0;
    n += hooks::attach(mEquipSelectSelectContent, &d_EquipSelectSelectContent, &o_EquipSelectSelectContent) ? 1 : 0;
    n += hooks::attach(mEquipCommandSetFocus, &d_EquipCommandSetFocus, &o_EquipCommandSetFocus) ? 1 : 0;
    n += hooks::attach(mDifference, &d_difference, &o_difference) ? 1 : 0;
    n += hooks::attach(mDescription, &d_description, &o_description) ? 1 : 0;
    return n;
}

core::Install s_install{"Menus.EquipMenu", &install, 120};

} // namespace

namespace detail {

bool equip_menu_active() { return menu_state::is_active(menu_state::EQUIP_MENU); }

std::wstring equip_description_from_ui()
{
    // EquipmentDescriptionWindowController -> view -> descriptionText -> text.
    void* controller = displayed::field(field::cache::get(kEquipmentWindowController), "descriptionController");
    if (!controller) return {};
    void* view = ref::rd<void*>(controller, fEquipDescWindowView);
    if (!view) return {};
    void* text_component = ref::rd<void*>(view, fEquipDescViewDescriptionText);
    return displayed::text(text_component);
}

} // namespace detail
} // namespace ffpr::menus
