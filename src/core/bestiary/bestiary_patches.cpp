// Ports FF1 Patches/BestiaryPatches.cs: BestiaryNavigationTracker, BestiaryStateTracker,
// BestiaryManualPatches (state changes, list, detail, formation, minimap, full map) and
// BestiaryManualPatches.ConfigBestiaryStateHandler. All five games; no hooked body is folded
// (each RVA appears once in every dump.cs). LibraryMenuController.ChangeState is inlined in FF1,
// so the minimap open/close is read one frame after SetupKeyHelp / SetupEnlargedMapKeyHelp.
//
// Not ported: GameObjectCache.Register(LibraryInfoController) (nothing reads that cache entry;
// the native readers use FindObjectOfType) and ConfigBestiaryStateHandler._previousState
// (written, never read).
#include <vector>
#include "bestiary.h"
#include "../core.h"
#include "../dedup.h"
#include "../frame.h"
#include "../game.h"
#include "../hooks.h"
#include "../log.h"
#include "../modtext.h"
#include "../refs.h"
#include "../say.h"
#include "../text.h"
#include "../unity.h"

namespace ffpr::bestiary {
namespace {

// --- game names -----------------------------------------------------------------------------

ref::Class  kExtraLibrary{"Last.Management", "SubSceneManagerExtraLibrary"};
ref::Method mExtraLibraryChangeState{kExtraLibrary, "ChangeState", 1};

ref::Class  kLibMenu{"Last.UI.KeyInput", "LibraryMenuController"};
ref::Method mLibMenuShow{kLibMenu, "Show", 3};
ref::Method mLibMenuOnContentSelected{kLibMenu, "OnContentSelected", 2};
ref::Method mLibMenuSetupKeyHelp{kLibMenu, "SetupKeyHelp", 0};
ref::Method mLibMenuSetupEnlargedMapKeyHelp{kLibMenu, "SetupEnlargedMapKeyHelp", 0};
ref::Field  fLibMenuSelectState{kLibMenu, "selectState"};

ref::Class  kLibInfo{"Last.UI.KeyInput", "LibraryInfoController"};
ref::Method mLibInfoSetData{kLibInfo, "SetData", 1};

ref::Class  kLibList{"Last.UI.KeyInput", "LibraryMenuListController"};
ref::Method mLibListCurrentContent{kLibList, "GetCurrentContent", 0};

ref::Class  kExtraLibraryInfo{"Last.Scene", "ExtraLibraryInfo"};
ref::Method mInfoOnChangedMonster{kExtraLibraryInfo, "OnChangedMonster", 1};
ref::Class  kExtraLibraryField{"Last.Scene", "ExtraLibraryField"};
ref::Method mFieldNextMap{kExtraLibraryField, "NextMap", 0};
ref::Method mFieldPreviousMap{kExtraLibraryField, "PreviousMap", 0};
// The config-menu bestiary: OnChangedMonster(MonsterData), not the (MonsterData, bool) overload.
ref::Class  kMenuExtraLibraryInfo{"Last.Scene", "MenuExtraLibraryInfo"};
ref::Method mMenuInfoOnChangedMonster{kMenuExtraLibraryInfo, "OnChangedMonster", 1};

ref::Class  kArBattleTop{"Last.UI.Common", "ArBattleTopController"};
ref::Method mInitMonsterPartyList{kArBattleTop, "InitMonsterPartyList", 0};
ref::Method mChangeMonsterParty{kArBattleTop, "ChangeMonsterParty", 0};
ref::Field  fMonsterPartyList{kArBattleTop, "monsterPartyList"};
ref::Field  fSelectMonsterPartyIndex{kArBattleTop, "selectMonsterPartyIndex"};

ref::Class  kPictureBookClient{"Last.Management", "PictureBookClient"};
ref::Method mPictureBookClientInstance{kPictureBookClient, "Instance", 0};
ref::Method mGetPictureBooks{kPictureBookClient, "GetPictureBooks", 0};

ref::Class  kInfoContent{"Last.UI.Common.Library", "LibraryInfoContent"};

ref::Class  kMonsterData{"Last.OutGame.Library", "MonsterData"};
ref::Field  fMonsterPictureBook{kMonsterData, "<pictureBookData>k__BackingField"};
ref::Method mMonsterHabitatNames{kMonsterData, "get_HabitatNameList", 0};

ref::Class  kPictureBookData{"Last.Data.PictureBooks", "PictureBookData"};
ref::Method mPbIsRelease{kPictureBookData, "get_IsRelease", 0};
ref::Method mPbMonsterName{kPictureBookData, "get_MonsterName", 0};

// SubSceneManagerExtraLibrary.State
constexpr int kStateList = 1;
constexpr int kStateField = 2;
constexpr int kStateInfo = 4;
constexpr int kStateArTop = 5;
constexpr int kStateGotoTitle = 7;
// LibraryMenuController.State
constexpr int kSelectMonsterList = 0;
constexpr int kSelectEnlargedMap = 1;

// --- state ----------------------------------------------------------------------------------

// BestiaryStateTracker
int g_current_state = -1;
bool g_suppress_next_list_entry = false;
int g_full_map_index = 0;
std::wstring g_cached_entry_name;                 // C# null = empty
std::vector<std::wstring> g_cached_habitat_names; // C# null = empty

Tracker g_tracker;

// Bestiary.Minimap: FF1's lastSelectState, a state-change check (DEDUP_INVENTORY.md row 8).
// lastSelectState started at -1 (no baseline: the first read only records) and
// ResetUpdateControllerState set it to 0 (the list). Reset only there, as in FF1.
constexpr const char* kCtxMinimap = "Bestiary.Minimap";
bool g_minimap_baseline = false;

// ConfigBestiaryStateHandler.WasInConfigBestiary
bool g_was_in_config_bestiary = false;

// The formation entry read is pending: set on ChangeState(ArTop), cleared by whichever event
// reads it first (the one-frame entry check, or InitMonsterPartyList when the list fills later).
bool g_formation_announce_pending = false;

void* picture_book(void* monster_data)
{
    return monster_data ? ref::rd<void*>(monster_data, fMonsterPictureBook) : nullptr;
}

void reset_bestiary_menu_states()
{
    menu_state::reset(menu_state::BESTIARY_LIST);
    menu_state::reset(menu_state::BESTIARY_DETAIL);
    menu_state::reset(menu_state::BESTIARY_FORMATION);
    menu_state::reset(menu_state::BESTIARY_MAP);
}

// BestiaryManualPatches.ResetUpdateControllerState
void reset_update_controller_state()
{
    dedup::reset(kCtxMinimap);
    dedup::index(kCtxMinimap, kSelectMonsterList, dedup::Scope::Session);
    g_minimap_baseline = true;
}

// --- AnnounceListOpen -----------------------------------------------------------------------

// One frame after the list opens: the encounter summary. Two frames later: the entry under the
// cursor (queued behind the summary); the list-entry suppression ends either way.
void list_open_entry()
{
    hooks::guarded("Bestiary.ListOpenEntry", [] {
        void* list_controller = unity::find_object(kLibList);
        if (!list_controller) return;
        void* data = ref::call<void*>(mLibListCurrentContent, list_controller);
        if (!data) return;
        tracker().current_monster_data = data;
        if (void* pbd = picture_book(data)) {
            std::wstring entry = read_list_entry(pbd);
            if (!entry.empty()) ffpr::say(entry, false);
        }
    });
    g_suppress_next_list_entry = false;
}

void announce_list_open()
{
    frame::next_frame([] {
        hooks::guarded("Bestiary.ListOpenSummary", [] {
            void* client = ref::call_static<void*>(mPictureBookClientInstance);
            if (!client) return;
            void* list = ref::call<void*>(mGetPictureBooks, client);
            std::wstring summary = read_encounter_summary(list);
            if (!summary.empty()) ffpr::say(summary, true);
        });
        frame::after_frames(2, [] { list_open_entry(); }, "Bestiary.ListOpen");
    }, "Bestiary.ListOpen");
}

// --- AnnounceMapView ------------------------------------------------------------------------

void announce_map_view()
{
    frame::after_frames(2, [] {
        if (!g_cached_habitat_names.empty())
            ffpr::say(TF(L"Map open: {0}", {g_cached_habitat_names[0]}), true);
        else
            ffpr::say(T(L"Map open"), true);
    }, "Bestiary.MapView");
}

// --- formation ------------------------------------------------------------------------------

void read_current_formation(void* controller)
{
    try {
        void* party_list = ref::rd<void*>(controller, fMonsterPartyList);
        int party_index = ref::rd<int32_t>(controller, fSelectMonsterPartyIndex);
        int count = unity::list_count(party_list);
        if (!party_list || count == 0) {
            ffpr::say(T(L"No formations available"), true);
            return;
        }
        if (party_index < 0 || party_index >= count) party_index = 0;

        std::wstring announcement = read_formation(party_index, unity::list_at(party_list, party_index));
        if (count > 1)
            announcement += L" " + TF(L"({0} of {1})", {text::num(party_index + 1), text::num(count)});
        ffpr::say(announcement, true);
    } catch (...) {
        LOGF("[Bestiary] Error reading formation data");
        ffpr::say(T(L"Formation view"), true);
    }
}

// AnnounceFormation: one check a frame after ChangeState(ArTop). A filled list is read now;
// otherwise the InitMonsterPartyList postfix reads it when the game fills it.
void announce_formation()
{
    g_formation_announce_pending = true;
    frame::next_frame([] {
        hooks::guarded("Bestiary.Formation", [] {
            if (!g_formation_announce_pending || !is_in_formation()) return;
            void* controller = unity::find_object(kArBattleTop);
            if (!controller) return;   // not created yet: InitMonsterPartyList will read
            if (unity::list_count(ref::rd<void*>(controller, fMonsterPartyList)) > 0) {
                g_formation_announce_pending = false;
                read_current_formation(controller);
            }
        });
    }, "Bestiary.Formation");
}

void reannounce_formation()
{
    if (!is_in_formation()) return;
    if (void* controller = unity::find_object(kArBattleTop)) read_current_formation(controller);
}

// --- Patch 1: SubSceneManagerExtraLibrary.ChangeState ---------------------------------------

void on_change_state(int state)
{
    int previous_state = g_current_state;
    g_current_state = state;

    // Clear every bestiary menu state first.
    reset_bestiary_menu_states();

    switch (state) {
    case kStateList:
        menu_state::set_active(menu_state::BESTIARY_LIST, true);
        if (previous_state <= 0) {
            // Entering the bestiary from outside.
            tracker().reset();
            g_suppress_next_list_entry = true;
            announce_list_open();
        } else {
            // Back from the detail, map or formation view.
            std::wstring reannounce;
            if (previous_state == kStateField && !g_cached_entry_name.empty()) {
                reannounce = TF(L"Map closed. {0}", {g_cached_entry_name});
                g_cached_entry_name.clear();
                g_cached_habitat_names.clear();
            } else if (void* pbd = picture_book(tracker().current_monster_data)) {
                reannounce = read_list_entry(pbd);
            }
            tracker().reset();
            if (!reannounce.empty()) ffpr::say(reannounce, true);
        }
        break;

    case kStateField: {   // the full map
        menu_state::set_active(menu_state::BESTIARY_MAP, true);
        g_full_map_index = 0;
        if (void* data = tracker().current_monster_data) {
            if (void* pbd = picture_book(data)) g_cached_entry_name = read_list_entry(pbd);
            void* habitats = ref::call<void*>(mMonsterHabitatNames, data);
            int count = unity::list_count(habitats);
            if (habitats && count > 0) {
                g_cached_habitat_names.clear();
                for (int i = 0; i < count; ++i) {
                    void* name = unity::list_at(habitats, i);
                    g_cached_habitat_names.push_back(name ? ref::str(name) : T(L"Unknown location"));
                }
            }
        }
        announce_map_view();
        break;
    }

    case kStateInfo:
        menu_state::set_active(menu_state::BESTIARY_DETAIL, true);
        break;

    case kStateArTop:
        menu_state::set_active(menu_state::BESTIARY_FORMATION, true);
        announce_formation();
        break;

    case kStateGotoTitle:   // leaving the bestiary
        clear_state();
        break;

    default:
        break;
    }
}

using ChangeState_t = void (*)(void* self, int32_t state, const void* mi);
ChangeState_t o_ChangeState;
void d_ChangeState(void* self, int32_t state, const void* mi)
{
    o_ChangeState(self, state, mi);
    hooks::guarded("SubSceneManagerExtraLibrary.ChangeState", [&] { on_change_state(state); });
}

// --- Patch 2 / 2b: list entry and cursor ----------------------------------------------------

using Show_t = void (*)(void* self, void* select_data, bool is_init, bool is_in_game, const void* mi);
Show_t o_LibMenuShow;
void d_LibMenuShow(void* self, void* select_data, bool is_init, bool is_in_game, const void* mi)
{
    o_LibMenuShow(self, select_data, is_init, is_in_game, mi);
    hooks::guarded("LibraryMenuController.Show", [&] {
        if (!select_data) return;
        tracker().current_monster_data = select_data;
        if (!is_in_list() || g_suppress_next_list_entry) return;
        void* pbd = picture_book(select_data);
        if (!pbd) return;
        std::wstring entry = read_list_entry(pbd);
        if (!entry.empty()) ffpr::say(entry, true);
    });
}

using OnContentSelected_t = void (*)(void* self, int32_t index, void* monster_data, const void* mi);
OnContentSelected_t o_LibMenuOnContentSelected;
void d_LibMenuOnContentSelected(void* self, int32_t index, void* monster_data, const void* mi)
{
    o_LibMenuOnContentSelected(self, index, monster_data, mi);
    hooks::guarded("LibraryMenuController.OnContentSelected", [&] {
        if (!monster_data || !is_in_list()) return;
        tracker().current_monster_data = monster_data;
        if (g_suppress_next_list_entry) return;
        void* pbd = picture_book(monster_data);
        if (!pbd) return;
        // No position suffix: the catalog number ("042: Goblin") already gives the position.
        std::wstring entry = read_list_entry(pbd);
        if (!entry.empty()) ffpr::say(entry, true);
    });
}

// --- Patch 3: detail view -------------------------------------------------------------------

// BuildAndInitializeStatBuffer: the sole detail announcer. The "Name: {monster}" entry heads the
// buffer and is read on entry.
void build_and_initialize_stat_buffer()
{
    void* content = unity::find_object(kInfoContent);
    if (!content) {
        LOGF("[Bestiary] LibraryInfoContent not found");
        return;
    }
    Tracker& t = tracker();
    std::vector<StatEntry> entries = build_stat_buffer(content, t.current_monster_data);

    void* pbd = picture_book(t.current_monster_data);
    std::wstring name = (pbd && ref::call<bool>(mPbIsRelease, pbd)) ? ref::call_str(mPbMonsterName, pbd) : T(L"Unknown");
    entries.insert(entries.begin(), StatEntry{T(L"Name"), name, StatGroup::MonsterData});

    nav_initialize(entries);
    t.is_navigation_active = !entries.empty();
    if (!entries.empty()) ffpr::say(entries[0].to_string(), false);
    menu_state::set_active(menu_state::BESTIARY_DETAIL, true);
}

using SetData_t = void (*)(void* self, void* data, const void* mi);
SetData_t o_LibInfoSetData;
void d_LibInfoSetData(void* self, void* data, const void* mi)
{
    o_LibInfoSetData(self, data, mi);
    hooks::guarded("LibraryInfoController.SetData", [&] {
        if (!self || !data) return;
        Tracker& t = tracker();
        t.current_monster_data = data;
        t.active_controller = self;
        // DelayedDetailAnnouncement: two frames, for the detail UI to fill.
        void* controller = self;
        frame::after_frames(2, [controller] {
            hooks::guarded("Bestiary.DelayedDetail", [&] {
                if (!unity::alive(controller) || !unity::active_in_hierarchy(controller)) return;
                build_and_initialize_stat_buffer();
            });
        }, "Bestiary.Detail");
    });
}

// --- Patch 5 / 9: monster switching in the detail view --------------------------------------
// Only keeps CurrentMonsterData fresh: the announce and buffer rebuild come from SetData.

using OnChangedMonster_t = void (*)(void* self, void* data, const void* mi);
OnChangedMonster_t o_InfoOnChangedMonster;
void d_InfoOnChangedMonster(void* self, void* data, const void* mi)
{
    o_InfoOnChangedMonster(self, data, mi);
    hooks::guarded("ExtraLibraryInfo.OnChangedMonster", [&] {
        if (!data || !is_in_detail()) return;
        tracker().current_monster_data = data;
    });
}

OnChangedMonster_t o_MenuInfoOnChangedMonster;
void d_MenuInfoOnChangedMonster(void* self, void* data, const void* mi)
{
    o_MenuInfoOnChangedMonster(self, data, mi);
    hooks::guarded("MenuExtraLibraryInfo.OnChangedMonster", [&] {
        if (!data || !is_in_detail()) return;
        tracker().current_monster_data = data;
    });
}

// --- Patch 6: the list's minimap ------------------------------------------------------------

// CheckSelectStateChange: compare selectState one frame after the key-help setup.
void check_select_state_change(void* controller)
{
    if (!unity::alive(controller) || !is_in_list()) return;
    int current = ref::rd<int32_t>(controller, fLibMenuSelectState);
    bool changed = dedup::index(kCtxMinimap, current, dedup::Scope::Session);
    if (changed && g_minimap_baseline) {
        Tracker& t = tracker();
        if (current == kSelectEnlargedMap) {
            if (void* pbd = picture_book(t.current_monster_data)) g_cached_entry_name = read_list_entry(pbd);
            std::wstring map_info = T(L"Minimap open");
            if (t.current_monster_data) {
                std::wstring map_name = read_map_name(t.current_monster_data, 0);
                if (!map_name.empty()) map_info = TF(L"Minimap open: {0}", {map_name});
            }
            ffpr::say(map_info, true);
        } else if (current == kSelectMonsterList) {
            std::wstring close_msg = T(L"Minimap closed");
            if (!g_cached_entry_name.empty()) close_msg += L". " + g_cached_entry_name;
            g_cached_entry_name.clear();
            ffpr::say(close_msg, true);
        }
    }
    g_minimap_baseline = true;
}

// SetupKeyHelp / SetupEnlargedMapKeyHelp run inside the inlined state switch just before
// selectState is written (also on list open and cursor moves, where it does not change).
void key_help_setup(void* self)
{
    if (!self || !is_in_list()) return;
    void* controller = self;
    frame::next_frame([controller] {
        hooks::guarded("Bestiary.SelectState", [&] { check_select_state_change(controller); });
    }, "Bestiary.SelectState");
}

using Void_t = void (*)(void* self, const void* mi);
Void_t o_SetupKeyHelp;
void d_SetupKeyHelp(void* self, const void* mi)
{
    o_SetupKeyHelp(self, mi);
    hooks::guarded("LibraryMenuController.SetupKeyHelp", [&] { key_help_setup(self); });
}

Void_t o_SetupEnlargedMapKeyHelp;
void d_SetupEnlargedMapKeyHelp(void* self, const void* mi)
{
    o_SetupEnlargedMapKeyHelp(self, mi);
    hooks::guarded("LibraryMenuController.SetupEnlargedMapKeyHelp", [&] { key_help_setup(self); });
}

// --- Patch 6b / 7: formation ----------------------------------------------------------------

Void_t o_InitMonsterPartyList;
void d_InitMonsterPartyList(void* self, const void* mi)
{
    o_InitMonsterPartyList(self, mi);
    hooks::guarded("ArBattleTopController.InitMonsterPartyList", [&] {
        if (!g_formation_announce_pending || !is_in_formation() || !self) return;
        g_formation_announce_pending = false;
        read_current_formation(self);   // an empty list says "No formations available"
    });
}

Void_t o_ChangeMonsterParty;
void d_ChangeMonsterParty(void* self, const void* mi)
{
    o_ChangeMonsterParty(self, mi);
    hooks::guarded("ArBattleTopController.ChangeMonsterParty", [] {
        if (!is_in_formation()) return;
        frame::next_frame([] { hooks::guarded("Bestiary.FormationChange", [] { reannounce_formation(); }); },
                          "Bestiary.FormationChange");
    });
}

// --- Patch 8: full map cycling --------------------------------------------------------------

void announce_full_map_cycle(int direction)
{
    frame::next_frame([direction] {
        if (g_cached_habitat_names.empty()) return;
        int count = static_cast<int>(g_cached_habitat_names.size());
        g_full_map_index += direction;
        if (g_full_map_index >= count) g_full_map_index = 0;
        else if (g_full_map_index < 0) g_full_map_index = count - 1;
        const std::wstring& name = g_cached_habitat_names[static_cast<size_t>(g_full_map_index)];
        if (!name.empty()) ffpr::say(name, true);
    }, "Bestiary.MapCycle");
}

Void_t o_NextMap;
void d_NextMap(void* self, const void* mi)
{
    o_NextMap(self, mi);
    hooks::guarded("ExtraLibraryField.NextMap", [] {
        if (is_in_map()) announce_full_map_cycle(1);
    });
}

Void_t o_PreviousMap;
void d_PreviousMap(void* self, const void* mi)
{
    o_PreviousMap(self, mi);
    hooks::guarded("ExtraLibraryField.PreviousMap", [] {
        if (is_in_map()) announce_full_map_cycle(-1);
    });
}

// --- install --------------------------------------------------------------------------------

int install()
{
    int n = 0;
    auto add = [&n](bool ok, const char* what) {
        if (ok) ++n;
        else LOGF("[Bestiary] %s not hooked", what);
    };
    add(hooks::attach(mExtraLibraryChangeState, &d_ChangeState, &o_ChangeState), "SubSceneManagerExtraLibrary.ChangeState");
    add(hooks::attach(mLibMenuShow, &d_LibMenuShow, &o_LibMenuShow), "LibraryMenuController.Show");
    add(hooks::attach(mLibMenuOnContentSelected, &d_LibMenuOnContentSelected, &o_LibMenuOnContentSelected),
        "LibraryMenuController.OnContentSelected");
    add(hooks::attach(mLibInfoSetData, &d_LibInfoSetData, &o_LibInfoSetData), "LibraryInfoController.SetData");
    add(hooks::attach(mInfoOnChangedMonster, &d_InfoOnChangedMonster, &o_InfoOnChangedMonster),
        "ExtraLibraryInfo.OnChangedMonster");
    add(hooks::attach(mLibMenuSetupKeyHelp, &d_SetupKeyHelp, &o_SetupKeyHelp), "LibraryMenuController.SetupKeyHelp");
    add(hooks::attach(mLibMenuSetupEnlargedMapKeyHelp, &d_SetupEnlargedMapKeyHelp, &o_SetupEnlargedMapKeyHelp),
        "LibraryMenuController.SetupEnlargedMapKeyHelp");
    add(hooks::attach(mInitMonsterPartyList, &d_InitMonsterPartyList, &o_InitMonsterPartyList),
        "ArBattleTopController.InitMonsterPartyList");
    add(hooks::attach(mChangeMonsterParty, &d_ChangeMonsterParty, &o_ChangeMonsterParty),
        "ArBattleTopController.ChangeMonsterParty");
    add(hooks::attach(mFieldNextMap, &d_NextMap, &o_NextMap), "ExtraLibraryField.NextMap");
    add(hooks::attach(mFieldPreviousMap, &d_PreviousMap, &o_PreviousMap), "ExtraLibraryField.PreviousMap");
    add(hooks::attach(mMenuInfoOnChangedMonster, &d_MenuInfoOnChangedMonster, &o_MenuInfoOnChangedMonster),
        "MenuExtraLibraryInfo.OnChangedMonster");
    LOGF("[Bestiary] %d patches applied", n);

    queries::define("BestiaryStateTracker.IsInBestiary", [] { return is_in_bestiary(); });
    queries::define("BestiaryStateTracker.IsInList", [] { return is_in_list(); });
    queries::define("BestiaryStateTracker.IsInDetail", [] { return is_in_detail(); });
    queries::define("BestiaryStateTracker.IsInFormation", [] { return is_in_formation(); });
    queries::define("BestiaryStateTracker.IsInMap", [] { return is_in_map(); });
    queries::define("BestiaryNavigationTracker.IsNavigationActive", [] { return tracker().is_navigation_active; });
    queries::define("BestiaryNavigationTracker.ValidateState", [] { return tracker().validate_state(); });
    queries::define("BestiaryNavigationTracker.IsActive", [] {
        const Tracker& t = tracker();
        return t.is_navigation_active && t.validate_state();
    });
    queries::define("ConfigBestiaryStateHandler.WasInConfigBestiary", [] { return was_in_config_bestiary(); });
    actions::define("ConfigBestiaryStateHandler.HandleExit", [] { config_handle_exit(); });
    actions::define("ConfigBestiaryStateHandler.HandleStateChange.MenuLibraryUi", [] { config_handle_library_state(true); });
    actions::define("ConfigBestiaryStateHandler.HandleStateChange.MenuLibraryInfo", [] { config_handle_library_state(false); });
    return n;
}

core::Install s_install{"Bestiary", &install, 130};

} // namespace

// --- BestiaryNavigationTracker --------------------------------------------------------------

Tracker& tracker() { return g_tracker; }

void Tracker::reset()
{
    is_navigation_active = false;
    current_monster_data = nullptr;
    active_controller = nullptr;
    nav_reset();
}

bool Tracker::validate_state() const
{
    return is_navigation_active && current_monster_data && active_controller && unity::alive(active_controller) &&
           unity::active_in_hierarchy(active_controller);
}

// --- BestiaryStateTracker -------------------------------------------------------------------

bool is_in_bestiary() { return g_current_state >= 1 && g_current_state <= 6; }
bool is_in_list() { return g_current_state == kStateList; }
bool is_in_detail() { return g_current_state == kStateInfo; }
bool is_in_formation() { return g_current_state == kStateArTop; }
bool is_in_map() { return g_current_state == kStateField; }

void clear_state()
{
    g_current_state = -1;
    g_suppress_next_list_entry = false;
    g_full_map_index = 0;
    g_cached_entry_name.clear();
    g_cached_habitat_names.clear();
    reset_bestiary_menu_states();
    tracker().reset();
    reset_update_controller_state();
}

// --- ConfigBestiaryStateHandler -------------------------------------------------------------

void config_handle_library_state(bool list)
{
    int previous_bestiary_state = g_current_state;
    g_was_in_config_bestiary = true;
    menu_state::reset(menu_state::BESTIARY_LIST);
    menu_state::reset(menu_state::BESTIARY_DETAIL);

    if (!list) {
        // MenuLibraryInfo: the detail. Its announce and stat buffer come from
        // LibraryInfoController.SetData, which fires for the config path too.
        g_current_state = kStateInfo;
        menu_state::set_active(menu_state::BESTIARY_DETAIL, true);
        return;
    }

    // MenuLibraryUi: the list.
    g_current_state = kStateList;
    menu_state::set_active(menu_state::BESTIARY_LIST, true);
    if (previous_bestiary_state <= 0) {
        tracker().reset();
        g_suppress_next_list_entry = true;
        announce_list_open();
    } else if (previous_bestiary_state == kStateInfo) {
        tracker().reset();
        void* list_controller = unity::find_object(kLibList);
        if (!list_controller) return;
        void* data = ref::call<void*>(mLibListCurrentContent, list_controller);
        if (!data) return;
        tracker().current_monster_data = data;
        if (void* pbd = picture_book(data)) {
            std::wstring entry = read_list_entry(pbd);
            if (!entry.empty()) ffpr::say(entry, true);
        }
    }
}

void config_handle_state_change(int main_game_state)
{
    // SubSceneManagerMainGame.State MenuLibraryUi / MenuLibraryInfo, from each game's dump.cs.
    int library_ui = 17, library_info = 18;   // FF3, FF4, FF5
    if (game::is(game::Id::FF1)) { library_ui = 19; library_info = 20; }
    else if (game::is(game::Id::FF2)) { library_ui = 18; library_info = 19; }

    hooks::guarded("ConfigBestiaryStateHandler.HandleStateChange", [&] {
        if (main_game_state == library_ui) config_handle_library_state(true);
        else if (main_game_state == library_info) config_handle_library_state(false);
    });
}

void config_handle_exit()
{
    hooks::guarded("ConfigBestiaryStateHandler.HandleExit", [] {
        g_was_in_config_bestiary = false;
        clear_state();
    });
}

bool was_in_config_bestiary() { return g_was_in_config_bestiary; }

} // namespace ffpr::bestiary
