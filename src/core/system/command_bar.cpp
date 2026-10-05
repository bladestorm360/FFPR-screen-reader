// Ports FF1 Patches/CommandBarPatches.cs (the item / equipment / magic command bars: the focused
// command on open and on each move, with "(n of N)") and Menus/CommandBarReader.cs (the command
// name maps and the shared announce).
//
// Names come from the command ID enums, never the on-screen Text (placeholder-proof). A read is
// scheduled by each bar's own cursor-set method (open via ResetCursor, the move callback, a click)
// and by the command-state entry (back-out from a deeper screen, where the cursor does not move);
// it runs one frame later with a short bounded retry while the focused data is not populated.
//
// The magic bar is FF1's Serial.FF1.UI.KeyInput.AbilityCommandController / AbilityWindowController
// (each game has its own Serial.FFn classes): game mask FF1 only.
#include <string>
#include "core.h"
#include "dedup.h"
#include "frame.h"
#include "hooks.h"
#include "log.h"
#include "modtext.h"
#include "refs.h"
#include "say.h"
#include "ui_util.h"
#include "unity.h"
#include "system/system.h"

namespace ffpr::system {
namespace {

ref::Class kItemCommand{"Last.UI.KeyInput", "ItemCommandController"};
ref::Class kEquipCommand{"Last.UI.KeyInput", "EquipmentCommandController"};
ref::Class kItemWindow{"Last.UI.KeyInput", "ItemWindowController"};
ref::Class kEquipWindow{"Last.UI.KeyInput", "EquipmentWindowController"};
ref::Class kItemCommandView{"Last.UI.KeyInput", "ItemCommandContentView"};
ref::Class kItemCommandData{"Last.UI", "ItemCommadContentData"};   // (sic)
ref::Class kAbilityCommandData{"Last.UI", "AbilityCommandContentData"};
ref::Class kMagicCommand{"Serial.FF1.UI.KeyInput", "AbilityCommandController", game::kFF1};
ref::Class kMagicWindow{"Serial.FF1.UI.KeyInput", "AbilityWindowController", game::kFF1};
ref::Class kMagicCommandView{"Serial.FF1.UI.KeyInput", "AbilityCommandContentView", game::kFF1};

ref::Field fItemCursor{kItemCommand, "selectCursor"};
ref::Field fItemContents{kItemCommand, "contentList"};
ref::Field fEquipCursor{kEquipCommand, "selectCursor"};
ref::Field fEquipContents{kEquipCommand, "contents"};
ref::Field fEquipIdCash{kEquipCommand, "<EquipmentCommandIdCash>k__BackingField"};
ref::Field fItemWindowCommand{kItemWindow, "commandController"};
ref::Field fEquipWindowCommand{kEquipWindow, "commandController"};
ref::Field fItemWindowState{kItemWindow, "stateMachine"};
ref::Field fEquipWindowState{kEquipWindow, "stateMachine"};
ref::Field fItemViewData{kItemCommandView, "<Data>k__BackingField"};
ref::Field fItemDataId{kItemCommandData, "<Id>k__BackingField"};
ref::Field fAbilityDataId{kAbilityCommandData, "<Id>k__BackingField"};
ref::Field fMagicCursor{kMagicCommand, "selectCursor"};
ref::Field fMagicContents{kMagicCommand, "contentList"};
ref::Field fMagicWindowCommand{kMagicWindow, "commandController"};
ref::Field fMagicWindowState{kMagicWindow, "stateMachine"};
ref::Field fMagicViewData{kMagicCommandView, "<Data>k__BackingField"};

ref::Method mItemSetCursor{kItemCommand, "SetCommandSelectCursor", 0};
ref::Method mEquipSetCursor{kEquipCommand, "SetCursor", 1};
ref::Method mMagicSetCursor{kMagicCommand, "SetCommandSelectCursor", 0};
ref::Method mItemWindowSetActive{kItemWindow, "SetActive", 1};
ref::Method mEquipWindowSetActive{kEquipWindow, "SetActive", 1};
ref::Method mMagicCommandInit{kMagicWindow, "CommandInit", 0};
ref::Method mItemCommandSelectInit{kItemWindow, "CommandSelectInit", 0};
ref::Method mEquipCommandInit{kEquipWindow, "CommandInit", 0};

// Command-phase state-machine tags.
constexpr int kStateItemCommand = 1;    // ItemWindowController command-select
constexpr int kStateEquipCommand = 1;   // EquipmentWindowController command
constexpr int kStateMagicCommand = 4;   // AbilityWindowController.State.Command (Use/Forget bar)

// Last announced focused index per bar (FF1 _lastItemIndex / _lastEquipIndex / _lastMagicIndex).
// Session scope: reset exactly where FF1 reset them.
constexpr const char* kCtxItem = "Item.Command";
constexpr const char* kCtxEquip = "Equip.Command";
constexpr const char* kCtxMagic = "Magic.Command";
constexpr dedup::Scope kScope = dedup::Scope::Session;

// Frames a scheduled read may wait for the focused command's data.
constexpr int kReadMaxFrames = 10;

// Read generations: a newer event for the same bar supersedes a pending read.
int g_item_gen = 0;
int g_equip_gen = 0;
int g_magic_gen = 0;

// Cached window controllers, re-found when destroyed or INACTIVE (FindObjectOfType returns only
// active objects, so a cached-but-inactive window after a back-out must count as a miss).
void* g_item_win = nullptr;
void* g_equip_win = nullptr;
void* g_magic_win = nullptr;

void* cached_window(void*& cache, ref::Class& cls)
{
    if (!unity::alive(cache) || !unity::active_in_hierarchy(cache)) cache = unity::find_object(cls);
    return cache;
}

void* item_win() { return cached_window(g_item_win, kItemWindow); }
void* equip_win() { return cached_window(g_equip_win, kEquipWindow); }
void* magic_win() { return kMagicWindow.applies() ? cached_window(g_magic_win, kMagicWindow) : nullptr; }

// ReadRoutine: one frame after the event, retry (bounded) only while "not ready"; stop early if
// a newer event for the same bar superseded it.
void schedule_read(int& gen_counter, void* ctrl, bool (*try_read)(void*), const char* label)
{
    if (!ctrl) return;
    const int gen = ++gen_counter;
    int* counter = &gen_counter;
    int tries = 0;
    frame::every_frame([gen, counter, ctrl, try_read, tries]() mutable {
        if (gen != *counter) return false;
        bool done = hooks::guarded_r("CommandBar.Read", true, [&] { return try_read(ctrl); });
        return !done && ++tries < kReadMaxFrames;
    }, label, kReadMaxFrames);
}

// --- Item command bar (Use / Sort / Key Items). False only to retry (focus data not set yet). ---
bool try_read_item(void* ctrl)
{
    if (!unity::alive(ctrl)) return true;

    // Only while the item window is in its command-select phase.
    void* win = item_win();
    if (!win || ui::state_tag(win, fItemWindowState) != kStateItemCommand) {
        dedup::reset(kCtxItem);
        return true;
    }

    void* cur = ref::rd<void*>(ctrl, fItemCursor);
    void* list = ref::rd<void*>(ctrl, fItemContents);
    if (!cur || !list) return false;
    const int index = ui::cursor_index(cur);
    const int count = unity::list_count(list);
    if (index < 0 || index >= count) return false;
    // The data is checked before the index compare (FF1 compared first; the outcome is the same:
    // an unchanged index is never spoken, an unready one retries).
    void* view = unity::list_at(list, index);
    if (!view) return false;
    void* data = ref::rd<void*>(view, fItemViewData);
    if (!data) return false;   // focus not set yet: retry (don't store the index)
    const std::wstring name = item_command_name(ref::rd<int32_t>(data, fItemDataId));
    if (name.empty()) return false;
    if (!dedup::index(kCtxItem, index, kScope)) return true;
    announce_command(name, index, count);
    return true;
}

// --- Equipment command bar (Equip / Optimal / Remove All) ---
// Name from the focused-command cash (refreshed by the bar's per-frame UpdateFocus, hence the
// deferred read); index and count from its cursor and contents list.
bool try_read_equip(void* ctrl)
{
    if (!unity::alive(ctrl)) return true;

    void* win = equip_win();
    if (!win || ui::state_tag(win, fEquipWindowState) != kStateEquipCommand) {
        dedup::reset(kCtxEquip);
        return true;
    }

    void* cur = ref::rd<void*>(ctrl, fEquipCursor);
    void* list = ref::rd<void*>(ctrl, fEquipContents);
    if (!cur || !list) return false;
    const int index = ui::cursor_index(cur);
    const int count = unity::list_count(list);
    if (index < 0 || index >= count) return false;
    const std::wstring name = equipment_command_name(ref::rd<int32_t>(ctrl, fEquipIdCash));
    if (name.empty()) return false;
    if (!dedup::index(kCtxEquip, index, kScope)) return true;
    announce_command(name, index, count);
    return true;
}

// --- Magic command bar (Use / Forget), FF1 only ---
bool try_read_magic(void* ctrl)
{
    if (!unity::alive(ctrl) || !unity::active_in_hierarchy(ctrl)) return true;

    void* win = magic_win();
    if (!win || ui::state_tag(win, fMagicWindowState) != kStateMagicCommand) {
        dedup::reset(kCtxMagic);   // not in the command bar: re-arm for the next entry
        return true;
    }

    void* cur = ref::rd<void*>(ctrl, fMagicCursor);
    void* list = ref::rd<void*>(ctrl, fMagicContents);
    if (!cur || !list) return false;
    const int index = ui::cursor_index(cur);
    const int count = unity::list_count(list);
    if (index < 0 || index >= count) return false;
    void* view = unity::list_at(list, index);
    if (!view) return false;
    void* data = ref::rd<void*>(view, fMagicViewData);
    if (!data) return false;   // focus not set yet: retry
    const std::wstring name = ability_command_name(ref::rd<int32_t>(data, fAbilityDataId));
    if (name.empty()) return false;
    if (!dedup::index(kCtxMagic, index, kScope)) return true;
    announce_command(name, index, count);
    return true;
}

void schedule_item_read(void* ctrl) { schedule_read(g_item_gen, ctrl, &try_read_item, "CommandBar.Item"); }
void schedule_equip_read(void* ctrl) { schedule_read(g_equip_gen, ctrl, &try_read_equip, "CommandBar.Equip"); }
void schedule_magic_read(void* ctrl) { schedule_read(g_magic_gen, ctrl, &try_read_magic, "CommandBar.Magic"); }

// --- detours ---------------------------------------------------------------------------------
using Void_t = void (*)(void* self, const void* mi);
using Int_t = void (*)(void* self, int32_t value, const void* mi);
using Bool_t = void (*)(void* self, bool value, const void* mi);
Void_t o_ItemSetCursor;
Int_t o_EquipSetCursor;
Void_t o_MagicSetCursor;
Bool_t o_ItemWindowSetActive;
Bool_t o_EquipWindowSetActive;
Void_t o_MagicCommandInit;
Void_t o_ItemCommandSelectInit;
Void_t o_EquipCommandInit;

// Cursor-set events (open / move / click).
void d_ItemSetCursor(void* self, const void* mi)
{
    o_ItemSetCursor(self, mi);
    hooks::guarded("CommandBar.ItemCursorSet", [&] { if (kItemCommand.is(self)) schedule_item_read(self); });
}

void d_EquipSetCursor(void* self, int32_t index, const void* mi)
{
    o_EquipSetCursor(self, index, mi);
    hooks::guarded("CommandBar.EquipCursorSet", [&] { if (kEquipCommand.is(self)) schedule_equip_read(self); });
}

void d_MagicSetCursor(void* self, const void* mi)
{
    o_MagicSetCursor(self, mi);
    hooks::guarded("CommandBar.MagicCursorSet", [&] { if (kMagicCommand.is(self)) schedule_magic_read(self); });
}

// Window SetActive(false): reset the last index (re-announce on reopen).
void d_ItemWindowSetActive(void* self, bool active, const void* mi)
{
    o_ItemWindowSetActive(self, active, mi);
    if (!active) dedup::reset(kCtxItem);
}

void d_EquipWindowSetActive(void* self, bool active, const void* mi)
{
    o_EquipWindowSetActive(self, active, mi);
    if (!active) dedup::reset(kCtxEquip);
}

// Command-state ENTRY (from char-select AND from a sub-list back-out, where the cursor does not
// move): reset and read, so the bar re-announces its focused command every time it gains focus.
void d_MagicCommandInit(void* self, const void* mi)
{
    o_MagicCommandInit(self, mi);
    hooks::guarded("CommandBar.MagicCommandInit", [&] {
        dedup::reset(kCtxMagic);
        if (!kMagicWindow.is(self)) return;
        if (void* cmd = ref::rd<void*>(self, fMagicWindowCommand)) schedule_magic_read(cmd);
    });
}

void d_ItemCommandSelectInit(void* self, const void* mi)
{
    o_ItemCommandSelectInit(self, mi);
    hooks::guarded("CommandBar.ItemCommandSelectInit", [&] {
        dedup::reset(kCtxItem);
        if (!kItemWindow.is(self)) return;
        if (void* cmd = ref::rd<void*>(self, fItemWindowCommand)) schedule_item_read(cmd);
    });
}

void d_EquipCommandInit(void* self, const void* mi)
{
    o_EquipCommandInit(self, mi);
    hooks::guarded("CommandBar.EquipCommandInit", [&] {
        dedup::reset(kCtxEquip);
        if (!kEquipWindow.is(self)) return;
        if (void* cmd = ref::rd<void*>(self, fEquipWindowCommand)) schedule_equip_read(cmd);
    });
}

int install()
{
    int n = 0;
    if (hooks::attach(mItemSetCursor, &d_ItemSetCursor, &o_ItemSetCursor)) ++n;
    if (hooks::attach(mEquipSetCursor, &d_EquipSetCursor, &o_EquipSetCursor)) ++n;
    if (kMagicCommand.applies() && hooks::attach(mMagicSetCursor, &d_MagicSetCursor, &o_MagicSetCursor)) ++n;
    if (hooks::attach(mItemWindowSetActive, &d_ItemWindowSetActive, &o_ItemWindowSetActive)) ++n;
    if (hooks::attach(mEquipWindowSetActive, &d_EquipWindowSetActive, &o_EquipWindowSetActive)) ++n;
    if (kMagicWindow.applies() && hooks::attach(mMagicCommandInit, &d_MagicCommandInit, &o_MagicCommandInit)) ++n;
    if (hooks::attach(mItemCommandSelectInit, &d_ItemCommandSelectInit, &o_ItemCommandSelectInit)) ++n;
    if (hooks::attach(mEquipCommandInit, &d_EquipCommandInit, &o_EquipCommandInit)) ++n;
    LOGF("[CommandBar] Command-bar patches applied (%d)", n);

    queries::define("CommandBarPatches.IsCommandBarActive", [] { return is_command_bar_active(); });
    return n;
}

core::Install s_install{"CommandBar", &install, 100};

} // namespace

// IsCommandBarActive: a command bar is in its command-select phase (Item 1, Equip 1, Magic 4).
// The generic cursor reader stays silent then, so it does not double the bar's announce.
bool is_command_bar_active()
{
    return hooks::guarded_r("CommandBar.IsActive", false, [] {
        void* item = item_win();
        if (item && ui::state_tag(item, fItemWindowState) == kStateItemCommand) return true;
        void* equip = equip_win();
        if (equip && ui::state_tag(equip, fEquipWindowState) == kStateEquipCommand) return true;
        void* magic = magic_win();
        if (magic && ui::state_tag(magic, fMagicWindowState) == kStateMagicCommand) return true;
        return false;
    });
}

// --- CommandBarReader ------------------------------------------------------------------------------

void announce_command(const std::wstring& name)
{
    if (name.empty()) return;
    ffpr::say(name, true);
}

void announce_command(const std::wstring& name, int index, int count)
{
    announce_command(ui::menu_position(name, index, count));
}

std::wstring shop_command_name(int id)
{
    switch (id) {   // Last.Defaine.ShopCommandId
    case 1: return T(L"Buy");
    case 2: return T(L"Sell");
    case 3: return T(L"Equipment");
    case 4: return T(L"Back");
    default: return {};
    }
}

std::wstring item_command_name(int id)
{
    switch (id) {   // Last.Defaine.UI.ItemCommandId
    case 1: return T(L"Use");
    case 2: return T(L"Sort");        // Organize
    case 3: return T(L"Key Items");   // Important
    default: return {};
    }
}

std::wstring equipment_command_name(int id)
{
    switch (id) {   // Last.UI.EquipmentCommandId
    case 1: return T(L"Equip");
    case 2: return T(L"Optimal");     // Strongest
    case 3: return T(L"Remove All");  // RemoveEverything
    default: return {};
    }
}

std::wstring ability_command_name(int id)
{
    switch (id) {   // Last.Defaine.UI.AbilityCommandId
    case 1: return T(L"Use");
    case 2: return T(L"Forget");
    case 3: return T(L"Memorize");
    case 4: return T(L"Remove");
    case 5: return T(L"Exchange");
    default: return {};
    }
}

std::wstring menu_command_name(int id)
{
    switch (id) {   // Last.Defaine.MenuCommandId
    case 1: return T(L"Item");
    case 2: return T(L"Magic");
    case 3: return T(L"Equipment");
    case 4: return T(L"Status");
    case 5: return T(L"Order");   // the main menu's "Order" (MSG_SYSTEM_046), not the item menu's Sort
    case 6: return T(L"Words");
    case 7: return T(L"Config");
    case 8: return T(L"Interruption");
    case 9: return T(L"Save");
    case 10: return T(L"Back");
    case 11: return T(L"Job");
    case 12: return T(L"Ability");
    case 13: return T(L"Load");
    default: return {};
    }
}

} // namespace ffpr::system
