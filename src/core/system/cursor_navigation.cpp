// Ports FF1 Core/FFI_ScreenReaderMod.cs: ManualPatches (CursorNavigation_Postfix and its menu
// routing, ClearAllMenuStates, ClearOtherMenuStates), TryPatchCursorNavigation,
// FFI_ScreenReaderMod.ClearOtherMenuStates / ClearAllMenuStates / ClearMenuFlagsForMapTransition;
// and Patches/CursorNavigationPatches.cs (an empty placeholder in FF1: the patching is here).
#include <string>
#include <utility>
#include <vector>
#include "core.h"
#include "hooks.h"
#include "log.h"
#include "refs.h"
#include "text.h"
#include "ui_util.h"
#include "unity.h"
#include "dialogue/dialogue.h"
#include "system/cursor_event.h"
#include "system/system.h"

namespace cursor {
namespace {
CursorEvent g_current;
CursorEvent g_empty;
bool g_active = false;
std::vector<const char*>* g_suppressors = nullptr;

// The generic reader's scope: current() is this event until the scope ends (nesting restores).
struct Scope {
    CursorEvent saved;
    bool was_active;
    explicit Scope(CursorEvent e) : saved(std::move(g_current)), was_active(g_active)
    {
        g_current = std::move(e);
        g_active = true;
    }
    ~Scope()
    {
        g_current = std::move(saved);
        g_active = was_active;
    }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};

const std::vector<const char*>& suppressors()
{
    static const std::vector<const char*> none;
    return g_suppressors ? *g_suppressors : none;
}

} // namespace

const CursorEvent& current() { return g_active ? g_current : g_empty; }

void add_suppressor(const char* query_name)
{
    if (!query_name) return;
    if (!g_suppressors) g_suppressors = new std::vector<const char*>();
    g_suppressors->push_back(query_name);
}

} // namespace cursor

namespace ffpr::system {

// Used by menu_text_discovery.cpp: the cursor being read, for the CharacterSelectionReader query.
void with_cursor_event(void* cur, int index, void* transform, void (*fn)(void*), void* state)
{
    cursor::CursorEvent e;
    e.cursor = cur;
    e.index = index;
    e.instance = cur;
    e.transform = transform;
    cursor::Scope scope(std::move(e));
    fn(state);
}

namespace {

ref::Class kCursor{"Last.UI", "Cursor"};
ref::Method mNextIndex{kCursor, "NextIndex", 3};
ref::Method mPrevIndex{kCursor, "PrevIndex", 3};
ref::Method mSkipNextIndex{kCursor, "SkipNextIndex", 5};
ref::Method mSkipPrevIndex{kCursor, "SkipPrevIndex", 5};

// NextIndex/PrevIndex(Action<int> action, int count, bool isLoop = false)
using Index_t = void (*)(void* self, void* action, int32_t count, bool is_loop, const void* mi);
// SkipNextIndex/SkipPrevIndex(Action<int> action, int count, int skipCount, bool isEndPoint, bool isLoop)
using SkipIndex_t = void (*)(void* self, void* action, int32_t count, int32_t skip_count, bool is_end_point,
                             bool is_loop, const void* mi);
Index_t o_NextIndex;
Index_t o_PrevIndex;
SkipIndex_t o_SkipNextIndex;
SkipIndex_t o_SkipPrevIndex;

void run_action(const char* name)
{
    if (actions::defined(name)) actions::run(name);
}

// A per-menu ShouldSuppress. When the owning area has not published the query, fall back to the
// registry flag: FF1 MenuStateBase.ShouldSuppress's default (IsActive).
bool query_is(const char* query, const char* fallback_key)
{
    if (queries::defined(query)) return queries::is(query);
    return fallback_key && menu_state::is_active(fallback_key);
}

// ManualPatches.CursorNavigation_Postfix
void cursor_navigation_postfix(void* cur)
{
    if (!cur || !kCursor.is(cur)) return;

    // === BUILD CURSOR PATH FOR DETECTION === (grandparent/parent/cursor)
    void* t = unity::transform(cur);
    std::wstring path = t ? unity::name(t) : std::wstring(L"null");
    void* p = t ? unity::parent(t) : nullptr;
    if (p) path = unity::name(p) + L"/" + path;
    void* gp = p ? unity::parent(p) : nullptr;
    if (gp) path = unity::name(gp) + L"/" + path;

    cursor::CursorEvent e;
    e.cursor = cur;
    e.index = ui::cursor_index(cur);
    e.instance = cur;
    e.transform = t;
    e.path = path;
    cursor::Scope scope(std::move(e));

    // === BATTLE PAUSE MENU SPECIAL CASE ===
    // Checked BEFORE suppression because battle states would suppress it. The cursor path
    // contains "curosr_parent" (game typo) in the pause menu.
    if (text::contains(path, L"curosr_parent")) {
        wait_and_read_cursor(cur);
        return;
    }

    // Skip if new game or job selection is handling cursor events.
    if (queries::is("NewGamePatches.IsHandlingCursor") || queries::is("JobSelectionPatches.IsHandlingCursor")) {
        // Clear flags to prevent them getting stuck if the player navigates away.
        run_action("NewGamePatches.ClearHandlingFlag");
        run_action("JobSelectionPatches.ClearHandlingFlag");
        return;
    }

    // === ACTIVE STATE CHECKS ===
    // Popup with buttons: read the button, UNLESS the popup has its own focus reader
    // (CommonPopup), which would otherwise double-read it.
    if (dialogue::popup_state::should_suppress()) {
        if (!dialogue::popup_state::has_own_focus_reader()) dialogue::read_current_button(cur);
        return;
    }

    // Save/load confirmation: also uses PopupState for button reading.
    if (save_load_should_suppress()) {
        dialogue::read_current_button(cur);
        return;
    }

    if (query_is("ConfigMenuState.ShouldSuppress", menu_state::CONFIG_MENU)) return;
    if (query_is("BattleCommandState.ShouldSuppress", menu_state::BATTLE_COMMAND)) return;
    if (query_is("BattleTargetState.ShouldSuppress", menu_state::BATTLE_TARGET)) return;
    if (query_is("BattleItemMenuState.ShouldSuppress", menu_state::BATTLE_ITEM)) return;
    if (query_is("BattleMagicMenuState.ShouldSuppress", menu_state::BATTLE_MAGIC)) return;
    if (queries::is("ShopMenuTracker.ValidateState")) return;
    if (query_is("ItemMenuState.ShouldSuppress", menu_state::ITEM_MENU)) return;
    if (query_is("EquipMenuState.ShouldSuppress", menu_state::EQUIP_MENU)) return;
    if (query_is("MagicMenuState.ShouldSuppress", menu_state::MAGIC_MENU)) return;
    if (query_is("StatusMenuState.ShouldSuppress", menu_state::STATUS_MENU)) return;
    if (query_is("StatusDetailsState.ShouldSuppress", menu_state::STATUS_DETAILS)) return;

    // Extras menus: bestiary, music player, gallery have their own patches.
    if (queries::is("BestiaryStateTracker.IsInBestiary")) return;
    if (queries::is("MusicPlayerStateTracker.IsInMusicPlayer")) return;
    if (queries::is("GalleryStateTracker.IsInGallery")) return;

    // Command bars (Item/Equip/Magic): CommandBarPatches announces the focused command with its
    // index. Checked after the list-phase checks so heavy list scrolling never pays the lookup.
    if (is_command_bar_active()) return;

    // A game module's own screens.
    for (const char* q : cursor::suppressors())
        if (queries::is(q)) return;

    // === DEFAULT: Read via MenuTextDiscovery (one frame later, after the UI updates) ===
    wait_and_read_cursor(cur);
}

void d_NextIndex(void* self, void* action, int32_t count, bool is_loop, const void* mi)
{
    o_NextIndex(self, action, count, is_loop, mi);
    hooks::guarded("Cursor.NextIndex", [&] { cursor_navigation_postfix(self); });
}

void d_PrevIndex(void* self, void* action, int32_t count, bool is_loop, const void* mi)
{
    o_PrevIndex(self, action, count, is_loop, mi);
    hooks::guarded("Cursor.PrevIndex", [&] { cursor_navigation_postfix(self); });
}

void d_SkipNextIndex(void* self, void* action, int32_t count, int32_t skip, bool end_point, bool is_loop,
                     const void* mi)
{
    o_SkipNextIndex(self, action, count, skip, end_point, is_loop, mi);
    hooks::guarded("Cursor.SkipNextIndex", [&] { cursor_navigation_postfix(self); });
}

void d_SkipPrevIndex(void* self, void* action, int32_t count, int32_t skip, bool end_point, bool is_loop,
                     const void* mi)
{
    o_SkipPrevIndex(self, action, count, skip, end_point, is_loop, mi);
    hooks::guarded("Cursor.SkipPrevIndex", [&] { cursor_navigation_postfix(self); });
}

// ManualPatches.menuKeyMap: caller strings to MenuStateRegistry keys.
const char* menu_key(const std::string& except_menu)
{
    static const std::pair<const char*, const char*> kMap[] = {
        {"Config", menu_state::CONFIG_MENU},
        {"BattleCommand", menu_state::BATTLE_COMMAND},
        {"BattleTarget", menu_state::BATTLE_TARGET},
        {"BattleItem", menu_state::BATTLE_ITEM},
        {"BattleMagic", menu_state::BATTLE_MAGIC},
        {"Item", menu_state::ITEM_MENU},
        {"Equip", menu_state::EQUIP_MENU},
        {"Magic", menu_state::MAGIC_MENU},
        {"Status", menu_state::STATUS_MENU},
        {"StatusDetails", menu_state::STATUS_DETAILS},
    };
    for (const auto& kv : kMap)
        if (except_menu == kv.first) return kv.second;
    return nullptr;
}

// TryPatchCursorNavigation
int install()
{
    int n = 0;
    if (hooks::attach(mNextIndex, &d_NextIndex, &o_NextIndex)) ++n;
    if (hooks::attach(mPrevIndex, &d_PrevIndex, &o_PrevIndex)) ++n;
    if (hooks::attach(mSkipNextIndex, &d_SkipNextIndex, &o_SkipNextIndex)) ++n;
    if (hooks::attach(mSkipPrevIndex, &d_SkipPrevIndex, &o_SkipPrevIndex)) ++n;
    LOGF("[Cursor] %d cursor navigation patches applied", n);

    actions::define("Mod.ClearAllMenuStates", [] { clear_all_menu_states(); });
    actions::define("Mod.ClearMenuFlagsForMapTransition", [] { clear_menu_flags_for_map_transition(); });
    // FFI_ScreenReaderMod.ClearOtherMenuStates("<Menu>"), one action per caller argument.
    actions::define("Mod.ClearOtherMenuStates.Config", [] { clear_other_menu_states("Config"); });
    actions::define("Mod.ClearOtherMenuStates.BattleCommand", [] { clear_other_menu_states("BattleCommand"); });
    actions::define("Mod.ClearOtherMenuStates.BattleTarget", [] { clear_other_menu_states("BattleTarget"); });
    actions::define("Mod.ClearOtherMenuStates.BattleItem", [] { clear_other_menu_states("BattleItem"); });
    actions::define("Mod.ClearOtherMenuStates.BattleMagic", [] { clear_other_menu_states("BattleMagic"); });
    actions::define("Mod.ClearOtherMenuStates.Item", [] { clear_other_menu_states("Item"); });
    actions::define("Mod.ClearOtherMenuStates.Equip", [] { clear_other_menu_states("Equip"); });
    actions::define("Mod.ClearOtherMenuStates.Magic", [] { clear_other_menu_states("Magic"); });
    actions::define("Mod.ClearOtherMenuStates.Status", [] { clear_other_menu_states("Status"); });
    actions::define("Mod.ClearOtherMenuStates.StatusDetails", [] { clear_other_menu_states("StatusDetails"); });
    actions::define("Mod.ClearOtherMenuStates.Shop", [] { clear_other_menu_states("Shop"); });

    // FF1 MapTransitionPatches.CheckMapTransition calls ClearMenuFlagsForMapTransition when the
    // map id changes, before "Entering X".
    events::on(events::Kind::MapChanged, [] { clear_menu_flags_for_map_transition(); });
    return n;
}

core::Install s_install{"CursorNavigation", &install, 100};

} // namespace

// ManualPatches.ClearOtherMenuStates
void clear_other_menu_states(const std::string& except_menu)
{
    // Reset all registry states except the one being activated AND keep MAIN_MENU active: it is
    // the parent container for all sub-menus (clearing it leaves a gap where no menu is active).
    if (const char* except_key = menu_key(except_menu)) {
        const std::string keep = except_key;
        for (const std::string& key : menu_state::active_states())
            if (key != keep && key != menu_state::MAIN_MENU)
                menu_state::reset(key.c_str());   // C# SetActive(key, false): runs the reset handler
    } else {
        menu_state::reset_all();
    }

    // Shop is not in the registry: clear separately.
    if (except_menu != "Shop") actions::run("ShopMenuTracker.ClearState");
}

// ManualPatches.ClearAllMenuStates
void clear_all_menu_states()
{
    menu_state::reset_all();
    actions::run("ShopMenuTracker.ClearState");
    run_action("BattlePausePatches.Reset");
}

// FFI_ScreenReaderMod.ClearMenuFlagsForMapTransition: menu and popup flags after a map change.
// Battle flags are left alone (battle scene changes never change the map id). POPUP is cleared
// because some flows (game over -> load, scene reset) dismiss popups without Popup.Close.
void clear_menu_flags_for_map_transition()
{
    const std::vector<std::string> stuck = menu_state::active_states();
    if (!stuck.empty()) {
        std::string joined;
        for (const std::string& s : stuck) {
            if (!joined.empty()) joined += ",";
            joined += s;
        }
        LOGF("[MapTransition] Pre-clear active flags: %s", joined.c_str());
    }

    static const char* const kKeys[] = {
        menu_state::MAIN_MENU, menu_state::MAGIC_MENU, menu_state::ITEM_MENU, menu_state::EQUIP_MENU,
        menu_state::STATUS_MENU, menu_state::STATUS_DETAILS, menu_state::CONFIG_MENU, menu_state::SAVE_LOAD_MENU,
        menu_state::SHOP_MENU, menu_state::BESTIARY_LIST, menu_state::BESTIARY_DETAIL,
        menu_state::BESTIARY_FORMATION, menu_state::BESTIARY_MAP, menu_state::MUSIC_PLAYER, menu_state::GALLERY,
        menu_state::POPUP,
    };
    for (const char* k : kKeys) menu_state::reset(k);
    actions::run("ShopMenuTracker.ClearState");
    dialogue::popup_state::clear();
}

} // namespace ffpr::system
