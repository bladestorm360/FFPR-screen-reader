// Ports FF1 Patches/BattleCommandPatches.cs: "X's turn", the command menu, and target selection
// (initial focus on state entry and cursor moves), with the command back-out re-announce.
//
// All classes are Last.UI.KeyInput and exist in all five games with these names.
// Folded body: BattleTargetSelectController.ShowWindow shares its RVA with 9 other methods in every
// game (x10), so the detour class-checks self. Every other hooked RVA is unique.
// SelectContent has two 2-argument overloads (IEnumerable<BattlePlayerData> / <BattleEnemyData>);
// they are told apart by the first parameter's type name, as the C# code did.
//
// FF6: every hooked method has FF6's FF1-FF5 signature (dump.cs). FF6 also enters single-ally
// targeting in side attacks through SideAttackPlayerInit / EnemySideAttackPlayerInit, which, like
// PlayerInit, do not call SelectContent (GameAssembly.dll), so they get PlayerInit's initial read
// (FF6-only refs, unique RVAs). Their enemy-side counterparts call SelectContent(enemy) themselves.
#include <string>
#include <vector>
#include "battle_internal.h"
#include "battle_util.h"
#include "../core.h"
#include "../dedup.h"
#include "../frame.h"
#include "../hooks.h"
#include "../log.h"
#include "../modtext.h"
#include "../say.h"
#include "../settings.h"
#include "../text.h"
#include "../ui_util.h"
#include "../unity.h"

namespace ffpr::battle {
namespace {

ref::Class kCmdSelect{"Last.UI.KeyInput", "BattleCommandSelectController"};
ref::Method mSetCommandData{kCmdSelect, "SetCommandData", 3};
ref::Method mSetCursor{kCmdSelect, "SetCursor", 1};
ref::Field fContentList{kCmdSelect, "contentList"};
ref::Class kCmdContent{"Last.UI.KeyInput", "BattleCommandSelectContentController"};
ref::Field fTargetCommand{kCmdContent, "<TargetCommand>k__BackingField"};

ref::Class kTargetSelect{"Last.UI.KeyInput", "BattleTargetSelectController"};
ref::Method mShowWindow{kTargetSelect, "ShowWindow", 1};
ref::Method mEnemysInit{kTargetSelect, "EnemysInit", 0};
ref::Method mPlayerInit{kTargetSelect, "PlayerInit", 0};
ref::Field fSelectCursor{kTargetSelect, "selectCursor"};
ref::Field fPlayerDataList{kTargetSelect, "playerDataList"};
ref::Field fEnemyDataList{kTargetSelect, "enemyDataList"};
ref::Field fTargetPlayerList{kTargetSelect, "<TargetPlayerList>k__BackingField"};
ref::Field fTargetEnamyList{kTargetSelect, "<TargetEnamyList>k__BackingField"};
ref::Class kTargetSelectFF6{"Last.UI.KeyInput", "BattleTargetSelectController", game::kFF6};
ref::Method mSideAttackPlayerInit{kTargetSelectFF6, "SideAttackPlayerInit", 0};
ref::Method mEnemySideAttackPlayerInit{kTargetSelectFF6, "EnemySideAttackPlayerInit", 0};

ref::Class kTransform{"UnityEngine", "Transform"};

// lastCharacterId (character identity) and lastAnnouncedCmdMesId (command identity: the menu has
// two pages and left/right switches page while the index stays, so the index cannot be used).
constexpr const char* kTurnCtx = "BattleCommand.Turn";
constexpr const char* kCommandCtx = "BattleCommand.Command";

// Command-announce window: opened by the SetCommandData postfix ("X's turn"); closed by the
// SetCommandData prefix and by ShowWindow(false) (the actor's commit/teardown, where the spurious
// "Attack" bursts fire). SetCursor only announces while it is open.
bool g_command_turn_ready = false;

// Cached target controller (identity only; re-checked with unity::alive before use).
void* g_cached_target_controller = nullptr;

// Command back-out re-announce one-shot. Armed when the player leaves the command menu for a
// sub-context (targeting, the magic/item list). When set, SetCursor does not announce inline: a
// commit's teardown burst is identical to a cancel return at that instant, so the split is made
// one frame later from observed state.
bool g_command_reannounce_pending = false;
// Bumped on every SetCursor: a later cursor event supersedes a pending deferred re-announce.
int g_reannounce_gen = 0;
// Bumped in the SetCommandData prefix: a change means a new turn started (a commit).
int g_set_command_data_seq = 0;

// Initial-target read generation: bumped by every EnemysInit/PlayerInit and by ShowWindow(false).
int g_target_read_gen = 0;
// Bumped each time the enemy SelectContent hook speaks. In FF2-FF5, EnemysInit calls the enemy
// SelectContent itself (KeyInput EnemysInit +0x139 / +0x148 in each GameAssembly.dll; FF1's does
// not), so the entry target was spoken by that hook and again by the initial read. EnemysInit
// compares this around its original: the initial read runs only when the game's own call spoke
// nothing (one event, one line; an identity check, not a text comparison).
int g_enemy_spoken_seq = 0;
// Frames a pending initial-target read may wait for the cursor and list.
constexpr int kTargetReadMaxFrames = 10;

void* content_list(void* controller) { return controller ? ref::rd<void*>(controller, fContentList) : nullptr; }

/// ", Poison, Sleep" from a battle unit's CurrentConditionList (BuildStatusSuffix).
std::wstring status_suffix(void* param)
{
    if (!param) return std::wstring();
    std::vector<std::wstring> names;
    for (void* c : current_conditions(param)) {
        std::wstring n = condition_name(c);
        if (!blank(n)) names.push_back(n);
    }
    if (names.empty()) return std::wstring();
    return L", " + text::join(names, L", ");
}

/// Announces the command at contentList[index] (name + "(X of Y)" over the active slots), deduped
/// by command id.
void announce_command_at(void* list, int index)
{
    int count = unity::list_count(list);
    if (count == 0) return;
    if (index < 0 || index >= count) return;
    void* cc = unity::list_at(list, index);
    if (!cc) return;
    void* command = ref::rd<void*>(cc, fTargetCommand);
    if (!command) return;
    std::wstring mes_id = command_mes_id(command);
    if (blank(mes_id)) return;

    std::wstring name = message(mes_id);
    if (blank(name)) return;
    name = strip_icon(name);

    // Count populated AND active slots on the current page (contentList is a fixed slot list; a
    // stale Extra-page leftover must not inflate it).
    int visible = 0;
    for (int i = 0; i < count; ++i) {
        void* c = unity::list_at(list, i);
        if (!c) continue;
        if (ref::rd<void*>(c, fTargetCommand) && unity::active_in_hierarchy(c)) ++visible;
    }
    if (visible <= 0) visible = count;

    name = ui::menu_position(name, index, visible);
    if (!dedup::text(kCommandCtx, mes_id, dedup::Scope::Session)) return;
    // Command selection doesn't interrupt: it queues after the turn announcement.
    ffpr::say(name, false);
}

/// CheckAndUpdateTargetSelectionActive: verifies the target flag against the controller's view.
bool check_and_update_target_selection_active()
{
    // Fast path: the flag is set by the SelectContent hooks, so a false flag is trusted.
    if (!state_active(menu_state::BATTLE_TARGET)) return false;

    if (!g_cached_target_controller || !unity::alive(g_cached_target_controller))
        g_cached_target_controller = unity::find_object(kTargetSelect);
    if (!g_cached_target_controller) {
        set_target_selection_active(false);
        return false;
    }

    // Active children (the view is shown)?
    bool actually_active = false;
    void* self_go = unity::game_object(g_cached_target_controller);
    for (void* t : unity::components_in_children(g_cached_target_controller, kTransform, false)) {
        if (t && unity::game_object(t) != self_go) {
            actually_active = true;
            break;
        }
    }
    if (!actually_active) set_target_selection_active(false);
    return state_active(menu_state::BATTLE_TARGET);
}

void announce_player_target(void* list, int index)
{
    if (!is_list(list)) return;
    int count = unity::list_count(list);
    if (count == 0) return;
    if (index < 0 || index >= count) return;
    void* player = unity::list_at(list, index);
    if (!player) return;

    std::wstring name = T(L"Unknown");
    int current_hp = 0, max_hp = 0, current_mp = 0, max_mp = 0;
    const bool has_mp = game::flags().has_mp;
    if (void* owned = player_owned_character(player)) {
        std::wstring n = character_name(owned);
        if (!n.empty()) name = n;
        if (void* char_param = character_parameter(owned)) {
            int v = 0;
            if (param_confirmed(char_param, Stat::MaxHp, v)) max_hp = v;
            if (has_mp && param_confirmed(char_param, Stat::MaxMp, v)) max_mp = v;
        }
    }
    void* battle_param = unit_parameter(player);
    if (battle_param) {
        current_hp = param_current_hp(battle_param);
        if (max_hp == 0) max_hp = param_confirmed_or_base(battle_param, Stat::MaxHp);
        if (has_mp) {
            current_mp = param_current_mp(battle_param);
            if (max_mp == 0) max_mp = param_confirmed_or_base(battle_param, Stat::MaxMp);
        }
    }

    // MP joins HP in the games that have it (FF2, FF4, FF5), as their old mods read it.
    std::wstring announcement = has_mp
        ? TF(L"{0}: HP {1}/{2}, MP {3}/{4}", {name, text::num(current_hp), text::num(max_hp),
                                              text::num(current_mp), text::num(max_mp)})
        : TF(L"{0}: HP {1}/{2}", {name, text::num(current_hp), text::num(max_hp)});
    announcement += status_suffix(battle_param);
    announcement = ui::menu_position(announcement, index, count);

    // Entering targeting = left the command menu: arm the back-out re-announce. Safe on commit:
    // SetCursor's target-active check returns before consuming it while the target view is shown.
    g_command_reannounce_pending = true;
    ffpr::say(announcement, true);
}

/// True when it spoke.
bool announce_enemy_target(void* list, int index)
{
    if (!is_list(list)) return false;
    int count = unity::list_count(list);
    if (count == 0) return false;
    if (index < 0 || index >= count) return false;
    void* enemy = unity::list_at(list, index);
    if (!enemy) return false;

    std::wstring name = T(L"Unknown");
    int current_hp = 0, max_hp = 0;
    std::wstring localized = enemy_name(enemy);
    if (!localized.empty()) name = localized;

    void* battle_param = unit_parameter(enemy);
    if (battle_param) {
        current_hp = param_current_hp(battle_param);
        max_hp = param_confirmed_or_base(battle_param, Stat::MaxHp);
    }

    // Several enemies with the same name: append A, B, C... by position in the group.
    std::wstring announcement = name;
    int same_name_count = 0;
    int position_in_group = 0;
    for (int i = 0; i < count; ++i) {
        void* other = unity::list_at(list, i);
        if (!other) continue;
        std::wstring other_name = enemy_name(other);
        if (!other_name.empty() && other_name == name) {
            ++same_name_count;
            if (i < index) ++position_in_group;
        }
    }
    if (same_name_count > 1) {
        announcement += L" ";
        announcement.push_back(static_cast<wchar_t>(L'A' + position_in_group));
    }

    switch (settings::get_int("EnemyHPDisplay", 0)) {
    case 0:   // Numbers (default)
        announcement += TF(L": HP {0}/{1}", {text::num(current_hp), text::num(max_hp)});
        break;
    case 1: { // Percentage
        int pct = max_hp > 0 ? (current_hp * 100 / max_hp) : 0;
        announcement += L": " + text::num(pct) + L"%";
        break;
    }
    default:  // 2 = Hidden: no HP appended
        break;
    }

    announcement += status_suffix(battle_param);
    announcement = ui::menu_position(announcement, index, count);

    g_command_reannounce_pending = true;
    ffpr::say(announcement, true);
    return true;
}

/// Announces the focused target if the cursor and target list are ready. True once it spoke (or
/// can never speak for this instance), false to retry.
bool try_read_initial_target(void* instance, bool is_enemy)
{
    void* controller = instance;
    if (!controller || !unity::alive(controller)) return true;

    // Attack's targeting path presents an INACTIVE controller (magic/item an active one), so
    // activeInHierarchy is not required; prefer an active instance when this one is inactive.
    if (!unity::active_in_hierarchy(controller)) {
        void* active = unity::find_object(kTargetSelect);
        if (active && unity::active_in_hierarchy(active)) controller = active;
    }

    void* cursor = ref::rd<void*>(controller, fSelectCursor);
    if (!cursor) return false;
    int index = ui::cursor_index(cursor);
    if (index < 0) return false;

    if (is_enemy) {
        void* list = ref::rd<void*>(controller, fEnemyDataList);
        if (!is_list(list) || unity::list_count(list) == 0) list = ref::rd<void*>(controller, fTargetEnamyList);
        if (!is_list(list) || index >= unity::list_count(list)) return false;   // not ready: retry
        announce_enemy_target(list, index);
    } else {
        void* list = ref::rd<void*>(controller, fPlayerDataList);
        if (!is_list(list) || unity::list_count(list) == 0) list = ref::rd<void*>(controller, fTargetPlayerList);
        if (!is_list(list) || index >= unity::list_count(list)) return false;
        announce_player_target(list, index);
    }
    return true;
}

/// The single-target state's Init sets the cursor index, so this normally speaks at once; when the
/// cursor/list are not ready yet a bounded retry (started from this event) waits for them.
void begin_initial_target_read(void* instance, bool is_enemy)
{
    int gen = ++g_target_read_gen;
    if (try_read_initial_target(instance, is_enemy)) return;
    frame::every_frame(
        [instance, is_enemy, gen] {
            if (gen != g_target_read_gen) return false;   // superseded, or the window closed
            return !try_read_initial_target(instance, is_enemy);
        },
        "BattleTarget.InitialRead", kTargetReadMaxFrames);
}

// --- SetCommandData (prefix + postfix) ----------------------------------------------------------
using SetCommandData_t = void (*)(void* self, void* data, void* targets, void* items, const void* mi);
SetCommandData_t o_SetCommandData;

void d_SetCommandData(void* self, void* data, void* targets, void* items, const void* mi)
{
    hooks::guarded("BattleCommandSelectController.SetCommandData.prefix", [] {
        // Close the command-announce window before the body runs: the cursor resets it fires during
        // the actor handoff are suppressed.
        g_command_turn_ready = false;
        dedup::reset(kCommandCtx);
        g_command_reannounce_pending = false;   // never carry a back-out arm across the handoff
        ++g_set_command_data_seq;               // the definitive commit signal
    });
    o_SetCommandData(self, data, targets, items, mi);
    hooks::guarded("BattleCommandSelectController.SetCommandData", [&] {
        if (!data) return;
        // Open the window before any early return: a same-character re-entry (cancel from target
        // back to the command menu) is still that actor's input turn.
        g_command_turn_ready = true;
        int character_id_v = character_id(data);
        // Always track the current actor (H key), even for a repeat of the same character.
        set_current_actor(data);

        if (!dedup::index(kTurnCtx, character_id_v, dedup::Scope::Session)) return;

        std::wstring character_name_v = character_name(data);
        if (character_name_v.empty()) return;

        state_set(menu_state::BATTLE_COMMAND, true);
        set_last_selected_command_index(-1);
        state_set(menu_state::BATTLE_MAGIC, false);
        state_set(menu_state::BATTLE_ITEM, false);

        // Turn announcements interrupt.
        ffpr::say(TF(L"{0}'s turn", {character_name_v}), true);
    });
}

// --- SetCursor (postfix) ------------------------------------------------------------------------
using SetCursor_t = void (*)(void* self, int32_t index, const void* mi);
SetCursor_t o_SetCursor;

void d_SetCursor(void* self, int32_t index, const void* mi)
{
    o_SetCursor(self, index, mi);
    hooks::guarded("BattleCommandSelectController.SetCursor", [&] {
        if (!self) return;
        int my_gen = ++g_reannounce_gen;

        // The cursor resets to Attack one frame before the menu goes inactive at end of turn.
        if (!unity::active_in_hierarchy(self)) return;
        // Turn-window gate: only between "X's turn" and the next SetCommandData prefix.
        if (!g_command_turn_ready) return;

        state_set(menu_state::BATTLE_COMMAND, true);
        set_last_selected_command_index(index);
        state_set(menu_state::BATTLE_MAGIC, false);
        state_set(menu_state::BATTLE_ITEM, false);

        // Targeting active: do not announce commands.
        if (check_and_update_target_selection_active()) return;

        // Back-out re-announce: defer one frame and announce only if no commit signal appeared.
        if (g_command_reannounce_pending) {
            g_command_reannounce_pending = false;
            void* controller = self;
            int idx = index;
            int seq = g_set_command_data_seq;
            frame::next_frame(
                [controller, idx, my_gen, seq] {
                    if (my_gen != g_reannounce_gen) return;         // a newer cursor event superseded it
                    if (!g_command_turn_ready) return;              // a commit's ShowWindow(false)
                    if (seq != g_set_command_data_seq) return;      // a turn handoff: a commit
                    if (!unity::alive(controller) || !unity::active_in_hierarchy(controller)) return;
                    // A genuine cancel-return: clear the dedup once so the focused command speaks.
                    dedup::reset(kCommandCtx);
                    announce_command_at(content_list(controller), idx);
                },
                "BattleCommand.Reannounce");
            return;
        }

        announce_command_at(content_list(self), index);
    });
}

// --- SelectContent (postfix), player and enemy overloads ----------------------------------------
using SelectContent_t = void (*)(void* self, void* list, int32_t index, const void* mi);
SelectContent_t o_SelectPlayer;
SelectContent_t o_SelectEnemy;

void d_SelectPlayer(void* self, void* list, int32_t index, const void* mi)
{
    o_SelectPlayer(self, list, index, mi);
    hooks::guarded("BattleTargetSelectController.SelectContent(Player)", [&] {
        set_target_selection_active(true);
        announce_player_target(is_list(list) ? list : nullptr, index);
    });
}

void d_SelectEnemy(void* self, void* list, int32_t index, const void* mi)
{
    o_SelectEnemy(self, list, index, mi);
    hooks::guarded("BattleTargetSelectController.SelectContent(Enemy)", [&] {
        set_target_selection_active(true);
        if (announce_enemy_target(is_list(list) ? list : nullptr, index)) ++g_enemy_spoken_seq;
    });
}

// --- ShowWindow (prefix; folded body, class-checked) --------------------------------------------
using ShowWindow_t = void (*)(void* self, bool is_show, const void* mi);
ShowWindow_t o_ShowWindow;

void d_ShowWindow(void* self, bool is_show, const void* mi)
{
    hooks::guarded("BattleTargetSelectController.ShowWindow", [&] {
        if (!kTargetSelect.is(self)) return;
        set_target_selection_active(is_show);
        if (!is_show) {
            // Hide = the actor committed / target torn down: close the command-announce window and
            // drop any pending initial-target read.
            g_command_turn_ready = false;
            g_cached_target_controller = nullptr;
            ++g_target_read_gen;
        }
    });
    o_ShowWindow(self, is_show, mi);
}

// --- EnemysInit / PlayerInit (postfix): single-target state entry -------------------------------
using Init_t = void (*)(void* self, const void* mi);
Init_t o_EnemysInit;
Init_t o_PlayerInit;

void d_EnemysInit(void* self, const void* mi)
{
    const int spoken_before = g_enemy_spoken_seq;
    o_EnemysInit(self, mi);
    hooks::guarded("BattleTargetSelectController.EnemysInit", [&] {
        if (g_enemy_spoken_seq != spoken_before) {
            ++g_target_read_gen;   // the game's own SelectContent spoke the entry target
            return;
        }
        begin_initial_target_read(self, true);
    });
}

void d_PlayerInit(void* self, const void* mi)
{
    o_PlayerInit(self, mi);
    hooks::guarded("BattleTargetSelectController.PlayerInit", [&] { begin_initial_target_read(self, false); });
}

// FF6 side attacks: the same single-ally state entry as PlayerInit.
Init_t o_SideAttackPlayerInit;
Init_t o_EnemySideAttackPlayerInit;

void d_SideAttackPlayerInit(void* self, const void* mi)
{
    o_SideAttackPlayerInit(self, mi);
    hooks::guarded("BattleTargetSelectController.SideAttackPlayerInit", [&] { begin_initial_target_read(self, false); });
}

void d_EnemySideAttackPlayerInit(void* self, const void* mi)
{
    o_EnemySideAttackPlayerInit(self, mi);
    hooks::guarded("BattleTargetSelectController.EnemySideAttackPlayerInit",
                   [&] { begin_initial_target_read(self, false); });
}

int install()
{
    actions::define("BattleCommandPatches.ClearCachedTargetController", [] { clear_cached_target_controller(); });
    // Game modules with their own battle submenus (FF3's spell list) call this on open.
    actions::define("BattleCommandPatches.NotifyCommandSubmenuActive", [] { notify_command_submenu_active(); });

    int n = 0;
    n += hooks::attach(mSetCommandData, &d_SetCommandData, &o_SetCommandData) ? 1 : 0;
    n += hooks::attach(mSetCursor, &d_SetCursor, &o_SetCursor) ? 1 : 0;

    il2cpp::Class* k = kTargetSelect.get();
    il2cpp::Method* player = find_method_param_type(k, "SelectContent", 2, 0, "BattlePlayerData");
    il2cpp::Method* enemy = find_method_param_type(k, "SelectContent", 2, 0, "BattleEnemyData");
    if (!player && !enemy)
        LOGF("battle: no SelectContent overloads found - cursor-move target announcements unavailable");
    if (player)
        n += hooks::attach_addr(il2cpp::method_pointer(player), "BattleTargetSelectController.SelectContent(Player)",
                                reinterpret_cast<void*>(&d_SelectPlayer), reinterpret_cast<void**>(&o_SelectPlayer)) ? 1 : 0;
    if (enemy)
        n += hooks::attach_addr(il2cpp::method_pointer(enemy), "BattleTargetSelectController.SelectContent(Enemy)",
                                reinterpret_cast<void*>(&d_SelectEnemy), reinterpret_cast<void**>(&o_SelectEnemy)) ? 1 : 0;

    n += hooks::attach(mShowWindow, &d_ShowWindow, &o_ShowWindow) ? 1 : 0;
    n += hooks::attach(mEnemysInit, &d_EnemysInit, &o_EnemysInit) ? 1 : 0;
    n += hooks::attach(mPlayerInit, &d_PlayerInit, &o_PlayerInit) ? 1 : 0;
    // FF6 only (the class ref's mask; silent elsewhere).
    n += hooks::attach(mSideAttackPlayerInit, &d_SideAttackPlayerInit, &o_SideAttackPlayerInit) ? 1 : 0;
    n += hooks::attach(mEnemySideAttackPlayerInit, &d_EnemySideAttackPlayerInit, &o_EnemySideAttackPlayerInit) ? 1 : 0;
    return n;
}
core::Install s_install{"Battle.Command", &install, 110};

} // namespace

void notify_command_submenu_active()
{
    // The magic/item list gained focus: re-open the command-announce window (a target cancel's
    // ShowWindow(false) closed it) and arm the command back-out re-announce.
    g_command_turn_ready = true;
    g_command_reannounce_pending = true;
}

void command_reset_state()
{
    dedup::reset(kTurnCtx);
    g_command_turn_ready = false;
    dedup::reset(kCommandCtx);
    g_command_reannounce_pending = false;
    ++g_target_read_gen;   // drop any pending initial-target read
    set_current_actor(nullptr);
}

void clear_cached_target_controller() { g_cached_target_controller = nullptr; }

} // namespace ffpr::battle
