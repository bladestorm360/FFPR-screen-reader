// Ports FF1 Patches/ItemMenuPatches.cs: ItemMenuState, ItemWindowController_SetActive_Patch,
// ItemListController_SelectContent_Patch, ItemUseController_SelectContent_Patch and
// FieldItemReannouncePatches. All five games (every name below exists in each dump.cs).
//
// Not ported: ItemMenuState.GetCharacterRow and GetItemCommandName, which nothing in FF1 calls
// (StatusMenuPatches and CommandBarReader have their own copies).
#include <memory>
#include <vector>
#include "menus_internal.h"
#include "../core.h"
#include "../hooks.h"
#include "../frame.h"
#include "../modtext.h"
#include "../say.h"
#include "../text.h"
#include "../ui_util.h"
#include "../unity.h"

namespace ffpr::menus {
namespace {

ref::Class  kItemWindowController{"Last.UI.KeyInput", "ItemWindowController"};
ref::Field  fItemWindowStateMachine{kItemWindowController, "stateMachine"};
ref::Method mItemWindowSetActive{kItemWindowController, "SetActive", 1};

ref::Class  kItemListController{"Last.UI.KeyInput", "ItemListController"};
ref::Field  fItemListSelectCursor{kItemListController, "selectCursor"};
ref::Field  fItemListDataList{kItemListController, "dataList"};
ref::Method mItemListSelectContent{kItemListController, "SelectContent", 4};
ref::Method mItemListUseSelectInit{kItemListController, "UseSelectInit", 0};
ref::Method mItemListImportantSelectInit{kItemListController, "ImportantSelectInit", 0};
ref::Method mItemListOrganizeSelectInit{kItemListController, "OrganizeSelectInit", 0};

ref::Class  kItemUseController{"Last.UI.KeyInput", "ItemUseController"};
ref::Field  fItemUseContentList{kItemUseController, "contentList"};
ref::Field  fItemUseSelectCursor{kItemUseController, "selectCursor"};
ref::Method mItemUseSelectContent{kItemUseController, "SelectContent", 2};
ref::Method mItemUseSingleInit{kItemUseController, "SingleInit", 0};
ref::Method mItemUseSingleExit{kItemUseController, "SingleExit", 0};
ref::Method mItemUseAllInit{kItemUseController, "AllInit", 0};

ref::Class  kItemListContentData{"Last.UI", "ItemListContentData"};
ref::Field  fItemDataName{kItemListContentData, "<Name>k__BackingField"};
ref::Field  fItemDataDescription{kItemListContentData, "<Description>k__BackingField"};
ref::Field  fItemDataCount{kItemListContentData, "<Count>k__BackingField"};
ref::Field  fItemDataItemType{kItemListContentData, "<ItemType>k__BackingField"};
ref::Field  fItemDataItemId{kItemListContentData, "<ItemId>k__BackingField"};

ref::Class  kItemTargetSelectContentController{"Last.UI.KeyInput", "ItemTargetSelectContentController"};
ref::Field  fItemTargetCurrentData{kItemTargetSelectContentController, "<CurrentData>k__BackingField"};

// ItemWindowController.State (KeyInput; identical in all five games).
constexpr int STATE_NONE = 0;
constexpr int STATE_COMMAND_SELECT = 1;   // Command bar (Use/Key Items/Sort)
constexpr int STATE_USE_SELECT = 2;       // Item list for Use
constexpr int STATE_IMPORTANT_SELECT = 3; // Key Items list
constexpr int STATE_ORGANIZE_SELECT = 4;  // Sort list
constexpr int STATE_TARGET_SELECT = 5;    // Target selection

// ItemMenuState.LastSelectedItem, kept as the values the detail keys read: the row object itself
// is rebuilt with every list update, so its pointer is not kept.
bool g_last_item_valid = false;
int g_last_item_type = 0;
int g_last_item_id = 0;

// FieldItemReannouncePatches: a newer Init supersedes a pending read.
int g_item_list_gen = 0;
int g_item_target_gen = 0;

// ItemUseController.SingleInit and SingleExit both set the cursor to 0 and call SelectContent
// themselves (the shared SelectContent/SetCursor body; KeyInput SingleInit +0x191, SingleExit
// +0x2D in all five GameAssembly.dlls). On entry that spoke the first target from the
// SelectContent hook and again from the bounded entry read one frame later; on backing out it
// spoke the first party member just before the item list's own read. One event, one line:
// SingleInit skips its entry read when its own SelectContent call spoke (counter compared around
// the original), and SelectContent stays silent inside SingleExit. Identity, not text.
int g_item_target_spoken_seq = 0;
bool g_in_single_exit = false;
// Same cap the old per-frame consume used.
constexpr int MAX_FRAMES = 30;

void set_item_menu_active(bool active)
{
    if (active) menu_state::set_active(menu_state::ITEM_MENU, true);
    else menu_state::reset(menu_state::ITEM_MENU);   // FF1 SetActive(false) runs the reset handler
}

/// ItemMenuState.ClearState / ResetState.
void clear_state() { set_item_menu_active(false); }

/// ItemMenuState.ShouldSuppress: true while an item list or the item target is focused.
bool should_suppress()
{
    if (!detail::item_menu_active()) return false;

    if (void* window = unity::find_object(kItemWindowController)) {
        int state = ui::state_tag(window, fItemWindowStateMachine);
        // Command bar or nothing: let the generic reader handle it.
        if (state == STATE_COMMAND_SELECT || state == STATE_NONE) {
            clear_state();
            return false;
        }
        if (state == STATE_USE_SELECT || state == STATE_IMPORTANT_SELECT ||
            state == STATE_ORGANIZE_SELECT || state == STATE_TARGET_SELECT)
            return true;
    }
    // Fallback: clear state if we can't determine.
    clear_state();
    return false;
}

/// ItemMenuState.AnnounceItemListData: name, count, AutoDetail description, "(X of Y)".
bool announce_item_list_data(void* item, int index, int count)
{
    g_last_item_valid = item != nullptr;
    g_last_item_type = ref::rd<int32_t>(item, fItemDataItemType);
    g_last_item_id = ref::rd<int32_t>(item, fItemDataItemId);

    std::wstring name = ref::rd_str(item, fItemDataName);
    if (name.empty()) return false;
    name = detail::strip_icon(name);
    if (name.empty()) return false;

    std::wstring base = name;
    int c = ref::rd<int32_t>(item, fItemDataCount);
    if (c > 1) base += L", " + text::num(c);

    std::wstring announcement = base;
    if (detail::auto_detail()) {
        std::wstring description = ref::rd_str(item, fItemDataDescription);
        if (!detail::blank(description)) {
            description = detail::strip_icon(description);
            if (!detail::blank(description)) announcement = base + L": " + description;
        }
    }

    detail::clear_other_menu_states("Item");
    set_item_menu_active(true);

    ffpr::say(ui::menu_position(announcement, index, count), true);
    return true;
}

/// ItemMenuState.AnnounceItemUseTarget: name, HP, status effects, "(X of Y)".
bool announce_item_use_target(void* content, int index, int count)
{
    void* character = ref::rd<void*>(content, fItemTargetCurrentData);
    if (!character) return false;

    std::wstring name = detail::character_name(character);
    if (detail::blank(name)) return false;

    std::wstring announcement = name;
    detail::Vitals v = detail::character_vitals(character);
    if (v.has_parameter) {
        announcement += L", " + TF(L"HP {0}/{1}", {text::num(v.hp), text::num(v.max_hp)});
        if (game::flags().has_mp)
            announcement += L", " + TF(L"MP {0}/{1}", {text::num(v.mp), text::num(v.max_mp)});
        if (!v.conditions.empty()) announcement += L", " + detail::join_all(v.conditions, L", ");
    }

    detail::clear_other_menu_states("Item");
    set_item_menu_active(true);

    ffpr::say(ui::menu_position(announcement, index, count), true);
    return true;
}

// --- ItemWindowController.SetActive: clear the state when the item window closes -------------

using SetActive_t = void (*)(void* self, bool is_active, const void* mi);
SetActive_t o_ItemWindowSetActive;

void d_ItemWindowSetActive(void* self, bool is_active, const void* mi)
{
    o_ItemWindowSetActive(self, is_active, mi);
    hooks::guarded("ItemWindowController.SetActive", [&] {
        if (!is_active) clear_state();
    });
}

// --- ItemListController.SelectContent: item rows while navigating ---------------------------

using ItemListSelectContent_t = void (*)(void* self, void* targets, int32_t index, void* cursor, int32_t type, const void* mi);
ItemListSelectContent_t o_ItemListSelectContent;

void d_ItemListSelectContent(void* self, void* targets, int32_t index, void* cursor, int32_t type, const void* mi)
{
    o_ItemListSelectContent(self, targets, index, cursor, type, mi);
    hooks::guarded("ItemListController.SelectContent", [&] {
        if (!targets) return;
        std::vector<void*> list;
        if (!detail::enumerate(targets, list) || list.empty()) return;
        if (index < 0 || index >= static_cast<int>(list.size())) return;
        void* item = list[static_cast<size_t>(index)];
        if (!item) return;
        announce_item_list_data(item, index, static_cast<int>(list.size()));
    });
}

// --- ItemUseController.SelectContent: the item's target character ---------------------------
// Its body is shared with ItemUseController.SetCursor (same class) in all five games; Harmony's
// postfix ran for both as well.

using ItemUseSelectContent_t = void (*)(void* self, void* contents, void* cursor, const void* mi);
ItemUseSelectContent_t o_ItemUseSelectContent;

void d_ItemUseSelectContent(void* self, void* contents, void* cursor, const void* mi)
{
    o_ItemUseSelectContent(self, contents, cursor, mi);
    hooks::guarded("ItemUseController.SelectContent", [&] {
        if (!kItemUseController.is(self)) return;
        if (!cursor || !contents) return;
        int index = ui::cursor_index(cursor);
        std::vector<void*> list;
        if (!detail::enumerate(contents, list) || list.empty()) return;
        if (index < 0 || index >= static_cast<int>(list.size())) return;
        void* content = list[static_cast<size_t>(index)];
        if (!content) return;
        if (g_in_single_exit) return;   // the game's own reset call while leaving the target list
        if (announce_item_use_target(content, index, static_cast<int>(list.size()))) ++g_item_target_spoken_seq;
    });
}

// --- FieldItemReannouncePatches: speak the focused row when a list (re)gains focus ----------
// The nav SelectContent hooks only fire on cursor movement, so they are silent on (re)entry.
// The list controllers' state-entry Init methods fire on both; each starts a bounded read (one
// try per frame, up to MAX_FRAMES tries) that speaks once the list is genuinely built.

/// One try per frame, starting the frame after the Init, for at most MAX_FRAMES tries; frames
/// where the controller is inactive don't count as tries, but the read still ends after
/// 2 x MAX_FRAMES frames so it can never linger.
template <typename Current, typename Try>
void bounded_read(void* controller, const char* label, Current still_current, Try try_announce)
{
    struct Progress { int frame = 0; int tries = 0; };
    auto p = std::make_shared<Progress>();
    frame::every_frame([=]() -> bool {
        if (!still_current()) return false;
        if (!unity::alive(controller)) return false;     // controller destroyed
        ++p->frame;
        if (unity::active_in_hierarchy(controller)) {
            ++p->tries;
            if (try_announce(controller)) return false;
        }
        return p->frame < MAX_FRAMES * 2 && p->tries <= MAX_FRAMES;
    }, label, MAX_FRAMES * 2 + 1);
}

bool try_announce_item_list_initial(void* controller)
{
    if (!detail::menu_manager_open()) return false;
    void* data_list = ref::rd<void*>(controller, fItemListDataList);
    if (!data_list) return false;
    std::vector<void*> list;
    if (!detail::enumerate(data_list, list) || list.empty()) return false;

    void* cursor = ref::rd<void*>(controller, fItemListSelectCursor);
    if (!cursor) return false;
    int index = ui::cursor_index(cursor);
    if (index < 0 || index >= static_cast<int>(list.size())) return false;

    void* item = list[static_cast<size_t>(index)];
    if (!item) return false;
    return announce_item_list_data(item, index, static_cast<int>(list.size()));
}

bool try_announce_item_target_initial(void* controller)
{
    if (!detail::menu_manager_open()) return false;
    void* content_list = ref::rd<void*>(controller, fItemUseContentList);
    int count = unity::list_count(content_list);
    if (count <= 0) return false;

    void* cursor = ref::rd<void*>(controller, fItemUseSelectCursor);
    if (!cursor) return false;
    int index = ui::cursor_index(cursor);
    if (index < 0 || index >= count) return false;

    void* content = unity::list_at(content_list, index);
    if (!content) return false;
    return announce_item_use_target(content, index, count);
}

void item_list_init(void* self)
{
    if (!kItemListController.is(self)) return;
    int gen = ++g_item_list_gen;
    bounded_read(self, "ItemMenu.ListRead", [gen] { return gen == g_item_list_gen; }, &try_announce_item_list_initial);
}

void item_target_init(void* self)
{
    if (!kItemUseController.is(self)) return;
    int gen = ++g_item_target_gen;
    bounded_read(self, "ItemMenu.TargetRead", [gen] { return gen == g_item_target_gen; }, &try_announce_item_target_initial);
}

using Init_t = void (*)(void* self, const void* mi);
Init_t o_UseSelectInit, o_ImportantSelectInit, o_OrganizeSelectInit, o_SingleInit, o_SingleExit, o_AllInit;

void d_UseSelectInit(void* self, const void* mi)
{
    o_UseSelectInit(self, mi);
    hooks::guarded("ItemListController.UseSelectInit", [&] { item_list_init(self); });
}

void d_ImportantSelectInit(void* self, const void* mi)
{
    o_ImportantSelectInit(self, mi);
    hooks::guarded("ItemListController.ImportantSelectInit", [&] { item_list_init(self); });
}

void d_OrganizeSelectInit(void* self, const void* mi)
{
    o_OrganizeSelectInit(self, mi);
    hooks::guarded("ItemListController.OrganizeSelectInit", [&] { item_list_init(self); });
}

void d_SingleInit(void* self, const void* mi)
{
    const int spoken_before = g_item_target_spoken_seq;
    o_SingleInit(self, mi);
    hooks::guarded("ItemUseController.SingleInit", [&] {
        if (g_item_target_spoken_seq != spoken_before) {
            ++g_item_target_gen;   // the game's own SelectContent call spoke the entry target
            return;
        }
        item_target_init(self);
    });
}

void d_SingleExit(void* self, const void* mi)
{
    struct Scope {
        bool was = g_in_single_exit;
        Scope() { g_in_single_exit = true; }
        ~Scope() { g_in_single_exit = was; }
    } scope;
    o_SingleExit(self, mi);
}

void d_AllInit(void* self, const void* mi)
{
    o_AllInit(self, mi);
    hooks::guarded("ItemUseController.AllInit", [&] { item_target_init(self); });
}

int install()
{
    // ItemMenuState's static constructor.
    menu_state::register_reset(menu_state::ITEM_MENU, [] { g_last_item_valid = false; });

    queries::define("ItemMenuState.IsItemMenuActive", [] { return detail::item_menu_active(); });
    queries::define("ItemMenuState.ShouldSuppress", [] { return should_suppress(); });
    actions::define("ItemMenuState.ClearState", [] { clear_state(); });
    actions::define("ItemMenuState.ResetState", [] { clear_state(); });

    int n = 0;
    n += hooks::attach(mItemWindowSetActive, &d_ItemWindowSetActive, &o_ItemWindowSetActive) ? 1 : 0;
    n += hooks::attach(mItemListSelectContent, &d_ItemListSelectContent, &o_ItemListSelectContent) ? 1 : 0;
    n += hooks::attach(mItemUseSelectContent, &d_ItemUseSelectContent, &o_ItemUseSelectContent) ? 1 : 0;
    n += hooks::attach(mItemListUseSelectInit, &d_UseSelectInit, &o_UseSelectInit) ? 1 : 0;
    n += hooks::attach(mItemListImportantSelectInit, &d_ImportantSelectInit, &o_ImportantSelectInit) ? 1 : 0;
    n += hooks::attach(mItemListOrganizeSelectInit, &d_OrganizeSelectInit, &o_OrganizeSelectInit) ? 1 : 0;
    n += hooks::attach(mItemUseSingleInit, &d_SingleInit, &o_SingleInit) ? 1 : 0;
    n += hooks::attach(mItemUseSingleExit, &d_SingleExit, &o_SingleExit) ? 1 : 0;   // unique RVA in all five
    n += hooks::attach(mItemUseAllInit, &d_AllInit, &o_AllInit) ? 1 : 0;
    return n;
}

core::Install s_install{"Menus.ItemMenu", &install, 120};

} // namespace

namespace detail {

bool item_menu_active() { return menu_state::is_active(menu_state::ITEM_MENU); }

bool item_last_selected(int& item_type, int& item_id)
{
    item_type = g_last_item_type;
    item_id = g_last_item_id;
    return g_last_item_valid;
}

} // namespace detail
} // namespace ffpr::menus
