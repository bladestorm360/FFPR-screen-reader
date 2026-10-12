// Ports FF1 Patches/EquipMenuPatches.cs: EquipMenuState, EquipmentInfoWindowController_SelectContent_Patch,
// EquipmentWindowController_SetActive_Patch, EquipmentSelectWindowController_SelectContent_Patch,
// EquipMenuPatches (command bar SetFocus) and EquipDetailsAnnouncer. All five games (every name
// below exists in each dump.cs).
#include "menus_internal.h"
#include "../core.h"
#include "../dedup.h"
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
#include "../displayed_ui.h"

namespace ffpr::menus {
namespace {

ref::Class  kEquipmentWindowController{"Last.UI.KeyInput", "EquipmentWindowController"};
ref::Field  fEquipWindowStateMachine{kEquipmentWindowController, "stateMachine"};
ref::Method mEquipWindowSetActive{kEquipmentWindowController, "SetActive", 1};

ref::Class  kEquipmentInfoWindowController{"Last.UI.KeyInput", "EquipmentInfoWindowController"};
ref::Field  fEquipInfoContentList{kEquipmentInfoWindowController, "contentList"};
ref::Field  fEquipInfoTargetIndex{kEquipmentInfoWindowController, "targetIndex"};
ref::Field  fEquipInfoView{kEquipmentInfoWindowController, "view"};
ref::Method mEquipInfoSelectContent{kEquipmentInfoWindowController, "SelectContent", 1};

ref::Class  kEquipmentInfoContentView{"Last.UI.KeyInput", "EquipmentInfoContentView"};
ref::Field  fEquipContentPartText{kEquipmentInfoContentView, "partText"};
ref::Field  fEquipContentSlot{kEquipmentInfoContentView, "<Slot>k__BackingField"};
ref::Field  fEquipContentData{kEquipmentInfoContentView, "<Data>k__BackingField"};

ref::Class  kEquipmentInfoWindowView{"Last.UI.KeyInput", "EquipmentInfoWindowView"};
ref::Field  fEquipInfoViewNameText{kEquipmentInfoWindowView, "nameText"};
ref::Field  fEquipInfoViewJobNameText{kEquipmentInfoWindowView, "jobNameText"};

ref::Class  kEquipmentSelectWindowController{"Last.UI.KeyInput", "EquipmentSelectWindowController"};
ref::Field  fEquipSelectContentDataList{kEquipmentSelectWindowController, "contentDataList"};
ref::Method mEquipSelectSelectContent{kEquipmentSelectWindowController, "SelectContent", 2};

ref::Class  kEquipmentCommandController{"Last.UI.KeyInput", "EquipmentCommandController"};
ref::Method mEquipCommandSetFocus{kEquipmentCommandController, "SetFocus", 2};

ref::Class kEquipmentStatus{"Last.UI.KeyInput", "EquipmentStatusController"};
ref::Method mDifference{kEquipmentStatus, "SetDifferenceData", 3};
using Difference = void (*)(void*, void*, void*, int32_t, const void*);
Difference o_difference;
unsigned g_comparison_generation = 0;

void d_difference(void* self, void* data, void* item, int32_t slot, const void* mi)
{
    ++g_comparison_generation;
    frame::cancel_label("Equipment.Comparison");
    if (ui::review::owns(self)) ui::review::clear();
    o_difference(self, data, item, slot, mi);
    hooks::guarded("Equipment.DisplayedComparison", [self] {
        const unsigned generation = ++g_comparison_generation;
        frame::cancel_label("Equipment.Comparison");
        frame::next_frame([self, generation] {
            if (generation != g_comparison_generation || !detail::equip_menu_active() || !displayed::visible(self)) return;
            std::vector<std::wstring> rows;
            void* list = displayed::field(self, "contentList");
            for (int i = 0; i < unity::list_count(list); ++i) {
                auto line = displayed::comparison(displayed::field(unity::list_at(list, i), "view"));
                if (!line.empty()) rows.push_back(std::move(line));
            }
            auto description = detail::equip_description_from_ui();
            auto comparisons = text::join(rows, L". ");
            if (!description.empty()) rows.push_back(std::move(description));
            ui::review::Options options;
            options.domain = ui::review::Domain::Information;
            options.announce = false;
            ui::review::show(self, {}, std::move(rows), options);
            if (!comparisons.empty()) ffpr::say(comparisons, false);
        }, "Equipment.Comparison");
    });
}

ref::Class  kEquipmentDescriptionWindowController{"Last.UI.KeyInput", "EquipmentDescriptionWindowController"};
ref::Field  fEquipDescWindowView{kEquipmentDescriptionWindowController, "view"};
ref::Class  kEquipmentDescriptionWindowView{"Last.UI.KeyInput", "EquipmentDescriptionWindowView"};
ref::Field  fEquipDescViewDescriptionText{kEquipmentDescriptionWindowView, "descriptionText"};

ref::Class  kEquipUtility{"Last.Systems", "EquipUtility"};
ref::Method mGetSlotMessageId{kEquipUtility, "GetSlotMessageId", 1};

ref::Class  kOwnedItemData{"Last.Data.User", "OwnedItemData"};
ref::Method mOwnedItemGetName{kOwnedItemData, "get_Name", 0};

ref::Class  kEquipItemListContentData{"Last.UI", "ItemListContentData"};
ref::Field  fEquipItemDataName{kEquipItemListContentData, "<Name>k__BackingField"};

// EquipmentWindowController.State (identical in all five games).
constexpr int STATE_NONE = 0;
constexpr int STATE_COMMAND = 1;   // Command bar (Equip/Optimal/Remove All)

// EquipSlotType (identical in all five games).
constexpr int SLOT1 = 1, SLOT2 = 2, SLOT3 = 3, SLOT4 = 4, SLOT5 = 5, SLOT6 = 6;

// EquipMenuState.EnteredFromShop: restore the shop state when the equipment menu closes.
bool g_entered_from_shop = false;
// EquipMenuState.LastTargetIndex (the party member, changes only on LB/RB) is the dedup context
// "Equip.Character". It is reset only with the equip state, as LastTargetIndex was; this flag is
// "LastTargetIndex >= 0" (a previous member exists, so a change is a switch).
constexpr const char* kCtxCharacter = "Equip.Character";
bool g_have_target = false;

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

/// EquipMenuState.GetSlotName: the game's slot name, else the English fallback.
std::wstring slot_name(int slot)
{
    std::wstring id = ref::str(ref::call_static<void*>(mGetSlotMessageId, static_cast<int32_t>(slot)));
    if (!id.empty()) {
        std::wstring localized = detail::message(id, true);
        if (!detail::blank(localized)) return localized;
    }
    switch (slot) {
    case SLOT1: return T(L"Right Hand");
    case SLOT2: return T(L"Left Hand");
    case SLOT3: return T(L"Head");
    case SLOT4: return T(L"Body");
    case SLOT5: return T(L"Accessory");
    case SLOT6: return T(L"Accessory 2");
    default: return TF(L"Slot {0}", {text::num(slot)});
    }
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
    o_EquipInfoSelectContent(self, cursor, mi);
    hooks::guarded("EquipmentInfoWindowController.SelectContent", [&] {
        // Don't set IsActive before the data is validated: setting it early causes suppression
        // during menu transitions.
        if (!cursor) return;
        int index = ui::cursor_index(cursor);

        void* content_list = ref::rd<void*>(self, fEquipInfoContentList);
        int count = unity::list_count(content_list);
        if (count <= 0) {
            LOGF("[Equipment Slot] contentList is null or empty");
            return;
        }
        if (index < 0 || index >= count) {
            LOGF("[Equipment Slot] Index %d out of range (count=%d)", index, count);
            return;
        }
        void* view = unity::list_at(content_list, index);
        if (!view) {
            LOGF("[Equipment Slot] Content view is null");
            return;
        }

        // Slot name from partText, else the localized slot name.
        std::wstring slot;
        if (void* part = ref::rd<void*>(view, fEquipContentPartText)) slot = unity::ui_text(part);
        if (detail::blank(slot)) slot = slot_name(ref::rd<int32_t>(view, fEquipContentSlot));

        // Equipped item name (stats are behind the I key).
        std::wstring equipped;
        if (void* data = ref::rd<void*>(view, fEquipContentData)) equipped = ref::call_str(mOwnedItemGetName, data);

        std::wstring announcement;
        if (!detail::blank(slot)) announcement = slot;
        if (!detail::blank(equipped)) {
            if (!detail::blank(announcement)) announcement += L": " + equipped;
            else announcement = equipped;
        } else {
            announcement += L": " + T(L"Empty");
        }
        if (detail::blank(announcement)) return;

        // Strip icon markup (e.g. <ic_knife>).
        announcement = detail::strip_icon(announcement);

        // AutoDetail: append stats/description for the equipped item.
        if (detail::auto_detail()) {
            std::wstring extra = detail::equip_description_from_ui();
            if (!detail::blank(extra)) announcement = announcement + L": " + extra;
        }

        activate_equip_menu();

        // Character switch via targetIndex (changes only on RB/LB): name and job first.
        bool character_switched = false;
        int target_index = ref::rd<int32_t>(self, fEquipInfoTargetIndex);
        if (dedup::index(kCtxCharacter, target_index, dedup::Scope::Session)) {
            character_switched = g_have_target;
            g_have_target = true;
            if (character_switched) {
                if (void* info_view = ref::rd<void*>(self, fEquipInfoView)) {
                    std::wstring name = unity::ui_text(ref::rd<void*>(info_view, fEquipInfoViewNameText));
                    std::wstring job = unity::ui_text(ref::rd<void*>(info_view, fEquipInfoViewJobNameText));
                    if (!detail::blank(name)) {
                        std::wstring who = name;
                        if (!detail::blank(job)) who += L", " + job;
                        ffpr::say(who, true);
                    }
                }
            }
        }

        // Position last, after AutoDetail. After a switch, queue the slot behind the name.
        ffpr::say(ui::menu_position(announcement, index, count), !character_switched);
    });
}

// --- EquipmentWindowController.SetActive: clear the state, restore the shop -----------------

using SetActive_t = void (*)(void* self, bool is_active, const void* mi);
SetActive_t o_EquipWindowSetActive;

void d_EquipWindowSetActive(void* self, bool is_active, const void* mi)
{
    o_EquipWindowSetActive(self, is_active, mi);
    hooks::guarded("EquipmentWindowController.SetActive", [&] {
        if (!kEquipmentWindowController.is(self)) return;
        if (is_active) field::cache::register_object(kEquipmentWindowController, self);
        else if (field::cache::get(kEquipmentWindowController) == self) field::cache::clear(kEquipmentWindowController);
        if (is_active) return;
        ++g_comparison_generation;
        frame::cancel_label("Equipment.Comparison");
        if (ui::review::domain() == ui::review::Domain::Information) ui::review::clear();
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
    o_EquipSelectSelectContent(self, cursor, type, mi);
    hooks::guarded("EquipmentSelectWindowController.SelectContent", [&] {
        if (!cursor) return;
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

        // Name only; AutoDetail appends what the I key reads.
        std::wstring announcement = name;
        if (detail::auto_detail()) {
            std::wstring extra = detail::equip_description_from_ui();
            if (!detail::blank(extra)) announcement = name + L": " + extra;
        }

        activate_equip_menu();
        ffpr::say(ui::menu_position(announcement, index, count), true);
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
        ++g_comparison_generation;
        frame::cancel_label("Equipment.Comparison");
        if (ui::review::domain() == ui::review::Domain::Information) ui::review::clear();
        g_entered_from_shop = false;
        g_have_target = false;
        dedup::reset(kCtxCharacter);
    });

    queries::define("EquipMenuState.IsActive", [] { return detail::equip_menu_active(); });
    queries::define("EquipMenuState.EnteredFromShop", [] { return g_entered_from_shop; });
    queries::define("EquipMenuState.ShouldSuppress", [] { return should_suppress(); });
    actions::define("EquipMenuState.ClearState", [] { clear_state(); });
    actions::define("EquipMenuState.ResetState", [] { clear_state(); });
    actions::define("EquipDetailsAnnouncer.AnnounceCurrentItemDetails", [] { announce_current_item_details(); });

    int n = 0;
    n += hooks::attach(mEquipInfoSelectContent, &d_EquipInfoSelectContent, &o_EquipInfoSelectContent) ? 1 : 0;
    n += hooks::attach(mEquipWindowSetActive, &d_EquipWindowSetActive, &o_EquipWindowSetActive) ? 1 : 0;
    n += hooks::attach(mEquipSelectSelectContent, &d_EquipSelectSelectContent, &o_EquipSelectSelectContent) ? 1 : 0;
    n += hooks::attach(mEquipCommandSetFocus, &d_EquipCommandSetFocus, &o_EquipCommandSetFocus) ? 1 : 0;
    n += hooks::attach(mDifference, &d_difference, &o_difference) ? 1 : 0;
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
